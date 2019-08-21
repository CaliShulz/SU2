/*!
 * \file driver_direct_singlezone.cpp
 * \brief The main subroutines for driving single-zone problems.
 * \author R. Sanchez
 * \version 6.0.1 "Falcon"
 *
 * The current SU2 release has been coordinated by the
 * SU2 International Developers Society <www.su2devsociety.org>
 * with selected contributions from the open-source community.
 *
 * The main research teams contributing to the current release are:
 *  - Prof. Juan J. Alonso's group at Stanford University.
 *  - Prof. Piero Colonna's group at Delft University of Technology.
 *  - Prof. Nicolas R. Gauger's group at Kaiserslautern University of Technology.
 *  - Prof. Alberto Guardone's group at Polytechnic University of Milan.
 *  - Prof. Rafael Palacios' group at Imperial College London.
 *  - Prof. Vincent Terrapon's group at the University of Liege.
 *  - Prof. Edwin van der Weide's group at the University of Twente.
 *  - Lab. of New Concepts in Aeronautics at Tech. Institute of Aeronautics.
 *
 * Copyright 2012-2019, Francisco D. Palacios, Thomas D. Economon,
 *                      Tim Albring, and the SU2 contributors.
 *
 * SU2 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SU2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SU2. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../../include/drivers/CSinglezoneDriver.hpp"
#include "../../include/definition_structure.hpp"

CSinglezoneDriver::CSinglezoneDriver(char* confFile,
                       unsigned short val_nZone,
                       SU2_Comm MPICommunicator) : CDriver(confFile,
                                                          val_nZone,
                                                          MPICommunicator,
                                                          false) {

  /*--- Initialize the counter for TimeIter ---*/
  TimeIter = 0;

}

CSinglezoneDriver::~CSinglezoneDriver(void) {

}

void CSinglezoneDriver::StartSolver() {

  /*--- Main external loop of the solver. Runs for the number of time steps required. ---*/

  if (rank == MASTER_NODE)
    cout << endl <<"------------------------------ Begin Solver -----------------------------" << endl;

  if (rank == MASTER_NODE){
    cout << endl <<"Simulation Run using the Single-zone Driver" << endl;
    if (driver_config->GetTime_Domain())
      cout << "The simulation will run for " << driver_config->GetnTime_Iter() << " time steps." << endl;
  }

  /*--- Set the initial time iteration to the restart iteration. ---*/
  if (config_container[ZONE_0]->GetRestart() && driver_config->GetTime_Domain())
    TimeIter = config_container[ZONE_0]->GetRestart_Iter();
  
  /*--- Run the problem until the number of time iterations required is reached. ---*/
  while ( TimeIter < config_container[ZONE_0]->GetnTime_Iter() ) {

    /*--- Perform some preprocessing before starting the time-step simulation. ---*/

    Preprocess(TimeIter);

    /*--- Run a time-step iteration of the single-zone problem. ---*/

    Run();

    /*--- Perform some postprocessing on the solution before the update ---*/

    Postprocess();

    /*--- Update the solution for dual time stepping strategy ---*/

    Update();

    /*--- Monitor the computations after each iteration. ---*/

    Monitor(TimeIter);

    /*--- Output the solution in files. ---*/

    Output(TimeIter);

    /*--- If the convergence criteria has been met, terminate the simulation. ---*/

    if (StopCalc) break;

    TimeIter++;

  }

}

