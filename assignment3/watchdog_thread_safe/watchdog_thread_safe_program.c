#include <pthread.h> 
#include <stdio.h> 
#include <signal.h>
#include <sched.h>
#include <time.h>
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h>
#include <sys/time.h>

timer_t reader_timer;
struct itimerspec reader_timespec;
struct sigevent reader_time_event; 

timer_t timeout_timer;
struct itimerspec timeout_timespec;
struct sigevent timeout_event; 

pthread_mutex_t lock;

struct thread_data{
	struct timespec current_time;

	double x;
	double y;
	double z;

	double x_acceleration;
	double y_acceleration;
	double z_acceleration;

	double roll;
	double pitch;
	double yaw;
};

struct thread_data global_data = {{0, 0}, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// buf needs to store 30 characters
//Code found at https://stackoverflow.com/questions/8304259/formatting-struct-timespec
int timespec2str(char *buf, uint len, struct timespec *ts) {
    int ret;
    struct tm t;

    tzset();
    if (localtime_r(&(ts->tv_sec), &t) == NULL)
        return 1;

    ret = strftime(buf, len, "%F %T", &t);
    if (ret == 0)
        return 2;
    len -= ret - 1;

    ret = snprintf(&buf[strlen(buf)], len, ".%09ld", ts->tv_nsec);
    if (ret >= len)
        return 3;

    return 0;
}

void timer_update_handler(union sigval sv){
	char buf[30];
	struct timespec current_time;

	int retval = pthread_mutex_lock(&lock); 
	if(retval == 0){
		clock_gettime(CLOCK_REALTIME, &current_time);
		timespec2str(buf, sizeof(buf), &current_time);
		printf("-------------------------------------No new data available at time %s----------------------------------\r\n", buf);
		pthread_mutex_unlock(&lock);
	}
}

void reader_timer_handler(union sigval sv){
	char buf[30];
	//struct timespec timeoutTime;
	//timeoutTime = global_data.current_time;
	//timeoutTime.tv_sec += 10;
	//pthread_mutex_timedlock(&lock, &timeoutTime); 

	pthread_mutex_lock(&lock);

	printf("Current yaw:%lf, pitch:%lf, roll:%lf\r\n", global_data.yaw, global_data.pitch, global_data.roll);

	printf("X position:%lf, Y position:%lf, Z position:%lf\r\n", global_data.x, global_data.y, global_data.z);

	printf("X acceleration:%lf, Y acceleration:%lf, Z acceleration:%lf\r\n", global_data.x_acceleration, global_data.y_acceleration, global_data.z_acceleration);
	timespec2str(buf, sizeof(buf), &global_data.current_time);

	printf("Time:%s\r\n", buf); 

	pthread_mutex_unlock(&lock);

}

//This thread will update the acceleration, position, and angle.
void *updater_thread_handler(void* args){
	while(1){
		pthread_mutex_lock(&lock);
		clock_gettime(CLOCK_REALTIME, &global_data.current_time);
		
		//Update X Y Z positions based off acceleration
		global_data.x+= global_data.x_acceleration;

		global_data.y+= global_data.y_acceleration;

		global_data.z+= global_data.z_acceleration;

		//Update Roll Pitch and Yaw
		if(global_data.roll < 30){
			global_data.roll+=0.05;
		}

		if(global_data.roll > 30){
			global_data.roll-=0.05;
		}

		if(global_data.pitch < 12){
			global_data.pitch+=0.28;
		}

		if(global_data.pitch > 12){
			global_data.pitch-=0.28;
		}

		if(global_data.yaw < 8){
			global_data.yaw+=0.36;
		}

		if(global_data.yaw > 8){
			global_data.yaw-=0.36;
		}
		
		global_data.z_acceleration+=0.125;
		global_data.y_acceleration+=0.004;
		global_data.x_acceleration+=0.076;

		pthread_mutex_unlock(&lock); 
	}
}

void main(void){

	pthread_t updater_thread;
	
	//Initialize mutex lock
	if (pthread_mutex_init(&lock, NULL) != 0) { 
		printf("\n mutex init has failed\n"); 
		exit(EXIT_FAILURE); 
	} 

	//Start updater thread
	if(pthread_create(&updater_thread, NULL, updater_thread_handler, NULL) < 0) {
		perror("updater thread creation failed");
		exit(EXIT_FAILURE);
	}

	//configure timespec to start in 1 seconds
	reader_timespec.it_value.tv_sec = 0;
	reader_timespec.it_value.tv_nsec = 1000;

	//configure timespec to restart every 20 seconds
	reader_timespec.it_interval.tv_sec = 20;
	reader_timespec.it_interval.tv_nsec = 0;

	//Pass function pointer and timer value to the event structure
	reader_time_event.sigev_notify = SIGEV_THREAD;
	reader_time_event.sigev_notify_function = &reader_timer_handler;
	reader_time_event.sigev_notify_attributes = NULL;
	reader_time_event.sigev_value.sival_ptr = &reader_timer;
	reader_time_event.sigev_value.sival_int = 0;

	//create the Timer with the proper event
	int ret1 = timer_create(CLOCK_REALTIME, &reader_time_event, &reader_timer);
	printf("timer ret1 val=%d\r\n", ret1);

	if (ret1){
		perror ("reader timer_create");
	}
	printf("created reader timer \r\n");

	//Start the timer so it executes the function
	int reader_ret = timer_settime(reader_timer, 0, &reader_timespec, NULL);
	if(reader_ret){
		perror ("timer_settime");
	}

	//configure timespec to start in 10 seconds
	timeout_timespec.it_value.tv_sec = 10;
	timeout_timespec.it_value.tv_nsec = 0;

	//configure timespec to restart every 20 seconds
	timeout_timespec.it_interval.tv_sec = 20;
	timeout_timespec.it_interval.tv_nsec = 0;

	//Pass function pointer and timer value to the event structure
	timeout_event.sigev_notify = SIGEV_THREAD;
	timeout_event.sigev_notify_function = &timer_update_handler;
	timeout_event.sigev_notify_attributes = NULL;
	timeout_event.sigev_value.sival_ptr = &timeout_timer;
	timeout_event.sigev_value.sival_int = 0;

	//create the Timer with the proper event
	int ret = timer_create(CLOCK_REALTIME, &timeout_event, &timeout_timer);
	printf("timer ret val=%d\r\n", ret1);

	if (ret){
		perror ("timeout timer_create");
	}
	printf("created timeout timer \r\n");

	//Start the timer so it executes the function
	int timeout_ret = timer_settime(timeout_timer, 0, &timeout_timespec, NULL);
	if(timeout_ret){
		perror ("timer_settime");
	}
	
	sleep(100);
	pthread_join(updater_thread, NULL); 
	pthread_mutex_destroy(&lock);
	

}
