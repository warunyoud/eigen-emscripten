

// #define EIGEN_DEFAULT_TO_ROW_MAJOR
#define _FLOAT

#include <Eigen/Array>
#include <Eigen/Core>
#include "BenchTimer.h"

// include the BLAS headers
#include <cblas.h>
#include <string>

#ifdef _FLOAT
typedef float Scalar;
#define CBLAS_GEMM cblas_sgemm
#else
typedef double Scalar;
#define CBLAS_GEMM cblas_dgemm
#endif


typedef Eigen::Matrix<Scalar,Eigen::Dynamic,Eigen::Dynamic> MyMatrix;
void bench_eigengemm(MyMatrix& mc, const MyMatrix& ma, const MyMatrix& mb, int nbloops);
void bench_eigengemm_normal(MyMatrix& mc, const MyMatrix& ma, const MyMatrix& mb, int nbloops);
void check_product(int M, int N, int K);
void check_product(void);

int main(int argc, char *argv[])
{
  {
    int aux;
    asm(
    "stmxcsr   %[aux]           \n\t"
    "orl       $32832, %[aux]   \n\t"
    "ldmxcsr   %[aux]           \n\t"
    : : [aux] "m" (aux));
  }

  int nbtries=1, nbloops=1, M, N, K;

  if (argc==2)
  {
    if (std::string(argv[1])=="check")
      check_product();
    else
      M = N = K = atoi(argv[1]);
  }
  else if ((argc==3) && (std::string(argv[1])=="auto"))
  {
    M = N = K = atoi(argv[2]);
    nbloops = 1000000000/(M*M*M);
    if (nbloops<1)
      nbloops = 1;
    nbtries = 6;
  }
  else if (argc==4)
  {
    M = N = K = atoi(argv[1]);
    nbloops = atoi(argv[2]);
    nbtries = atoi(argv[3]);
  }
  else if (argc==6)
  {
    M = atoi(argv[1]);
    N = atoi(argv[2]);
    K = atoi(argv[3]);
    nbloops = atoi(argv[4]);
    nbtries = atoi(argv[5]);
  }
  else
  {
    std::cout << "Usage: " << argv[0] << " size nbloops nbtries\n";
    std::cout << "Usage: " << argv[0] << " M N K nbloops nbtries\n";
    exit(1);
  }

  double nbmad = double(M) * double(N) * double(K) * double(nbloops);

  if (!(std::string(argv[1])=="auto"))
    std::cout << M << " x " << N << " x " << K << "\n";

  Scalar alpha, beta;
  MyMatrix ma(M,K), mb(K,N), mc(M,N);
  ma = MyMatrix::random(M,K);
  mb = MyMatrix::random(K,N);
  mc = MyMatrix::random(M,N);

  Eigen::BenchTimer timer;

  // we simply compute c += a*b, so:
  alpha = 1;
  beta = 1;

  // bench cblas
  // ROWS_A, COLS_B, COLS_A, 1.0,  A, COLS_A, B, COLS_B, 0.0, C, COLS_B);
  if (!(std::string(argv[1])=="auto"))
  {
    timer.reset();
    for (uint k=0 ; k<nbtries ; ++k)
    {
        timer.start();
        for (uint j=0 ; j<nbloops ; ++j)
              #ifdef EIGEN_DEFAULT_TO_ROW_MAJOR
              CBLAS_GEMM(CblasRowMajor, CblasNoTrans, CblasNoTrans, M, N, K, alpha, ma.data(), K, mb.data(), N, beta, mc.data(), N);
              #else
              CBLAS_GEMM(CblasColMajor, CblasNoTrans, CblasNoTrans, M, N, K, alpha, ma.data(), M, mb.data(), K, beta, mc.data(), M);
              #endif
        timer.stop();
    }
    if (!(std::string(argv[1])=="auto"))
      std::cout << "cblas: " << timer.value() << " (" << 1e-3*floor(1e-6*nbmad/timer.value()) << " GFlops/s)\n";
    else
        std::cout << M << " : " << timer.value() << " ; " << 1e-3*floor(1e-6*nbmad/timer.value()) << "\n";
  }

  // clear
  ma = MyMatrix::random(M,K);
  mb = MyMatrix::random(K,N);
  mc = MyMatrix::random(M,N);

  // eigen fast ?
//   if (!(std::string(argv[1])=="auto"))
  {
      timer.reset();
      for (uint k=0 ; k<nbtries ; ++k)
      {
          timer.start();
          bench_eigengemm(mc, ma, mb, nbloops);
          timer.stop();
      }
      if (!(std::string(argv[1])=="auto"))
        std::cout << "eigen : " << timer.value() << " (" << 1e-3*floor(1e-6*nbmad/timer.value()) << " GFlops/s)\n";
      else
        std::cout << M << " : " << timer.value() << " ; " << 1e-3*floor(1e-6*nbmad/timer.value()) << "\n";
  }

  // clear
  ma = MyMatrix::random(M,K);
  mb = MyMatrix::random(K,N);
  mc = MyMatrix::random(M,N);

  // eigen normal
  if (!(std::string(argv[1])=="auto"))
  {
      timer.reset();
      for (uint k=0 ; k<nbtries ; ++k)
      {
          timer.start();
          bench_eigengemm_normal(mc, ma, mb, nbloops);
          timer.stop();
      }
      std::cout << "eigen : " << timer.value() << " (" << 1e-3*floor(1e-6*nbmad/timer.value()) << " GFlops/s)\n";
  }

  return 0;
}

