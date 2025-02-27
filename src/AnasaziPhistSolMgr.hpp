#ifndef ANASAZI_PHIST_SOLMGR_HPP
#define ANASAZI_PHIST_SOLMGR_HPP

#include "HYMLS_config.h"

#include "Teuchos_ParameterList.hpp"
#include "Teuchos_RCPDecl.hpp"

#include "AnasaziConfigDefs.hpp"
#include "AnasaziTypes.hpp"
#include "AnasaziMultiVecTraits.hpp"
#include "AnasaziEigenproblem.hpp"
#include "AnasaziSolverManager.hpp"

#include <Ifpack_Preconditioner.h>
#include <ml_MultiLevelPreconditioner.h>

#include "phist_config.h"
#ifndef PHIST_KERNEL_LIB_EPETRA
#error "Your PHIST installation is incompatible, maybe you used the wrong PHIST_KERNEL_LIB when building it?"
#endif

// Include this before phist
#include "HYMLS_PhistWrapper.hpp"
#include "HYMLS_PhistCustomCorrectionSolver.hpp"

#include "phist_macros.h"
#include "phist_enums.h"
#include "phist_kernels.h"
#include "phist_operator.h"
#include "phist_precon.h"
#include "phist_jadaOpts.h"
#include "phist_subspacejada.h"
#include "phist_schur_decomp.h"

#include "HYMLS_Tester.hpp"
#include "Epetra_Util.h"


namespace HYMLS {
namespace phist {

//! small static helper to get the phist type for the preconditioner right
template<class PREC>
class traits {

public:

  static phist_Eprecon get_phist_Eprecon(){return phist_INVALID_PRECON;}
  static phist_ElinSolv get_phist_ElinSolv(bool hermitian=false){return hermitian? phist_MINRES: phist_GMRES;}
  static Teuchos::RCP<SolverWrapper> get_custom_solver(Teuchos::RCP<PREC> solver, bool doBordering) {return Teuchos::null;}
};

template <>
phist_Eprecon traits< ::HYMLS::Preconditioner>::get_phist_Eprecon(){return phist_USER_PRECON;}

template <>
phist_Eprecon traits< ::HYMLS::Solver>::get_phist_Eprecon(){return phist_NO_PRECON;}

template <>
phist_ElinSolv traits< ::HYMLS::Solver>::get_phist_ElinSolv(bool hermitian){return phist_USER_LINSOLV;}

template <>
Teuchos::RCP<SolverWrapper> traits< ::HYMLS::Solver>::get_custom_solver
        (Teuchos::RCP< ::HYMLS::Solver> solver, bool doBordering) 
        {return Teuchos::rcp(new SolverWrapper(solver,doBordering));}


template <>
phist_Eprecon traits<Ifpack_Preconditioner>::get_phist_Eprecon(){return phist_IFPACK;}

template <>
phist_Eprecon traits< ::ML_Epetra::MultiLevelPreconditioner>::get_phist_Eprecon(){return phist_ML;}

}//namespace phist
}//namespace HYMLS

namespace Anasazi {


/*!
 * \class PhistSolMgr
 * \brief Solver Manager for Jacobi Davidson in phist
 *
 * This class provides a simple interface to the Jacobi Davidson
 * eigensolver.  This manager creates
 * appropriate managers based on user
 * specified ParameterList entries (or selects suitable defaults),
 * provides access to solver functionality, and manages the restarting
 * process.
 *
 * This class is currently only implemented for scalar type double

 * This class accepts the preconditioner type to be used as a template parameter, however,
 * *it is not implemented for general PREC types in any way*. Instead, it is implemented  
 * for the Trilinos base class Ifpack_Preconditioner, for ML_Epetra::MultiLevelPreconditioner,
 * and for the HYMLS classes Preconditioner and Solver. For Ifpack, ML and HYMLS::Preconditioner
 * we rely on the phist jadaCorrectionSolver implementation and only provide the preconditioning
 * interface. For HYMLS::Solver we use our own implementation of a bordered correction solver (see
 * file HYMLS_PhistCustomCorrectionSolver.H/.C) instead.

 \ingroup anasazi_solver_framework

 */
template <class ScalarType, class MV, class OP, class PREC>
class PhistSolMgr : public SolverManager<ScalarType,MV,OP>
{
    public:

