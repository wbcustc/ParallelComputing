 /* in file mw_api.h */
#include <mpi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <gmp.h>
#include <ctype.h>
#include <time.h>

#ifndef __MW_API__
#define __MW_API__

struct userdef_work_t; /* definition provided by user */
struct userdef_result_t; /* definition provided by user */
typedef struct userdef_work_t mw_work_t;
typedef struct userdef_result_t mw_result_t;

struct mw_api_spec {
   mw_work_t **(*create) (int sz);

   int (*result) (int np, int *sz, mw_result_t **res);     

   int (*compute) (mw_work_t *work, mw_result_t **res);   
      
   int work_sz, res_sz;
   
   MPI_Datatype work_type;

   MPI_Datatype result_type;
};

void MW_Run (int argc, char** argv, struct mw_api_spec *f); /* run master-worker */

#endif