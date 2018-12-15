#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <ctype.h>
#include <getopt.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <fcntl.h>
#include <time.h>

#define PERM (S_IRUSR | S_IWUSR)

void closePs();
pid_t childpids[25], chpids[25];
pid_t child_pid, parent_pid;

int ShmID, ShmMSG;
int *ShmPTR, *ShmPTR_MSG;
sem_t *sema;
pid_t pid, pid2;
int i, status;

int key = 985047, msgkey = 91500, option, sig_time, users_spawned;

FILE *file;
char *filename = "log";

int main(int argc, char *argv[]){

	parent_pid = getpid();

	/* check for valid number of command-line arguments */
        if (argc < 2){
		printf("Usage: %s [-h] [-s max_no._of_process] [-l filename] [-t total running time]\n", argv[0]);
                return 1;
        }

	while((option = getopt(argc, argv, "hs:l:t:")) != -1){
		
		switch(option){

			case 'h':
				printf("Usage: %s [-h] [-s max_no._of_process] [-l filename] [-t total running time]\n", argv[0]);
				return 1;

			case 's':
				users_spawned = atoi(optarg);
				if(users_spawned <= 0)
					users_spawned = 5;
				break;

			case 'l':
				filename = strdup(optarg);
				break;

			case 't':
				sig_time = atoi(optarg);
				if(sig_time <= 0)
					sig_time = 2;
				break;
			
			case '?':
				if(optopt == 's' || optopt == 'l' || optopt == 't'){
					fprintf(stderr, "The option -%c requires an argument.\n", optopt);
					perror("An argument is needed!\n");
					return 1;
				}
				
				else if(isprint(optopt)){
					fprintf (stderr, "Unknown option `-%c'.\n", optopt);
					perror("Invalid arguement is given! \n");
					return 1;
				}

				else{
					fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
					perror("Invalid arguement is given! \n");
					return 1;
				}
			default:
				return 1;
		}
	}

	//signal handling here
	signal(SIGALRM, closePs);
        signal(SIGINT, closePs);
        alarm(sig_time);

	//open log file to write into
        file = fopen(filename, "w+");

	//create shared memory segment
	ShmID = shmget(key, 8*sizeof(int), PERM | IPC_CREAT | IPC_EXCL);

	if(ShmID < 0){
		perror(" shmget(Server) failed to create shared memory segment.\n");
		return 1;
	}

	//attach shared memory segment
	ShmPTR = (int *)shmat(ShmID, NULL, 0);

	if ((int) ShmPTR == -1) {
	  perror("shmat(Server) failed to attach shared memory segment.\n");
          return 1;
     	}

	//create shared memory for message
	ShmMSG = shmget(msgkey, 8*sizeof(int), PERM | IPC_CREAT | IPC_EXCL);	
	
	if(ShmMSG < 0){
                perror(" shmget(Server) failed to create shared memory segment for message.\n");
                return 1;
        }

	//attach shared memory segment for message
	ShmPTR_MSG = (int *)shmat(ShmMSG, NULL, 0);

	 if ((int) ShmPTR_MSG == -1) {
          perror("shmat(Server) failed to attach shared memory segment for message.\n");
          return 1;
        }

	//set shared to zeros.
	ShmPTR[0] = 0;		//shared mememory to hold seconds
	ShmPTR[1] = 0;		//shared memeory to hold nanoseconds

	ShmPTR_MSG[0] = 0;	//for pid

	ShmPTR_MSG[1] = 0;	//for seconds;
	ShmPTR_MSG[2] = 0;	//for nanoseconds
	ShmPTR_MSG[3] = 0;	// for ready

	//initialize semaphore for shared processes,with a name and an initial value of 1.
	sema = sem_open("sema", O_CREAT | O_EXCL, 0644, 1);

	if(sema == SEM_FAILED){
		perror("sem_open failed!\n");
		return 1;
	}

	for(i = 0; i < users_spawned; i++){
		child_pid = fork();
		if(child_pid == -1){
			perror("Unable to fork children.\n");
			return 1;
		}
		if(child_pid == 0){
			printf("child_pid %i\n", (long)getpid());
			sprintf(childpids, "%i", (long)getpid());
			execlp("./user", "./user", childpids, NULL);
			perror("execlp failed to execute. \n");
			return 1;
		}

		chpids[i] = child_pid;
	}

	printf("Total Children: %d. \n", i);

	char shmSec[2], shmNano[10], msgSec[2], msgNano[10], msgText[100];

	while(i > 0)
	{
		ShmPTR[1] += 100000;
		
		//manage shared memory clock
		if( ShmPTR[1] > 999900000 ){		
			ShmPTR[1] = 0;			//set nanoseconds to zero
			ShmPTR[0] += 1;			//increase seconds by 1
		}
		
		//if a message has been sent by child
		if(ShmPTR_MSG[3] == 1){

			sprintf(shmSec, "%d", ShmPTR[0]);
			sprintf(shmNano, "%d", ShmPTR[1]);

			sprintf(msgSec, "%d", ShmPTR_MSG[1]);
			sprintf(msgNano, "%d", ShmPTR_MSG[2]);

			sprintf(msgText, "OSS: Child pid %ld is terminating at my time %d.%d because it reached %d.%d in user \n", ShmPTR_MSG[0], ShmPTR[0], ShmPTR[1], ShmPTR_MSG[1], ShmPTR_MSG[2]);

			fputs(msgText, file);

			ShmPTR_MSG[0] = 0;
			ShmPTR_MSG[1] = 0;
			ShmPTR_MSG[2] = 0;
			ShmPTR_MSG[3] = 0;

			i--;
			continue;
		}
	}

	//wait for children to terminate
	//while((pid2 = wait(&status)) > 0);
	int j;
	for(j = 0; j <= i; j++)
		wait(NULL);

	printf("All children terminated\n");

	//disconnet from semaphore
	sem_close(sema);
	sem_unlink("sema");

	printf("Server has detected the completion of its child...\n");
	
	shmdt((void *) ShmPTR);
	shmdt((void *) ShmPTR_MSG);

	printf("Server has detached its shared memory...\n");
	
	shmctl(ShmID, IPC_RMID, NULL);
	shmctl(ShmMSG, IPC_RMID, NULL);
	printf("Server has removed its shared memory");
	
	printf("Server exits...\n");
	

	return 0;
}

void closePs(){

	pid_t pid = getpgrp();
	printf("Termininating Process Group ID: %i due to time limit.\n", pid);

	int j;
	for(j = 0; j <= i; j++ ){
		kill(chpids[i], SIGINT);
		wait(NULL);
	}

	//Wait for children to terminate.
	//while((pid2 = wait(&status)) > 0);

	 printf("------All children terminated\n");
    	 printf("------Server has detected the completion of its child...\n");

	 //disconnet from semaphore
	 sem_close(sema);
	 sem_unlink("sema");
	 
	 shmdt((void *) ShmPTR);
	 shmdt((void *) ShmPTR_MSG);
     	 printf("------Server has detached its shared memory...\n");
     	 shmctl(ShmID, IPC_RMID, NULL);
	 shmctl(ShmMSG, IPC_RMID, NULL);
	
	 kill(pid, SIGINT);
     	 kill(parent_pid, SIGTERM);

     	 printf("------Server has removed its shared memory...\n");
     	 printf("------Server exits...\n");

     exit(0);
}
