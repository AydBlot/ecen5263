#include <pthread.h>
#include <stdio.h>
#include <sched.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <mqueue.h>

#define MAX_MSG_SIZE 128
#define MESSAGE_QUEUE_NAME "/send_receive_mq"
struct mq_attr attr;
struct timespec current_time;

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

void *sender_thread_handler(void* args){
	mqd_t mq;
	char time_buf[50]; 

	/* create the message queue */
	mq = mq_open(MESSAGE_QUEUE_NAME, O_CREAT|O_WRONLY, 0, &attr);
	printf(" Value of MQ open in sender errno: %d\n ", errno);

	if(mq == (mqd_t)-1){
		printf("womp womp sender\r\n");
		exit(1);
	}
	
	while(1){
		clock_gettime(CLOCK_REALTIME, &current_time);
		timespec2str(time_buf, sizeof(time_buf), &current_time);
		printf("Sending message at time:%s\r\n", time_buf);
	}

}

static char imagebuff[4096];
void *receiver_thread_handler(void* args){
	mqd_t mq;
	char time_buf[50];
	int i, j;
	char pixel = 'A';

	for(i=0;i<4096;i+=64) {
		pixel = 'A';
		for(j=i;j<i+64;j++) {
			imagebuff[j] = (char)pixel++;
		}
		imagebuff[j-1] = '\n';
	}

	imagebuff[4095] = '\0';
	imagebuff[63] = '\0';

	printf("buffer = %s\r\n", imagebuff);

	/* create the message queue */
	mq = mq_open(MESSAGE_QUEUE_NAME, O_RDONLY);
	printf(" Value of errno: %d\n ", errno);

	if(mq == (mqd_t)-1){
		printf("womp womp receiver\r\n");
		exit(1);
	}
	while(1){
		clock_gettime(CLOCK_REALTIME, &current_time);
		timespec2str(time_buf, sizeof(time_buf), &current_time);
		printf("Receiving message at time:%s\r\n", time_buf);
	}

}

void SIGhandler(int signo) {
	printf("Caught signal, exiting\r\n");
	mq_unlink(MESSAGE_QUEUE_NAME);
	exit(1);
}

void main(void){
	pthread_t sender_thread;
	pthread_t receiver_thread;

	char buffer[MAX_MSG_SIZE + 1];

	/* initialize the queue attributes */
	attr.mq_flags = 0;
	attr.mq_maxmsg = 100;
	attr.mq_msgsize = MAX_MSG_SIZE;

	//Handle sigint and sigterm signals to ensure sockets are closed properly 
	//and memory is cleaned	
	signal(SIGINT, SIGhandler);
	signal(SIGTERM, SIGhandler);

	//Start receiver thread
	if(pthread_create(&sender_thread, NULL, sender_thread_handler, NULL) < 0) {
		printf("womp womp\r\n");
		perror("sender thread creation failed");
		exit(EXIT_FAILURE);
	}

	sleep(1);
	//Start sender thread
	if(pthread_create(&receiver_thread, NULL, receiver_thread_handler, NULL) < 0) {
		printf("womp womp\r\n");
		perror("receiver thread creation failed");
		exit(EXIT_FAILURE);
	}

	sleep(100);
}
