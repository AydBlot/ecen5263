/*
Joshua Ayden Blotnick
ECEN5623-RTES
Referenced this code for assistance: http://ecee.colorado.edu/~ecen5623/ecen/ex/Linux/code/sequencer/lab1.c
*/
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <syslog.h>
#include <semaphore.h>
#include <sys/sysinfo.h>


#define USEC_PER_MSEC (1000)
#define NUM_CPU_CORES (1)
#define FIB_TEST_CYCLES (100)
#define NUM_THREADS (3)     // service threads + sequencer
sem_t semS1_S3, semS2;

#define FIB_LIMIT_FOR_32_BIT (47)
#define FIB_LIMIT (10)

typedef struct
{
    int threadIdx;
    int MajorPeriods;
} threadInfo;

unsigned int seqIterations = FIB_LIMIT;
unsigned int idx = 0, jdx = 1;
unsigned int fib = 0, fib0 = 0, fib1 = 1;
#define FIB_TEST(seqCnt, iterCnt)      \
   for(idx=0; idx < iterCnt; idx++)    \
   {                                   \
      fib0=0; fib1=1; jdx=1;           \
      fib = fib0 + fib1;               \
      while(jdx < seqCnt)              \
      {                                \
         fib0 = fib1;                  \
         fib1 = fib;                   \
         fib = fib0 + fib1;            \
         jdx++;                        \
      }                                \
   }                                   \



int abortTest = 0;
double start_time;

struct

void *S1_S3(void *threadp){
	static struct timespec rtclk_start_time = {0, 0};
	static struct timespec rtclk_stop_time = {0, 0};
	sleep_requested.tv_sec=sleep_time.tv_sec;
	sleep_requested.tv_nsec=sleep_time.tv_nsec;

	/* start time stamp */ 
	clock_gettime(CLOCK_REALTIME, &rtclk_start_time);
	
	clock_gettime(CLOCK_REALTIME, &rtclk_stop_time);

}


void main(void){
	threadParams_t threadParams[NUM_THREADS];
	pthread_t threads[NUM_THREADS];

	if (sem_init (&semS1_S3, 0, 0)) { printf ("Failed to initialize semS1_S3 semaphore\n"); exit (-1); }
	if (sem_init (&semS2, 0, 0)) { printf ("Failed to initialize semS2 semaphore\n"); exit (-1); }

	rc=pthread_create(&threads[1],               // pointer to thread descriptor
			  &rt_sched_attr[1],         // use specific attributes
			  //(void *)0,                 // default attributes
			  fib10,                     // thread function entry point
			  (void *)&(threadParams[1]) // parameters to pass in
			);

	threadParams[0].MajorPeriods=10;

	for(i=0;i<NUM_THREADS;i++){
		pthread_join(threads[i], NULL);
	}
}
