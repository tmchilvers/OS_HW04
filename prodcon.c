/*
* Programmed by Tristan Chilvers
* ID: 2288893
* Email: chilvers@chapman.edu
*
*	References:
*			- https://www.geeksforgeeks.org/use-posix-semaphores-c/
* 		- https://docs.oracle.com/cd/E19455-01/806-5257/sync-42602/index.html
*	 		- https://caligari.dartmouth.edu/doc/ibmcxx/en_US/doc/libref/tasks/tumemex2.htm
* 		- As well as all example files available on blackboard.
*
*	Debugging help from classmate: Matthew Raymond
*/

#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>

//	----------------------------------------------------------------------------
//	CONSTANTS:
#define BLOCK_SIZE 32
#define MAX_SIZE 64000

//	----------------------------------------------------------------------------
//	GLOBAL VARIABLES
//
int memsize = 0;	//	Overall size of shared memory (multiple of 32)
int ntimes = 0;	/*	Number of times producer writes to consumer and consumer
										reads from the shared memory region */
int blocks = 0;	//	Total number of blocks of shared memory

//	Variables for Threading:
pthread_mutex_t	mut;	/*	mutex: used to restrict shared memory to one thread
													at a time	*/
sem_t sem[2];	//	semaphore: allow multiple threads to access the shared memory
char* sm;	//	shared memory

//	----------------------------------------------------------------------------
//	FUNCTION DECLARATIONS
//

//	Producer:
//	Input: None
//	Output:	None. (Will generate random data and store checksum into a global
//					variable for shared memory)
void *producer();

//	Consumer:
//	Input: None
//	Output:	None. (Will calculate checksum and compare between checksum from
//					producer thread. It will output and error if there is a difference)
void *consumer();

//	============================================================================
//	MAIN
//
int main(int argc, char *argv[])
{
	//	The thread identifiers
	pthread_t prod;	//	producer
	pthread_t con;	//	consumer

	//	ERROR CHECK --------------------------------------------------------------
	//	No Input
	if (argc != 3) {
		printf("Program requires three arguments: ./prodcon <memsize> <ntimes> \n");
		return -1;
	}

	//	Ensure both inputs are positive
	if (atoi(argv[2]) < 0 || atoi(argv[1]) < 0) {
		printf("Both inputs must be a positive.\n");
		return -1;
	}

	//	Ensure that memsize is divisible by 32
	if (atoi(argv[1])  % 32 != 0) {
		printf("Memsize must be divisible by 32\n");
		return -1;
	}

	//	Ensure that memsize does not exceed the maximum size
	if (atoi(argv[1]) > MAX_SIZE)
	{
		printf("Memsize cannot exceed the max size of 64K\n");
		return -1;
	}

	//	Cast user inputs to int and save to global variables ---------------------
	memsize = atoi(argv[1]);	//	grab size of memory
	ntimes = atoi(argv[2]);	//	grab number of times to call
	blocks = memsize / BLOCK_SIZE;	//	calculate how many blocks of memory
	sm = malloc(memsize);	//	allocate memory to be shared between threads

	//	Initialize Semaphores ----------------------------------------------------
	if (sem_init(&sem[0], 0, 1) != 0)
	{
		perror("First semaphore initialization failed");
		return -1;
	}

  if (sem_init(&sem[1], 0, 0) != 0)
	{
		perror("Second semaphore initialization failed");
		return -1;
	}

	//	THREADING ----------------------------------------------------------------
	//	Initializes the mutex (lock)
	pthread_mutex_init(&mut, NULL);

	//	Create the threads for producer and consumer
	pthread_create(&prod, NULL, producer, NULL);
	pthread_create(&con, NULL, consumer, NULL);

	//	Wait for threads to exit
	pthread_join(prod, NULL);
	pthread_join(con, NULL);

	//	CLEANUP ------------------------------------------------------------------
	//	Destroy the semaphores
	if (sem_destroy(&sem[0]) != 0)
	{
		printf("Destroying semaphore error\n");
		return -1;
	}

	if (sem_destroy(&sem[1]) != 0)
	{
		printf("Destroying semaphore error\n");
		return -1;
	}

  //	Free up memory
  free(sm);

	//	Destroy the mutex
  pthread_mutex_destroy(&mut);

	return 0;
}

