#include "HYMLS_MainUtils.hpp"

#include "HYMLS_config.h"

#include <iostream>

#include "Epetra_Map.h"
#include "Epetra_MultiVector.h"
#include "Epetra_Vector.h"
#include "Epetra_CrsMatrix.h"
#include "Epetra_Import.h"

#include "Teuchos_RCP.hpp"

#include "EpetraExt_CrsMatrixIn.h"
#include "EpetraExt_VectorIn.h"

#include "HYMLS_Tools.hpp"
#include "HYMLS_Macros.hpp"
#include "HYMLS_CartesianPartitioner.hpp"
#include "HYMLS_SkewCartesianPartitioner.hpp"

#include "GaleriExt_Cross2DN.h"
#include "Galeri_CrsMatrices.h"

#include "GaleriExt_Darcy2D.h"
#include "GaleriExt_Darcy3D.h"
#include "GaleriExt_Stokes2D.h"
#include "GaleriExt_Stokes3D.h"

namespace HYMLS {

namespace MainUtils {

Teuchos::RCP<Epetra_CrsMatrix> read_matrix(std::string datadir,
  std::string file_format, Teuchos::RCP<Epetra_Map> map, std::string name)
  {
  if (map==Teuchos::null)
    {
    HYMLS::Tools::Error("map must have been allocated before this function",
        __FILE__,__LINE__);
    }
  
  std::string suffix;
  if (file_format=="MatrixMarket")
    {
    suffix=".mtx";
    }
  else if (file_format=="MatrixMarket (2)")
    {
    suffix="2.mtx";
    }
  else
    {
    HYMLS::Tools::Error("File format '"+file_format+"' not supported",__FILE__,__LINE__);
    }

  std::string filename = datadir+"/"+name+suffix;

  HYMLS::Tools::Out("... read matrix from file '"+filename+"'");
  HYMLS::Tools::Out("    file format: "+file_format);

  Teuchos::RCP<Epetra_CrsMatrix> K=Teuchos::null;

  if (file_format=="MatrixMarket" || file_format=="MatrixMarket (2)")
    {
    Epetra_CrsMatrix* Kptr;
#ifdef HYMLS_LONG_LONG
    CHECK_ZERO(EpetraExt::MatrixMarketFileToCrsMatrix64(filename.c_str(), *map, Kptr));
#else
    CHECK_ZERO(EpetraExt::MatrixMarketFileToCrsMatrix(filename.c_str(), *map, Kptr));
#endif
    K=Teuchos::rcp(Kptr, true);
    }
  else
    {
    HYMLS::Tools::Error("File format '"+file_format+"' not supported",__FILE__,__LINE__);
    }
  return K;
  }

Teuchos::RCP<Epetra_Vector> read_vector(std::string name,std::string datadir,
  std::string file_format,Teuchos::RCP<Epetra_Map> map)
  {
  if (map==Teuchos::null)
    {
    HYMLS::Tools::Error("map must have been allocated before this function",
        __FILE__,__LINE__);
    }
  
  std::string suffix;
  if (file_format=="MatrixMarket")
    {
    suffix=".mtx";
    }
  else if (file_format=="MatrixMarket (2)")
    {
    suffix="2.mtx";
    }
  else
    {
    HYMLS::Tools::Error("File format '"+file_format+"' not supported",__FILE__,__LINE__);
    }

  std::string filename = datadir+"/"+name+suffix;
  
  HYMLS::Tools::Out("... read vector from file '"+filename+"'");
  HYMLS::Tools::Out("    file format: "+file_format);
  
  Teuchos::RCP<Epetra_Vector> v;

  if (file_format=="MatrixMarket" || file_format=="MatrixMarket (2)")
    {
    // the EpetraExt function only works for a linear map in parallel,
    // so we need to reindex ourselves:
    Epetra_Map linearMap((hymls_gidx)map->NumGlobalElements64(),
                         map->NumMyElements(),
                         (hymls_gidx)map->IndexBase64(),
                         map->Comm());

    Epetra_Vector* vptr;
    CHECK_ZERO(EpetraExt::MatrixMarketFileToVector(filename.c_str(),linearMap,vptr));
    
    v=Teuchos::rcp(new Epetra_Vector(*map));
    Epetra_Import import(*map,linearMap);
    CHECK_ZERO(v->Import(*vptr,import,Insert));
    delete vptr;
    }
  else
    {
    HYMLS::Tools::Error("File format '"+file_format+"' not supported",__FILE__,__LINE__);
    }
  return v;
  }

/////////////////////////////////////////////////////////////////////////////////////////

#if 0
void ReadTestCase(std::string problem, int nx, int sx, 
                  RCP<Epetra_Comm> comm,
                  RCP<Epetra_Map>& map,
                  RCP<Epetra_CrsMatrix>& K,
                  RCP<Epetra_Vector>& u_ex,
                  RCP<Epetra_Vector>& f)

