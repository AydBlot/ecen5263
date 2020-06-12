#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <time.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <arpa/inet.h>
#include <stdbool.h> 
#include <pthread.h> 
#include <errno.h>
#include "aesd_ioctl.h"
#include <sys/ioctl.h>

#define SIGINT  2
#define SIGTERM 15
#define PORT 9000

//#undef USE_AESD_CHAR_DEVICE
#define USE_AESD_CHAR_DEVICE

#ifndef USE_AESD_CHAR_DEVICE
#else
#endif

//global signal handler flag
int server_fd; 
bool signal_handler_flag_set = false;
bool time_signal_flag_set = false;

//the thread function
void *connection_handler(void *);
void time_signal_handler(union sigval sv);
void *time_thread_handler(void *);

timer_t timer;

struct itimerspec ts;

struct sigevent time_event;

void start_daemon(void) {
	pid_t process_id = 0;
	pid_t sid = 0;
	//Create child process
	process_id = fork();
	// // Indication of fork() failure
	if (process_id < 0) {
		printf("fork failed!\n");
		// Return failure in exit status
		exit(1);
	}
	// PARENT PROCESS. Need to kill it.
	else if (process_id > 0) {
		printf("%d", process_id);
		// // return success in exit status
		exit(0);
	}
	//unmask the file mode
	umask(0);
	////set new session
	sid = setsid();
	if(sid < 0) {
		//// Return failure
		exit(1);
	}
	//// Change the current working directory to root.
	chdir("/");
	//// Close stdin. stdout and stderr
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

}

struct threadData{
	bool thread_complete_flag; //make a bool get rid of locking
	pthread_t threadID;
	int client_thread_socket;
};

struct LinkedList{
	struct threadData thread_data;
	int node_number;
	struct LinkedList *next;
};

//struct LinkedList *pthread_linked_list;
typedef struct LinkedList *node; //Define node as pointer of data type struct LinkedList
node pthread_linked_list;

void removeNode(node head, int node_number){
	node temp, previous;
	temp = head;	
	// If head node itself holds the key to be deleted 
	if ( temp != NULL && temp->node_number == node_number){
		head = temp->next;
		free(temp);
		return;
	}

	while(temp != NULL && temp->node_number != node_number){
		previous = temp;
		temp = temp->next;
	}

	//if the node_number wasn't in the list
	if(temp == NULL){
		return;
	}

	// Unlink the node from linked list 
	previous->next = temp->next;

	//free the node
	free(temp);

}

node addNode(node head, pthread_t threadID, node new_node){
	node p;// declare two nodes temp and p
	new_node->thread_data.threadID = threadID; // add element's value to data part of node
	new_node->thread_data.thread_complete_flag = 0;
	
	if(head == NULL){
		head = new_node;     //when linked list is empty
	}
	else{
		p  = head;//assign head to p 
		while(p->next != NULL){
			p = p->next;//traverse the list until p is the last node.The last node always points to NULL.
		}
		p->next = new_node;//Point the previous last node to the new node created.
	}
	return head;
}

/* Function to delete the entire linked list */
void deleteList(node head)  
{  
	      
	/* deref head_ref to get the real head */
	node current = head;  
	node next;  
	  
	while (current != NULL) {  
		next = current->next;  
		free(current);  
		current = next;  
	}  
	/* deref head_ref to affect the real head back  
	 *     in the caller. */
	head = NULL;  
}  

void SIGhandler(int signo) {
	printf("Caught signal, exiting\r\n");
	syslog(LOG_INFO, "Caught Signal:%d, exiting", signo);
	if(shutdown(server_fd, SHUT_RDWR)){
		perror("Failed on shutdown()");
		syslog(LOG_ERR, "Could not close socket file descriptor in signal handler:%s", strerror(errno));
	}
	signal_handler_flag_set = true;
}


//create global mutex lock for use inside threads
pthread_mutex_t count_mutex;
int main(int argc, char const *argv[]) {
	//Handle sigint and sigterm signals to ensure sockets are closed properly 
	//and memory is cleaned	
	signal(SIGINT, SIGhandler);
	signal(SIGTERM, SIGhandler);
	
	struct sockaddr_in server_address, client_address; 
	int opt = 1; 

	if ((server_fd = socket(PF_INET, SOCK_STREAM, 0)) == 0) { 
		perror("socket failed"); 
		return -1;
		exit(EXIT_FAILURE);
	}
	
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
				&opt, sizeof(opt))) {
		perror("Setsockopt");
		return -1;
		exit(EXIT_FAILURE);
	}
	
	//Populate the addres structure address so the bind function can associate the information with
	//the server file descriptor
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = INADDR_ANY;
	server_address.sin_port = htons( PORT );

	if(bind(server_fd, (struct sockaddr *) &server_address,
				sizeof(server_address))<0) {
		perror("bind failed");
		return -1;
		exit(EXIT_FAILURE);
	}

	//start process as daemon if-d argument was given
	if (argc > 1){
		char *testString = "-d";
		if (strcmp(testString, argv[1]) == 0) {
			start_daemon();
		}
	}
	
