#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <time.h>

#define PERM (S_IRUSR | S_IWUSR)
#define HIPRIORITY 10000
#define LOWPRIORITY 100000000
#define QUANTUM 50000

#define MAXCHILDREN 18
#define HI 0
#define LOW 2

//for signal handling
void closePs();

//for the queue
int hi_queue[MAXCHILDREN];
int low_queue[MAXCHILDREN];
int front, rear;

//for shared memory 
int key = 93333;
int clockKey = 95555;

//process control block for each user processess.
typedef struct{
	//schedulling related items
	long total_CPU_time_sec;
        long total_CPU_time_ns;

        long total_time_sec;
        long total_time_ns;

        long last_burst_sec;
        long last_burst_ns;

        long start_wait;
        long end_wait;
        long wait_total;

        clock_t begin;
        clock_t end;

        pid_t pid;

        int queue;
        int scheduled;
        int complete;
        int ready;

}pcb;

//for the clock
typedef struct {
        unsigned int seconds;
        unsigned int nanoseconds;
}timer;

//time to run program
int max_time = 60;

//create log file to write output
FILE *file;
char *fname = "log";

//prototypes for queue management
void pushHI(int n);
void pushLO(int n);
void popHI(int n);
void popLO(int n);
void printList();

int pcb_id, timer_id, active_children = 0;;
pcb *PCB;
timer *shmTime;
sem_t *sem;
pid_t pid;