  {
  std::stringstream ss;
  ss<<"data/"<<problem<<nx<<".h5";
  std::string filename=ss.str();
  
  std::cout << "***************************************"<<std::endl;
  std::cout << "* READING TEST PROBLEM FROM '"<<filename<<"'"<<std::endl;
  std::cout << "***************************************"<<std::endl;
  
  EpetraExt::HDF5 file(*comm);
  file.Open(filename);
  
  if (!file.IsOpen())
    {
    std::cerr << "Error opening testcase file."<<std::endl;
    std::cerr << "Make sure an appropriate data file exists."<<std::endl;
    std::cerr << "For your input it should be '"<<filename<<"'"<<std::endl;
    PrintUsage(std::cerr);
    MPI_Finalize();
    exit(0);
    }
  int nx_,ny_;
  std::cout << "Read grid size..."<<std::endl;
  try{
  file.Read("grid","nx",nx_);
  } catch (EpetraExt::Exception e){e.Print();}
  std::cout << nx_<<std::endl;
  return;
  file.Read("grid","ny",ny_);
  std::cout << "grid-size: "<<nx_<< "x"<<ny_<<std::endl;
  
  if (nx!=nx_||nx!=ny_)
    {
    Error("Dimension mismatch between filename and contents!",-1);
    }
  
  }
#endif

Teuchos::RCP<Epetra_Map> create_map(const Epetra_Comm& comm,
  Teuchos::RCP<Teuchos::ParameterList> const &params)
  {
  std::string partMethod = params->sublist("Preconditioner").get("Partitioner", "Cartesian");
  Teuchos::RCP<HYMLS::BasePartitioner> part = Teuchos::null;
  if (partMethod == "Cartesian")
    {
    part = Teuchos::rcp(new HYMLS::CartesianPartitioner(
        Teuchos::null, params, comm));
    }
  else if (partMethod == "Skew Cartesian")
    {
    part = Teuchos::rcp(new HYMLS::SkewCartesianPartitioner(
        Teuchos::null, params, comm));
    }
  else
    HYMLS::Tools::Error("Partitioner not recognised", __FILE__, __LINE__);

  CHECK_ZERO(part->Partition(true));

  return Teuchos::rcp(new Epetra_Map(part->Map()));
  }

Teuchos::RCP<Epetra_Vector> create_testvector(
  Teuchos::ParameterList &probl_params,
  Epetra_CrsMatrix const &matrix)
  {
  std::string eqn = probl_params.get("Equations", "Laplace");

  Teuchos::RCP<Epetra_Vector> testvector =
    Teuchos::rcp(new Epetra_Vector(matrix.RowMap()));
  testvector->PutScalar(1.0);

  if (eqn == "Stokes-B" || eqn == "Stokes-L" || eqn == "Stokes-T")
    {
    int nx = probl_params.get("nx", 32);
    int ny = probl_params.get("ny", nx);
    int dim = probl_params.get("Dimension", -1);
    int dof = probl_params.get("Degrees of Freedom", -1);
    for (int i = 0; i < matrix.NumMyRows(); i++)
      {
      hymls_gidx grid = matrix.GRID64(i);
      if (grid % dof == 0)
        (*testvector)[i] = (((grid / dof) % nx) % 2) * 2 - 1;
      else if (grid % dof == 1)
        (*testvector)[i] = (((grid / dof / nx) % ny) % 2) * 2 - 1;
      else if (dim > 2 && grid % dof == 2 && eqn == "Stokes-B")
        (*testvector)[i] = ((grid / dof / nx / ny) % 2) * 2 - 1;
      else
        (*testvector)[i] = 1.0;
      }
    }

  // Remove boundary conditions
  for (int i = 0; i < matrix.NumMyRows(); i++)
    {
    int len;
    double *values;
    int *indices;
    bool is_diag = true;
    CHECK_ZERO(matrix.ExtractMyRowView(i, len, values, indices));
    for (int j = 0; j < len; j++)
    {
        if (values[j] && matrix.GCID64(indices[j]) != matrix.GRID64(i))
        {
            is_diag = false;
            break;
        }
    }
    if (is_diag)
      (*testvector)[i] = 0.0;
    }
  return testvector;
  }

Teuchos::RCP<Epetra_CrsMatrix> create_matrix(const Epetra_Map& map,
                                Teuchos::ParameterList& probl_params,
                                std::string galeriLabel,
                                Teuchos::ParameterList& galeriList
                                )
  {
  Teuchos::RCP<Epetra_CrsMatrix> matrix = Teuchos::null;
  std::string eqn = probl_params.get("Equations","Laplace");
  int dim = probl_params.get("Dimension",2);
  int nx=probl_params.get("nx",32);
  int ny=probl_params.get("ny",nx);
  int nz=probl_params.get("nz",(dim>2)?nx:1);

  galeriList.set("nx",nx);
  galeriList.set("ny",ny);
  galeriList.set("nz",nz);

  bool xperio = probl_params.get("x-periodic", false);
  bool yperio = probl_params.get("y-periodic", false);
  bool zperio = probl_params.get("z-periodic", false);

  GaleriExt::PERIO_Flag perio = GaleriExt::NO_PERIO;

  if (xperio) perio = (GaleriExt::PERIO_Flag)(perio | GaleriExt::X_PERIO);
  if (yperio) perio = (GaleriExt::PERIO_Flag)(perio | GaleriExt::Y_PERIO);
  if (zperio) perio = (GaleriExt::PERIO_Flag)(perio | GaleriExt::Z_PERIO);

  if (galeriLabel == "Laplace Neumann")
    {
    if (dim == 2)
      {
      matrix = Teuchos::rcp(GaleriExt::Matrices::Cross2DN(&map,
          nx, ny, 4, -1, -1, -1, -1));
      }
    }
  else if (galeriLabel == "Darcy")
    {
    if (dim == 2)
      {
      matrix = Teuchos::rcp(GaleriExt::Matrices::Darcy2D(&map,
          nx, ny, 1, -1, perio));
      }
    else if (dim == 3)
      {
      matrix = Teuchos::rcp(GaleriExt::Matrices::Darcy3D(&map,
          nx, ny, nz, 1, -1, perio));
      }
    }
  else if (galeriLabel.rfind("Stokes") == 0)
    {
    char stokesType = galeriLabel.back();
    if (dim == 2)
      {
      if (nx != ny)
        HYMLS::Tools::Warning("GaleriExt::Stokes2D only gives correct matrix entries if nx=ny, but the graph is correct.\n",__FILE__,__LINE__);
      matrix = Teuchos::rcp(GaleriExt::Matrices::Stokes2D(&map,
          nx, ny, nx*nx, 1, perio, stokesType));
      }
    else if (dim == 3)
      {
      if (nx != ny || nx != nz)
        HYMLS::Tools::Warning("GaleriExt::Stokes3D only gives correct matrix entries if nx=ny, but the graph is correct.\n",__FILE__,__LINE__);
      matrix = Teuchos::rcp(GaleriExt::Matrices::Stokes3D(&map,
          nx, ny, nz, nx*nx, 1, perio, stokesType));
      }
    else
      {
      HYMLS::Tools::Error("not implemented!",__FILE__,__LINE__);
      }
    }
  else
    {
    std::string matrixType = galeriLabel;
    if (galeriLabel == "")
      {
      matrixType = eqn + Teuchos::toString(dim)+"D";
      }
    try {
      matrix = Teuchos::rcp(Galeri::CreateCrsMatrix(matrixType, &map, galeriList));
      } catch (Galeri::Exception G) {G.Print();}
    }
  if (probl_params.get("Equations","Laplace")=="Laplace")
    {
    matrix->Scale(-1.0); // we like our matrix negative definite
    // (just to conform with the diffusion operator in the NSE,
    // the solver works anyway, of course).
    }
  return matrix;
  }

