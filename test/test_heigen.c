//============================================================================
// Name        : main.c
// Author      : Christian Napoli
// Version     :
// Copyright   : Released under GNU public licence
// Description :
//============================================================================

#undef Complex

#include "cs_workerpool.h"

#include "test_heigen_CDiag.h"
#include "test_heigen_diag-c.h"
#include "test_heigen_defs.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>



#ifndef PI
    #define PI  3.14159265
#endif

#define A(i,j) A[i-1][j-1]
#define H(i,j) H[i-1][j-1]
#define Q(i,j) Q[i-1][j-1]
#define E(i,j) E[i-1][j-1]

#define QSIZE 100

/**	    **
** ROUTINES **
**	    **/
void SetMatrixH(Complex A[N][N], double x) {
    int i, j;
    for( i = 1; i <= N; ++i ) {
        for( j = 1; j <= N; ++j ) {
            if((j==i+1)||(j==i-1)) A(i,j) = ToComplex(CHARGEF(),0);
            else if (j==i) A(i,j) = ToComplex(HDIAGF(i,x),0);
            else A(i,j)=ToComplex(0,0);
        }
    }
}

void SetMatrixQ(double Q[N][N]){
    int i, j;
    for( i = 1; i <= N; ++i ) {
        for( j = 1; j <= N; ++j ) {
            if (j==i) Q(i,j) = QDIAGF(i);
            else Q(i,j)=0;
        }
    }
}

#define printr(name, dr) \
{ int i; \
printf("\n" name ":\n"); \
for( i = 0; i < N; ++i ) \
printf("E(%d) = %10.6f\n", i + 1, dr[i]); }

#define printc(name, dc) \
{ int i; \
printf("\n" name ":\n"); \
for( i = 0; i < N; ++i ) \
printf("E(%d) = %10.6f%+10.6f I\n", i + 1, Re(dc[i]), Im(dc[i])); }

void HEigensystem(cint n, Complex *A, cint ldA, Real *d, Complex *U, cint ldU, cint sort){
    int p, q;
    cReal red = .04/(n*n*n*n);
    Real ev[n][2];

    for( p = 0; p < n; ++p ) {
        ev[p][0] = 0;
        ev[p][1] = d[p] = Re(Element(A,p,p));
    }

    for( p = 0; p < n; ++p ) {
        memset(&Element(U,p,0), 0, n*sizeof(Complex));
        Element(U,p,p) = 1;
    }

    for( nsweeps = 1; nsweeps <= 50; ++nsweeps ) {
        Real thresh = 0;
        for( q = 1; q < n; ++q )
            for( p = 0; p < q; ++p )
                thresh += Sq(Element(A,p,q));
        if( !(thresh > SYM_EPS) ) goto done;

        thresh = (nsweeps < 4) ? thresh*red : 0;

        for( q = 1; q < n; ++q )
            for( p = 0; p < q; ++p ) {
                cComplex Apq = Element(A,p,q);
                cReal off = Sq(Apq);
                if( nsweeps > 4 && off < SYM_EPS*(sq(ev[p][1]) + sq(ev[q][1])) )
                    Element(A,p,q) = 0;
                else if( off > thresh ) {
                    Real delta, t, s, invc;
                    int j;
                    
                    t = .5*(ev[p][1] - ev[q][1]);
                    t = 1/(t + sign(sqrt(t*t + off), t));
                    delta = off*t;
                    ev[p][1] = d[p] + (ev[p][0] += delta);
                    ev[q][1] = d[q] + (ev[q][0] -= delta);
                    invc = sqrt(delta*t + 1);
                    s = t/invc;
                    t = delta/(invc + 1);
                    
                    for( j = 0; j < p; ++j ) {
                        cComplex x = Element(A,j,p);
                        cComplex y = Element(A,j,q);
                        Element(A,j,p) = x + s*(Conjugate(Apq)*y - t*x);
                        Element(A,j,q) = y - s*(Apq*x + t*y);
                    }
                    
                    for( j = p + 1; j < q; ++j ) {
                        cComplex x = Element(A,p,j);
                        cComplex y = Element(A,j,q);
                        Element(A,p,j) = x + s*(Apq*Conjugate(y) - t*x);
                        Element(A,j,q) = y - s*(Apq*Conjugate(x) + t*y);
                    }
                    
                    for( j = q + 1; j < n; ++j ) {
                        cComplex x = Element(A,p,j);
                        cComplex y = Element(A,q,j);
                        Element(A,p,j) = x + s*(Apq*y - t*x);
                        Element(A,q,j) = y - s*(Conjugate(Apq)*x + t*y);
                    }
                    
                    Element(A,p,q) = 0;
                    
                    for( j = 0; j < n; ++j ) {
                        cComplex x = Element(U,p,j);
                        cComplex y = Element(U,q,j);
                        Element(U,p,j) = x + s*(Apq*y - t*x);
                        Element(U,q,j) = y - s*(Conjugate(Apq)*x + t*y);
                    }
                }
            }
        
        for( p = 0; p < n; ++p ) {
            ev[p][0] = 0;
            d[p] = ev[p][1];
        }
    }
    
    fputs("Bad convergence in HEigensystem\n", stderr);
    
done:
    if( sort == 0 ) return;
    
    /* sort the eigenvalues */
    
    for( p = 0; p < n - 1; ++p ) {
        int j = p;
        Real t = d[p];
        for( q = p + 1; q < n; ++q )
            if( sort*(t - d[q]) > 0 ) t = d[j = q];
        if( j == p ) continue;
        d[j] = d[p];
        d[p] = t;
        for( q = 0; q < n; ++q ) {
            cComplex x = Element(U,p,q);
            Element(U,p,q) = Element(U,j,q);
            Element(U,j,q) = x;
        }
    }
}