using namespace Eigen;



void bench_eigengemm(MyMatrix& mc, const MyMatrix& ma, const MyMatrix& mb, int nbloops)
{
  for (uint j=0 ; j<nbloops ; ++j)
    #ifdef EIGEN_WIP_PRODUCT_DIRTY
      mc = ma * mb;
    #else
      mc += (ma * mb).lazy();
      /*static_cast<MatrixBase<MyMatrix>& >*//*(mc).operator+=( (ma * mb).lazy() );*/

//       Flagged<Product<MyMatrix,MyMatrix,CacheFriendlyProduct>, 0, EvalBeforeNestingBit | EvalBeforeAssigningBit>(
//         Product<MyMatrix,MyMatrix,CacheFriendlyProduct>(ma, mb)));
    #endif
}

void bench_eigengemm_normal(MyMatrix& mc, const MyMatrix& ma, const MyMatrix& mb, int nbloops)
{
  for (uint j=0 ; j<nbloops ; ++j)
    mc += Product<MyMatrix,MyMatrix,NormalProduct>(ma,mb).lazy();
}

#define MYVERIFY(A,M) if (!(A)) { \
    std::cout << "FAIL: " << M << "\n"; \
  }

void check_product(int M, int N, int K)
{
  MyMatrix ma(M,K), mb(K,N), mc(M,N), maT(K,M), mbT(N,K), meigen(M,N), mref(M,N);
  ma = MyMatrix::random(M,K);
  mb = MyMatrix::random(K,N);
  maT = ma.transpose();
  mbT = mb.transpose();
  mc = MyMatrix::random(M,N);

  MyMatrix::Scalar eps = 1e-4;

  meigen = mref = mc;
  CBLAS_GEMM(CblasColMajor, CblasNoTrans, CblasNoTrans, M, N, K, 1, ma.data(), M, mb.data(), K, 1, mref.data(), M);
  meigen += ma * mb;
  MYVERIFY(meigen.isApprox(mref, eps),". * .");

//   meigen = mref = mc;
//   CBLAS_GEMM(CblasColMajor, CblasTrans, CblasNoTrans, M, N, K, 1, maT.data(), K, mb.data(), K, 1, mref.data(), M);
//   meigen += maT.transpose() * mb;
//   MYVERIFY(meigen.isApprox(mref, eps),"T * .");
// 
//   meigen = mref = mc;
//   CBLAS_GEMM(CblasColMajor, CblasTrans, CblasTrans, M, N, K, 1, maT.data(), K, mbT.data(), N, 1, mref.data(), M);
//   meigen += (maT.transpose()) * (mbT.transpose());
//   MYVERIFY(meigen.isApprox(mref, eps),"T * T");
// 
//   meigen = mref = mc;
//   CBLAS_GEMM(CblasColMajor, CblasNoTrans, CblasTrans, M, N, K, 1, ma.data(), M, mbT.data(), N, 1, mref.data(), M);
//   meigen += ma * mbT.transpose();
//   MYVERIFY(meigen.isApprox(mref, eps),". * T");
}

void check_product(void)
{
  int M, N, K;
  for (uint i=0; i<1000; ++i)
  {
    M = ei_random<int>(1,64);
    N = ei_random<int>(1,768);
    K = ei_random<int>(1,768);
    M = (0 + M) * 1;
    std::cout << M << " x " << N << " x " << K << "\n";
    check_product(M, N, K);
  }
}