        /*!
         * \brief Basic constructor for PhistSolMgr
         *
         * This constructor accepts the Eigenproblem to be solved and a parameter list of options
         * for the solver.
         * The following options control the behavior
         * of the solver:
         * - "Which" -- a string specifying the desired eigenvalues: SM, LM, SR, LR, SI, or LI. Default: "LM."
         * - "Block Size" -- block size used by algorithm.  Default: 1.
         * - "Maximum Subspace Dimension" -- maximum number of basis vectors for subspace.  Two
         *  (for standard eigenvalue problems) or three (for generalized eigenvalue problems) sets of basis
         *  vectors of this size will be required. Default: 3*problem->getNEV()*"Block Size"
         * - "Restart Dimension" -- Number of vectors retained after a restart.  Default: NEV
         * - "Maximum Restarts" -- an int specifying the maximum number of restarts the underlying solver
         *  is allowed to perform.  Default: 20
         * - "Verbosity" -- a sum of MsgType specifying the verbosity.  Default: AnasaziErrors
         * - "Convergence Tolerance" -- a MagnitudeType specifying the level that residual norms must
         *  reach to decide convergence.  Default: machine precision
         * - "Inner Iterations" - maximum number of inner GMRES or MINRES iterations allowed
         *   If "User," the value in problem->getInitVec() will be used.  Default: "Random".
         * - "Print Number of Ritz Values" -- an int specifying how many Ritz values should be printed
         *   at each iteration.  Default: "NEV".
         */
        PhistSolMgr( const Teuchos::RCP< Eigenproblem<ScalarType,MV,OP> > &problem,
                                   const Teuchos::RCP<PREC> &prec,
                                   Teuchos::ParameterList &pl );

         //! destructor
         ~PhistSolMgr();

        /*!
         * \brief Return the eigenvalue problem.
         */
        const Eigenproblem<ScalarType,MV,OP> & getProblem() const { return *d_problem; }

        /*!
         * \brief Get the iteration count for the most recent call to solve()
         */
        int getNumIters() const { return numIters_; }

        /*!
         * \brief This method performs possibly repeated calls to the underlying eigensolver's iterate()
         *  routine until the problem has been solved (as decided by the StatusTest) or the solver manager decides to quit.
         */
        ReturnType solve();

    private:

        typedef MultiVecTraits<ScalarType,MV>        MVT;
        typedef Teuchos::ScalarTraits<ScalarType>    ST;
        typedef typename ST::magnitudeType           MagnitudeType;
        typedef Teuchos::ScalarTraits<MagnitudeType> MT;

        Teuchos::RCP< Eigenproblem<ScalarType,MV,OP> >           d_problem;
        Teuchos::RCP< const Epetra_CrsMatrix >                   d_Amat;
        Teuchos::RCP< const Epetra_CrsMatrix >                   d_Bmat;
        Teuchos::RCP< PREC >                                     d_prec;
        // if PREC is HYMLS::Solver, we need an additional wrapper object
        Teuchos::RCP<HYMLS::phist::SolverWrapper> d_customSolver;
        // otherwise, these are used to pass HYMLS, Ifpack or ML object to phist for preconditioning
        Teuchos::RCP<phist_DlinearOp>                   d_preconOp, d_preconPointers;
        // options pased to phist
        phist_jadaOpts                                  d_opts;

        bool borderedSolver;
        int numIters_; //! number of iterations performed in previous solve()

}; // class PhistSolMgr

//---------------------------------------------------------------------------//
// Prevent instantiation on complex scalar type
//---------------------------------------------------------------------------//
template <class MagnitudeType, class MV, class OP, class PREC>
class PhistSolMgr<std::complex<MagnitudeType>,MV,OP,PREC>
{
  public:

    typedef std::complex<MagnitudeType> ScalarType;
    PhistSolMgr(
            const Teuchos::RCP<Eigenproblem<ScalarType,MV,OP> > &problem,
            Teuchos::ParameterList &pl )
    {
        // Provide a compile error when attempting to instantiate on complex type
        MagnitudeType::this_class_is_missing_a_specialization();
    }
};

//---------------------------------------------------------------------------//
// Start member definitions
//---------------------------------------------------------------------------//

//---------------------------------------------------------------------------//
// Constructor
//---------------------------------------------------------------------------//
template <class ScalarType, class MV, class OP, class PREC>
PhistSolMgr<ScalarType,MV,OP,PREC>::PhistSolMgr(
        const Teuchos::RCP<Eigenproblem<ScalarType,MV,OP> > &problem,
        const Teuchos::RCP<PREC> &prec,
        Teuchos::ParameterList &pl )
   : d_problem(problem), d_prec(prec), numIters_(0)
  {
    int iflag = 0;
    int argc = 0;
    phist_kernels_init(&argc, NULL, &iflag);
    TEUCHOS_TEST_FOR_EXCEPTION(iflag != 0, std::runtime_error, "Could not initialize PHIST!");

    TEUCHOS_TEST_FOR_EXCEPTION( d_problem == Teuchos::null,
      std::invalid_argument, "Problem not given to solver manager." );
    TEUCHOS_TEST_FOR_EXCEPTION( !d_problem->isProblemSet(),
      std::invalid_argument, "Problem not set." );
    TEUCHOS_TEST_FOR_EXCEPTION( d_problem->getA() == Teuchos::null &&
      d_problem->getOperator() == Teuchos::null,
      std::invalid_argument, "A operator not supplied on Eigenproblem." );
    TEUCHOS_TEST_FOR_EXCEPTION( d_problem->getInitVec() == Teuchos::null,
      std::invalid_argument, "No vector to clone from on Eigenproblem." );
    TEUCHOS_TEST_FOR_EXCEPTION( d_problem->getNEV() <= 0,
      std::invalid_argument, "Number of requested eigenvalues must be positive.");

    d_Amat = Teuchos::rcp_dynamic_cast<const Epetra_CrsMatrix>(d_problem->getA());
    if (d_Amat == Teuchos::null)
    {
      d_Amat = Teuchos::rcp_dynamic_cast<const Epetra_CrsMatrix>(d_problem->getOperator());
    }
    d_Bmat = Teuchos::rcp_dynamic_cast<const Epetra_CrsMatrix>(d_problem->getM());
    TEUCHOS_TEST_FOR_EXCEPTION(d_Amat == Teuchos::null,
      std::invalid_argument, "problem->getA() is null or not an Epetra_CrsMatrix!");

    // Initialize phist options
    phist_jadaOpts_setDefaults(&d_opts);
    d_opts.numEigs = d_problem->getNEV();
    d_opts.symmetry = d_problem->isHermitian() ? phist_HERMITIAN : phist_GENERAL;
    d_opts.innerSolvStopAfterFirstConverged = true;

    if( !pl.isType<int>("Block Size") )
    {
        pl.set<int>("Block Size",2);
    }


    if( !pl.isType<int>("Maximum Subspace Dimension") )
    {
        pl.set<int>("Maximum Subspace Dimension",3*problem->getNEV()*pl.get<int>("Block Size"));
    }

    if( !pl.isType<int>("Print Number of Ritz Values") )
    {
        int numToPrint = std::max( pl.get<int>("Block Size"), d_problem->getNEV() );
        pl.set<int>("Print Number of Ritz Values",numToPrint);
    }

    // Get convergence info
    MagnitudeType tol = pl.get<MagnitudeType>("Convergence Tolerance", MT::eps() );
    TEUCHOS_TEST_FOR_EXCEPTION( pl.get<MagnitudeType>("Convergence Tolerance") <= MT::zero(),
                                std::invalid_argument, "Convergence Tolerance must be greater than zero." );

    d_opts.convTol = tol;
    d_opts.numEigs = pl.get<int>("How Many");
    d_opts.blockSize = pl.get<int>("Block Size");
    d_opts.minBas = pl.get<int>("Restart Dimension");
    d_opts.maxBas = pl.get<int>("Maximum Subspace Dimension");

    d_opts.innerSolvMaxIters = pl.get("Inner Iterations",d_opts.innerSolvMaxIters);
    d_opts.innerSolvMaxBas   = d_opts.innerSolvMaxIters;
    d_opts.innerSolvBlockSize=d_opts.blockSize;
    d_opts.preconOp=NULL;
    d_opts.innerSolvType = HYMLS::phist::traits<PREC>::get_phist_ElinSolv(d_opts.symmetry!=phist_GENERAL);
    d_opts.preconType=HYMLS::phist::traits<PREC>::get_phist_Eprecon();
    d_opts.preconUpdate=pl.get("Update Preconditioner",false);

    TEUCHOS_TEST_FOR_EXCEPTION( d_opts.minBas < d_opts.numEigs+d_opts.blockSize,
            std::invalid_argument, "Restart Dimension must be at least NEV+blockSize" );

    // Get maximum restarts
    if( pl.isType<int>("Maximum Restarts") )
    {
        d_opts.maxIters = (d_opts.maxBas-d_opts.minBas+1) * pl.get<int>("Maximum Restarts");
        TEUCHOS_TEST_FOR_EXCEPTION( d_opts.maxIters < 0, std::invalid_argument, "Maximum Restarts must be non-negative" );
    }
    else
    {
        d_opts.maxIters = d_opts.maxBas * 20;
    }

    borderedSolver = pl.get("Bordered Solver",false);

    // if the parameter "Bordered Solver" is set and the preconditioning type is HYMLS,
    // we provide our own correction solver instead of using the one with the projections
    // in PHIST.
    if (d_opts.innerSolvType==phist_USER_LINSOLV)
    {
      d_customSolver=HYMLS::phist::traits<PREC>::get_custom_solver(d_prec,borderedSolver);
      d_opts.customSolver=(void*)d_customSolver.get();
//      d_opts.customSolver_run  = &HYMLS::phist::jadaCorrectionSolver_run;
      d_opts.customSolver_run  = NULL; // phist will try solving the systems in sequence using the _run1 function
      d_opts.customSolver_run1 = &HYMLS::phist::jadaCorrectionSolver_run1;
    }

    // Get sort type
    std::string which;
    if( pl.isType<std::string>("Which") )
    {
        which = pl.get<std::string>("Which");
        TEUCHOS_TEST_FOR_EXCEPTION( which!="LM" && which!="SM" && which!="LR" && which!="SR",
                                    std::invalid_argument,
                                    "Which must be one of LM,SM,LR,SR." );
        // Convert to eigSort_t
        d_opts.which=str2eigSort(which.c_str());
    }
    else
    {
        d_opts.which = phist_LM;
    }

    // the preconOp is the phist preconditioning object, the
    // preconPointers are just used to define how our user-
    // defined preconditioner can be applied or updated.
    d_preconOp=Teuchos::rcp(new phist_DlinearOp);
    if (d_opts.preconType==phist_USER_PRECON)
    {
      d_preconPointers=Teuchos::rcp(new phist_DlinearOp);
      d_preconPointers->A=d_prec.get();
      d_preconPointers->aux=d_prec.get();
      d_preconPointers->range_map=&(d_prec->OperatorRangeMap());
      d_preconPointers->domain_map=&(d_prec->OperatorDomainMap());
      d_preconPointers->apply=&::HYMLS::PhistPreconTraits<PREC>::apply;
      d_preconPointers->apply_shifted=&::HYMLS::PhistPreconTraits<PREC>::apply_shifted;
      d_preconPointers->update=&::HYMLS::PhistPreconTraits<PREC>::update;
      d_preconPointers->destroy=NULL;
      // this function just wraps the preconditioner, if NULL is given as the options string.
      phist_Dprecon_create(d_preconOp.get(),d_Amat.get(),0.,d_Bmat.get(),NULL,NULL,
              precon2str(d_opts.preconType),NULL,d_preconPointers.get(),&iflag);
      // adjust some settings if we do our own bordering
      if (borderedSolver)
      {
        d_opts.preconUpdate=2;
        d_opts.preconSkewProject=0;
      } 
    }
    else
    {
      //this function just wraps the preconditioner, if NULL is given as the options string.
      phist_Dprecon_create(d_preconOp.get(),d_Amat.get(),0.,d_Bmat.get(),NULL,NULL,
                              precon2str(d_opts.preconType),NULL,d_prec.get(),&iflag);
    }
    TEUCHOS_TEST_FOR_EXCEPTION(iflag!=0,std::runtime_error,"iflag!=0 returned from phist_Dprecon_create");
    d_opts.preconOp=d_preconOp.get();

    std::string jadaOptsOverrideFile = pl.get("Override JadaOpts from File","None");
    if (jadaOptsOverrideFile!="None")
    {
      HYMLS::Tools::out() << "updating jadaOpts from file "<<jadaOptsOverrideFile<<std::endl;
      phist_Eprecon pt=d_opts.preconType;
      phist_jadaOpts_fromFile(&d_opts, jadaOptsOverrideFile.c_str(), &iflag);
      // the user may want to disable the preconditioner
      if (pt!=d_opts.preconType)
      {
        if (d_opts.preconType==phist_NO_PRECON)
        { 
          HYMLS::Tools::out() << "disabling preconditioner after update from file "<<jadaOptsOverrideFile<<std::endl;
          d_opts.preconOp=NULL;
        }
        else
        {
          HYMLS::Tools::Warning("cannot change preconType in override file for phist!",__FILE__,__LINE__);
          d_opts.preconType=pt;
        }
        
      }
    }
    
    pl.unused(HYMLS::Tools::out());
  }

template <class ScalarType, class MV, class OP, class PREC>
PhistSolMgr<ScalarType,MV,OP,PREC>::~PhistSolMgr()
{
       int iflag=0;
       if (d_preconOp!=Teuchos::null) phist_Dprecon_delete(d_preconOp.get(),&iflag);
       // print phist timing results etc.
       iflag=0;
       phist_kernels_finalize(&iflag);
}

template <class ScalarType>
bool eigSort(Anasazi::Value<ScalarType> const &a, Anasazi::Value<ScalarType> const &b)
{
  return (a.realpart * a.realpart + a.imagpart * a.imagpart) <
         (b.realpart * b.realpart + b.imagpart * b.imagpart);
}

//---------------------------------------------------------------------------//
// Solve
//---------------------------------------------------------------------------//
template <class ScalarType, class MV, class OP, class PREC>
ReturnType PhistSolMgr<ScalarType,MV,OP,PREC>::solve()
{
  int iflag;

  // create operator wrapper for computing Y=A*X using a CRS matrix
  Teuchos::RCP<phist_DlinearOp> A_op = Teuchos::rcp(new phist_DlinearOp);
  Teuchos::RCP<phist_DlinearOp> B_op = Teuchos::null;
  
  if (d_Bmat != Teuchos::null)
  {
    B_op = Teuchos::rcp(new phist_DlinearOp());
    phist_DlinearOp_wrap_sparseMat_pair(A_op.get(), d_Amat.get(), d_Bmat.get(),&iflag);
    TEUCHOS_TEST_FOR_EXCEPTION(iflag != 0, std::runtime_error,
    "PhistSolMgr::solve: phist_DlinearOp_wrap_sparseMat returned nonzero error code "+Teuchos::toString(iflag));
    phist_DlinearOp_wrap_sparseMat(B_op.get(), d_Bmat.get(), &iflag);
    TEUCHOS_TEST_FOR_EXCEPTION(iflag != 0, std::runtime_error,
      "PhistSolMgr::solve: phist_DlinearOp_wrap_sparseMat returned nonzero error code "+Teuchos::toString(iflag));
  }
  else
  {
    phist_DlinearOp_wrap_sparseMat(A_op.get(), d_Amat.get(), &iflag);
    TEUCHOS_TEST_FOR_EXCEPTION(iflag != 0, std::runtime_error,
      "PhistSolMgr::solve: phist_DlinearOp_wrap_sparseMat returned nonzero error code "+Teuchos::toString(iflag));
  }


  int num_eigs, block_dim;
  num_eigs = d_problem->getNEV();
  block_dim = d_opts.blockSize;
  
  if (num_eigs!=d_opts.numEigs)
  {
    HYMLS::Tools::Warning("numEigs in jadaOpts and Eigenproblem differ, using the one from Eigenproblem",
        __FILE__,__LINE__);
    d_opts.numEigs=num_eigs;
  }

  // allocate memory for eigenvalues and residuals. We allocate
  // one extra entry because in the real case we may get that the
  // last EV to converge is a complex pair (requirement of JDQR)
  std::vector<MagnitudeType> resid(num_eigs+block_dim-1);
  std::vector<std::complex<double> > ev(num_eigs+block_dim-1);

  Teuchos::RCP<MV> v0 = Teuchos::null;

  if (d_problem->getInitVec()!=Teuchos::null)
  {
    v0 = MVT::CloneCopy(*d_problem->getInitVec());
  }

  d_opts.v0 = v0.get();

  int nQ=d_opts.minBas;
  Teuchos::RCP<MV> Q = MVT::Clone(*d_problem->getInitVec(), nQ);

  Eigensolution<ScalarType,MV> sol;
  
#ifdef HYMLS_TESTING
  HYMLS::Tools::out() << "jadaOpts before subspacejada:\n";
  if (Q->Comm().MyPID()==0)
  {
    phist_jadaOpts_toFile(&d_opts,stdout);
  }
#endif   
  
  //using Djdqr, R could be NULL. But using subspacejada, we need to create R
  phist_DsdMat_ptr  R = NULL;
  phist_const_comm_ptr comm = NULL;
  phist_Dmvec_get_comm(Q.get(),&comm,&iflag);

  //phist_Dmvec_get_comm(X.get(),&comm,&iflag); //need const_comm
  // wrap MPI_COMM_WORLD
  
  phist_DsdMat_create(&R,nQ,nQ,comm,&iflag); 

  phist_Dsubspacejada(A_op.get(), B_op.get(), d_opts, Q.get(), R, &ev[0], &resid[0],  &num_eigs, &numIters_, &iflag);
  TEUCHOS_TEST_FOR_EXCEPTION(iflag != 0, std::runtime_error,
    "PhistSolMgr::solve: phist_Dsubspacejada returned nonzero error code "+Teuchos::toString(iflag));

  sol.numVecs = num_eigs;

  sol.index.resize(sol.numVecs);
  sol.Evals.resize(sol.numVecs);

  for (int i = 0; i < sol.numVecs; i++)
  {
    sol.index[i] = i;
    sol.Evals[i].realpart = ev[i].real();
    sol.Evals[i].imagpart = ev[i].imag();
  }

  Teuchos::RCP<MV> X = MVT::Clone(*Q, num_eigs);
 
  phist_DComputeEigenvectors(Q.get(),R,X.get(),&iflag);
  TEUCHOS_TEST_FOR_EXCEPTION(iflag != 0, std::runtime_error,
        "PHIST error "+Teuchos::toString(iflag)+" returned from call phist_Despace_to_evecs");


  phist_DsdMat_delete(R,&iflag);

  if (sol.numVecs)
  {
    // we return the complete subspace we have available, subsequently we must
    // remember that dim(Espace)>dim(Evecs)
    sol.Espace = MVT::CloneCopy(*Q, Teuchos::Range1D(0, nQ-1));
    sol.Evecs = MVT::CloneCopy(*X, Teuchos::Range1D(0, num_eigs-1));
  }
  d_problem->setSolution(sol);

  // Return convergence status
  if( sol.numVecs < d_problem->getNEV() )
  {
    return Unconverged;
  }

  return Converged;
}

} // namespace Anasazi

#endif // ANASAZI_PHIST_SOLMGR_HPP