#ifndef USE_AESD_CHAR_DEVICE
	//Initialize timer.
	int ret;
	ts.it_interval.tv_sec = 10;
	ts.it_interval.tv_nsec = 0;
	ts.it_value.tv_sec = 10;
	ts.it_value.tv_nsec = 0;

	time_event.sigev_notify = SIGEV_THREAD;
	time_event.sigev_notify_function = &time_signal_handler;
	time_event.sigev_notify_attributes = NULL;
	time_event.sigev_value.sival_ptr = &timer;

	ret = timer_create(CLOCK_REALTIME, &time_event, &timer);
	
	pthread_t time_thread;
	
	if(pthread_create(&time_thread, NULL, time_thread_handler, NULL) < 0) {
		perror("Timer thread creation failed");
		exit(EXIT_FAILURE);
	}

	if (ret){
		 perror ("timer_create");
	}
	ret = timer_settime(timer, 0, &ts, NULL);
	if(ret){
		perror ("timer_settime");
	}
#endif
	if (listen(server_fd, 3) < 0) { 
		perror("listen"); 
		exit(EXIT_FAILURE); 
	} 

	int addrlen = sizeof(client_address); 
	int client_socket_descriptor;
	//Accept client connections while available and spawn a thread to handle it
	while(!signal_handler_flag_set){
		if ( (client_socket_descriptor = accept(server_fd, (struct sockaddr *)&client_address, (socklen_t*)&addrlen)) == -1){
			printf("Accept statement failed\r\n");
		}	
		char buf[INET_ADDRSTRLEN];
		if (inet_ntop(AF_INET, &client_address.sin_addr, buf, sizeof(buf)) != NULL) {
			printf("Accepted Connection from %s\r\n", buf);
			syslog(LOG_INFO, "Accespted connection from %s", buf);
		}
		else {
			perror("inet_ntop");
			exit(EXIT_FAILURE);
		}

		//Create the new threading information
		pthread_t client_thread;
		node new_thread_node = (node)malloc(sizeof(struct LinkedList)); // allocate memory using malloc()

		new_thread_node->next = NULL;// make next point to NULL
		new_thread_node->thread_data.client_thread_socket = client_socket_descriptor;

		if(pthread_create(&client_thread, NULL, connection_handler, (void*) new_thread_node) < 0) {
			perror("could not create thread");
			exit(EXIT_FAILURE);
		}
		else{
			addNode(pthread_linked_list, client_thread, new_thread_node);
		}
		
		//THREAD MEMORY AND LINKED LIST CLEANUP
		node previous;
		node temp = pthread_linked_list;
		
		//if the first node needs to be removed
		if ( temp != NULL && temp->thread_data.thread_complete_flag == 1 ){
			pthread_join(temp->thread_data.threadID, NULL);
			pthread_linked_list = temp->next;
			free(temp);
		}
		while(temp != NULL){
			previous = temp;
			if(temp->next != NULL && temp->next->thread_data.thread_complete_flag == 1){
				pthread_join(temp->next->thread_data.threadID, NULL);
				previous->next = temp->next->next;
				free(temp->next);
			}
		}

		if (inet_ntop(AF_INET, &client_address.sin_addr, buf, sizeof(buf)) != NULL) {
			printf("Closed Connection from %s\r\n", buf);
			syslog(LOG_INFO, "Closed connection from %s", buf);
		}
	}

	//Handle the shutdown and clean up all threads
	node p;
	p = pthread_linked_list;

	while(p != NULL){
		pthread_join(p->thread_data.threadID, NULL);
		p = p->next;
	}

	deleteList(pthread_linked_list);
#ifndef USE_AESD_CHAR_DEVICE
	timer_delete(timer);
	remove("/var/tmp/aesdsocketdata");
#endif
	close(server_fd);
	exit(0);
}

