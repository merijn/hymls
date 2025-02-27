#ifndef HYMLS_HOUSEHOLDER_H
#define HYMLS_HOUSEHOLDER_H

#include "HYMLS_config.h"

#include "HYMLS_OrthogonalTransform.hpp"

class Epetra_RowMatrixTransposer;

namespace HYMLS {

//! Householder transform I-2vv'/v'v, where v_1=1+sqrt(n) and v_j=1 for 1<j<=n
class Householder : public OrthogonalTransform
  {

public:

  //! constructor. The level parameter is just to get the object label right.
  Householder(int lev=0);

  //!
  virtual ~Householder();

  //! compute X=H*X in place, with H=I-2v'v
  int Apply(Epetra_SerialDenseMatrix& X,
    Epetra_SerialDenseVector v) const;

  //! compute X=X*H' in place, with H=I-2v'v
  int ApplyR(Epetra_SerialDenseMatrix& X,
    Epetra_SerialDenseVector v) const;

  //! explicitly form the OT as a sparse matrix. The dimension and indices
  //! of the entries to be transformed are given by
  //! the size of the input vector. The function may be called repeatedly
  //! for different sets of indices (separator groups) to construct a matrix
  //! for simultaneously applying many transforms. Always use the corresponding
  //! Apply() functions to apply the transform rather than sparse matrix-matrix
  //! products.
  virtual int Construct(Epetra_CrsMatrix& H,
#ifdef HYMLS_LONG_LONG
    const Epetra_LongLongSerialDenseVector& inds,
#else
    const Epetra_IntSerialDenseVector& inds,
#endif
    Epetra_SerialDenseVector vec) const;

  //! apply a sparse matrix representation of a set of transforms from the left
  //! and right to a sparse matrix.
  Teuchos::RCP<Epetra_CrsMatrix> Apply(
    const Epetra_CrsMatrix& T, const Epetra_CrsMatrix& A) const ;

  //! apply a sparse matrix representation of a set of transforms from the left
  //! and right to a sparse matrix. This variant is to be preferred if the
  //! sparsity pattern of the transformed matrix TAT is already known.
  int Apply(
    Epetra_CrsMatrix& TAT, const Epetra_CrsMatrix& T, const Epetra_CrsMatrix& A) const;

  //! apply a sparse matrix representation of a set of transforms from the left
  //! to a vector.
  int Apply(
    Epetra_MultiVector& Tv, const Epetra_CrsMatrix& T, const Epetra_MultiVector& v) const;

  //! apply a sparse matrix representation of a set of transforms from the left
  //! to a vector.
  int ApplyInverse(
    Epetra_MultiVector& Tv, const Epetra_CrsMatrix& T, const Epetra_MultiVector& v) const;

  bool SaveMemory() const {return save_mem_;}

protected:

  //! object label
  std::string label_;

  //!
  int myLevel_;

  //! we store pointers to sparse matrices so that we can
  //! apply a series of transforms as T'AT more efficiently
  mutable Teuchos::RCP<Epetra_RowMatrixTransposer> Transp_;
  mutable Teuchos::RCP<const Epetra_CrsMatrix> Wmat_;
  mutable Teuchos::RCP<Epetra_CrsMatrix> WTmat_,Cmat_, WCmat_;

private:

  // if false, intermediate results are stored in the class that make
  // subsequent computations of T*A*T a bit faster. Since this costs a
  // lot of memory, we disable this feature until it becomes more of a
  // performance problem.
  static const bool save_mem_ = true;

  };

  }
#endif
