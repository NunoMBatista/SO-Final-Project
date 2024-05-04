/*
  Authors: 
    Nuno Batista uc2022216127
    Miguel Castela uc2022212972
*/
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <semaphore.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/msg.h>
#include <sys/ipc.h>

#include "global.h"

#define BACKOFFICE_SEMAPHORE "backoffice_semaphore"

/*
    Execution instructions:
    ./backoffice_user
*/

void signal_handler(int signal);
void clean_up();
void interpret_command(char *command);
void send_message(char *message);
void *receiver();
void print_statistics(char *message);

int fd_back_pipe;
int back_msq_id;

pthread_t receiver_thread;

int main(){
    // Check if the backoffice user can create the main lockfile
    int lockfile = open(MAIN_LOCKFILE, O_RDWR | O_CREAT, 0640);
    if (lockfile == -1){
        perror("open");
        return 1;
    }
    // If the lockfile was successfully locked, the system is not online
    if(lockf(lockfile, F_TLOCK, 0) == 0){ // The lock was successfully apllied
        printf("\033[31m!!! THE SYSTEM IS OFFLINE !!!\n\033[0m");
        return 1;
    }

    // Create a lockfile to prevent multiple backoffice users
    lockfile = open(BACKOFFICE_LOCKFILE, O_RDWR | O_CREAT, 0640); 
    if(lockfile == -1){
        perror("<ERROR> Could not create lockfile\n");
        return 1;
    }
    // Try to lock the file
    if(lockf(lockfile, F_TLOCK, 0) == -1){
        printf("!!! THERE CAN ONLY BE ONE BACKOFFICE USER !!!\n");
        return 1;
    }    

    #ifdef DEBUG
    printf("DEBUG# Backoffice user - PROCESS ID: %d\n", getpid());
    printf("DEBUG# Redirecting SIGINT to signal handler\n");
    #endif
    signal(SIGINT, signal_handler);
    
    char message[100];
    sprintf(message, "BACKOFFICE USER STARTING - PROCESS ID: %d", getpid());
    printf("%s\n", message);

    #ifdef DEBUG
    printf("DEBUG# Creating receiver thread");
    #endif
    // Create a thread to receive messages from the system
    if(pthread_create(&receiver_thread, NULL, receiver, NULL) != 0){
        perror("<ERROR> Could not create receiver thread\n");
        return 1;
    }

    #ifdef DEBUG
    printf("DEBUG# Opening back pipe\n");
    #endif
    if((fd_back_pipe = open(BACK_PIPE, O_WRONLY)) < 0){
        perror("<ERROR> Could not open back pipe\n");
        return 1;
    }
    
    printf("\t-> Welcome to the backoffice user interface <-\n");
    printf("\n\n\t -> data_stats - Gets consumption statistics\n\t -> reset - Resets the statistics\n\t -> exit - Exits the backoffice user interface\n\n\n");
    // Wait for a command 
    char command[100];
    while(1){
        printf("\n$ ");

        if(fgets(command, 100, stdin) == NULL){
            perror("<ERROR> Could not read command\n");
            return 1;
        }

        // Remove newline character
        if(command[strlen(command) - 1] == '\n'){
            command[strlen(command) - 1] = '\0';
        }
        
        interpret_command(command);
    }
}

void *receiver(){
    key_t queue_key = ftok(MESSAGE_QUEUE_KEY, 'a');
    if((back_msq_id = msgget(queue_key, 0777)) == -1){
        perror("<ERROR> Could not get message queue\n");
        return NULL;
    }

    #ifdef DEBUG
    printf("DEBUG# Receiver thread started\n");
    #endif
  
    char message_copy[PIPE_BUFFER_SIZE];
    QueueMessage qmsg;

    while(1){
        if(msgrcv(back_msq_id, &qmsg, sizeof(QueueMessage), 1, 0) == -1){
            perror("<ERROR> Could not receive message\n");
            return NULL;
        }

        strcpy(message_copy, qmsg.text);

        char *token = strtok(message_copy, "#");

        if(strcmp(token, "SHM") == 0){
            print_statistics(qmsg.text);
        }
        

    }

}

void print_statistics(char *message){
    int spent_video, spent_music, spent_social;
    int reqs_video, reqs_music, reqs_social;

    printf("Received message: %s\n", message);

    // Token 1 is the shared memory type key
    char *token = strtok(message, "#");

    // Token 2 the amount of data spent on video
    token = strtok(NULL, "#");
    spent_video = atoi(token);

    // Token 3 is the amount of video requests
    token = strtok(NULL, "#");
    reqs_video = atoi(token);

    // Token 4 is the amount of data spent on music
    token = strtok(NULL, "#");
    spent_music = atoi(token);

    // Token 5 is the amount of music requests
    token = strtok(NULL, "#");
    reqs_music = atoi(token);

    // Token 6 is the amount data spent on social 
    token = strtok(NULL, "#");
    spent_social = atoi(token);

    // Token 7 is the amount of social requests
    token = strtok(NULL, "#");
    reqs_social = atoi(token);


    // Print everything
    printf("Video:\n\tData spent: %d\n\tRequests: %d\n", spent_video, reqs_video);
    printf("Music:\n\tData spent: %d\n\tRequests: %d\n", spent_music, reqs_music);
    printf("Social:\n\tData spent: %d\n\tRequests: %d\n\n", spent_social, reqs_social);

}

void interpret_command(char *command){
    char message[PIPE_BUFFER_SIZE] = "1#";

    if((strcmp(command, "data_stats") == 0) || strcmp(command, "reset") == 0){
        strcat(message, command);
        printf("about to send <%s>\n", message);
        send_message(message);
        return;
    }

    if(strcmp(command, "exit") == 0){
        printf("Exiting backoffice user interface...\n");
        clean_up();
        exit(0);
    }
    else{
        printf("Invalid command\n");
    }

}

void send_message(char *message){
    if(write(fd_back_pipe, message, strlen(message) + 1) < 0){
        perror("<ERROR> Could not write to back pipe\n");
        return;
    }
    printf("Message sent\n");
}
    
void signal_handler(int signal){
    if(signal == SIGINT){
        printf("\tExiting backoffice user interface\n");
        clean_up();
        exit(0);
    }
}

void clean_up(){

}               