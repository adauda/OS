#include <stdio.h>                               // printf()
#include <stdlib.h>                              // exit()
#include <time.h>                                // time()
#include <wait.h>                                // time()
#include <unistd.h>                              // sleep(), etc.
#include <errno.h>                               // perror()
#include <sys/ipc.h>                             // Inter-process communication
#include <sys/shm.h>                             // Shared memory
#include <sys/sem.h>                             // Semaphores

#define maxChild 18
#define resMax 20
#define instMax 10
#define IPCD_SZ sizeof(ShmPTR_t)                   // Size of the IPC data structure

// Data structure for resource descriptors
typedef struct {
    int inst_tt;                                // How many total instances of this resource
    int inst_avail;                              // How many available instances of this resource
    int shared;                                  // Resource shared indicator
    int max_claim[maxChild];                    // Max claims by child number
    int request[maxChild];                      // Requests by child number
    int allocated[maxChild];                    // Allocation by child number
    int release[maxChild];                      // Releases notices by child number
} resource_t;


// Data structure for shared memory
typedef struct {
    unsigned int secTime;                      // Clock seconds
    unsigned int milliTime;                    // Milliseconds since the last clock second
    unsigned int nanoTime;                     // Nanoseconds since the last clock second
    resource_t resources[resMax];               // Array of resource descriptors
    int childTaken[maxChild];                // Run status of fork'd children
    int throughput[maxChild];                   // Counter for child throughput
    int wait_time[maxChild];                    // Accumulator for child wait time
    unsigned long cpu_util[maxChild];           // Accumulator for child CPU time
} ShmPTR_t;

int ShmID;                                  
int signum;                                     
char msgerr[50] = "";                         
ShmPTR_t *ShmPTR;                                    
int SemID_clock;                            
int SemID_res;                                  
int mypid;                                      
int curI;                                  

void writelog();
void sem_wait();
void sem_signal();