void knl_matrix(double x, double Q[N][N]) {
    enum { sort = 0 };
    Complex H[N][N], A[N][N], U[N][N];
    // Complex V[N][N], dc[N];
    double dr[N], E[N][N], EQ[N][N];
    int i, j, k;
    
    SetMatrixH(H, x);
    SetMatrixH(A, x);    
    HEigensystem(N, Matrix(H), dr, Matrix(U), sort);
    //printr("HEigensystem", dr); FM
    
    for(i=0;i<N;++i) {
        for (j=0;j<N;++j) {
            E[i][j]=sqrt((Re(U[i][j])*Re(U[i][j]))+(Im(U[i][j])*Im(U[i][j])));
            //printf("%10.6f%+10.6fi\t", Re(U[i][j]), Im(U[i][j]) );
            //printf("%10.6f%\t", E[i][j] ); FM
        }
    //printf("\n"); FM
    }
    //printf("\n"); FM
    SetMatrixQ(Q);
    
    for(i=0;i<N;++i) {
        for (j=0;j<N;++j) {
            EQ[i][j]= 0;
            for (k=0;k<N;++k){
                EQ[i][j]=EQ[i][j]+(E[i][k]*Q[k][j]);
            }
        }
    }
    for(i=0;i<N;++i) {
        for (j=0;j<N;++j) {
            Q[i][j]= 0;
            for (k=0;k<N;++k){
                Q[i][j]=Q[i][j]+(EQ[i][k]*E[j][k]);
            }
            //printf("%10.6f%\t", Q[i][j]); FM
        }
        //printf("\n"); FM
    }
}


/** COMPLEX-SIM RELATED ROUTINES/FUNCTION **/

typedef struct heigen_tsk_data{
  double x1,x2;
  double dx;
}heigen_tsk_data_t;

cs_wp_tsk_exit_status  heigen_task(cs_data_ptr in, cs_data_ptr *out){
  double x;
  heigen_tsk_data_t *data = (heigen_tsk_data_t*) in;
  int i,j;
  double **Qin = (double**) malloc(sizeof(double*)*N);
  double Q[N][N];

  for(i=0; i<N; i++){
    Qin[i] = (double*) malloc(sizeof(double)*N); //allocates matrix
    memset(Qin[i], (int) 0x0, sizeof(double)*N); 
  }

  for(x=data->x1; x<=data->x2; x+=data->dx){
    knl_matrix(x, Q);
    for(i=0; i<N; i++)
      for(j=0; j<N; j++)
	Qin[i][j]+=Q[i][j]*GAUSSF(x);
  }

  *out = (void*) Qin;
  return CS_TSK_SUCCESS;
}

void free_matrix(void **ptr, int dim){
  int i = 0;
  for(i=0; i<dim; i++)
    free(ptr[i]);
}

