#include <pthread.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h>
#include <sys/time.h>


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

void *timer_update_handler(void* args){
	struct timespec timeoutTime;
	char buf[30];

	while(1){
		clock_gettime(CLOCK_REALTIME, &timeoutTime);
		timeoutTime.tv_sec += 10;
		int retval = pthread_mutex_timedlock(&lock, &timeoutTime); 
		printf("retval for timer update:%d\r\n", retval);
		if(retval != 0){
			timespec2str(buf, sizeof(buf), &timeoutTime);
			printf("No new data available at time %s\r\n", buf);
		}
		else{
			pthread_mutex_unlock(&lock);
		}
	}
}

void *reader_thread_handler(void* args){
	char buf[30];
	struct timespec timeoutTime;
	while(1){
		clock_gettime(CLOCK_REALTIME, &timeoutTime);
		timeoutTime.tv_sec += 10;
		pthread_mutex_timedlock(&lock, &timeoutTime); 

		//pthread_mutex_lock(&lock);

		printf("Current yaw:%lf, pitch:%lf, roll:%lf\r\n", global_data.yaw, global_data.pitch, global_data.roll);

		printf("X position:%lf, Y position:%lf, Z position%lf\r\n", global_data.x, global_data.y, global_data.z);

		printf("X acceleration:%lf, Y acceleration:%lf, Z acceleration:%lf\r\n", global_data.x_acceleration, global_data.y_acceleration, global_data.z_acceleration);
		timespec2str(buf, sizeof(buf), &global_data.current_time);

		printf("at time:%s\r\n", buf); 

		pthread_mutex_unlock(&lock);
		sleep(1);
	}

}

//This thread will update the acceleration, position, and angle.
void *updater_thread_handler(void* args){
	while(1){
		pthread_mutex_lock(&lock);
		clock_gettime(CLOCK_REALTIME, &global_data.current_time);
		
		//Update X Y z positions based off acceleration
		if(global_data.x < 2670.0){
			global_data.x+= global_data.x_acceleration;
		}

		if(global_data.y < 500){
			global_data.y+= global_data.y_acceleration;
		}

		if(global_data.z < 36000.0){
			global_data.z+= global_data.z_acceleration;
		}

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
		sleep(1);
	}
}

void main(void){

	pthread_t reader_thread;
	pthread_t updater_thread;
	pthread_t timer_update_thread;

	printf("made ittttt\r\n");

	printf("made it\r\n");
	//Initialize mutex lock
	if (pthread_mutex_init(&lock, NULL) != 0) { 
		printf("\n mutex init has failed\n"); 
		exit(EXIT_FAILURE); 
	} 

	printf("made it1\r\n");

	if(pthread_create(&updater_thread, NULL, updater_thread_handler, NULL) < 0) {
		perror("updater thread creation failed");
		exit(EXIT_FAILURE);
	}

	if(pthread_create(&reader_thread, NULL, reader_thread_handler, NULL) < 0) {
		perror("reader thread creation failed");
		exit(EXIT_FAILURE);
	}

	if(pthread_create(&timer_update_thread, NULL, timer_update_handler, NULL) < 0) {
		perror("Timer update thread creation failed");
		exit(EXIT_FAILURE);
	}

	sleep(100);
	pthread_join(reader_thread, NULL); 
	pthread_join(updater_thread, NULL); 
	pthread_mutex_destroy(&lock);
	

}
