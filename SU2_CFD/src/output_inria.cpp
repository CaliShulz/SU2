/*!
 * \file output_inria.cpp
 * \brief Main subroutines for output in Inria format.
 * \author V. Menier, B. Munguía
 * \version 6.1.0 "Falcon"
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
 * Copyright 2012-2018, Francisco D. Palacios, Thomas D. Economon,
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


#include "../include/output_structure.hpp"

enum BCVAR  { bcMach, bcTemp, bcPres, bcDens, bcGoal };

void COutput::SetInriaRestart(CConfig *config, CGeometry *geometry, CSolver **solver, unsigned short val_iZone) {
  
#ifdef HAVE_INRIA
  
  /*--- Local variables ---*/
	
  unsigned short iVar, nDim = geometry->GetnDim();
  unsigned short nVar_Buf = nVar_Par-nDim;
  unsigned long iPoint;
  string filename;
  
  unsigned long OutSol, npoin = geometry->GetGlobal_nPointDomain();
  unsigned long myPoint, offset, Global_Index;
  int VarTyp[GmfMaxTyp];
  passivedouble bufDbl[GmfMaxTyp];
  char OutNam[1024];
	
  SPRINTF (OutNam, "current.solb");

  /*--- Open the restart file and write the solution. ---*/
	
	OutSol = GmfOpenMesh(OutNam,GmfWrite,GmfDouble,nDim);
	
	if ( !OutSol ) {
	  printf("\n\n   !!! Error !!!\n" );
      printf("Unable to open %s", OutNam);
      printf("Now exiting...\n\n");
      exit(EXIT_FAILURE);
	}
	
  /*--- Write the restart file ---*/

	for (iVar = 0; iVar < nVar_Buf; iVar++) {
		VarTyp[iVar] = GmfSca;
	}
	
	if ( !GmfSetKwd(OutSol, GmfSolAtVertices, npoin, nVar_Buf, VarTyp) ) {
	  printf("\n\n   !!! Error !!!\n" );
      printf("Unable to write %s", OutNam);
      printf("Now exiting...\n\n");
      exit(EXIT_FAILURE);
	}

  myPoint = 0;
  offset = 0;
  for (unsigned short iProcessor = 0; iProcessor < size; iProcessor++) {
    if (rank == iProcessor) {
      for (iPoint = 0; iPoint < nParallel_Poin; iPoint++) {
        
        /*--- Global Index of the current point. (note outer loop over procs) ---*/
        
        Global_Index = iPoint + offset;
        
        /*--- Only write original domain points, i.e., exclude any periodic
         or halo nodes, even if they are output in the viz. files. ---*/
        
        if (Global_Index < nPoint_Restart) {
                    
          myPoint++;
          
          /*--- Loop over the variables and write the values to file,
           excluding mesh coordinates [0 - (nDim-1)] ---*/
          
          for (iVar = 0; iVar < nVar_Buf; iVar++){
            bufDbl[iVar] = SU2_TYPE::GetValue(Parallel_Data[iVar+nDim][iPoint]);
          }
          GmfSetLin(OutSol, GmfSolAtVertices, bufDbl);
        }
      }
    }
#ifdef HAVE_MPI
    SU2_MPI::Allreduce(&myPoint, &offset, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
    SU2_MPI::Barrier(MPI_COMM_WORLD);
#endif
    
  }
  
	if ( !GmfCloseMesh(OutSol) ) {
	  printf("\n\n   !!! Error !!!\n" );
      printf("Cannot close solution file %s.", OutNam);
      printf("Now exiting...\n\n");
      exit(EXIT_FAILURE);
	}
  
#else // Not built with Inria support
  
  cout << "GMF file requested but SU2 was built without GMF support. No file written" << "\n";
  
#endif
	
}