int main(int argc, char *argv[])
{

	signal(SIGALRM, closePs);
	signal(SIGINT, closePs);
	alarm(2);

	file = fopen(fname, "w+");

	//mem segment for oss
	pcb_id = shmget(key, sizeof(pcb)*18, PERM | IPC_CREAT | IPC_EXCL);
        if (pcb_id == -1) {
                perror("Failed to create shared memory segment. \n");
                return 1;
        }

	//attach shared mem segment for oss
	 PCB = (pcb*)shmat(pcb_id, NULL, 0);

	 if (PCB == (void*)-1) {
                perror("Failed to attach shared memory segment. \n");
                return 1;
        }

	//mem segment for the clock
	timer_id = shmget(clockKey, sizeof(timer), PERM | IPC_CREAT | IPC_EXCL);
        if (timer_id == -1) {
                perror("Failed to create shared memory segment for clock. \n");
                return 1;
        }
	
	//attach shared mem segment for clock
	shmTime = (timer*)shmat(timer_id, NULL, 0);
	if (shmTime == (void*)-1) {
                perror("Failed to attach shared message segment. \n");
                return 1;
        }
	
	// Initialize named semaphore for shared processes.
	sem = sem_open("semaph", O_CREAT | O_EXCL, 0644, 1);
        if(sem == SEM_FAILED) {
                perror("Failed to sem_open clock. \n");
                return;
        }

	// set shmTime to zero.
	shmTime->seconds = 0;
        shmTime->nanoseconds = 0;

	int i;
	//initialize all pcb items
	 for(i = 0; i < MAXCHILDREN; i++){
                PCB[i].total_CPU_time_sec = 0;
                PCB[i].total_CPU_time_ns = 0;
                PCB[i].total_time_sec = 0;
                PCB[i].total_time_ns = 0;
                PCB[i].last_burst_sec = 0;
                PCB[i].last_burst_ns = 0;
                PCB[i].pid = 0;
                PCB[i].scheduled = 0;
                PCB[i].ready = 1;
                PCB[i].complete = 0;
                PCB[i].start_wait = 0;
                PCB[i].end_wait = 0;
                PCB[i].wait_total = 0;
                PCB[i].begin = 0;
                PCB[i].end = 0;
        }

	//for totalCpuTime
	char shsec[2], 
	shnano[10], 

	//for write to log file
	msgtext[132];

	 pid_t childpid;
        char cpid[12];
        //int active_children = 0;

        long nano = 0;			
        int sec = 0;			

        long random_time = 0;
        int random_number;
        struct timespec delay;

        int total_log_lines = 0;

        front = -1, rear = -1;

        srand(time(NULL));

	do{

		 random_time = rand() % 1000 + 1;

		 if((random_time + shmTime->nanoseconds)  > 999999000){
                        shmTime->nanoseconds += 0;
                        shmTime->seconds  += 2;
                }
                else {
                        shmTime->nanoseconds += random_time;
                        shmTime->seconds += 1;
                }


                if(active_children > 18 || total_log_lines >= 10000){

                        pid = getpgrp();		// gets process grp

			printf("lines in log file exceeds 10000 lines.");
			printf("Terminating PID: %i due to limit met. \n", pid);
                        sem_close(sem);  		
			sem_unlink("semaph"); 
			shmctl(pcb_id, IPC_RMID, NULL);
                        shmctl(timer_id, IPC_RMID, NULL);
                        shmdt(PCB);
                        shmdt(shmTime);
                        killpg(pid, SIGINT); 
			exit(EXIT_SUCCESS);
		}

	
		 for (i = 0; i < MAXCHILDREN; i++) 
			{
                        if (PCB[i].scheduled == 1 && PCB[i].complete == 1 && childpid != 0)
				{
                                sprintf(shsec, "%d", shmTime->seconds);
                                sprintf(shnano, "%ld", shmTime->nanoseconds);
                                sprintf(msgtext, "Master: Child pid %d is terminating at my time %d:%ld. \n ", PCB[i].pid, shmTime->seconds, shmTime->nanoseconds);
                                fputs(msgtext, file);
                                fputs(shsec, file);
                                fputs(".", file);
                                fputs(shnano, file);
                                fputs(". \n", file);
                                total_log_lines += 1;
                                popHI(i);
                                printList();
                                PCB[i].end = clock();
                                PCB[i].total_time_ns = (((PCB[i].end - PCB[i].begin) / CLOCKS_PER_SEC) * 1000000000);
                                PCB[i].wait_total = 0;
                                PCB[i].complete = 0;
                                PCB[i].ready = 1;
                                PCB[i].scheduled = 0;
                                active_children -= 1;
                        	}

			 if(active_children < MAXCHILDREN && PCB[i].ready == 1)
				{
                                	delay.tv_sec = 1;	//for sec 
					delay.tv_nsec = 0;	//for nano
				 	nanosleep(&delay, NULL);

                                	random_time = rand() % 1000 + 1;
                                	if((random_time + shmTime->nanoseconds)  > 999999000){
                                        	shmTime->nanoseconds += 0;
                                        	shmTime->seconds  += 2;
                                	}
                                	else 
					{
                                        	shmTime->nanoseconds += random_time;
                                        	shmTime->seconds += 1;
                                	}

                                	childpid = fork();
                                	if (childpid == -1) {
                                        	perror("Master: Failed to fork.");
                                        	return 1;
                                	}
                                	if (childpid == 0) {
                                        	shmTime->seconds += 1;
                                        	printf("Master: Child pid %d is starting at my time %d:%ld. \n ", i, shmTime->seconds, shmTime->nanoseconds);
                                        	sprintf(cpid, "%d", i);
                                        	execlp("user", "user", cpid, NULL);
                                        	active_children++;
                                        	printf("Active Children: %d. \n", active_children);
                                	}

				  	if (childpid != 0 && PCB[i].ready == 1) 
					{
                                        pushHI(i);
                                        PCB[i].queue = HI;
                                        printList();
                                        PCB[i].pid = i;
                                        PCB[i].total_CPU_time_sec = 0;
                                        PCB[i].total_CPU_time_ns = 0;
                                        PCB[i].total_time_sec = 0;
                                        PCB[i].total_time_ns = 0;
                                        PCB[i].last_burst_sec = 0;
                                        PCB[i].last_burst_ns = 0;
                                        PCB[i].scheduled = 0;
                                        PCB[i].complete = 0;
                                        PCB[i].ready = 0;
                                        PCB[i].wait_total = 0;
                                        PCB[i].begin = clock();
                                        sprintf(shsec, "%d", shmTime->seconds);
                                        sprintf(shnano, "%ld", shmTime->nanoseconds);
                                        sprintf(msgtext, "OSS: Generating process with PID %d at time ", PCB[i].pid);
                                        fputs(msgtext, file);
                                        fputs(shsec, file);
                                        fputs(":", file);
                                        fputs(shnano, file);
                                        fputs(". \n", file);
					}
				}

				//for schedulling
				if(PCB[i].ready == 0 && PCB[i].complete == 0){
                                	if(PCB[i].wait_total >= HIPRIORITY) {
                                        	if(PCB[i].queue = HI){
                                                	popHI(i);
                                                	pushLO(i);
                                        	}
                                        	PCB[i].queue = LOW;
                                	}
                        	}
				PCB[hi_queue[0]].scheduled = 1;
        	                PCB[low_queue[0]].scheduled = 1;
			}
			 pid = getpgrp();
	}while (active_children > 0);	


      /*wait for all children*/
        int j;
        for (j = 0; j <= MAXCHILDREN; j++){
                wait(NULL);
        }

        printf("Total Children end: %d. \n", active_children);

        sem_close(sem);
        sem_unlink("semaph");

        /* detach from shared memory segment*/
        int detach = shmdt(PCB);
        if (detach == -1){
                perror("Failed to detach shared memory segment. \n");
                return 1;
        }

        /* delete shared memory segment*/
        int delete_mem = shmctl(pcb_id, IPC_RMID, NULL);
        if (delete_mem == -1){
                perror("Failed to remove shared memory segment. \n");
                return 1;
        }

        /*detach from msg memory segment*/
         detach = shmdt(shmTime);
        if (detach == -1){
                perror("Failed to detach msg memory segment. \n");
                return 1;
        }

        /* delete msg memory segment*/
        delete_mem = shmctl(timer_id, IPC_RMID, NULL);


         if (delete_mem == -1){
                perror("Failed to remove msg memory segment. \n");
                return 1;
        }

return 0;

}


