/* mw_api.c : implementation of everything needed by the API */
#include "mw_api.h"
#include <queue>
#include <unordered_set>
#include <string>

#define MESSAGE_CHANNEL 7
#define STATUS_CHANNEL 8
#define REQUEST 1
#define RECEIVE 2
#define MASTERCHECK 3
#define SHUTDOWN 1
#define MOVEON 2

int id;
double master_fail_p;
double slave_fail_p;
int WORK_NUM;
double timeout = 0.2;
MPI_Request request;
MPI_Status mpi_status;

int random_fail(double p){
	
	double r = (double)(rand() % 10000)/10000.0;
	//printf("%f\n",r);
	if (r < p)
		return 1;
	return 0;
}


int F_Send(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, double p) {
   if (random_fail(p)) {
   	  printf("%d Failed!\n", id);      
      MPI_Finalize();
      exit (0);
      return 0;
   } else {
      MPI_Send (buf, count, datatype, dest, tag, comm);
      
      return 1;
   }
}

int Timeout_recv(void* buf, int count, MPI_Datatype datatype, int source,
              int tag, MPI_Comm comm, MPI_Status *status, double timeout) {
	MPI_Request request;
	
	double start_time = MPI_Wtime();
	double interval = 0.0;
	MPI_Irecv(buf, count, datatype, source, tag, comm, &request);
	while(1){
		// Better sleep for a short time interval
		int flag = 0;
		MPI_Test(&request, &flag, status);
		//printf("Flag: %d\n", flag);
		if(flag) { 
			return 1;
		}else {
			double curr_time = MPI_Wtime();
			interval = curr_time - start_time;
			if(interval > timeout) {
				MPI_Cancel(&request);
				return 0;
			}
		}
	}
}



void master(struct mw_api_spec *f, int sz, int myid){
	// Master Code

	mw_work_t** work_list;

	// Dynamic
	int curr;
	int message[3]; // 0 for dest, 1 for message, 2 for result index
	int status[2]; // 0 for status, 1 for work index
	int dest = 0;

	mw_result_t** result_list;
	int *res_size = (int *)malloc((WORK_NUM) * sizeof(int));
	result_list = (mw_result_t **) malloc((WORK_NUM) * sizeof(mw_result_t *));
	std::unordered_set<int> result_set;
	std::unordered_set<int> waiting_list;  
	std::queue<int> work_queue;
	double *work_start_time = (double *)malloc((WORK_NUM) * sizeof(double));


	for(int i = 0;i < WORK_NUM; i++){
		work_queue.push(i);
	}

	work_list = f->create(WORK_NUM);

	if(myid != 0) {
		message[0] = myid;
		message[1] = MASTERCHECK;
		while(1) {
			MPI_Isend(message, 3, MPI_INT, 0, MESSAGE_CHANNEL, MPI_COMM_WORLD, &request);
			MPI_Wait(&request, &mpi_status);
			if(!Timeout_recv(status, 2, MPI_INT, 0, MESSAGE_CHANNEL, 
				MPI_COMM_WORLD, MPI_STATUS_IGNORE, timeout)) {
				//MPI_Barrier(MPI_COMM_WORLD);
				break;
			} else{
				if(status[0] != MOVEON) {
					return;
				} 
			}
		}
	}

	while(1) {
		//Receive a message and do correspond operation.

		int flag;
		if(!Timeout_recv(message, 3, MPI_INT, MPI_ANY_SOURCE, MESSAGE_CHANNEL, 
			MPI_COMM_WORLD, MPI_STATUS_IGNORE, 3.0)) {
			printf("%d Master No requests from slaves.\n", myid);
			break;
		}

		dest = message[0];

		// Send a piece of work
		if(message[1] == REQUEST) {
			printf("%d Master get request from %d.\n", myid, dest);
			if(result_set.size() != WORK_NUM) {
				int work_index = work_queue.front();
				work_queue.pop();
				status[0] = MOVEON;
				status[1] = work_index;
				if(myid == 0){
					F_Send(status, 2, MPI_INT, dest, STATUS_CHANNEL, MPI_COMM_WORLD, master_fail_p);
				} else {
					MPI_Send(status, 2, MPI_INT, dest, STATUS_CHANNEL, MPI_COMM_WORLD);
				}
				mw_work_t *temp = *(work_list + work_index);
				if(myid == 0){
					F_Send(temp, 1, f->work_type, dest, 0, MPI_COMM_WORLD, master_fail_p);
				} else {
					MPI_Send(temp, 1, f->work_type, dest, 0, MPI_COMM_WORLD);
				}
				waiting_list.insert(work_index);
				work_start_time[work_index] = MPI_Wtime();

			} else {
				status[0] = SHUTDOWN;
				F_Send(status, 2, MPI_INT, dest, STATUS_CHANNEL, MPI_COMM_WORLD, master_fail_p);
			}

		}

		// Receive a piece of result
		if(message[1] == RECEIVE) {
			
			printf("%d Master receive result from %d.\n", myid, dest);
			int result_index = message[2];
			int *curr_size = res_size + result_index;
			
			if(!Timeout_recv(curr_size, 1, MPI_INT, dest, 1,
				MPI_COMM_WORLD, MPI_STATUS_IGNORE, timeout)){
				continue;
			}
			//printf("received size:%d\n", *curr_size);
			*(result_list + result_index) = (mw_result_t *) malloc((*curr_size) * f->res_sz);
			//printf("2 Flag: %d\n", flag);
			if(!Timeout_recv(*(result_list + result_index), *curr_size, f->result_type, dest, 2, 
				MPI_COMM_WORLD, MPI_STATUS_IGNORE, timeout)) {
				continue;
			}
			waiting_list.erase(result_index);
			result_set.insert(result_index);
		}

		if(myid == 0 && message[1] == MASTERCHECK) {
			status[0] = MOVEON;
			printf("MASTERCHECK\n");
			F_Send(status, 2, MPI_INT, dest, STATUS_CHANNEL, MPI_COMM_WORLD, master_fail_p);
		}

		double curr_time = MPI_Wtime();
		
		//printf("Check waiting list.\n");
		for(auto iter = waiting_list.begin(); iter != waiting_list.end(); ++iter) {
			int curr_index = *iter;
			double interval = curr_time - work_start_time[curr_index];
			if(interval > timeout) {
				work_queue.push(curr_index);
				waiting_list.erase(curr_index);
				
			}
		}
	}


	if(result_set.size() == WORK_NUM){
		f->result(WORK_NUM, res_size, result_list);
	} else if(myid == 0){
		printf("Don't exist any active slaves. Tasks are aborted! Please restart!\n");
	}
	printf("Master Exit!\n");
}