/*

	Write solutions of interest : mach, dens, pres etc.

*/
void COutput::WriteInriaOutputs(CConfig *config, CGeometry *geometry, CSolver **solver, unsigned short val_iZone) {
  
#ifdef HAVE_INRIA
  
  /*--- Local variables ---*/
	
  unsigned short nZone = geometry->GetnZone();
  unsigned short Kind_Solver  = config->GetKind_Solver();
  unsigned short iVar, nDim = geometry->GetnDim();
  unsigned short nVar_First = 0, nVar_Second = 0, nVar_Consv_Par;
  unsigned long iPoint;
  unsigned long FirstIndex = NONE, SecondIndex = NONE;
  bool fem = (config->GetKind_Solver() == FEM_ELASTICITY);
  string filename;
  
  unsigned long OutMach, OutPres, OutMetr;
  unsigned long npoin = geometry->GetGlobal_nPointDomain();
  unsigned long myPoint, offset, Global_Index;
  int VarTyp[GmfMaxTyp];
  passivedouble bufDbl[GmfMaxTyp];
  char OutNam[1024];
	
  int NbrVar, idxVar;

  /*--- Use a switch statement to decide how many solver containers we have
   in this zone for output. ---*/
  
  switch (config->GetKind_Solver()) {
    case EULER : case NAVIER_STOKES: FirstIndex = FLOW_SOL; SecondIndex = NONE; break;
    case RANS : FirstIndex = FLOW_SOL; SecondIndex = TURB_SOL; break;
    default: SecondIndex = NONE; break;
  }
  
  nVar_First = solver[FirstIndex]->GetnVar();
  if (SecondIndex != NONE) nVar_Second = solver[SecondIndex]->GetnVar();
  nVar_Consv_Par = nVar_First + nVar_Second;
	
  /* Get indices of mach, pres, etc. in the solution array */
  unsigned short *TagBc;
  TagBc = new unsigned short [100];
	
  idxVar=0;
  idxVar += nDim + nVar_Consv_Par; // Add coordinates and conservative variables
  
  if (!config->GetLow_MemoryOutput()) {
    
    if (config->GetWrt_Limiters()) {
      idxVar += nVar_Consv_Par; // Add limiters
    }
    if (config->GetWrt_Residuals()) {
      idxVar += nVar_Consv_Par; // Add residuals
    }
    
    if ((Kind_Solver == EULER) || (Kind_Solver == NAVIER_STOKES) || (Kind_Solver == RANS)) {
      TagBc[bcPres] = idxVar;
      TagBc[bcTemp] = idxVar+1;
      TagBc[bcMach] = idxVar+2;
      idxVar += 4; // Add pressure, temperature, Mach, Cp
    }
		
  }

  /* Get index of adaptation parameter if performing error estimation */

  if(config->GetError_Estimate() || config->GetKind_SU2() == SU2_MET){
    
    if ((Kind_Solver == NAVIER_STOKES) || (Kind_Solver == RANS)) {
      idxVar += nDim + 2; // Add laminar viscosity, skin friction, heat flux
      if(config->GetBuffet_Monitoring() || config->GetKind_ObjFunc() == BUFFET_SENSOR) idxVar += 1; // Add buffet sensor
    }
        
    if (Kind_Solver == RANS) idxVar += 2; // Add y-plus, eddy viscosity
        
    if (config->GetWrt_SharpEdges()) idxVar += 1; // Add sharp edges
        
    if (config->GetKind_Trans_Model() == BC) idxVar += 1; // Add the intermittency for the BC trans. model
    
    if (config->GetKind_HybridRANSLES()!=NO_HYBRIDRANSLES) idxVar += 2; // Add DES length scale and wall distance
    
    if (config->GetKind_RoeLowDiss() != NO_ROELOWDISS) idxVar += 1; // Add Roe dissipation

    TagBc[bcGoal] = idxVar;
    idxVar += 1; // Add adaptation parameter
  }
	
  /*--- Retrieve filename from config ---*/
  
  if ((config->GetContinuous_Adjoint()) || (config->GetDiscrete_Adjoint())) {
    filename = config->GetRestart_AdjFileName();
    filename = config->GetObjFunc_Extension(filename);
  } else if (fem){
    filename = config->GetRestart_FEMFileName();
  } else {
    filename = config->GetRestart_FlowFileName();
  }

  /*--- Append the zone number if multizone problems ---*/
  if (nZone > 1)
    filename= config->GetMultizone_FileName(filename, val_iZone);

  /*--- Number of variables (1 for sensor files). ---*/
  NbrVar = 1;
  VarTyp[0]  = GmfSca;
	
  /*--- Open the restart file and write the solution. ---*/

  if(config->GetWrt_Aniso_Sensor()){

  /*--- Write MACH ---*/

  SPRINTF (OutNam, "current.mach.solb");
  OutMach = GmfOpenMesh(OutNam,GmfWrite,GmfDouble,nDim);
	
  if ( !OutMach ) {
    printf("\n\n   !!! Error !!!\n" );
    printf("Unable to open %s", OutNam);
    printf("Now exiting...\n\n");
    exit(EXIT_FAILURE);
  }	
	
  if ( !GmfSetKwd(OutMach, GmfSolAtVertices, npoin, NbrVar, VarTyp) ) {
    printf("\n\n   !!! Error !!!\n" );
    printf("Unable to write Mach");
    printf("Now exiting...\n\n");
    exit(EXIT_FAILURE);
  }

  myPoint = 0;
  offset = 0;
  for (unsigned short iProcessor = 0; iProcessor < size; iProcessor++) {
    if (rank == iProcessor) {
      for (iPoint = 0; iPoint < nParallel_Poin; iPoint++) {
        
        /*--- Global Index of the current point. (note outer loop over procs) ---*/
        
        Global_Index = iPoint + offset;
        
        /*--- Only write original domain points, i.e., exclude any periodic
         or halo nodes, even if they are output in the viz. files. ---*/
        
        if (Global_Index < nPoint_Restart) {
                    
          myPoint++;
          
          /*--- Loop over the variables and write the values to file ---*/
          
          iVar = TagBc[bcMach];
          bufDbl[0] = SU2_TYPE::GetValue(Parallel_Data[iVar][iPoint]);
          GmfSetLin(OutMach, GmfSolAtVertices, bufDbl);
        }
      }
    }
#ifdef HAVE_MPI
    SU2_MPI::Allreduce(&myPoint, &offset, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
    SU2_MPI::Barrier(MPI_COMM_WORLD);
#endif
    
  }
	
  if ( !GmfCloseMesh(OutMach) ) {
    printf("\n\n   !!! Error !!!\n" );
    printf("Cannot close solution file");
    printf("Now exiting...\n\n");
    exit(EXIT_FAILURE);
  }
	
  /*--- Write PRES ---*/

  SPRINTF (OutNam, "current.pres.solb");
  OutPres = GmfOpenMesh(OutNam,GmfWrite,GmfDouble,nDim);
	
  if ( !OutPres ) {
    printf("\n\n   !!! Error !!!\n" );
    printf("Unable to open %s", OutNam);
    printf("Now exiting...\n\n");
    exit(EXIT_FAILURE);
  }
	
  if ( !GmfSetKwd(OutPres, GmfSolAtVertices, npoin, NbrVar, VarTyp) ) {
    printf("\n\n   !!! Error !!!\n" );
    printf("Unable to write pressure");
    printf("Now exiting...\n\n");
    exit(EXIT_FAILURE);
  }

  myPoint = 0;
  offset = 0;
  for (unsigned short iProcessor = 0; iProcessor < size; iProcessor++) {
    if (rank == iProcessor) {
      for (iPoint = 0; iPoint < nParallel_Poin; iPoint++) {
        
        /*--- Global Index of the current point. (note outer loop over procs) ---*/
        
        Global_Index = iPoint + offset;
        
        /*--- Only write original domain points, i.e., exclude any periodic
         or halo nodes, even if they are output in the viz. files. ---*/
        
        if (Global_Index < nPoint_Restart) {
                    
          myPoint++;
          
          /*--- Loop over the variables and write the values to file ---*/
          
          iVar = TagBc[bcPres];
          bufDbl[0] = SU2_TYPE::GetValue(Parallel_Data[iVar][iPoint]);
          GmfSetLin(OutPres, GmfSolAtVertices, bufDbl);
        }
      }
    }
#ifdef HAVE_MPI
    SU2_MPI::Allreduce(&myPoint, &offset, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
    SU2_MPI::Barrier(MPI_COMM_WORLD);
#endif
    
  }
		
	/*--- Close files ---*/
  	
  if ( !GmfCloseMesh(OutPres) ) {
    printf("\n\n   !!! Error !!!\n" );
    printf("Cannot close solution file");
    printf("Now exiting...\n\n");
    exit(EXIT_FAILURE);
  }

  } // End Wrt_Aniso_Sensor

  /*--- Write METR ---*/

  if(config->GetError_Estimate() || config->GetKind_SU2() == SU2_MET){

    /*--- Write metric tensor ---*/

    SPRINTF (OutNam, "current_sensor.solb");
    OutMetr = GmfOpenMesh(OutNam,GmfWrite,GmfDouble,nDim);
  
    if ( !OutMetr ) {
      printf("\n\n   !!! Error !!!\n" );
      printf("Unable to open %s", OutNam);
      printf("Now exiting...\n\n");
      exit(EXIT_FAILURE);
    }

    unsigned short iMetr, nMetr;
    if(nDim == 2) nMetr = 3;
    else          nMetr = 6;

    VarTyp[0] = GmfSymMat;
  
    if ( !GmfSetKwd(OutMetr, GmfSolAtVertices, npoin, NbrVar, VarTyp) ) {
      printf("\n\n   !!! Error !!!\n" );
      printf("Unable to write metric");
      printf("Now exiting...\n\n");
      exit(EXIT_FAILURE);
    }

    myPoint = 0;
    offset = 0;
    for (unsigned short iProcessor = 0; iProcessor < size; iProcessor++) {
      if (rank == iProcessor) {
        for (iPoint = 0; iPoint < nParallel_Poin; iPoint++) {
          
          /*--- Global Index of the current point. (note outer loop over procs) ---*/
          
          Global_Index = iPoint + offset;
          
          /*--- Only write original domain points, i.e., exclude any periodic
           or halo nodes, even if they are output in the viz. files. ---*/
          
          if (Global_Index < nPoint_Restart) {
                      
            myPoint++;
            
            /*--- Loop over the variables and write the values to file ---*/
            
            iVar = TagBc[bcGoal];
            for (iMetr = 0; iMetr < nMetr; iMetr++)
              bufDbl[iMetr] = SU2_TYPE::GetValue(Parallel_Data[iVar+iMetr][iPoint]);
            GmfSetLin(OutMetr, GmfSolAtVertices, bufDbl);
          }
        }
      }
#ifdef HAVE_MPI
      SU2_MPI::Allreduce(&myPoint, &offset, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
      SU2_MPI::Barrier(MPI_COMM_WORLD);
#endif
      
    }
    
    /*--- Close files ---*/
    
    if ( !GmfCloseMesh(OutMetr) ) {
      printf("\n\n   !!! Error !!!\n" );
      printf("Cannot close solution file");
      printf("Now exiting...\n\n");
      exit(EXIT_FAILURE);
    }

  }
	
  delete [] TagBc;
  
#else // Not built with Inria support
  
  cout << "GMF file requested but SU2 was built without GMF support. No file written" << "\n";
  
#endif
	
}




void COutput::SetInriaMesh(CConfig *config, CGeometry *geometry) {
  
#ifdef HAVE_INRIA
  
  unsigned long iElem, iPoint, iNode;
  unsigned short iMarker, iDim, nDim = geometry->GetnDim();
  ofstream output_file;
  ifstream input_file;
  string Grid_Marker, text_line, Marker_Tag, str;
	
  unsigned short nMarker = config->GetnMarker_All();
  unsigned long cptElem = 0, nTri=0, nLin=0, nQua=0;
  unsigned long myPoint, offset, Global_Index;

  unsigned long OutMsh;
  passivedouble bufDbl[8];
  char OutNam[2014];
	
  unsigned long *PointSurface=NULL;
  unsigned long nPointSurface=0;
	
  CPrimalGrid* bnd = NULL;

  SPRINTF (OutNam, "current.meshb");
	
  OutMsh = GmfOpenMesh(OutNam,GmfWrite,GmfDouble,nDim);
  if ( !OutMsh ) {
    printf("  ## ERROR: Cannot open mesh file %s ! \n",OutNam);
	return;
  }
  
  /*--- Write vertices ---*/
	
  GmfSetKwd(OutMsh, GmfVertices, nPoint_Restart);

  myPoint = 0;
  offset = 0;
  for (unsigned short iProcessor = 0; iProcessor < size; iProcessor++) {
    if (rank == iProcessor) {
      for (iPoint = 0; iPoint < nParallel_Poin; iPoint++) {
        
        /*--- Global Index of the current point. (note outer loop over procs) ---*/
        
        Global_Index = iPoint + offset;
        
        /*--- Only write original domain points, i.e., exclude any periodic
         or halo nodes, even if they are output in the viz. files. ---*/
        
        if (Global_Index < nPoint_Restart) {
                    
          myPoint++;
          
          /*--- Loop over the variables and write the values to file ---*/
          
          for (iDim = 0; iDim < nDim; iDim++)
            bufDbl[iDim] = SU2_TYPE::GetValue(Parallel_Data[iDim][iPoint]);

          if ( nDim == 2 ) {
            GmfSetLin(OutMsh, GmfVertices,bufDbl[0],bufDbl[1],0); 
          }
          else {
            GmfSetLin(OutMsh, GmfVertices,bufDbl[0],bufDbl[1],bufDbl[2],0); 
          }
        }
      }
    }
#ifdef HAVE_MPI
    SU2_MPI::Allreduce(&myPoint, &offset, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
    SU2_MPI::Barrier(MPI_COMM_WORLD);
#endif
    
  }

  /*--- Write 2D elements ---
    Note: in 3D, triangles/quads are boundary markers
  */
	
  if ( nDim == 2 ){
		
    /*--- Write triangles ---*/
		
	GmfSetKwd(OutMsh, GmfTriangles, nParallel_Tria);
  	for (iElem = 0; iElem < nParallel_Tria; iElem++) {
  	  iNode = iElem*N_POINTS_TRIANGLE;
	  GmfSetLin(OutMsh, GmfTriangles,Conn_Tria_Par[iNode+0],Conn_Tria_Par[iNode+1],Conn_Tria_Par[iNode+2], 1);  
  	}	

	/*--- Write quadrilaterals ---*/
		
	if ( nParallel_Quad > 0  ) {
	  GmfSetKwd(OutMsh, GmfQuadrilaterals, nParallel_Quad);
	  for (iElem = 0; iElem < nParallel_Quad; iElem++) {
  		  iNode = iElem*N_POINTS_QUADRILATERAL;
				GmfSetLin(OutMsh, GmfQuadrilaterals,Conn_Quad_Par[iNode+0],Conn_Quad_Par[iNode+1],Conn_Quad_Par[iNode+2], Conn_Quad_Par[iNode+3], 1);  
  		}
	  }
	
	}
	
	/*--- Write tetrahedra ---*/
	
	if ( nParallel_Tetr > 0  ) {
		GmfSetKwd(OutMsh, GmfTetrahedra, nParallel_Tetr);
		for (iElem = 0; iElem < nParallel_Tetr; iElem++) {
	    iNode = iElem*N_POINTS_TETRAHEDRON;
			GmfSetLin(OutMsh, GmfTetrahedra,Conn_Tetr_Par[iNode+0],Conn_Tetr_Par[iNode+1],Conn_Tetr_Par[iNode+2], Conn_Tetr_Par[iNode+3], 1); 
	  }
	}
	
	/*--- Write hexahedra ---*/
	
	if ( nParallel_Hexa > 0 ) {
		GmfSetKwd(OutMsh, GmfHexahedra, nParallel_Hexa);
		for (iElem = 0; iElem < nParallel_Hexa; iElem++) {
	    iNode = iElem*N_POINTS_HEXAHEDRON;
			GmfSetLin(OutMsh, GmfHexahedra,Conn_Hexa_Par[iNode+0],Conn_Hexa_Par[iNode+1], Conn_Hexa_Par[iNode+2], Conn_Hexa_Par[iNode+3], Conn_Hexa_Par[iNode+4],Conn_Hexa_Par[iNode+5],Conn_Hexa_Par[iNode+6], Conn_Hexa_Par[iNode+7],  1); 
	  }
	}
	
	/*--- Write prisms ---*/
	
	if ( nParallel_Pris > 0 ) {
		GmfSetKwd(OutMsh, GmfPrisms, nParallel_Pris);
		for (iElem = 0; iElem < nParallel_Pris; iElem++) {
	    iNode = iElem*N_POINTS_PRISM;
			GmfSetLin(OutMsh, GmfPrisms,Conn_Pris_Par[iNode+0],Conn_Pris_Par[iNode+1], Conn_Pris_Par[iNode+2], Conn_Pris_Par[iNode+3], Conn_Pris_Par[iNode+4],Conn_Pris_Par[iNode+5],  1); 
	  }
	}
	
	/*--- Write pyramids ---*/
	
	if ( nParallel_Pyra > 0 ) {
		GmfSetKwd(OutMsh, GmfPyramids, nParallel_Pyra);
		for (iElem = 0; iElem < nParallel_Pyra; iElem++) {
	    iNode = iElem*N_POINTS_PYRAMID;
	  	GmfSetLin(OutMsh, GmfPyramids,Conn_Pyra_Par[iNode+0],Conn_Pyra_Par[iNode+1], Conn_Pyra_Par[iNode+2], Conn_Pyra_Par[iNode+3], Conn_Pyra_Par[iNode+4],1); 
		}
	}
	
	
	/* --- Boundary elements ---*/
	
	/*--- Get surface points ---*/
	
	nPointSurface = 0;
	PointSurface = new unsigned long[geometry->GetnPoint()];
  for (iPoint = 0; iPoint < geometry->GetnPoint(); iPoint++) {
    if (geometry->node[iPoint]->GetBoundary()) {
      PointSurface[nPointSurface] = iPoint;
      nPointSurface++;
    }
	}
	
	/*--- Count elements ---*/
	
	nLin = nTri = nQua = 0;
	
	for (iMarker = 0; iMarker < nMarker; iMarker++) {
		for (iElem = 0; iElem < geometry->GetnElem_Bound(iMarker); iElem++) {
			bnd = geometry->bound[iMarker][iElem];
			switch ( bnd->GetVTK_Type() ) {
				case LINE:          nLin++; break;
				case TRIANGLE:      nTri++; break;
				case QUADRILATERAL: nQua++; break;
			}
		}
	}
	
	
	/*--- Write edges ---*/
	
	if ( nLin > 0 ) {
		
		GmfSetKwd(OutMsh, GmfEdges, nLin);
		
		cptElem = 0;
		
		for (iMarker = 0; iMarker < nMarker; iMarker++) {
			for (iElem = 0; iElem < geometry->GetnElem_Bound(iMarker); iElem++) {
				
				bnd = geometry->bound[iMarker][iElem];
				
				if ( bnd->GetVTK_Type() != LINE ) 
					continue;
				
				cptElem++;

				
				GmfSetLin(OutMsh, GmfEdges, bnd->GetNode(0)+1, bnd->GetNode(1)+1, iMarker+2); 	
			}
		}
	
		if ( cptElem != nLin ) {
			cout << "  !! Error Inria output:  Inconsistent number of edges\n" << endl;
			exit(EXIT_FAILURE);
		}
		
	}
	
	/*--- Write triangles ---*/
	
	if ( nTri > 0 ) {
		
		GmfSetKwd(OutMsh, GmfTriangles, nTri);
		
		cptElem = 0;
		
		for (iMarker = 0; iMarker < nMarker; iMarker++) {
			for (iElem = 0; iElem < geometry->GetnElem_Bound(iMarker); iElem++) {
				
				bnd = geometry->bound[iMarker][iElem];
				
				if ( bnd->GetVTK_Type() != TRIANGLE ) 
					continue;
				
				cptElem++;
				
				GmfSetLin(OutMsh, GmfTriangles, bnd->GetNode(0)+1, bnd->GetNode(1)+1, 
                                        bnd->GetNode(2)+1, iMarker+2); 	
			}
		}
	
		if ( cptElem != nTri ) {
			cout << "  !! Error Inria output:  Inconsistent number of triangles\n" << endl;
			exit(EXIT_FAILURE);
		}
		
	}
	
	/*--- Write quadrilaterals ---*/
	
	if ( nQua > 0 ) {
		
		GmfSetKwd(OutMsh, GmfQuadrilaterals, nQua);
		
		cptElem = 0;
		
		for (iMarker = 0; iMarker < nMarker; iMarker++) {
			for (iElem = 0; iElem < geometry->GetnElem_Bound(iMarker); iElem++) {
				
				bnd = geometry->bound[iMarker][iElem];
				
				if ( bnd->GetVTK_Type() != QUADRILATERAL ) 
					continue;
				
				cptElem++;
				
				GmfSetLin(OutMsh, GmfQuadrilaterals, PointSurface[bnd->GetNode(0)]+1, PointSurface[bnd->GetNode(1)]+1, 
                                             PointSurface[bnd->GetNode(2)]+1, PointSurface[bnd->GetNode(3)]+1, iMarker+2); 	
			}
		}
	
		if ( cptElem != nQua ) {
			cout << "  !! Error Inria output:  Inconsistent number of quadrilaterals\n" << endl;
			exit(EXIT_FAILURE);
		}
		
	}
	
	if ( PointSurface )
		delete [] PointSurface;
	
	GmfCloseMesh(OutMsh);
  
#else // Not built with Inria support
  
  cout << "GMF file requested but SU2 was built without GMF support. No file written" << "\n";
  
#endif
  
}

void COutput::SetResult_Parallel(CSolver *****solver_container,
                                 CGeometry ****geometry,
                                 CConfig **config,
                                 unsigned short val_nZone) {

  unsigned short iZone, iVar, iInst;
  unsigned long iPoint;
  unsigned short nInst = 1;
  bool compressible = true;

  for (iZone = 0; iZone < val_nZone; iZone++) {

    /*--- Bool to distinguish between the FVM and FEM solvers. ---*/
    unsigned short KindSolver = config[iZone]->GetKind_Solver();
    bool fem_solver = ((KindSolver == FEM_EULER) ||
                       (KindSolver == FEM_NAVIER_STOKES) ||
                       (KindSolver == FEM_RANS) ||
                       (KindSolver == FEM_LES));
    
    for (iInst = 0; iInst < nInst; iInst++){

      bool cont_adj = config[iZone]->GetContinuous_Adjoint();
      bool disc_adj = config[iZone]->GetDiscrete_Adjoint();

      /*--- Check for compressible/incompressible flow problems. ---*/

      compressible = (config[iZone]->GetKind_Regime() == COMPRESSIBLE);

      /*--- First, prepare the offsets needed throughout below. ---*/

      PrepareOffsets(config[iZone], geometry[iZone][iInst][MESH_0]);


      /*--- This switch statement will become a call to a virtual function
       defined within each of the "physics" output child classes that loads
       the local data for that particular problem alone. ---*/

      if (rank == MASTER_NODE)
        cout << "Loading solution output data locally on each rank." << endl;

      switch (config[iZone]->GetKind_Solver()) {
        case EULER : case NAVIER_STOKES: case RANS :
          if (compressible)
            LoadLocalData_Flow(config[iZone], geometry[iZone][iInst][MESH_0], solver_container[iZone][iInst][MESH_0], iZone);
          else
            LoadLocalData_IncFlow(config[iZone], geometry[iZone][iInst][MESH_0], solver_container[iZone][iInst][MESH_0], iZone);
          break;
        case ADJ_EULER : case ADJ_NAVIER_STOKES : case ADJ_RANS :
        case DISC_ADJ_EULER: case DISC_ADJ_NAVIER_STOKES: case DISC_ADJ_RANS:
          LoadLocalData_AdjFlow(config[iZone], geometry[iZone][iInst][MESH_0], solver_container[iZone][iInst][MESH_0], iZone);
          break;
        case FEM_ELASTICITY: case DISC_ADJ_FEM:
          LoadLocalData_Elasticity(config[iZone], geometry[iZone][iInst][MESH_0], solver_container[iZone][iInst][MESH_0], iZone);
          break;
        case HEAT_EQUATION_FVM:
          LoadLocalData_Base(config[iZone], geometry[iZone][iInst][MESH_0], solver_container[iZone][iInst][MESH_0], iZone);
          break;
        case FEM_EULER: case FEM_NAVIER_STOKES: case FEM_RANS: case FEM_LES:
          LoadLocalData_FEM(config[iZone], geometry[iZone][iInst][MESH_0], solver_container[iZone][iInst][MESH_0], iZone);
        default: break;
      }
      
      /*--- After loading the data local to a processor, we perform a sorting,
       i.e., a linear partitioning of the data across all ranks in the communicator. ---*/
      
      if (rank == MASTER_NODE) cout << "Sorting output data across all ranks." << endl;

      if (fem_solver)
        SortOutputData_FEM(config[iZone], geometry[iZone][iInst][MESH_0]);
      else
        SortOutputData(config[iZone], geometry[iZone][iInst][MESH_0]);

      Adap_Data.resize(nParallel_Poin);
      for(unsigned long iPoin = 0; iPoin < nParallel_Poin; ++iPoin) {
        Adap_Data[iPoin].resize(nVar_Par);
        for(unsigned short iVar = 0; iVar < nVar_Par; ++iVar) {
          Adap_Data[iPoin][iVar] = SU2_TYPE::GetValue(Parallel_Data[iVar][iPoin]);
        }
      }

      /*--- Deallocate the nodal data needed for writing restarts. ---*/

      DeallocateData_Parallel(config[iZone], geometry[iZone][iInst][MESH_0]);

      /*--- Clear the variable names list. ---*/

      Variable_Names.clear();
      
    }

  }

}

vector<vector<passivedouble> > COutput::GetResult_Parallel() {

  return Adap_Data;
}

void COutput::CleanResult_Parallel( ){

  vector<vector<passivedouble> >().swap(Adap_Data);

}

void COutput::SetConnectivity_Parallel(CGeometry ****geometry,
                                       CConfig **config,
                                       unsigned short val_nZone) {

  unsigned short iZone, iInst;
  unsigned short nInst = 1;

  for (iZone = 0; iZone < val_nZone; iZone++) {
    
    for (iInst = 0; iInst < nInst; iInst++){

      /*--- Sort volume grid connectivity. ---*/

      if (rank == MASTER_NODE) cout <<"Sorting volume grid connectivity." << endl;

      SortVolumetricConnectivity(config[iZone], geometry[iZone][iInst][MESH_0], TRIANGLE     , true);
      SortVolumetricConnectivity(config[iZone], geometry[iZone][iInst][MESH_0], QUADRILATERAL, true);
      SortVolumetricConnectivity(config[iZone], geometry[iZone][iInst][MESH_0], TETRAHEDRON  , true);
      SortVolumetricConnectivity(config[iZone], geometry[iZone][iInst][MESH_0], HEXAHEDRON   , true);
      SortVolumetricConnectivity(config[iZone], geometry[iZone][iInst][MESH_0], PRISM        , true);
      SortVolumetricConnectivity(config[iZone], geometry[iZone][iInst][MESH_0], PYRAMID      , true);

      /*--- Sort surface grid connectivity. ---*/
          
      // if (rank == MASTER_NODE) cout <<"Sorting surface grid connectivity." << endl;
        
      // SortSurfaceConnectivity(config[iZone], geometry[iZone][iInst][MESH_0], LINE         );
      // SortSurfaceConnectivity(config[iZone], geometry[iZone][iInst][MESH_0], TRIANGLE     );
      // SortSurfaceConnectivity(config[iZone], geometry[iZone][iInst][MESH_0], QUADRILATERAL);

    }

  }

}

vector<vector<unsigned long> > COutput::GetConnEdg(CConfig *config, CGeometry *geometry) {

  unsigned short iMarker, iElem;  
  unsigned short nMarker = config->GetnMarker_All();

  vector<vector<unsigned long> > Edg;

  for (iMarker = 0; iMarker < nMarker; iMarker++) {
    for (iElem = 0; iElem < geometry->GetnElem_Bound(iMarker); iElem++) {
      CPrimalGrid* bnd = geometry->bound[iMarker][iElem];
      if (geometry->node[bnd->GetNode(0)]->GetDomain()) {
        switch ( bnd->GetVTK_Type() ) {
          case LINE:
            const unsigned long idx0 = geometry->node[bnd->GetNode(0)]->GetGlobalIndex()+1;
            const unsigned long idx1 = geometry->node[bnd->GetNode(1)]->GetGlobalIndex()+1;
            const unsigned long ref  = iMarker+2;
            const vector<unsigned long> tmp = {idx0, idx1, ref};
            Edg.push_back(tmp);
            break;
        }
      }
    }
  }

  return Edg; 
}

vector<vector<unsigned long> > COutput::GetConnTri(CConfig *config, CGeometry *geometry) {

  unsigned short iMarker, iElem;  
  unsigned short nMarker = config->GetnMarker_All(), nDim = geometry->GetnDim();

  vector<vector<unsigned long> > Tri;

  if(nDim == 2) {
    for(iElem = 0; iElem < nParallel_Tria; ++iElem) {
      const unsigned long idx0 = Conn_Tria_Par[iElem*3+0];
      const unsigned long idx1 = Conn_Tria_Par[iElem*3+1];
      const unsigned long idx2 = Conn_Tria_Par[iElem*3+2];
      const unsigned long ref   = 0;
      const vector<unsigned long> tmp = {idx0, idx1, idx2, ref};
      Tri.push_back(tmp);
    }
  }
  else {

    for (iMarker = 0; iMarker < nMarker; iMarker++) {
      for (iElem = 0; iElem < geometry->GetnElem_Bound(iMarker); iElem++) {
        CPrimalGrid* bnd = geometry->bound[iMarker][iElem];
        if (geometry->node[bnd->GetNode(0)]->GetDomain()) {
          switch ( bnd->GetVTK_Type() ) {
            case TRIANGLE:
              const unsigned long idx0 = geometry->node[bnd->GetNode(0)]->GetGlobalIndex()+1;
              const unsigned long idx1 = geometry->node[bnd->GetNode(1)]->GetGlobalIndex()+1;
              const unsigned long idx2 = geometry->node[bnd->GetNode(2)]->GetGlobalIndex()+1;
              const unsigned long ref  = iMarker+2;
              const vector<unsigned long> tmp = {idx0, idx1, idx2, ref};
              Tri.push_back(tmp);
              break;
          }
        }
      }
    }
  }

  return Tri; 
}

vector<vector<unsigned long> > COutput::GetConnTet(CConfig *config, CGeometry *geometry) {

  unsigned short iElem;  

  vector<vector<unsigned long> > Tet;

  for(iElem = 0; iElem < nParallel_Tetr; ++iElem) {
    const unsigned long idx0 = Conn_Tetr_Par[iElem*4+0];
    const unsigned long idx1 = Conn_Tetr_Par[iElem*4+1];
    const unsigned long idx2 = Conn_Tetr_Par[iElem*4+2];
    const unsigned long idx3 = Conn_Tetr_Par[iElem*4+3];
    const unsigned long ref   = 0;
    const vector<unsigned long> tmp = {idx0, idx1, idx2, idx3, ref};
    Tet.push_back(tmp);
  }

  return Tet; 
}

void COutput::CleanConnectivity_Parallel( ){

  if (Conn_Tria_Par != NULL) delete [] Conn_Tria_Par;
  if (Conn_Quad_Par != NULL) delete [] Conn_Quad_Par;
  if (Conn_Tetr_Par != NULL) delete [] Conn_Tetr_Par;
  if (Conn_Hexa_Par != NULL) delete [] Conn_Hexa_Par;
  if (Conn_Pris_Par != NULL) delete [] Conn_Pris_Par;
  if (Conn_Pyra_Par != NULL) delete [] Conn_Pyra_Par;

}