void *connection_handler(void *thread_node) {

	node self_node = (node)thread_node;
	//Receive from the client
	int client_socket = self_node->thread_data.client_thread_socket;//*(int*)thread_socket;

	char client_message[1024] = {0};
	bzero(client_message, 1024);
	//fseek(fp, 0, SEEK_END);
	//int fileSize = ftell(fp);
	//printf("filesize:%d\r\n", fileSize);
	ssize_t read_size;
	//uint8_t file_open_state = 0;
	//uint8_t file_write_state = 0;
	uint8_t seek_state = 0;
#ifdef USE_AESD_CHAR_DEVICE
	FILE *fp = fopen("/dev/aesdchar", "a+");
#else
	FILE *fp = fopen("/var/tmp/aesdsocketdata", "a+");
#endif
	while( (read_size = read(client_socket, client_message, 1024)) ){
		printf("Client message:%s\r\n", client_message);
		if(strstr(client_message, "AESDCHAR_IOCSEEKTO:") != NULL){
			fclose(fp);
			fp = fopen("/dev/aesdchar", "r");
			seek_state = 1;
			char* tokenized_message = strtok(client_message, ":");
			tokenized_message = strtok(NULL, ":");
			//printf("tokenized_message:%s\r\n", tokenized_message);
			uint32_t write_cmd = atoi(strtok(tokenized_message, ","));	
			//printf("write_cmd:%d\r\n", write_cmd);
			uint32_t offset = atoi(strtok(NULL, ","));	
			//printf("offset:%d\r\n", offset);
			struct aesd_seekto seekto;
			seekto.write_cmd = write_cmd;
			seekto.write_cmd_offset = offset;
			int fd = fileno(fp);
			int result_ret = ioctl(fd, AESDCHAR_IOCSEEKTO, &seekto);
			printf("result of ioctl:%d\r\n", result_ret);
			//fclose(fp);
			//close(client_socket);
			//let the main know the thread has completed
			//self_node->thread_data.thread_complete_flag = 1;
			//return NULL;
			break;
		}
		char* newLineCharPosition = strchr(client_message, '\n');
		if (newLineCharPosition == NULL){
			printf("made it\r\n");
#ifdef USE_AESD_CHAR_DEVICE
			fputs(client_message, fp);
#else
			pthread_mutex_lock(&count_mutex);
			fputs(client_message, fp);
			pthread_mutex_unlock(&count_mutex);
#endif
		}
		else{
			int index = newLineCharPosition-client_message;
			char finalClientMessage[index+2];
			bzero(finalClientMessage, index+2);
			strncpy(finalClientMessage, client_message, index+1);
			printf("finalClienMessage:%s\r\n", finalClientMessage);
			printf("strlen final msg:%ld\r\n", strlen(finalClientMessage));

#ifdef USE_AESD_CHAR_DEVICE
			fwrite(finalClientMessage, 1, strlen(finalClientMessage), fp);
			//fputs(finalClientMessage, fp);
#else
			pthread_mutex_lock(&count_mutex);
			fputs(finalClientMessage, fp);
			pthread_mutex_unlock(&count_mutex);
#endif
			break;
		}
	}

	printf("READ SIZE:%ld\r\n", read_size);

	if (read_size == -1){
		printf("Client closed connection\r\n");
	}

	if(seek_state != 1){
		fclose(fp);
	}

	//Open the file to read from and determine how many bytes are in the file
#ifdef USE_AESD_CHAR_DEVICE
	printf("MADE IT TO READ\r\n");
	if(seek_state != 1){
		fp = fopen("/dev/aesdchar", "r");
	}
	char server_response;
	//size_t read_rtn;
	while(fread(&server_response, 1, 1, fp)){
		printf("Server response:%c\r\n", server_response);
		send(client_socket, (const void*)&server_response, sizeof(char), 0 );
	}
#else
	fp = fopen("/var/tmp/aesdsocketdata", "r");
	char *server_response = NULL;
	ssize_t read_rtn;
	size_t len = 0;
	read_rtn = getdelim(&server_response, &len, '\0', fp);
	printf("Server response:%s\r\n", server_response);
	if(read_rtn != strlen(server_response)){
		printf("Error in file read:%zu\r\n", read_rtn);
	}
	printf("strlen(server_response):%ld\r\n", strlen(server_response));
	int write_size = write(client_socket, server_response, strlen(server_response));
	if(write_size != strlen(server_response)){
		printf("Write size incorrect\r\n");
		exit(1);
	}
#endif
	//int fd = fileno(fp);
	//while( (read_rtn = read(fd, server_response, sizeof(server_response))) > 0){}
	fclose(fp);
	close(client_socket);
	//let the main know the thread has completed
	self_node->thread_data.thread_complete_flag = 1;
	return NULL;
}

//Using a handler to set a flag because you don't want to call a lock inside a signal handler,
//set a flag and let the thread handle it.
#ifndef USE_AESD_CHAR_DEVICE
void time_signal_handler(union sigval sv){
	time_signal_flag_set = true;
}	

void *time_thread_handler(void *args){
	while(1){
		while(time_signal_flag_set){
			time_t t; 
			struct tm *current_time; 
			char MY_TIME[50]; 
			time( &t );

			current_time = localtime( &t );

			// using strftime to display time 
			strftime(MY_TIME, sizeof(MY_TIME),"%a, %d %b %Y %T %z", current_time);
			char time_to_write[100];
			sprintf(time_to_write, "timestamp:%s\n", MY_TIME);
			printf("time to write:%s\r\n", time_to_write);

			FILE *fp = fopen("/var/tmp/aesdsocketdata", "a+");
			pthread_mutex_lock(&count_mutex);
			fputs(time_to_write, fp);
			pthread_mutex_unlock(&count_mutex);
			fclose(fp);
			time_signal_flag_set = false;
		}
	}
	return NULL;
}
#endif
