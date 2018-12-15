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

void closePs();
pid_t childpids[25];
pid_t child_pid, parent_pid;

int    ShmID;
int    *ShmPTR;
pid_t  pid, pid2;
int    status;
key_t key;
key = 985047; 

int option, n, s;

int  main(int  argc, char *argv[])
{

signal(SIGALRM, closePs);
signal(SIGINT, closePs);
alarm(2);

parent_pid = getpid();


	/* check for valid number of command-line arguments */
        if (argc < 2){
		printf("Usage: %s [-n max_no._of_process] [-s no._allowed_at_a_time]\n", argv[0]);
                return 1;
        }
	
     	char arg1[10];
	char arg2[10];
	char arg3[10];

	int i = 0;
	s = 0;

	//getopt loop to parse command line options
	while((option = getopt(argc, argv, "hn:s:")) != -1){
		switch(option){
			case 'h':
				printf("Usage: %s [-n max_no._of_process] [-s no._allowed_at_a_time]\n", argv[0]);
				return 1;
	
			case 'n':
				n = atoi(optarg);
				if ( n > 20)
					n = 20;
				else if (n < 1)
					n = 0;
				break;

			case 's':
				s = atoi(optarg);
				break;

			default:
				return 1;
		}
		
	}

/*
	//if only -h is choosen, print usage
	if( (hflag == 1) && (nflag == 0) && (sflag == 0)){
		printf("Usage: %s [-n max_no._of_process] [-s no._allowed_at_a_time]\n", argv[0]);
		return 1;
	}

	if( (nflag == 1) && (hflag == 0) ) {
		if( argv[2] == NULL){
			n = 20;
			printf("%s: Executing with %d max processes...\n", argv[0], n);
		}
		else if( (n = atoi(argv[2])) >= 1 && n <= 20){
                    printf("%s: Executing with %d max processes...\n", argv[0], n);
			s = atoi(atoi(argv[4]));
                }
		else if( (n = atoi(argv[2])) > 20){
			n = 20;
                    printf("%s: Executing with %d max processes...\n", argv[0], n);
                }
		else{
		   printf("Usage: %s [-n max_no._of_process] [-s no._allowed_at_a_time]\n", argv[0]);
                    return 1;
		}
	}
*/	

	// Create string for our first command line argument to
	// the executable that we will exec
	snprintf(arg1,10,"%d", n);				//for my testing purpose only

	// Create string for the second command line arg
	snprintf(arg2,10,"%s","-n");				//this is for my testing

	// Create string for the third command line arg
	snprintf(arg3,10,"%d",7);				// for testing purpose only

     //Create and attache shared memory
     ShmID = shmget(key, 4*sizeof(int), IPC_CREAT | 0666);

     if (ShmID < 0) {
	  perror("shmget (server)\n");
          exit(1);
     }

     printf("Server has received a shared memory of two integers...\n");
     
     ShmPTR = (int *) shmat(ShmID, NULL, 0);

     if ((int) ShmPTR == -1) {
	  perror("shmat (server)\n");
          exit(1);
     }
     printf("Server has attached the shared memory...\n");
     
     ShmPTR[0] = 0;
     ShmPTR[1] = 0;
     
     printf("Server has filled %d %d in shared memory...\n",
            ShmPTR[1], ShmPTR[0]);
            
     printf("Server is about to fork a child process...\n");
	printf("%d\n", s);

	for(i = 0; i < s; i++){
		
			if( (child_pid = fork()) == 0){
				execlp("./child","./child",arg1,arg2,arg3,(char *)NULL);
				exit(0);
			}
	

		childpids[i] = child_pid; //store child pids
	}

	for(i = 0; i < (n - s); i++){
			wait(&status);
			if( (child_pid = fork()) == 0){
                                execlp("./child","./child",arg1,arg2,arg3,(char *)NULL);
                                exit(0);
                        }

		childpids[i] = child_pid; //store child pids
	}

     /*wait for children to terminate*/
     while((pid2 = wait(&status)) > 0);

     printf("All children terminated\n");

     printf("Server has detected the completion of its child...\n");

     printf("Server read %d %d in shared memory...\n", ShmPTR[1], ShmPTR[0]);

     shmdt((void *) ShmPTR);
     printf("Server has detached its shared memory...\n");
     shmctl(ShmID, IPC_RMID, NULL);
     printf("Server has removed its shared memory...\n");
     printf("Server exits...\n");
     exit(0);
}

void closePs(){
	int i;

    	for (i=0; i<n; i++) {
        	kill(childpids[i], SIGINT);
    	}

	/*wait for children to terminate*/
     while((pid2 = wait(&status)) > 0);

     printf("------All children terminated\n");
     printf("------Server has detected the completion of its child...\n");
     printf("------Server read %d %d in shared memory...\n", ShmPTR[1], ShmPTR[0]);

     shmdt((void *) ShmPTR);
     printf("------Server has detached its shared memory...\n");
     shmctl(ShmID, IPC_RMID, NULL);
     kill(parent_pid, SIGTERM);

     printf("------Server has removed its shared memory...\n");
     printf("------Server exits...\n");
     exit(0);

}
