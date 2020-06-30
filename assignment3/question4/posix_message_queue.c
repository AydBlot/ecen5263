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

	int nbytes;
	int i;
	char place_holder;
	mqd_t mq;
	char time_buf[50]; 
	char message_to_send[27] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

	printf("size is:%d\r\n", sizeof(message_to_send));

	/* create the message queue */
	mq = mq_open(MESSAGE_QUEUE_NAME, O_CREAT|O_WRONLY, 0, &attr);
	//printf(" Value of MQ open in sender errno: %d\n ", errno);

	if(mq == (mqd_t)-1){
		printf("womp womp sender\r\n");
		exit(1);
	}
	
	while(1){
		clock_gettime(CLOCK_REALTIME, &current_time);
		timespec2str(time_buf, sizeof(time_buf), &current_time);
		printf("Sending message at time:%s\r\n", time_buf);

		/* send message with priority=30 */
		if((nbytes = mq_send(mq, message_to_send, sizeof(message_to_send), 30)) == -1)
		{
			perror("mq_send");
		}
		else
		{
			printf("send: message successfully sent\n");
		}
		
		//Make the message a bit different each time
		place_holder = message_to_send[0];
		//printf("place holder:%c\r\n", place_holder);
		for(i = 0; i < sizeof(message_to_send) - 1; i++) {
			message_to_send[i] = message_to_send[i + 1]; 
		}		
		
		message_to_send[i-1] = place_holder;		
		sleep(1);
	}

}

void *receiver_thread_handler(void* args){
	mqd_t mq;
	int prio;
	int nbytes;
	char buffer[MAX_MSG_SIZE];
	char time_buf[50];

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
		
		/* read oldest, highest priority msg from the message queue */
		if((nbytes = mq_receive(mq, buffer, MAX_MSG_SIZE, &prio)) == -1)
		{
			perror("mq_receive");
		}
		else
		{
			buffer[nbytes] = '\0';
			printf("receive: msg %s received with priority = %d, length = %d\n",
			buffer, prio, nbytes);
		}
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