void pushHI(int n)
        {
                if (rear >= MAXCHILDREN - 1)
                {
                        printf("\nQueue overflow no more elements can be inserted");
                        return;
                }

                if ((front == -1) && (rear == -1))
                {

                        front++;
                        rear++;
                        hi_queue[rear] = n;
                        return;
                }
                else{
                        int i,j;

                        for (i = 0; i <= rear; i++)
                        {
                        if (n >= hi_queue[i])
                                {
                                        for (j = rear + 1; j > i; j--)
                                        {
                                                hi_queue[j] = hi_queue[j - 1];
                                        }

                                        hi_queue[i] = n;
                                        return;
                                }
                        }

                        hi_queue[i] = n;
                }
    		rear++;
}


void pushLO(int n)
	{
    		if (rear >= MAXCHILDREN - 1) {
        		printf("\nQueue overflow no more elements can be inserted");
        		return;
    		}

    		if ((front == -1) && (rear == -1)){
        		front++;
        		rear++;
        		low_queue[rear] = n;
        		return;
    		}
    		else{
        		int i,j;

        		for (i = 0; i <= rear; i++)
        		{
                		if (n >= low_queue[i])
                		{
                        		for (j = rear + 1; j > i; j--)
                        		{	
                                		low_queue[j] = low_queue[j - 1];
                        		}
                			low_queue[i] = n;
                		return;
                		}
        		}
        		low_queue[i] = n;

        	}
    		rear++;
}

void popHI(int n) {
    		int i;

    		if ((front==-1) && (rear==-1)) {
        		printf("Queue is empty no elements to delete. \n");
        		return;
    		}

    		for (i = 0; i <= rear; i++){
        		if (n == hi_queue[i]){
            			for (; i < rear; i++){
                			hi_queue[i] = hi_queue[i + 1];
            			}

        		hi_queue[i] = -99;
        		rear--;

        		if (rear == -1)
            			front = -1;
        		return;
        		}
    		}

 	   	printf("%d not found in queue to delete. \n", n);
        	return;
}

void popLO(int n) {
    	int i;

    		if ((front==-1) && (rear==-1)) {
        		printf("Queue is empty no elements to delete. \n");
        		return;
    		}

    		for (i = 0; i <= rear; i++){
        		if (n == low_queue[i]){
            			for (; i < rear; i++){
                			low_queue[i] = low_queue[i + 1];
            			}

        			low_queue[i] = -99;
        			rear--;

        			if (rear == -1)
           				 front = -1;
        			return;
        		}
    		}
    		printf("%d not found in queue to delete. \n", n);
        	return;
}

void printList() {
    		if ((front == -1) && (rear == -1))    {
        		printf("Queue is empty.\n");
        		return;
    		}

    		for (front = 0; front <= rear; front++) {
        		printf(" H-P Queue: %d ", hi_queue[front]);
    		}

    	front = 0;
        return;
}


void closePs(){
/*
	//wait for all children
        int j;
        for (j = 0; j <= MAXCHILDREN; j++){
                wait(NULL);
        }

        printf("Total Children end: %d. \n", active_children);

        sem_close(sem);
        sem_unlink("semaphore");

        // detach from shared memory segment
        shmdt(PCB);

        //delete shared memory segment
        shmctl(pcb_id, IPC_RMID, NULL);

        //detach from msg memory segment
        shmdt(shmTime);

        // delete msg memory segment
        shmctl(timer_id, IPC_RMID, NULL);

                        killpg(pid, SIGINT); 
                        exit(EXIT_SUCCESS);
*/

                        printf("2s Terminating PID: %i due to limit met. \n", pid);
                        sem_close(sem);
                        sem_unlink("semaph");
                        shmctl(pcb_id, IPC_RMID, NULL);
                        shmctl(timer_id, IPC_RMID, NULL);
                        shmdt(PCB);
                        shmdt(shmTime);
                        killpg(pid, SIGINT);
                        exit(EXIT_SUCCESS);

     exit(0);

}

