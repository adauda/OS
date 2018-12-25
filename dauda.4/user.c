
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

int key = 93333;
int clockKey = 95555; 


typedef struct {
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
	int priority;
	int scheduled;
	int complete;
	int ready;
} pcb;

typedef struct {
	unsigned int seconds;
	unsigned int nanoseconds;
} timer;


int main(int argc, char * argv[]) 
{
	if (argc <= 1)
	{
		fprintf(stderr, "incorrect args.\n");
		return 1;
	}
	
	int pid = atoi(argv[1]);

	int pcb_id = shmget(key, sizeof(pcb)*18, SHM_RDONLY | IPC_CREAT);

    	if (pcb_id == -1) {
        	perror("Failed to create shared memory segment. \n");
        	return 1;
	}
	

	pcb* PCB = (pcb*)shmat(pcb_id, NULL, 0);

    	if (PCB == (void*)-1) {
        	perror("Failed to attach shared PCB segment. \n");
        	return 1;
    	}

	
	int timer_id = shmget(clockKey, sizeof(timer), PERM | IPC_CREAT);
    	if (timer_id == -1) {
        	perror("Failed to create shared memory segment. \n");
        	return 1;
	}
	
	// attach shared memory segment
	timer* shmTime = (timer*)shmat(timer_id, NULL, 0);

    	if (shmTime == (void*)-1) {
        	perror("Failed to attach message segment. \n");
        	return 1;
    	}
	
	// Initialize named semaphore for shared processes.
	sem_t *sem = sem_open("semaph", 1);
	if(sem == SEM_FAILED) {
        	perror("Child Failed to sem_open. \n");
        	return;
    	}
	
	printf("Child %d start at seconds: %d and nanoseconds: %ld.\n", pid, shmTime->seconds, shmTime->nanoseconds);

	PCB[pid].start_wait = clock();
	
	srand(pid * time(NULL));
	
	struct timespec delay;
	
	int clear = 0;
	int quantum_check, completed;
	unsigned int run_time = QUANTUM;

	while(clear == 0){
		sem_wait(sem);  // wait until we can subtract 1

		PCB[pid].end_wait = clock();
		PCB[pid].wait_total += (((PCB[pid].end_wait - PCB[pid].start_wait) / CLOCKS_PER_SEC) * 1000000000);
		printf("Child: %d cleared sem_wait. \n", pid);

		// Critical Section
		if(PCB[pid].scheduled == 1){  
			
			quantum_check = rand() % 2;
			if(quantum_check == 0){
				run_time = rand() % QUANTUM;
				delay.tv_sec = 0; // sec;
				delay.tv_nsec = run_time; // nano;
				nanosleep(&delay, NULL);
				PCB[pid].last_burst_ns = run_time;
				PCB[pid].total_CPU_time_ns += run_time;
				completed = rand() % 2;
				if(completed == 0 && PCB[pid].total_time_ns < 50000000){
					sem_post(sem); // adds 1
					PCB[pid].start_wait = clock();
					printf("Child(.1): %d Wait. \n", pid);
					continue;	
				}
				if(completed == 1 || PCB[pid].total_time_ns >= 50000000){
					sem_post(sem); // adds 1
					clear = 1;
					PCB[pid].complete = 1;
					printf("Child(.1): %d cleared sem at sec: %d, nano: %ld \n", pid, shmTime->seconds, shmTime->nanoseconds);
					break;
				}
			}
			else {
				PCB[pid].last_burst_ns = QUANTUM;
				PCB[pid].total_CPU_time_ns += QUANTUM;
				run_time = QUANTUM;
				delay.tv_sec = 0; // sec;
				delay.tv_nsec = run_time; // nano;
				completed = rand() % 2;
				if(completed == 0 && PCB[pid].total_time_ns < 50000000){
					sem_post(sem); // adds 1
					printf("Child(.2): %d Wait. \n", pid);
					PCB[pid].start_wait = clock();
					continue;	
				}
				if(completed == 1 || PCB[pid].total_time_ns >= 50000000){
					sem_post(sem); // adds 1
					clear = 1;
					PCB[pid].complete = 1;
					printf("Child(.2): %d cleared sem at sec: %d, nano: %ld \n", pid, shmTime->seconds, shmTime->nanoseconds);
					break;
				}
			}
		}
		else {
			sem_post(sem); // adds 1, cede CS, not scheduled.
			printf("Child: %d Not scheduled. \n", pid);
			PCB[pid].start_wait = clock();
			continue;
		}
	}
	
	sem_close(sem);  // disconnect from semaphore
	
	// detach from shared memory segment
	int detach = shmdt(PCB);
	if (detach == -1){
		perror("Failed to detach shared memory segment. \n");
		return 1;
	}
	
	// detach from msg memory segment
	detach = shmdt(shmTime);
	if (detach == -1){
		perror("Failed to detach shared msg memory segment. \n");
		return 1;
	}
	
    return 0;
}