int main (int argc, char *argv[]){
    double **Q;
    double x, x_prev;
    int i,j,t;
    double IQ[N][N], AV_Q[N][N], CNORM;
    int ncore, ntask;
    FILE *fd;
    double dx,D;
    cs_wp_tsk_id *tsk_vec;
    heigen_tsk_data_t *data_in_vec;

    log4c_init();
    log4c_category_t *log = log4c_category_get("cs.test.H");

    if(argc<3){
      log4c_category_log(log, LOG4C_PRIORITY_FATAL, "Insufficient number of arguments. Usage: %s <dx> <ncore> [out-file]", argv[0]);
      return -1;
    }

    dx = atof(argv[1]);
    log4c_category_log(log, LOG4C_PRIORITY_NOTICE, "dx: %1.14f", dx);

    ncore = atoi(argv[2]);
    log4c_category_log(log, LOG4C_PRIORITY_NOTICE, "ncore: %d", ncore);
    ntask = ncore;

    D=(x_max-x_min)/ncore;
    log4c_category_log(log, LOG4C_PRIORITY_NOTICE, "D=%10.6f", D);
    tsk_vec = (cs_wp_tsk_id *) malloc(sizeof(cs_wp_tsk_id)*ntask);

    /* Allocate and initialize the workerpool */
    cs_workerpool_t* wp = (cs_workerpool_t *) malloc(sizeof(cs_workerpool_t));
    if(!wp)
      log4c_category_log(log, LOG4C_PRIORITY_ERROR, " Unable to malloc the worker pool");

    if(cs_wp_init(ncore, QSIZE, wp)<0 || cs_wp_start(wp)<0)
      log4c_category_log(log, LOG4C_PRIORITY_ERROR, " Unable to init the worker pool");

    /* initialize the input data */
    log4c_category_log(log, LOG4C_PRIORITY_NOTICE, "Initializing the input vector ");
    data_in_vec = (heigen_tsk_data_t*) malloc(sizeof(heigen_tsk_data_t)*ntask);
    i=0;
    for(x=x_min; x<x_max-delta_x; x+=D){
      data_in_vec[i].x1 = x;
      data_in_vec[i].x2 = min(x+D, x_max);
      data_in_vec[i].dx = dx;
      i++;
    }

    ntask=i; //can be == ncore or ncore+1
    log4c_category_log(log, LOG4C_PRIORITY_NOTICE, "ntask=%d", ntask);

    /* Initialize matrices IQ,AV_Q */
    log4c_category_log(log, LOG4C_PRIORITY_NOTICE, "Initializing IQ and AV_Q..");
    for(i=0;i<N;++i){
        for (j=0;j<N;++j){
            IQ[i][j]=0;
            AV_Q[i][j]=0;
        }
    }

    /* Enqueue the tasks */
    log4c_category_log(log, LOG4C_PRIORITY_NOTICE, "Enqueue %d tasks..", ntask);
    for(t=0; t<ntask; t++){
	if((tsk_vec[t]=cs_wp_tsk_enqueue(heigen_task, (void*) &data_in_vec[t], sizeof(double), wp))<0)
	    log4c_category_log(log, LOG4C_PRIORITY_ERROR, " Unable to enqueue task %d", t);
	else
	    log4c_category_log(log, LOG4C_PRIORITY_TRACE, "Task %d (id=%li, x-range=[%2.10f,%2.10f]) enqueued", t, tsk_vec[t], data_in_vec[t].x1, data_in_vec[t].x2);
	sleep(0.5);
    }

    /* Wait for the completion of the tasks and get(pop) the result */
    log4c_category_log(log, LOG4C_PRIORITY_NOTICE, "Getting results..", ntask);
    CNORM=0;
    x_prev=x=x_min;
    for(t=0; t<ntask; t++){
      cs_wp_tsk_wait(wp, tsk_vec[t]);
      Q = (double **) (cs_wp_tsk_pop_res(wp, tsk_vec[t]))->tsk_res_data; //result for x=x_min+t*dx

      //Accumulate CNORM for x=x_n to x_n+1 where x_(n+1)==x_n+D
      for(x=x_prev; x<=(t+1)*D+x_prev && x<=x_max; x+=dx)
	CNORM = CNORM + GAUSSF(x);

      //update IQ and AV_Q
      for(i=0;i<N;++i){
	for (j=0;j<N;++j){
	  IQ[i][j] = IQ[i][j]+Q[i][j];
	  AV_Q[i][j]=AV_Q[i][j]+Q[i][j];
	}
      }
  
      free_matrix((void**) Q,N); //release memory of matrix Q
    }

    if(argv[3]!=NULL){
      if(!(fd = fopen(argv[3], "w"))){
      log4c_category_log(log, LOG4C_PRIORITY_ERROR, " Error opening file %s", argv[3]);
      return -1;
    }

    log4c_category_log(log, LOG4C_PRIORITY_NOTICE, "Writing results..", ntask);
    for(i=0;i<N;++i) {
        for (j=0;j<N;++j){
            IQ[i][j] = IQ[i][j]*dx;
            AV_Q[i][j]=AV_Q[i][j]/CNORM;
            fprintf(fd, "%12.8f\t", AV_Q[i][j]);
        }
        fprintf(fd, "\n");
      }
    }
    return 0;
}
