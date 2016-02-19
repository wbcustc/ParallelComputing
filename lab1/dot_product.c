#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#define VECTOR_SIZE (1024*1024)

void init_vec(float *vec, int n){
	srand(time(NULL));
	int i;
	for(i = 0;i < n;i++){
		vec[i] = rand()/1000000000.0;
	}
}

float calculate(float *vec1,float *vec2, int n){
	float sum = 0.0;
	int i;
	for(i = 0;i<n;i++){
		sum += (*(vec1+i)) * (*(vec2+i));
	}
	return sum;
}

int main(int argc, char **argv){
	int sz,myid;
	double starttime,endtime;
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &sz);
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);
	float *vec1,*vec2;
	int blocksize = VECTOR_SIZE/sz;
	float local,global;
	MPI_Barrier(MPI_COMM_WORLD);

	if(myid == 0){
		vec1 = (float *)malloc(VECTOR_SIZE*sizeof(float));
		vec2 = (float *)malloc(VECTOR_SIZE*sizeof(float));
		init_vec(vec1, VECTOR_SIZE);
		init_vec(vec2, VECTOR_SIZE);
		starttime = MPI_Wtime();

	}

	float *temp_vec1 = (float *)malloc(blocksize*sizeof(float));
	float *temp_vec2 = (float *)malloc(blocksize*sizeof(float));
	
	// Dispatch the data chunk into different processors from processor 0.
	MPI_Scatter(vec1, blocksize, MPI_FLOAT, temp_vec1,
            blocksize, MPI_FLOAT, 0, MPI_COMM_WORLD);

	MPI_Scatter(vec2, blocksize, MPI_FLOAT, temp_vec2,
            blocksize, MPI_FLOAT, 0, MPI_COMM_WORLD);

	local = calculate(temp_vec1,temp_vec2,blocksize);

	// Reduce data from other processors to processor 0.
	MPI_Reduce(&local, &global, 1, MPI_FLOAT, MPI_SUM, 0, 
		MPI_COMM_WORLD);
	
	if(myid == 0) {
		endtime = MPI_Wtime();
		printf("Time cost is %lf\n", (endtime-starttime)*1000000000.0/VECTOR_SIZE);
		printf("Product is %lf\n", global);
	}

	MPI_Finalize();
	exit (0);
}