  // try to construct the nullspace for the operator, right now we only implement
Teuchos::RCP<Epetra_MultiVector> create_nullspace(
  const Epetra_BlockMap& map,
  const std::string& nullSpaceType,
  Teuchos::ParameterList& probl_params)
  {
  int dim  = probl_params.get("Dimension", -1);
  int dof = probl_params.get("Degrees of Freedom", -1);
  std::string eqn = probl_params.get("Equations", "Laplace");

  Teuchos::RCP<Epetra_MultiVector> nullSpace = Teuchos::null;
  if (nullSpaceType == "Constant")
    {
    if (dof == -1)
      {
      Tools::Error("'Degrees of Freedom' not set in 'Problem' sublist",
        __FILE__, __LINE__);
      }

    nullSpace = Teuchos::rcp(new Epetra_MultiVector(map, dof));
    CHECK_ZERO(nullSpace->PutScalar(0.0));

    for (int lid = 0; lid < nullSpace->MyLength(); lid++)
      {
      hymls_gidx gid = nullSpace->Map().GID64(lid);
      (*nullSpace)[gid % dof][lid] = 1.0 / sqrt(nullSpace->GlobalLength64() / dof);
      }
    }
  else if (nullSpaceType == "Constant P")
    {
    int pvar = probl_params.get("Pressure Variable", dim);
    if (pvar == -1 || dof == -1)
      {
      Tools::Error("'Dimension' or 'Degrees of Freedom' not set in 'Problem' sublist",
        __FILE__, __LINE__);
      }

    nullSpace = Teuchos::rcp(new Epetra_Vector(map));
    CHECK_ZERO(nullSpace->PutScalar(0.0));
    for (int lid = 0; lid < nullSpace->MyLength(); lid++)
      {
      if (nullSpace->Map().GID64(lid) % dof == pvar)
        (*nullSpace)[0][lid] = 1.0;
      }
    }
  else if (nullSpaceType == "Checkerboard")
    {
    int pvar = probl_params.get("Pressure Variable", dim);
    if (pvar == -1 || dof == -1)
      {
      Tools::Error("'Dimension' or 'Degrees of Freedom' not set in 'Problem' sublist",
        __FILE__, __LINE__);
      }

    nullSpace = Teuchos::rcp(new Epetra_MultiVector(map, 2));
    CHECK_ZERO(nullSpace->PutScalar(0.0));

    int nx = probl_params.get("nx", 1);
    int ny = probl_params.get("ny", nx);
    int nz = probl_params.get("nz", dim > 2 ? nx : 1);
    int stokes_b = eqn == "Stokes-B";
    for (int lid = 0; lid < nullSpace->MyLength(); lid++)
      {
      hymls_gidx gid = nullSpace->Map().GID64(lid);
      if (gid % dof == pvar)
        {
        int i, j, k, v;
        HYMLS::Tools::ind2sub(nx, ny, nz, dof, gid, i, j, k, v);
        double val1 = (i + j + k * stokes_b) % 2;
        double val2 = 1.0 - val1;
        (*nullSpace)[0][lid] = val1;
        (*nullSpace)[1][lid] = val2;
        }
      }
    }
  else if (nullSpaceType != "None")
    {
    Tools::Error("'Null Space'='"+nullSpaceType+"' not implemented",
      __FILE__, __LINE__);
    }

  // normalize each column
  int k = nullSpace->NumVectors();
  double *nrm2 = new double[k];
  CHECK_ZERO(nullSpace->Norm2(nrm2));
  for (int i=0;i<k;i++)
    {
    CHECK_ZERO((*nullSpace)(i)->Scale(1.0/nrm2[i]));
    }
  delete [] nrm2;
  return nullSpace;
  }


int MakeSystemConsistent(const Epetra_CrsMatrix& A,
                               Epetra_MultiVector& x_ex,
                               Epetra_MultiVector& b,
                               Teuchos::ParameterList& driverList)
  {
  return 0;
  }
                                                                                             

}//MainUtils

}//HYMLS
