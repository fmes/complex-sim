/*
   diag-c.h
   global declarations for the Diag routines in C
   this file is part of Diag
   last modified 9 Aug 11 th
*/

#ifndef DIAG_C
#define DIAG_C

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <complex.h>

#ifdef QUAD
#define Real long double
#define Re creall
#define Im cimagl
#define Conjugate conjl
#define Sqrt csqrtl
#define Abs cabsl
#define abs fabsl
#define sign copysignl
#else
#define Real double
#define Re creal
#define Im cimag
#define Conjugate conj
#define Sqrt csqrt
#define Abs cabs
#define abs fabs
#define sign copysign
#endif

typedef const int cint;
typedef const Real cReal;
// typedef Real complex Complex;
typedef const Complex cComplex;

static inline Real Sq(cComplex x) { return Re(x*Conjugate(x)); }

static inline Real sq(cReal x) { return x*x; }

static inline Real min(cReal a, cReal b) { return (a < b) ? a : b; }

static inline Real max(cReal a, cReal b) { return (a > b) ? a : b; }

static inline int imin(cint a, cint b) { return (a < b) ? a : b; }

static inline int imax(cint a, cint b) { return (a > b) ? a : b; }


int nsweeps;

#define Element(A,i,j) A[(i)*ld##A+(j)]


/* A matrix is considered diagonal if the sum of the squares
   of the off-diagonal elements is less than EPS.  SYM_EPS is
   half of EPS since only the upper triangle is counted for
   symmetric matrices.
   52 bits is the mantissa length for IEEE double precision. */

#define EPS 0x1p-102

#define SYM_EPS 0x1p-103

#define DBL_EPS 0x1p-52

#endif
