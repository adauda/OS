#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <limits.h>
#include <stdlib.h>

//check for overflow
int addOvf(int* result, int a, int b)
{
   if( a > INT_MAX - b)
     return -1;
   else
   {
     *result = a + b;
      return 0;
   }
}

int main(int argc, char** argv){

	int x = atoi(argv[1]);
	char *y;
	y = argv[2];
	int z = atoi(argv[3]);

	//printf("%d, %s, %d\n", x, y, z);

	int    ShmID;
        int    *ShmPTR;
        pid_t  pid;
        int    status;

        key_t key;
        key = 985047;
	
	ShmID = shmget(key, 4*sizeof(int), 0666);

	if (ShmID < 0) {
          perror("shmget (client)\n");
          exit(1);
     	}

	ShmPTR = (int *) shmat(ShmID, NULL, 0);

     	if ((int) ShmPTR == -1) {
          perror("shmat (client)\n");
          exit(1);
     	}

	printf("client has attached the shared memory...\n");

	int *res = (int *)malloc(sizeof(int));
        int j = x * 1000000;
        int k = ShmPTR[0];
        int result;

	// update shared memory
        result = addOvf(res, j, k);
        if(result == 0){
		ShmPTR[0] = ShmPTR[0] + *res;
        }
        else if(result == -1){
		ShmPTR[1] = ShmPTR[1] + 1;
		ShmPTR[0] = 0;
        }

    	printf("Client process started\n");
     	printf("Client updated %d %d in shared memory\n", ShmPTR[1], ShmPTR[0]);
	printf("Client is about to exit\n");

    return 0;

}

