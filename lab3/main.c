#include "mw_api.h"
#define MAX_DIGITS 100

struct userdef_work_t {
  char object[MAX_DIGITS];   
  char lower_bound[MAX_DIGITS];
  char upper_bound[MAX_DIGITS];
};

typedef struct my_node {
  mpz_t val;
  struct my_node *next;
} node;


struct userdef_result_t {
  char factor[MAX_DIGITS];
};

node *init_node(mpz_t val) {
  node *p = (node *) malloc(sizeof(node));
  p->next = NULL;
  mpz_init_set(p->val, val);
  return p;
}

MPI_Datatype encapsulate_work_type() {
  const int nitems = 3;
  int blocklengths[3] = {MAX_DIGITS, MAX_DIGITS, MAX_DIGITS};
  MPI_Datatype types[3] = {MPI_CHAR, MPI_CHAR, MPI_CHAR};
  MPI_Datatype mpi_work_type;
  MPI_Aint     offsets[3];

  offsets[0] = offsetof(struct userdef_work_t, object);
  offsets[1] = offsetof(struct userdef_work_t, lower_bound);
  offsets[2] = offsetof(struct userdef_work_t, upper_bound);

  MPI_Type_create_struct(nitems, blocklengths, offsets, types, &mpi_work_type);
  MPI_Type_commit(&mpi_work_type);
  return mpi_work_type;
}

MPI_Datatype encapsulate_result_type() {
  const int nitems = 1;
  int blocklengths[1] = {MAX_DIGITS};
  MPI_Datatype types[1] = {MPI_CHAR};
  MPI_Datatype mpi_result_type;
  MPI_Aint     offsets[1];

  offsets[0] = offsetof(struct userdef_result_t, factor);

  MPI_Type_create_struct(nitems, blocklengths, offsets, types, &mpi_result_type);
  MPI_Type_commit(&mpi_result_type);
  return mpi_result_type;
}




mw_work_t **create(int sz) {
  mw_work_t **work_list = (mw_work_t **) malloc(sz * sizeof(mw_work_t *));

  char number[MAX_DIGITS] = "10000000000000000";
  
  mpz_t object;
  
  mpz_init_set_str (object, number, 10);
  
  mpz_t factor_limit;
  mpz_init(factor_limit);
  mpz_sqrt (factor_limit, object);
  //gmp_printf ("factor_limit %Zd\n", factor_limit);

  mpz_t factor_chunk_size,num_processors;
  mpz_init_set_ui (num_processors, sz);
  //gmp_printf ("num_p %Zd\n", num_processors);

  mpz_init(factor_chunk_size);
  mpz_fdiv_q (factor_chunk_size, factor_limit, num_processors);
  //gmp_printf ("chunk %Zd\n", factor_chunk_size);

  int i;
  for(i = 0;i<sz;++i) {
    *(work_list+i) = (mw_work_t *) malloc(sizeof(mw_work_t));
    mw_work_t* work = *(work_list+i);
    mpz_get_str (work->object, 10, object);

    mpz_t lower_bound,upper_bound;
    mpz_init(lower_bound);
    mpz_init(upper_bound);
    mpz_mul_ui (lower_bound, factor_chunk_size, i);
    mpz_add_ui (lower_bound, lower_bound, 1);
    mpz_get_str (work->lower_bound, 10, lower_bound);
    if(i==sz-1){
      mpz_get_str (work->upper_bound, 10, factor_limit);
    } else {
      mpz_mul_ui (upper_bound, factor_chunk_size, (i+1));
      mpz_get_str (work->upper_bound, 10, upper_bound);
    }
  }
  return work_list;
}

int result(int np, int *sz, mw_result_t **result_list) {
  if(result_list==NULL){
    printf("No Result!\n");
    return 0;
  }
  printf("Factors: \n");
  int i,j;
  for(i = 0; i < np; ++i) {
    mw_result_t *curr_result = *(result_list+i);
    //printf("%d \n", *(sz+i));
    for(j = 0;j < *(sz+i); ++j) {
      char *str = (char *)(curr_result[j].factor);
      printf("%s ", str);
    }
  }
  printf("\n");
  return 1;
}

int compute(mw_work_t *work, mw_result_t **res) {
  // printf("lower_bound: %s, upper_bound: %s\n", 
  //   work->lower_bound, work->upper_bound);
  mpz_t start, end, object;  
  mpz_init_set_str(start, work->lower_bound, 10);
  mpz_init_set_str(end, work->upper_bound, 10);
  mpz_init_set_str(object, work->object, 10);
  mpz_add_ui(end, end, 1);

  node *head = NULL;
  node *prev = NULL;
  int size = 0;
  mpz_t remain;
  mpz_init(remain);
  while(mpz_cmp(start,end)){
    mpz_fdiv_r(remain, object, start);
    if(!mpz_cmp_ui(remain,0)){
      node *curr = init_node(start);
      if(head==NULL){
        head = curr;
        prev = curr;
      } else {
        prev->next = curr;
        prev = prev->next;
      }
      size++; 
    }
    mpz_add_ui(start, start, 1);
  }

  *res = (mw_result_t *) malloc((size+1) * sizeof(mw_result_t));
  int i = 0;
  for(i = 0;i < size; ++i) {
    mpz_get_str ((*res)[i].factor, 10, head->val);
    head = head->next;
  }
  
  return size;
}

int main (int argc, char **argv) {
  struct mw_api_spec f;
  MPI_Init (&argc, &argv);
  f.create = create;
  f.result = result;
  f.compute = compute;
  f.work_sz = sizeof (mw_work_t);
  f.res_sz = sizeof (mw_result_t);
  f.work_type = encapsulate_work_type();
  f.result_type = encapsulate_result_type();
  //printf("result size: %d\n", f.res_sz);
  
  MW_Run (argc, argv, &f);

  return 0;

}