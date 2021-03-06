#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wait.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define maxChild 18
#define resMax 20
#define instMax 10
#define IPCD_SZ sizeof(ShmPTR_t)


int req = 0;
int reqGranted = 0;
int deadlk = 0;
int resAlloc = 0;

//Data structure for resource descriptors
typedef struct{
	int inst_tt;			//how many total instances of this resource
	int inst_avail;			//how many available instances of this resource
	int shared;			//for shared resource
	int max_claim[maxChild];	//max claim by child number
	int request[maxChild];		//for request by child number
	int allocated[maxChild];	//for allocation by child number
	int release[maxChild];		//for release notices by child number
}res_t;

//Data structure for shared memory
typedef struct{
	unsigned int secTime;		
	unsigned int milliTime;		
	unsigned int nanoTime;	
	
	//array of resouce descriptors
	res_t resources[resMax];	

	int childTaken[maxChild];	//run status for fork'd children
	int throughput[maxChild];	//counter for child throughput
	int wait_time[maxChild];	//for child wait time

	unsigned long cpu_util[maxChild];	//for child cpu time
}ShmPTR_t;

int ShmID;
ShmPTR_t *ShmPTR;
int SemID_clock;                              
int SemID_res;                              
int signum;                                 
char msgerr[50] = "";                           
int child_pid[maxChild] = { 0 }; 

void resAllocation();
void resManage();
void updateClock();
void report();
void resInit();
void semInit();
void writelog();
void cleanup();
void fork_child();
void sem_wait();
void sem_signal();

int main(int argc, char *argv[]){
	
	// signal(SIGINT, sig_handler);
	
	srand(time(NULL));           
    	int child_sel;    
                         
    	int ShmKEY  = ftok(".", 43);  
	//printf("ShmKEY: %d\n", ShmKEY);      
    	int SemKEY1 = ftok(".", 44);          
    	int SemKEY2  = ftok(".", 45); 
     
    	int sleep_secs, i, x;

	printf("It will run for 2 seconds\n");

	   //Shared memory initialization
	   ShmID = shmget(ShmKEY, IPCD_SZ, 0600 | IPC_CREAT);

    		if (ShmID == -1) {
			perror("shmget Err");
        		exit(1);
    		}

    		if ( ! (ShmPTR = (ShmPTR_t *)(shmat(ShmID, 0, 0)))) {
			perror("shmat Err");
        		exit(1);
    		}

		// Semaphore  Memory Initialization for Clock	
		if ((SemID_clock = semget(SemKEY1, 1, 0600 | IPC_CREAT)) == -1) {
			perror("semget Err clock");
        		exit(1);
    		}

		//Semaphore memory initializatin for resource
		if ((SemID_res = semget(SemKEY2, 1, 0600 | IPC_CREAT)) == -1) {
			perror("semget Err resource");
        		exit(1);
    		}

	//Initialization
	semInit();
	resInit();

    	for (i = 0; i < maxChild; i++) 
		ShmPTR->childTaken[i] = 0;
		
    	// Go into fork loop
    	while (1) {
        	if ( sigcheck() ) 
			break;
        
		resManage();
		resAllocation();

        	// Only attempt a fork another child if we are below maxChild processes
        	if ( countChild() < maxChild ) {
            		// Determine which child to fork
            		for (i = 0; i <= maxChild; i++) {
                		if ( ShmPTR->childTaken[i] == 0 ) {
                    			child_sel = i;
                    			break;
                		}
            		}
            	sprintf(msgerr, "Selected child number %d to fork", child_sel);
            	writelog(msgerr);
            	fork_child(child_sel);
        	}

        	sem_wait(SemID_clock);             
		updateClock();
        	sem_signal(SemID_clock);    
	
		sprintf(msgerr, "Logical clock is now %d.%03d", ShmPTR->secTime, ShmPTR->milliTime);
        	sprintf(msgerr, "%s.%03d", msgerr, ShmPTR->nanoTime);
        	writelog(msgerr);

        	// Break if we have reached 2 elapsed seconds
        	if (ShmPTR->secTime >= 2) {
            	sprintf(msgerr, "Reached maximum run time - exiting.");
            	writelog(msgerr);
            	break;
        	}

		//write log entry and sleep
		sleep_secs = rand() % 2;                 // Random from 0 to 1
		sprintf(msgerr, "Sleep %d", sleep_secs);
        	writelog(msgerr);
        	sleep(sleep_secs);
    	}
		// Report Final Result
		report();
		cleanup(SIGTERM);
		return 0;
}

