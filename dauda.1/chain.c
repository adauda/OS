#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <getopt.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>

int main (int argc, char *argv[])
{

	/* check for valid number of command-line arguments */
        if (argc < 2){
                fprintf(stderr, "Usage: %s -h | -p | -n somevalue\n", argv[0]);
                return 1;
        }

	/* to concatenate file name(argv[0], the string(Error), 
 	   and the perror message together)*/
	const char str1[strlen(argv[0])];
	const char str2[] = ": Error";
	char *res;

	/* allocate memory to store arg and string messages that will be added to perror*/
	res = malloc(strlen(str1) + strlen(str2) +1);
	if(!res){
		fprintf(stderr, "malloc() failed: insufficient memory!\n");
		return EXIT_FAILURE;
	}

	strcpy(res, argv[0]);
	strcat(res, str2);
	
	/* for process ID*/
	pid_t childpid = 0;

	int i, n;
  	int index;
  	int c;
	int count = 0;
  	opterr = 0;

	/*Specifying the expected options with the use of getopt function.
	  The n option expect numbers as argument*/
  	while ((c = getopt (argc, argv, "n:hp")) != -1)
    	switch (c)
      	{
      	case 'h':
        	fprintf(stderr, "Usage: %s -h | -p | -n somevalue\n", argv[0]);
        	return 1;
      	case 'p':
		perror(res);
		free(res);
		return 1;
      	case 'n':
		n = atoi(optarg);	//store the argument in a return variable
        	break;
     	case '?':
        	if (optopt == 'n')
          		fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        	else if (isprint (optopt))
          		fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        	else
          		fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
        	return 1;
      	default:
        	return 1;
      	}

  	for (index = optind; index < argc; index++)
   		 printf ("Non-option argument %s\n", argv[index]);
	
	
	for(i = 1; i < n; i++)
		if(childpid = fork()) 		//if a process forks
			break;

	/* print out the process IDs*/
	fprintf(stderr, "i:%d process ID:%ld parent ID:%ld child ID:%ld\n",
		i, (long)getpid(), (long)getppid(), (long)childpid); 	

  return 0;
}
