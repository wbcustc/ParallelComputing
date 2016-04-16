/* mw_api.c : implementation of everything needed by the API */
#include "mw_api.h"

#define DYNAMIC


// If you want to use the static assignment, just comment this line.
#ifdef DYNAMIC

#define MESSAGE_CHANNEL 7
#define STATUS_CHANNEL 8
#define REQUEST 1
#define RECEIVE 2
#define SHUTDOWN 1
#define MOVEON 2

#endif

void master(struct mw_api_spec *f, int sz){
	// Master Code

	mw_work_t** work_list;
	MPI_Request request;
#ifndef DYNAMIC
	// Static 
	int dest;
	work_list = f->create(sz-1);
	for(dest = 1; dest < sz; ++dest) {
		mw_work_t *curr = *(work_list + dest - 1);
		MPI_Isend(curr, 1, f->work_type, dest, 0,
             MPI_COMM_WORLD, &request);
	}

	mw_result_t** result_list;
	int *res_size = (int *)malloc((sz-1) * sizeof(int));
	result_list = (mw_result_t **) malloc((sz-1) * sizeof(mw_result_t *));
	int src;
	mw_result_t *curr_result;
	for(src = 1; src < sz; ++src) {
		int *curr_size = res_size + src - 1;
		
		MPI_Recv(curr_size, 1, MPI_INT, src, 1,
			MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		*(result_list+src-1) = (mw_result_t *) malloc((*curr_size) * f->res_sz);
		MPI_Recv(*(result_list+src-1), *curr_size, f->result_type, src, 2, 
			MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}
	f->result(sz-1, res_size, result_list);
#endif
#ifdef DYNAMIC
	// Dynamic
	int curr;
	int message[2];
	int status;
	int dest = 0;
	int WORK_NUM = sz-1;
	printf("Input the number of works(Integer):\n");
	scanf("%d",&WORK_NUM);

	mw_result_t** result_list;
	int *res_size = (int *)malloc((WORK_NUM) * sizeof(int));
	result_list = (mw_result_t **) malloc((WORK_NUM) * sizeof(mw_result_t *));
	int result_index = 0;
	int work_index = 0;

	work_list = f->create(WORK_NUM);
	while(result_index != WORK_NUM) {
		//Receive a message and do correspond operation.
		MPI_Recv(message, 2, MPI_INT, MPI_ANY_SOURCE, MESSAGE_CHANNEL, 
			MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		dest = message[0];

		// Send a piece of work
		if(message[1] == REQUEST) {
			if(work_index == WORK_NUM) {
				status = SHUTDOWN;
				MPI_Send(&status, 1, MPI_INT, dest, STATUS_CHANNEL, MPI_COMM_WORLD);
			} else {
				status = MOVEON;
				MPI_Send(&status, 1, MPI_INT, dest, STATUS_CHANNEL, MPI_COMM_WORLD);

				mw_work_t *temp = *(work_list + work_index);
				MPI_Isend(temp, 1, f->work_type, dest, 0,
		         	MPI_COMM_WORLD, &request);
				work_index++;
				//printf("Send a piece of work to processor %d\n", dest);	
			}
		}

		// Receive a piece of result
		if(message[1] == RECEIVE) {
			int *curr_size = res_size + result_index;
			MPI_Recv(curr_size, 1, MPI_INT, dest, 1,
				MPI_COMM_WORLD, MPI_STATUS_IGNORE);

			*(result_list + result_index) = (mw_result_t *) malloc((*curr_size) * f->res_sz);
			MPI_Recv(*(result_list + result_index), *curr_size, f->result_type, dest, 2, 
				MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			result_index++;
			if(work_index == WORK_NUM) {
				status = SHUTDOWN;
				MPI_Send(&status, 1, MPI_INT, dest, STATUS_CHANNEL, MPI_COMM_WORLD);
			} else {
				status = MOVEON;
				MPI_Send(&status, 1, MPI_INT, dest, STATUS_CHANNEL, MPI_COMM_WORLD);
			}
		}

	}

	f->result(WORK_NUM, res_size, result_list);
	
#endif
}

void slave(struct mw_api_spec *f, int sz, int myid) {
	// Slave Code
	mw_work_t *my_work = (mw_work_t *) malloc(f->work_sz);
	mw_result_t **res = (mw_result_t **) malloc(sizeof(mw_result_t *)); 
#ifndef DYNAMIC
	// Static
	MPI_Recv(my_work, 1, f->work_type, 0, 0,
	       MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	int size = f->compute(my_work, res);
	
	MPI_Send(&size, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
	MPI_Send(*res, size, f->result_type, 0, 2, MPI_COMM_WORLD);
#endif
#ifdef DYNAMIC
	// Dynamic
	int message[2];
	int status; 
	MPI_Request request;
	message[0] = myid;
	while(1) {
		// Send Request for work
		message[1] = REQUEST;
		MPI_Send(message, 2, MPI_INT, 0, MESSAGE_CHANNEL, MPI_COMM_WORLD);
		// Receive current status
		MPI_Recv(&status, 1, MPI_INT, 0, STATUS_CHANNEL, 
			MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		if(status == SHUTDOWN){
			break;
		}
		MPI_Recv(my_work, 1, f->work_type, 0, 0,
	       	MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		// Compute
		int size = f->compute(my_work, res);
		// Send request for send back the result
		message[1] = RECEIVE;
		MPI_Send(message, 2, MPI_INT, 0, MESSAGE_CHANNEL, MPI_COMM_WORLD);
		MPI_Send(&size, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
		MPI_Send(*res, size, f->result_type, 0, 2, MPI_COMM_WORLD);
		// Receive current status
		MPI_Recv(&status, 1, MPI_INT, 0, STATUS_CHANNEL, 
			MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		if(status == SHUTDOWN){
			break;
		}
			
	}

#endif	
}


void MW_Run (struct mw_api_spec *f) {
	
	int sz,myid;
	MPI_Comm_size(MPI_COMM_WORLD, &sz);
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);
	MPI_Barrier(MPI_COMM_WORLD); 
	
  	
	if(myid == 0){
		master(f, sz);	
	} else {
		slave(f, sz, myid);
	}

  	MPI_Barrier(MPI_COMM_WORLD);
  	MPI_Finalize (); 
  	
}