//allocate resources based on updated allocation and new requests
void resAllocation(){
	int need, i, x;

	for (i = 0; i < resMax; i++) {
            for (x = 0; x < maxChild; x++) {
                if (ShmPTR->resources[i].request[x] > 0) {
                    // Calculating amount of resources child wants
                    need = ShmPTR->resources[i].max_claim[x] - ShmPTR->resources[i].allocated[x];
					req++;

					// If childrend Request more than MAX
					if ( ShmPTR->resources[i].request[x] > need ||
                            		     ShmPTR->resources[i].allocated[x] + ShmPTR->resources[i].request[x] >
                            		     ShmPTR->resources[i].inst_tt) {
                        				sprintf(msgerr, "%s Request denied : MAX resources", msgerr);
                        				writelog(msgerr);

                        				sem_wait(SemID_res);
                        				ShmPTR->resources[i].request[x] = -1;
                        				sem_signal(SemID_res);

                        				// Deadlock detection algorithm
					} else if ( ShmPTR->resources[i].request[x] <= ShmPTR->resources[i].inst_avail ) {
							ShmPTR->throughput[x] += ShmPTR->resources[i].request[x];
							deadlk++;

							// Allocate the resource
							sprintf(msgerr, "Child %02d has allocated %d", x, ShmPTR->resources[i].request[x]);
							sprintf(msgerr, "%s of resource %d", msgerr, i);
							writelog(msgerr);

							ShmPTR->resources[i].inst_avail -= ShmPTR->resources[i].request[x];
							ShmPTR->resources[i].allocated[x] += ShmPTR->resources[i].request[x];
							resAlloc = ShmPTR->resources[i].allocated[x];

							sem_wait(SemID_res);
							ShmPTR->resources[i].request[x] = 0;
							sem_signal(SemID_res);
                    				}
		}
	    }
	}	

}



//determine who has released resources and update the resource data
void resManage() {
	int i, x;
		for (i = 0; i < resMax; i++) {
            		for (x = 0; x < maxChild; x++) {
                		if ( ShmPTR->resources[i].release[x] != 0 ) {
                    			sprintf(msgerr, "Child %02d has released %d", x, ShmPTR->resources[i].release[x]);
                    			sprintf(msgerr, "%s instances of resource %d", msgerr, i);
                    			writelog(msgerr);

                    			ShmPTR->resources[i].inst_avail += ShmPTR->resources[i].release[x];

                    			sem_wait(SemID_res);
                    			ShmPTR->resources[i].release[x] = 0;
                    			sem_signal(SemID_res);
                		}
            		}
        	}
}

void updateClock() {
        ShmPTR->milliTime += rand() % 1000;      //  Random 0 to 999
        if ( ShmPTR->milliTime >= 1000 ) {
            ShmPTR->secTime++;
            ShmPTR->milliTime -= 1000;
        }
        ShmPTR->nanoTime += rand() % 1000;       //  Random 0 to 999
        if ( ShmPTR->nanoTime >= 1000 ) {
            ShmPTR->milliTime++;
            ShmPTR->nanoTime -= 1000;
        }

}

void report() {
		int i;
	    	unsigned int tp_tt = 0, wt_tt = 0;        
		unsigned long cu_tt = 0;                    
		float avg_tp, avg_tt, avg_wt;            
		float avg_cu;                             

 	       	for (i = 0; i < maxChild; i++) {
        	    	tp_tt += ShmPTR->throughput[i];     // throughput
            		wt_tt += ShmPTR->wait_time[i];       // waitt ime
            		cu_tt += ShmPTR->cpu_util[i];         // cpu utilization
        	}

		if ( maxChild > 0 ) {                   // Handle possibility of divide by zero
            		avg_tp = (float)tp_tt / maxChild;  // average throughput
			
            		// average turnaround time
            		if ( tp_tt > 0 ) avg_tt = ((float)wt_tt / tp_tt) / maxChild;
            		avg_wt = (float)wt_tt / maxChild;  // average wait time
            		avg_cu = ((float)cu_tt / maxChild) / 1000000;  // average cpu time
        	}
		printf("\n==============Statistics================\n");
		float perc = (float)resAlloc / req;

		printf("Total requests granted:\t\t%i\n", resAlloc);
		printf("Times Deadlock Algorithm ran:\t\t%i\n", deadlk);
		printf("Percentage of requests granted:\t\t%.2f\n", perc);

		printf("\n=== Averages for Completed Processes ===\n");
        	printf("Throughput:\t\t%.2f allocations/process\n", avg_tp);
        	printf("Turnaround Time:\t%.2f ms\n", avg_tt);
        	printf("Wait Time:\t\t%.2f ms\n", avg_wt);
        	printf("CPU Utilization:\t%.2f ms\n\n", avg_cu);

		printf("=== Total Time in System ===\n");
        	printf("%d.%03d.%03d \n\n ", ShmPTR->secTime, ShmPTR->milliTime, ShmPTR->nanoTime);
		
}


void resInit() {
	int x, i;
	int noShared;                             
	
    	noShared = resMax * ((float)(15 + (rand() % 10)) / 100);
    	sprintf(msgerr, "Determined that %d of %d resources will be shared", noShared, resMax);
    	writelog(msgerr);

    	for (i = 0; i < resMax; i++) {
        	// Randomly set  number of instances for the resource
		ShmPTR->resources[i].inst_tt = 1 + (rand() % instMax);
        	ShmPTR->resources[i].inst_avail = ShmPTR->resources[i].inst_tt;

        	sprintf(msgerr, "Resource %d will have %d instances", i, ShmPTR->resources[i].inst_tt);
        	writelog(msgerr);

        	if ( i <= noShared ) {
            		ShmPTR->resources[i].shared = 1;

            		sprintf(msgerr, "Resource %d will be shared", i);
            		writelog(msgerr);
        	} else ShmPTR->resources[i].shared = 0;

        	for (x = 0; x < maxChild; x++) {
            		ShmPTR->resources[i].max_claim[x] = 0;
            		ShmPTR->resources[i].request[x] = 0;
            		ShmPTR->resources[i].allocated[x] = 0;
            		ShmPTR->resources[i].release[x] = 0;
        	}
    }
    writelog("Initialized resources");
}
	


