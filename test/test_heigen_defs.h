//
//  definizioni.h
//  c
//
//  Created by Christian Napoli on 121406.
//  Copyright (c) 2012 University of Catania. All rights reserved.
//

#pragma once

#define N 16

#ifndef PI
    #define PI  3.14159265
#endif

#define sigma   0.5

#define J       0.5
#define x_min   -4.0e0
#define x_max   4.0e0
#define	  delta_x 1e-6
//#define dx       1e-3

#define El(A, i, j, n) (A[i*n+j])

double CHARGEF (void) {
    return J/2 ;
}

double E0F (void) {
    return (N/2)*(N/2)*fabs(J);
}

double ECHF (int i, double x) {
    return  (i-x)*(i-x)-E0F();
}

double HDIAGF (int i, double x) {
    return ECHF(i-(N/2), x);
}

double QDIAGF (int i) {
    return i-(N/2);
}

double GAUSSF (double x) {
        return (1/(sigma*sqrt(2*PI)))*exp(-(x*x)/(2*sigma*sigma));
}
