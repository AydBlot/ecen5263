/*
Joshua Ayden Blotnick
ECEN5623-RTES
Referenced this code for assistance: http://ecee.colorado.edu/~ecen5623/ecen/ex/Linux/code/sequencer/lab1.c
*/
#include <unistd.h>
#include <stdio.h> 
#include <semaphore.h>
#include <syslog.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h> 

int number_of_cycles = 1;
void S2(union sigval sv);
void Scheduler(union sigval sv);
void *S1_thread_handler(void* args);
double start_time = 0;

sem_t scheduler_flag;

timer_t S2_timer;
timer_t scheduler_timer;

struct itimerspec S2_timespec;
struct itimerspec scheduler_timespec;

struct sigevent S2_time_event; 
struct sigevent scheduler_time_event; 

#define FIB_TEST_CYCLES (100)
#define FIB_LIMIT_FOR_32_BIT (47)
#define FIB_LIMIT (10)
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

double getTimeMsec(void)
{
  struct timespec event_ts = {0, 0};

  clock_gettime(CLOCK_MONOTONIC, &event_ts);
  return ((event_ts.tv_sec)*1000.0) + ((event_ts.tv_nsec)/1000000.0);
}

void *S1_thread_handler(void* args){
	printf("Made S1 thread\r\n");
	//Calculate the number of iterations for 10ms
	double event_time=getTimeMsec();
	double end_time;
	FIB_TEST(seqIterations, FIB_TEST_CYCLES);
	double run_time=getTimeMsec() - event_time;
	int limit=0;

	int required_test_cycles = 1650; //(int)(10.0/run_time);
	start_time = getTimeMsec();
	
	while(number_of_cycles < 10){
		sem_wait(&scheduler_flag);

		do
		{
			FIB_TEST(seqIterations, FIB_TEST_CYCLES);
			limit++;
		}
		while(limit < required_test_cycles);

		end_time = getTimeMsec();
		syslog(LOG_ERR,"S1 time at iteration:%d = %lfms", number_of_cycles, (end_time-start_time));
		number_of_cycles++;
		limit=0;
	}
}

void S2(union sigval sv){
	double end_time = getTimeMsec();
	printf("S2 start time = %lf\r\n", (end_time-start_time));
	int limit=0;
	int required_test_cycles=1525; 

	while(number_of_cycles < 10 && sv.sival_int < 4){

		sem_wait(&scheduler_flag);

		do
		{
			FIB_TEST(seqIterations, FIB_TEST_CYCLES);
			limit++;
		}
		while(limit < required_test_cycles);

		end_time = getTimeMsec();
		syslog(LOG_ERR,"S2 time at iteration:%d = %lfms", number_of_cycles, (end_time-start_time));
		limit=0;
		number_of_cycles++;
		sv.sival_int++;
	}
}
void Scheduler(union sigval sv){
	sem_post(&scheduler_flag);
}

void main(void){
	openlog("timer_scheduler_RM.c",LOG_PID|LOG_CONS, LOG_USER);

	pthread_t S1_thread;
	if (sem_init (&scheduler_flag, 0, 0)) { printf ("Failed to initialize scheduler_flag semaphore\n"); exit (-1); }
	sem_post(&scheduler_flag);

	//Configure S1_timer for S1 to execute every 10 MS
	S2_timespec.it_value.tv_sec = 0;
	S2_timespec.it_value.tv_nsec = 10000000;

	//Configure S1_timer to never restart since it is infinite
	S2_timespec.it_interval.tv_sec = 0;
	S2_timespec.it_interval.tv_nsec = 0;

	S2_time_event.sigev_notify = SIGEV_THREAD;
	S2_time_event.sigev_notify_function = &S2;
	S2_time_event.sigev_notify_attributes = NULL;
	S2_time_event.sigev_value.sival_ptr = &S2_timer;
	S2_time_event.sigev_value.sival_int = 1;

	int ret1 = timer_create(CLOCK_REALTIME, &S2_time_event, &S2_timer);

	if (ret1){
		 perror ("S1_timer_create");
	}
	printf("created timer\r\n");

	//Configure S1_timer for S1 to execute every 10 MS
	scheduler_timespec.it_value.tv_sec = 0;
	scheduler_timespec.it_value.tv_nsec = 10000000;

	//Configure S1_timer to never restart since it is infinite
	scheduler_timespec.it_interval.tv_sec = 0;
	scheduler_timespec.it_interval.tv_nsec = 10000000;

	scheduler_time_event.sigev_notify = SIGEV_THREAD;
	scheduler_time_event.sigev_notify_function = &Scheduler;
	scheduler_time_event.sigev_notify_attributes = NULL;
	scheduler_time_event.sigev_value.sival_ptr = &S2_timer;
	scheduler_time_event.sigev_value.sival_int = 0;

	int scheduer_ret = timer_create(CLOCK_REALTIME, &scheduler_time_event, &scheduler_timer);

	if (ret1){
		 perror ("S1_timer_create");
	}
	printf("created timer\r\n");

	//Initialize scheduler
	int scheduler_ret = timer_settime(scheduler_timer, 0, &scheduler_timespec, NULL);
	if(scheduler_ret){
		perror ("timer_settime");
	}

	//intialize S2
	int ret = timer_settime(S2_timer, 0, &S2_timespec, NULL);
	if(ret){
		perror ("timer_settime");
	}

	if(pthread_create(&S1_thread, NULL, S1_thread_handler, NULL) < 0) {
		perror("Timer thread creation failed");
		exit(EXIT_FAILURE);
	}
	printf("made it main\r\n");

	
	sleep(10);

	closelog();
	sem_destroy(&scheduler_flag);
}