void semInit() {

    	union semun { 
		int val; 
		struct semid_ds *buf; 
		ushort * array; 
	} argument;

    	argument.val = 1;                            // Set the semaphore value to one
    
	if (semctl(SemID_clock, 0, SETVAL, argument) == -1) {
        	sprintf(msgerr, "oss: semctl (clock)");
        	perror(msgerr);
        	exit(1);
    	}

	if (semctl(SemID_res, 0, SETVAL, argument) == -1) {
        	sprintf(msgerr, "oss: semctl (resources)");
        	perror(msgerr);
        	exit(1);
    	}
   	 writelog("Created and initialized clock and resource semaphores to 1");

    sem_wait(SemID_clock);                       
    ShmPTR->secTime = 0;
    ShmPTR->nanoTime = 0;
    sem_signal(SemID_clock);                     
}

void sigproc(int sig) {
    signum = sig;
}

int sigcheck() {
    signal(SIGINT, sigproc);
    signal(SIGTERM, sigproc);
    return 0;
}

void writelog(char *msg) {

    FILE *fp;
    if (!(fp = fopen("file.log", "a"))) {
        perror("oss: opening file.log");
        exit(1);
    }

    fprintf(fp, "oss:\t%s\n", msg);
    fclose(fp);
}

int term_proc(int child, int sig) {
    int status;                                  // Hold status from wait()
    kill(child_pid[child], sig);
    wait(&status);
    return WEXITSTATUS(status);
}

void cleanup(int termsig) {
    int i;
    for (i = 0; i < maxChild; i++) {
        if ( ShmPTR->childTaken[i] > 0 ) {
            if ( term_proc(i, termsig) != 0 ) {
                sprintf(msgerr, "There was an issue terminating child %02d", i);
                writelog(msgerr);
            }
        }
    }

    int ShmPTR_ret = shmctl(ShmID, IPC_RMID, (struct shmid_ds *)NULL);
    if ((semctl(SemID_clock, 0, IPC_RMID, 1) == -1) && (errno != EINTR)) {
        sprintf(msgerr, "oss: clock removed");
        perror(msgerr);
    }
	
    if ((semctl(SemID_res, 0, IPC_RMID, 1) == -1) && (errno != EINTR)) {
        sprintf(msgerr, "oss: resources removed");
        perror(msgerr);
    }
    return;
}


//counting child 
int countChild() {
    int i, count = 0;
    for (i = 0; i < maxChild; i++) {
        if ( ShmPTR->childTaken[i] == 0 && child_pid[i] > 0 ) {
            sprintf(msgerr, "Child %02d has exited - attempting to clean it up", i);
            writelog(msgerr);

            if ( term_proc(i, SIGTERM) != 0 ) {
                sprintf(msgerr, "There was an issue terminating child %02d", i);
                writelog(msgerr);
                cleanup(SIGTERM);
                exit(1);
            }
            child_pid[i] = 0;
        }

        if ( ShmPTR->childTaken[i] ) count++;
    }
    sprintf(msgerr, "Current child count is %d", count);
    writelog(msgerr);
    return count;
}


//forking child
void fork_child(int child) {
    char child_arg[5] = "";                        // String to hold child argument

    if ((child_pid[child] = fork()) < 0) {
        sprintf(msgerr, "oss: fork() for child %02d", child);
        perror(msgerr);
        writelog("Error forking child");
        cleanup(SIGTERM);
        exit(1);
    } else {
        if (child_pid[child] == 0) {
            sprintf(child_arg, "%02d", child);
            execl("./uproc", "uproc", child_arg, (char *)NULL);
            sprintf(msgerr, "oss: exec child %02d after fork", child);
            perror(msgerr);
        } else {
            sprintf(msgerr, "Forked process ID %d for child %02d", child_pid[child], child);
            writelog(msgerr);
            sprintf(msgerr, "Setting child %02d status 'running'", child);
            writelog(msgerr);
            ShmPTR->childTaken[child] = 1;
        }
    }
    return;
}


//wait function
void sem_wait(int semid) {
	struct sembuf sbuf;			
	sbuf.sem_num = 0;							
	sbuf.sem_op = -1;							
	sbuf.sem_flg = 0;							
	if (semop(semid, &sbuf, 1) == -1)  {
			exit(0);
	}
	return;
}

// Singal Function
void sem_signal(int semid) {
	struct sembuf sbuf;						
	sbuf.sem_num = 0;							
	sbuf.sem_op = 1;						
	sbuf.sem_flg = 0;						
	if (semop(semid, &sbuf, 1) == -1)  {
			exit(1);
	}
	return;
}
