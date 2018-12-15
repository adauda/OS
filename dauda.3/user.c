#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <string.h>
#include <semaphore.h>

#define PERM (S_IRUSR | S_IWUSR)

int ShmID, ShmMSG, *ShmPTR, *ShmPTR_MSG;

sem_t *semaphore;

int key = 985047, msgkey = 91500;

int main(int argc, char *argv[]){
	
	 /* check for valid parameters passed */
        if (argc <= 1){
                printf(stderr, "Invalid parameters passed");
                return 1;
        }

	int pid = atoi(argv[1]);

	//create shared memory segment
	ShmID = shmget(key, 8*sizeof(int), 0666);

	 if(ShmID < 0){
                perror(" shmget(Child) failed to create shared memory segment.\n");
                return 1;
        }

	//attach shared memory segment
	ShmPTR = (int *)shmat(ShmID, NULL, 0);

        if ((int) ShmPTR == -1) {
         	perror("shmat(Child) failed to attach shared memory segment.\n");
          	return 1;
        }

        //create shared memory for message
        ShmMSG = shmget(msgkey, 8*sizeof(int), 0666);

        if(ShmMSG < 0){
                perror(" shmget(Child) failed to create shared memory segment for message.\n");
                return 1;
        }

        //attach shared memory segment for message
         ShmPTR_MSG = (int *)shmat(ShmMSG, NULL, 0);

         if ((int) ShmPTR_MSG == -1) {
          perror("shmat(Child) failed to attach shared memory segment for message.\n");
          return 1;
        }

	printf("Child %ld start at seconds: %d and nanoseconds: %ld.\n", pid, ShmPTR[0], ShmPTR[1]);

	srand(pid * time(NULL));
	long nano = 0;
	long sec = 0;
	
	long randTime = rand() % 1000000 + 1;		//generate random number from 1 to 1000000

	//to avoid overflow 
	if((ShmPTR[1] + randTime) < 1000000000){
		nano = ShmPTR[1] + rand() % 1000000 + 1;
		sec = ShmPTR[0];
	}
	else if((ShmPTR[1] + randTime) >= 1000000000){
		nano = (ShmPTR[1] + randTime) - ShmPTR[1];
		sec = ShmPTR[0] + 1;
	}
	
	printf("Initial clock time as oss: %ld, %ld\n", ShmPTR[0], ShmPTR[1]);
	printf("Deadline clock time from user: %ld, %ld\n", sec, nano);

	semaphore = sem_open("sema", 1);
	if(semaphore == SEM_FAILED){
		perror("Child failed to sem_open.\n");
		return;
	}
	

	int clear = 0;

	while(clear == 0){
		sem_wait(semaphore);
		
		//crititcal section.
		if(nano <= ShmPTR[1] && sec <= ShmPTR[0] && ShmPTR_MSG[3] == 0){

				ShmPTR_MSG[0] = pid;

				ShmPTR_MSG[1] = sec;
				ShmPTR_MSG[2] = nano;

				sem_post(semaphore);
				clear = 1;

				ShmPTR_MSG[3] = 1;
				break;
		}
		else{
				//cede critical section to another process, not ready to send msg.
				sem_post(semaphore);
				continue;
		}
	}
	
	//disconnect from semaphore.
	sem_close(semaphore);

	//detach from shared memory and shared message memory segments.
	shmdt((void *) ShmPTR);
	shmdt((void *) ShmPTR_MSG);

return 0;
}