int main(int argc, char *argv[]) {
    srand(time(NULL));                        
    int ShmKEY  = ftok(".", 43);     
    int SemKEY1 = ftok(".", 44);        
    int SemKEY2  = ftok(".", 45);       
    int sleep_secs, n, wait_milli, release, request;                      
    unsigned int start_sec;
                 
    curI = atoi(argv[1]);
    mypid = getpid();                            

    sprintf(msgerr, "New child forked PID: %d", mypid);
    writelog(msgerr);

	// Shared Memory Initialization
	if ((ShmID = shmget(ShmKEY, IPCD_SZ, 0600)) == -1) {
		perror("shmget Err");
        	exit(1);
    	}
    	if ( ! (ShmPTR = (ShmPTR_t *)(shmat(ShmID, 0, 0)))) {
		perror("shmat Err");
        	exit(1);
    	}
	// Semaphore Memory Initialization for Clock
	if ((SemID_clock = semget(SemKEY1, 1, 0600)) == -1) {
		perror("semget Err clock");
        	exit(1);
    	}
	// Semaphore Memory Initialization for Resource
	if ((SemID_res = semget(SemKEY2, 1, 0600)) == -1) {
		perror("semget Err resource");
        	exit(1);
    	}

	int res_use;        
                         
    	for (n = 0; n < resMax; n++) {
        	res_use = rand() % 4;	// have 25% of chance to use all resources
        	if ( res_use == 1 ) {
            		ShmPTR->resources[n].max_claim[curI] = 1 + (rand() % ShmPTR->resources[n].inst_tt);
            		sprintf(msgerr, "Resource Claimed = MAX %d", ShmPTR->resources[n].max_claim[curI]);
            		writelog(msgerr);
        	}
    	}

    	// Initialize statistics
    	ShmPTR->throughput[curI] = 0;
    	ShmPTR->wait_time[curI] = 0;
    	ShmPTR->cpu_util[curI] = 0;
    	start_sec = ShmPTR->secTime;

    	while (1) {
        	if ( sigcheck() ) {
            		sprintf(msgerr, "Received signal %d - exiting...", signum);
            		writelog(msgerr);
            		break;
        	}

        	if ( (rand() % 10) == 1 && ShmPTR->secTime - start_sec > 0 ) {
            		for (n = 0; n < resMax; n++) {
                		ShmPTR->resources[n].request[curI] = 0;
                		ShmPTR->resources[n].release[curI] = ShmPTR->resources[n].allocated[curI];
            		}
            		ShmPTR->childTaken[curI] = 0;
            		exit(0);
        	}

        	for (n = 0; n < resMax; n++) {
            		// Request or Release algorithm 
            		if ( ShmPTR->resources[n].allocated[curI] > 0 && rand() % 2 == 1 ) {
                		if ( rand() % 2 == 1 ) {
                    			release = rand() % ShmPTR->resources[n].allocated[curI];

                    			sem_wait(SemID_res);        
                    			ShmPTR->resources[n].release[curI] += release;
                    			sem_signal(SemID_res);     

                    			sprintf(msgerr, "Released %d instances of resource %d", release, n);
                    			writelog(msgerr);

                    			ShmPTR->cpu_util[curI] += 10; 
                		}
            		}
			 else if ( ShmPTR->resources[n].request[curI] == 0 ) {
                		if ( ShmPTR->resources[n].max_claim[curI] > 0 && rand() % 2 == 1 ) {
                    			// Requesting Resource 
            				request = rand()%( ShmPTR->resources[n].max_claim[curI] - ShmPTR->resources[n].allocated[curI] );

                   			 if ( request > 0 ) {        // negative number avoidance
                        			sem_wait(SemID_res);     
                        			ShmPTR->resources[n].request[curI] = request;
                        			sem_signal(SemID_res);  

                        			sprintf(msgerr, "Requested %d instances of resource %d", request, n);
                        			writelog(msgerr);

                        			ShmPTR->cpu_util[curI] += 15000000; 
                    			}
				}
			}
		}

	//rand time btw 0 and 250
	wait_milli = 1 + ( rand() % 250 );       

        sem_wait(SemID_clock);       	
        ShmPTR->milliTime += wait_milli;

        if ( ShmPTR->milliTime >= 1000 ) {
            ShmPTR->secTime++;
            ShmPTR->milliTime -= 1000;
        }
        sem_signal(SemID_clock);              
		ShmPTR->wait_time[curI] += wait_milli;

		
        sprintf(msgerr, "Logical clock is now %d.%03d%s.%03d", ShmPTR->secTime, ShmPTR->milliTime, msgerr, ShmPTR->nanoTime);
        writelog(msgerr);

        sleep_secs = 1;
        sprintf(msgerr, "Sleep %d", sleep_secs);
        writelog(msgerr);
        sleep(sleep_secs);
    }

    return 0;
}

//catch signals
void sigproc(int sig){
	signum = sig;
}            		
            		
//process signals
int sigcheck() {
    signal(SIGINT, sigproc);
    signal(SIGTERM, sigproc);
	if (signum == 2) fprintf(stderr, "userproc %02d: Caught CTRL-C (SIGINT)\n", curI);
    return 0;
}

//log function
void writelog(char *msg){
	FILE *fp;
	if(!(fp = fopen("file.log", "a"))){
		perror("uproc: opening file.log");
		exit(1);	
	}
	fprintf(fp, "uproc:\t%s\n", msg);
	fclose(fp);
}

// Wait Function
void sem_wait(int semid) {
	struct sembuf sbuf;					
	sbuf.sem_num = 0;							
	sbuf.sem_op = -1;							
	sbuf.sem_flg = 0;							
	if (semop(semid, &sbuf, 1) == -1) {
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
	if (semop(semid, &sbuf, 1) == -1) {
			exit(1);
	}
	return;
}

