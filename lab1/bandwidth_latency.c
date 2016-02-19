#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#define BLOCK_SIZE 1024
#define REP_TIMES 1000

int main(int argc, char **argv){
  int sz,myid;
  double starttime,endtime;
  MPI_Init(&argc,&argv);
  MPI_Comm_size(MPI_COMM_WORLD, &sz);
  MPI_Comm_rank(MPI_COMM_WORLD, &myid);
  int data[BLOCK_SIZE];
  int i;
  for(i = 0;i < BLOCK_SIZE;i++){
    data[i] = i;
  }
  int dest = sz - myid - 1; 
  MPI_Barrier(MPI_COMM_WORLD);
  // Change the data block size to show the impact of the bandwidth.
  for(int block = 1;block <= BLOCK_SIZE;block *= 2){ 
    
    if (myid < sz/2) {
      
      starttime = MPI_Wtime();
      // Send 1000 times to calculate the average message latency. 
      for(i = 0;i<REP_TIMES;i++){
        MPI_Send(data, block, MPI_INT, dest, 0,
                   MPI_COMM_WORLD);
      
        MPI_Recv(data, block, MPI_INT, dest, 0,
                  MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      }
      endtime = MPI_Wtime();
      printf("That took %f milliseconds\n",(endtime-starttime)*1000000.0/REP_TIMES);
    } else {
      for(i = 0;i<REP_TIMES;i++){
      MPI_Recv(data, block, MPI_INT, dest, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      
      MPI_Send(data, block, MPI_INT, dest, 0,
                MPI_COMM_WORLD);
      }
    }
  } 
  MPI_Finalize ();
  exit (0);
}