void CSinglezoneDriver::Preprocess(unsigned long TimeIter) {
    
  /*--- Set runtime option ---*/
  
  Runtime_Options();
  
  /*--- Set the current time iteration in the config ---*/
  
  config_container[ZONE_0]->SetTimeIter(TimeIter);

  /*--- Store the current physical time in the config container, as
   this can be used for verification / MMS. This should also be more
   general once the drivers are more stable. ---*/
  
  if (config_container[ZONE_0]->GetTime_Marching())
    config_container[ZONE_0]->SetPhysicalTime(static_cast<su2double>(TimeIter)*config_container[ZONE_0]->GetDelta_UnstTimeND());
  else
    config_container[ZONE_0]->SetPhysicalTime(0.0);
  
  /*--- Set the initial condition for EULER/N-S/RANS ---------------------------------------------*/
  if ((config_container[ZONE_0]->GetKind_Solver() ==  EULER) ||
      (config_container[ZONE_0]->GetKind_Solver() ==  NAVIER_STOKES) ||
      (config_container[ZONE_0]->GetKind_Solver() ==  RANS) ||
      (config_container[ZONE_0]->GetKind_Solver() ==  INC_EULER) ||
      (config_container[ZONE_0]->GetKind_Solver() ==  INC_NAVIER_STOKES) ||
      (config_container[ZONE_0]->GetKind_Solver() ==  INC_RANS) ) {
      solver_container[ZONE_0][INST_0][MESH_0][FLOW_SOL]->SetInitialCondition(geometry_container[ZONE_0][INST_0], solver_container[ZONE_0][INST_0], config_container[ZONE_0], TimeIter);
  }

#ifdef HAVE_MPI
  SU2_MPI::Barrier(MPI_COMM_WORLD);
#endif

  /*--- Run a predictor step ---*/
  if (config_container[ZONE_0]->GetPredictor())
    iteration_container[ZONE_0][INST_0]->Predictor(output_container[ZONE_0], integration_container, geometry_container, solver_container,
        numerics_container, config_container, surface_movement, grid_movement, FFDBox, ZONE_0, INST_0);

  /*--- Perform a dynamic mesh update if required. ---*/

  DynamicMeshUpdate(TimeIter);

}

void CSinglezoneDriver::Run() {

  unsigned long OuterIter = 0;
  config_container[ZONE_0]->SetOuterIter(OuterIter);

  /*--- Iterate the zone as a block, either to convergence or to a max number of iterations ---*/
  iteration_container[ZONE_0][INST_0]->Solve(output_container[ZONE_0], integration_container, geometry_container, solver_container,
        numerics_container, config_container, surface_movement, grid_movement, FFDBox, ZONE_0, INST_0);

}

void CSinglezoneDriver::Postprocess() {

    /*--- A corrector step can help preventing numerical instabilities ---*/

    if (config_container[ZONE_0]->GetRelaxation())
      iteration_container[ZONE_0][INST_0]->Relaxation(output_container[ZONE_0], integration_container, geometry_container, solver_container,
          numerics_container, config_container, surface_movement, grid_movement, FFDBox, ZONE_0, INST_0);

}

void CSinglezoneDriver::Update() {

  iteration_container[ZONE_0][INST_0]->Update(output_container[ZONE_0], integration_container, geometry_container,
        solver_container, numerics_container, config_container,
        surface_movement, grid_movement, FFDBox, ZONE_0, INST_0);

}