//	============================================================================
//	THREAD: producer
//
//	Input: None
//	Output:	Create 30 bytes of random data (0-255) and store the checksum in the
//					last 2 bytes of the shared memory block. The producer is to do this
//					ntimes synchronizing with the consumer.
//
void *producer()
{
	int index;

	//	Iterate n number of times (as provided by the user)
	for(int n = 0; n < ntimes; n++)
	{
		//	Block until consumer is ready for next runthrough
		if(sem_wait(&sem[0]) != 0)
		{
			printf("Semaphore error in producer thread\n");
			exit(EXIT_FAILURE);
		}

		//	Iterate through the memory blocks
		for(int i = 0; i < blocks; i++)
		{
			pthread_mutex_lock(&mut);	//	lock the shared memory
			unsigned short int checksum = 0;	/*	Must be unsigned short due
																					to size of data we are working with */

		  /*  Iterate through each byte in the current memory block. Stop at last
					two bytes of the current memory block */
			for(index = (i * BLOCK_SIZE); index < (((i + 1) * BLOCK_SIZE) - 2); index++)
			{
				sm[index] = rand() % 255;	//	Store random data between 0 and 255
				checksum += sm[index];	//	Update checksum
			}

			//	Store final checksum value in last two bytes of current memory block
			((unsigned short int *)sm)[(index + 1) / 2] = checksum;

			pthread_mutex_unlock(&mut);	//	Unlock the shared memory

			//	Allow consumer to read from shared memory
			if (sem_post(&sem[1]) != 0)
			{
				printf("error\n");
			}
		}
	}
	pthread_exit(0);
}

//	============================================================================
//	THREAD: consumer
//
//	Input: None.
//	Output: Read the shared memory buffer of 30 bytes, and then calculate and
//					store the checksum. Check against the checksum stored in the shared
//					memory calculated by the producer thread to ensure no corruption.
//					The consumer is to do this ntimes synchronizing with the producer.
void *consumer()
{
		//	Iterate n number of times (as provided by the user)
    for(int n = 0; n < ntimes; n++)
		{
        int index = 0;

				//	Iterate through the memory blocks
        for(int i = 0; i < blocks; i++)
				{
					//	Block until producer is ready for next runthrough
    	    if (sem_wait(&sem[1]) != 0)
					{
    	        printf("%s\n",strerror(errno));
					}

          pthread_mutex_lock(&mut);	//	lock the shared memory
          unsigned short int checksum = 0;	/*	Must be unsigned short due
																								to size of data we are working with */

					/*  Iterate through each byte in the current memory block. Stop at last
							two bytes of the current memory block */
          for(index = (i * BLOCK_SIZE); index < (((i + 1) * BLOCK_SIZE) - 2); index++) {
              checksum += sm[index];	//	Update checksum
          }

          /* Grab the checksum calculated by the producer, which is stored in
						 the last two bytes of the block */
          int prodChecksum = ((unsigned short int *)sm)[(index + 1) / 2];

          //	Compare the two checksums to ensure that there was no error in the process
          if(prodChecksum != checksum) {
              printf("The checksums at block %d, iteration %d do not match\n", i, n);
              printf("Producer Checksum: %d\nConsumer Checksum: %d\n", prodChecksum, checksum);
              exit(EXIT_FAILURE);
          }


          pthread_mutex_unlock(&mut);	//	Unlock the shared memory
        }

				//	Allow producer to read from shared memory
        if (sem_post(&sem[0]) != 0)
            printf("%s\n",strerror(errno));
    }

    pthread_exit(0);
}
