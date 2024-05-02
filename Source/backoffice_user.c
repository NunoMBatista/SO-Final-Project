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

#include "global.h"

#define BACKOFFICE_SEMAPHORE "backoffice_semaphore"

/*
    Execution instructions:
    ./backoffice_user
*/

sem_t *backoffice_semaphore;

void signal_handler(int signal);
void clean_up();
void interpret_command(char *command);
void send_message(char *message);
int check_other_backoffice_users(); // PERGUNTAR AO PROFESSOR SE VALE A PENA

int fd_back_pipe;

int main(){
    #ifdef DEBUG
    printf("DEBUG# Backoffice user - PROCESS ID: %d\n", getpid());
    printf("DEBUG# Redirecting SIGINT to signal handler\n");
    #endif
    signal(SIGINT, signal_handler);
    
    // Create a lockfile to prevent multiple backoffice users
    int lockfile = open(BACKOFFICE_LOCKFILE, O_RDWR | O_CREAT, 0640); 
    if(lockfile == -1){
        perror("<ERROR> Could not create lockfile\n");
        return 1;
    }
    // Try to lock the file
    if(lockf(lockfile, F_TLOCK, 0) == -1){
        printf("!!! THERE CAN ONLY BE ONE BACKOFFICE USER !!!\n");
        return 1;
    }    
    
    char message[100];
    sprintf(message, "BACKOFFICE USER STARTING - PROCESS ID: %d", getpid());
    printf("%s\n", message);

    // #ifdef DEBUG
    // printf("DEBUG# Checking if another backoffice user is running\n");
    // #endif
    // if(check_other_backoffice_users() == 1){
    //     printf("<ERROR> Another backoffice user is already running\n");
    //     return 1;
    // }

    #ifdef DEBUG
    printf("DEBUG# Trying to open backoffice_semaphore\n");
    #endif
    sem_unlink(BACKOFFICE_SEMAPHORE);
    sem_close(backoffice_semaphore);
    if((backoffice_semaphore = sem_open(BACKOFFICE_SEMAPHORE, O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED){
        perror("<ERROR> Could not create semaphore\n");
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

int check_other_backoffice_users(){
    // Even though the semaphore is not used as a synchronization device, it is created to prevent multiple backoffice users
    backoffice_semaphore = sem_open(BACKOFFICE_SEMAPHORE, O_CREAT | O_EXCL, 0666, 1);
    // Handle every error except the one that indicates that the semaphore already exists
    if((backoffice_semaphore == SEM_FAILED) && (errno == EEXIST)){
        perror("<ERROR> Could not create semaphore\n");
        exit(1);
    }
    // Check if semaphore already exists
    if(errno == EEXIST){ 
        return 1;
    }
    return 0;
}
    
void signal_handler(int signal){
    if(signal == SIGINT){
        printf("\tExiting backoffice user interface\n");
        clean_up();
        exit(0);
    }
}

void clean_up(){
    sem_close(backoffice_semaphore);
    sem_unlink(BACKOFFICE_SEMAPHORE);
}