void CSinglezoneDriver::Output(unsigned long TimeIter) {

  bool output_files = false;
  
  unsigned short RestartFormat = SU2_RESTART_ASCII;
  unsigned short OutputFormat = config_container[ZONE_0]->GetOutput_FileFormat();
  
  bool Wrt_Surf = config_container[ZONE_0]->GetWrt_Srf_Sol();
  bool Wrt_Vol  = config_container[ZONE_0]->GetWrt_Vol_Sol();
  bool Wrt_CSV  = config_container[ZONE_0]->GetWrt_Csv_Sol();
  bool TimeDomain = config_container[ZONE_0]->GetTime_Domain();
  
  if (config_container[ZONE_0]->GetWrt_Binary_Restart()){
    RestartFormat = SU2_RESTART_BINARY;
  }

  /*--- Determine whether a solution needs to be written
   after the current iteration ---*/

  if (

      /*--- General if statements to print output statements ---*/

      (StopCalc) ||

      /*--- Unsteady problems ---*/

      (((config_container[ZONE_0]->GetTime_Marching() == DT_STEPPING_1ST) ||
        (config_container[ZONE_0]->GetTime_Marching() == TIME_STEPPING)) &&
       ((TimeIter == 0) || (TimeIter % config_container[ZONE_0]->GetWrt_Sol_Freq_DualTime() == 0))) ||

      ((config_container[ZONE_0]->GetTime_Marching() == DT_STEPPING_2ND) &&
       ((TimeIter == 0) || ((TimeIter % config_container[ZONE_0]->GetWrt_Sol_Freq_DualTime() == 0) ||
                           ((TimeIter-1) % config_container[ZONE_0]->GetWrt_Sol_Freq_DualTime() == 0)))) ||

      ((config_container[ZONE_0]->GetTime_Marching() == DT_STEPPING_2ND) &&
       ((TimeIter == 0) || ((TimeIter % config_container[ZONE_0]->GetWrt_Sol_Freq_DualTime() == 0)))) ||

      ((config_container[ZONE_0]->GetTime_Domain()) &&
       ((TimeIter == 0) || (TimeIter % config_container[ZONE_0]->GetWrt_Sol_Freq_DualTime() == 0))) ||

      /*--- No inlet profile file found. Print template. ---*/

      (config_container[ZONE_0]->GetWrt_InletFile())

      ) {

    output_files = true;

  }

  /*--- Determine whether a solution doesn't need to be written
   after the current iteration ---*/

//  if (config_container[ZONE_0]->GetFixed_CL_Mode()) {
//    if (config_container[ZONE_0]->GetnExtIter()-config_container[ZONE_0]->GetIter_dCL_dAlpha() - 1 < ExtIter) output_files = false;
//    if (config_container[ZONE_0]->GetnExtIter() - 1 == ExtIter) output_files = true;
//  }

  /*--- write the solution ---*/

  if (output_files && config_container[ZONE_0]->GetWrt_Output()) {

    /*--- Time the output for performance benchmarking. ---*/
#ifndef HAVE_MPI
    StopTime = su2double(clock())/su2double(CLOCKS_PER_SEC);
#else
    StopTime = MPI_Wtime();
#endif
    UsedTimeCompute += StopTime-StartTime;
#ifndef HAVE_MPI
    StartTime = su2double(clock())/su2double(CLOCKS_PER_SEC);
#else
    StartTime = MPI_Wtime();
#endif

    if (rank == MASTER_NODE) cout << endl << "-------------------------- File Output Summary --------------------------";

    /*--- Execute the routine for writing restart, volume solution,
     surface solution, and surface comma-separated value files. ---*/
    
    for (unsigned short iInst = 0; iInst < nInst[ZONE_0]; iInst++){
      
      config_container[ZONE_0]->SetiInst(iInst);
      
      output_container[ZONE_0]->Load_Data(geometry_container[ZONE_0][iInst][MESH_0], config_container[ZONE_0], solver_container[ZONE_0][iInst][MESH_0]);
      
      /*--- Write restart files ---*/
      
      output_container[ZONE_0]->SetVolume_Output(geometry_container[ZONE_0][iInst][MESH_0], config_container[ZONE_0], RestartFormat, TimeDomain);
      
      /*--- Write visualization files ---*/
      
      if (Wrt_Vol)
        output_container[ZONE_0]->SetVolume_Output(geometry_container[ZONE_0][iInst][MESH_0], config_container[ZONE_0], OutputFormat, TimeDomain);
      if (Wrt_Surf)
        output_container[ZONE_0]->SetSurface_Output(geometry_container[ZONE_0][iInst][MESH_0], config_container[ZONE_0], OutputFormat, TimeDomain);
      if (Wrt_CSV)
        output_container[ZONE_0]->SetSurface_Output(geometry_container[ZONE_0][iInst][MESH_0], config_container[ZONE_0], CSV, TimeDomain);    
      
      output_container[ZONE_0]->DeallocateData_Parallel();
      
    }
    if (rank == MASTER_NODE) cout << "-------------------------------------------------------------------------" << endl << endl;

    /*--- Store output time and restart the timer for the compute phase. ---*/
#ifndef HAVE_MPI
    StopTime = su2double(clock())/su2double(CLOCKS_PER_SEC);
#else
    StopTime = MPI_Wtime();
#endif
    UsedTimeOutput += StopTime-StartTime;
    OutputCount++;
    BandwidthSum = config_container[ZONE_0]->GetRestart_Bandwidth_Agg();

  }
  
#ifndef HAVE_MPI
  StartTime = su2double(clock())/su2double(CLOCKS_PER_SEC);
#else
  StartTime = MPI_Wtime();
#endif
  config_container[ZONE_0]->Set_StartTime(StartTime);
  
}