void slave(struct mw_api_spec *f, int sz, int myid) {
	// Slave Code
	mw_work_t *my_work = (mw_work_t *) malloc(f->work_sz);
	mw_result_t **res = (mw_result_t **) malloc(sizeof(mw_result_t *)); 
	int curr_master = 0;
	// Dynamic
	int message[3];
	int status[2]; 
	message[0] = myid;
	while(1) {
		// Send Request for work
		message[1] = REQUEST;
		int work_index;
		F_Send(message, 3, MPI_INT, curr_master, MESSAGE_CHANNEL, MPI_COMM_WORLD, slave_fail_p);
		// Receive current status
		if(!Timeout_recv(status, 2, MPI_INT, curr_master, STATUS_CHANNEL, 
			MPI_COMM_WORLD, MPI_STATUS_IGNORE, 1.0)){
			curr_master = 1;
			continue;
		}
		
		if(status[0] == MOVEON){
			work_index = status[1];
		} else {
			break;
		}

		if(!Timeout_recv(my_work, 1, f->work_type, curr_master, 0,
	       	MPI_COMM_WORLD, MPI_STATUS_IGNORE, 1.0)) {
			curr_master = 1;
			continue;
		}

		// Compute
		int size = f->compute(my_work, res);
		// Send request for send back the result
		message[1] = RECEIVE;
		message[2] = work_index;
		F_Send(message, 3, MPI_INT, curr_master, MESSAGE_CHANNEL, MPI_COMM_WORLD, slave_fail_p);
		F_Send(&size, 1, MPI_INT, curr_master, 1, MPI_COMM_WORLD, slave_fail_p);
		F_Send(*res, size, f->result_type, curr_master, 2, MPI_COMM_WORLD, slave_fail_p);
		
			
	}
}


void MW_Run (int argc, char** argv, struct mw_api_spec *f) {
	
	int sz,myid;
	MPI_Comm_size(MPI_COMM_WORLD, &sz);
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);
	//MPI_Barrier(MPI_COMM_WORLD); 
	srand(time(NULL)*(myid+1));

  	id = myid;

  	WORK_NUM = std::stoi(*(argv+1));
  	master_fail_p = std::stod(*(argv+2));
	slave_fail_p = std::stod(*(argv+3));
	

	if(myid <= 1){
		master(f, sz, myid);	
	} 
	else {
		slave(f, sz, myid);
	}

  	//MPI_Barrier(MPI_COMM_WORLD);
  	MPI_Finalize (); 
  	
}