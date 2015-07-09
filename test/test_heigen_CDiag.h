/*
	CDiag.h
		the C prototypes for the Diag routines
		last modified 1 Aug 11 th
*/


#ifndef __CDiag_h__
#define __CDiag_h__

#ifndef Real
#define Real double
#endif

#ifdef __cplusplus

#include <complex>
typedef std::complex<Real> Complex;
#define Re(x) (x).real()
#define Im(x) (x).imag()
#define ToComplex(r,i) Complex(r, i)

extern "C" {

#else

#include <complex.h>
typedef Real complex Complex;
#define Re(x) creal(x)
#define Im(x) cimag(x)
#define ToComplex(r,i) ((r) + (i)*I)

#endif

void HEigensystem(const int n, Complex *A, const int ldA, Real *d, Complex *U, const int ldU, const int sort);
    
extern int nsweeps;

#ifdef __cplusplus
}
#endif

#define Matrix(A) (Complex *)(A),sizeof((A)[0])/sizeof((A)[0][0])

#endif
