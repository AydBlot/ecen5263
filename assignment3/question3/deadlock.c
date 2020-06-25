#include <pthread.h>
#include <stdio.h>
#include <sched.h>
#include <time.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NUM_THREADS 2
#define THREAD_1 1
#define THREAD_2 2

typedef struct
{
    int threadIdx;
} threadParams_t;

timer_t deadlock_release_timer;
struct itimerspec deadlock_release_timespec;
struct sigevent deadlock_release_time_eventt; 

pthread_t threads[NUM_THREADS];
threadParams_t threadParams[NUM_THREADS];

int lockA_owned = 0;
int lockB_owned = 0;

struct sched_param nrt_param;

// On the Raspberry Pi, the MUTEX semaphores must be statically initialized
//
// This works on all Linux platforms, but dynamic initialization does not work
// on the R-Pi in particular as of June 2020.
//
pthread_mutex_t rsrcA = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rsrcB = PTHREAD_MUTEX_INITIALIZER;

volatile int rsrcACnt=0, rsrcBCnt=0, noWait=0;


void *grabRsrcs(void *threadp)
{
   threadParams_t *threadParams = (threadParams_t *)threadp;
   int threadIdx = threadParams->threadIdx;


   if(threadIdx == THREAD_1)
   {
     printf("THREAD 1 grabbing resources\n");
     pthread_mutex_lock(&rsrcA);
     lockA_owned=1;
     rsrcACnt++;
     if(!noWait) sleep(1);
     printf("THREAD 1 got A, trying for B\n");
     pthread_mutex_lock(&rsrcB);
     lockB_owned=1;
     rsrcBCnt++;
     printf("THREAD 1 got A and B\n");
     pthread_mutex_unlock(&rsrcB);
     pthread_mutex_unlock(&rsrcA);
     lockA_owned=0;
     lockB_owned=0;
     printf("THREAD 1 done\n");
   }
   else
   {
     printf("THREAD 2 grabbing resources\n");
     pthread_mutex_lock(&rsrcB);
     rsrcBCnt++;
     lockB_owned=1;
     if(!noWait) sleep(1);
     printf("THREAD 2 got B, trying for A\n");
     pthread_mutex_lock(&rsrcA);
     rsrcACnt++;
     lockA_owned=1;
     printf("THREAD 2 got B and A\n");
     pthread_mutex_unlock(&rsrcA);
     pthread_mutex_unlock(&rsrcB);
     lockB_owned=0;
     lockA_owned=0;
     printf("THREAD 2 done\n");
   }
   pthread_exit(NULL);
}

// This timer is called every 2 seconds to attempt to prevent the deadlock from occuring.
void deadlock_release_timer_handler(union sigval sv){
     printf("made timer\r\n");
     if(lockA_owned == 1){
	     pthread_mutex_unlock(&rsrcA);
	     lockA_owned=0;
     }
     if(lockB_owned == 1){
	     pthread_mutex_unlock(&rsrcB);
	     lockB_owned=0;
     }
}

int main (int argc, char *argv[])
{
   int rc, safe=0;

   rsrcACnt=0, rsrcBCnt=0, noWait=0;

   if(argc < 2)
   {
     printf("Will set up unsafe deadlock scenario\n");
   }
   else if(argc == 2)
   {
     if(strncmp("safe", argv[1], 4) == 0)
       safe=1;
     else if(strncmp("race", argv[1], 4) == 0)
       noWait=1;
     else
       printf("Will set up unsafe deadlock scenario\n");
   }
   else
   {
     printf("Usage: deadlock [safe|race|unsafe]\n");
   }


   //configure deadlock timespec to start in 2 seconds
   deadlock_release_timespec.it_value.tv_sec = 2;
   deadlock_release_timespec.it_value.tv_nsec = 0;

   //configure deadlock timespec to restart every 2 seconds
   deadlock_release_timespec.it_interval.tv_sec = 2;
   deadlock_release_timespec.it_interval.tv_nsec = 0;

   //Pass function pointer and timer value to the event structure
   deadlock_release_time_eventt.sigev_notify = SIGEV_THREAD;
   deadlock_release_time_eventt.sigev_notify_function = &deadlock_release_timer_handler;
   deadlock_release_time_eventt.sigev_notify_attributes = NULL;
   deadlock_release_time_eventt.sigev_value.sival_ptr = &deadlock_release_timer;
   deadlock_release_time_eventt.sigev_value.sival_int = 0;

   //create the Timer with the proper event
   int ret1 = timer_create(CLOCK_REALTIME, &deadlock_release_time_eventt, &deadlock_release_timer);
   printf("timer ret1 val=%d\r\n", ret1);

   if (ret1){
	   perror ("S1_timer_create");
   }
   printf("created timer to control locks\r\n");

   //Start the timer so it executes the function
   int deadlock_release_ret = timer_settime(deadlock_release_timer, 0, &deadlock_release_timespec, NULL);
   if(deadlock_release_ret){
	   perror ("timer_settime");
   }

   printf("Creating thread %d\n", THREAD_1);
   threadParams[THREAD_1].threadIdx=THREAD_1;
   rc = pthread_create(&threads[0], NULL, grabRsrcs, (void *)&threadParams[THREAD_1]);
   if (rc) {printf("ERROR; pthread_create() rc is %d\n", rc); perror(NULL); exit(-1);}
   printf("Thread 1 spawned\n");

   if(safe) // Make sure Thread 1 finishes with both resources first
   {
     if(pthread_join(threads[0], NULL) == 0)
       printf("Thread 1: %x done\n", (unsigned int)threads[0]);
     else
       perror("Thread 1");
   }

   printf("Creating thread %d\n", THREAD_2);
   threadParams[THREAD_2].threadIdx=THREAD_2;
   rc = pthread_create(&threads[1], NULL, grabRsrcs, (void *)&threadParams[THREAD_2]);
   if (rc) {printf("ERROR; pthread_create() rc is %d\n", rc); perror(NULL); exit(-1);}
   printf("Thread 2 spawned\n");

   printf("rsrcACnt=%d, rsrcBCnt=%d\n", rsrcACnt, rsrcBCnt);
   printf("will try to join CS threads unless they deadlock\n");

   if(!safe)
   {
     if(pthread_join(threads[0], NULL) == 0)
       printf("Thread 1: %x done\n", (unsigned int)threads[0]);
     else
       perror("Thread 1");
   }

   if(pthread_join(threads[1], NULL) == 0)
     printf("Thread 2: %x done\n", (unsigned int)threads[1]);
   else
     perror("Thread 2");

   if(pthread_mutex_destroy(&rsrcA) != 0)
     perror("mutex A destroy");

   if(pthread_mutex_destroy(&rsrcB) != 0)
     perror("mutex B destroy");

   printf("All done\n");

   exit(0);
}