void CSinglezoneDriver::DynamicMeshUpdate(unsigned long TimeIter) {

  /*--- Dynamic mesh update ---*/
  if (config_container[ZONE_0]->GetGrid_Movement()) {
    iteration_container[ZONE_0][INST_0]->SetGrid_Movement(geometry_container[ZONE_0][INST_0],surface_movement[ZONE_0], 
                                                          grid_movement[ZONE_0][INST_0], solver_container[ZONE_0][INST_0],
                                                          config_container[ZONE_0], 0, TimeIter);  }

}

bool CSinglezoneDriver::Monitor(unsigned long TimeIter){

  unsigned long nInnerIter, InnerIter, nTimeIter;
  su2double MaxTime, CurTime;
  bool TimeDomain, InnerConvergence, FinalTimeReached, MaxIterationsReached;
  
  nInnerIter = config_container[ZONE_0]->GetnInner_Iter();
  InnerIter  = config_container[ZONE_0]->GetInnerIter();
  nTimeIter  = config_container[ZONE_0]->GetnTime_Iter();
  MaxTime    = config_container[ZONE_0]->GetMax_Time();
  CurTime    = output_container[ZONE_0]->GetHistoryFieldValue("CUR_TIME");
  
  TimeDomain = config_container[ZONE_0]->GetTime_Domain();
  
  
  /*--- Check whether the inner solver has converged --- */

  if (TimeDomain == NO){
    
    InnerConvergence     = output_container[ZONE_0]->GetConvergence();    
    MaxIterationsReached = InnerIter+1 >= nInnerIter;
        
    if ((MaxIterationsReached || InnerConvergence) && (rank == MASTER_NODE)) {
      cout << endl << "----------------------------- Solver Exit -------------------------------";
      if (InnerConvergence) cout << endl << "Convergence criteria satisfied." << endl;
      else cout << endl << "Maximum number of iterations reached (ITER = " << nInnerIter << " )." << endl;
      cout << "-------------------------------------------------------------------------" << endl;
    }
    
    StopCalc = MaxIterationsReached || InnerConvergence;
  }


  if (TimeDomain == YES) {
    
    /*--- Check whether the outer time integration has reached the final time ---*/
  
    FinalTimeReached     = CurTime >= MaxTime;
    MaxIterationsReached = TimeIter+1 >= nTimeIter;    
    
    if ((FinalTimeReached || MaxIterationsReached) && (rank == MASTER_NODE)){
      cout << endl << "----------------------------- Solver Exit -------------------------------";
      if (FinalTimeReached) cout << endl << "Maximum time reached (MAX_TIME = " << MaxTime << "s)." << endl;
      else cout << endl << "Maximum number of time iterations reached (TIME_ITER = " << nTimeIter << ")." << endl;
      cout << "-------------------------------------------------------------------------" << endl;      
    }
    
    StopCalc = FinalTimeReached || MaxIterationsReached;
  }

  /*--- Reset the inner convergence --- */
  
  output_container[ZONE_0]->SetConvergence(false);

  return StopCalc;
}

void CSinglezoneDriver::Runtime_Options(){
  
  ifstream runtime_configfile;
  
  /*--- Try to open the runtime config file ---*/
  
  runtime_configfile.open(runtime_file_name, ios::in);
  
  /*--- If succeeded create a temporary config object ---*/
  
  if (runtime_configfile.good()){
    CConfig *runtime = new CConfig(runtime_file_name, config_container[ZONE_0]);
    delete runtime;
  }
  
}