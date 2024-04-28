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
#include <sys/types.h>
#include <ctype.h>

#include "global.h"

/*
    Execution instructions:
    ./mobile_user {plafond} {max_requests} {delta_video} {delta_music} {delta_social} {data_ammount}
*/

// Struct to pass arguments to the requests threads
typedef struct{
    int delta;
    int data_ammount;
    char type[10];
} thread_args;

void sleep_milliseconds(int milliseconds);
int is_positive_integer(char *str);
int send_initial_request(int initial_plafond);
void *send_requests(void *args);
void signal_handler(int signal);
void clean_up();

pthread_t request_threads[3];
pthread_mutex_t max_requests_mutex = PTHREAD_MUTEX_INITIALIZER;
int requests_left;

int threads_should_exit = 0; // Flag to signal the threads to exit

int fd_user_pipe;

int main(int argc, char *argv[]){
    #ifdef DEBUG
    printf("DEBUG# Mobile user starting - USER ID: %d\n", getpid());
    printf("DEBUG# Redirecting SIGINT to signal handler\n");
    #endif
    signal(SIGINT, signal_handler);
    
    #ifdef DEBUG
    printf("DEBUG# Checking argument validity\n");
    #endif
    // Check correct number of arguments
    if (argc != 7){
        printf("<INCORRECT NUMBER OF ARGUMENTS>\n Correct usage: %s {plafond} {max_requests} {delta_video} {delta_music} {delta_social} {data_ammount}\n", argv[0]);
        return 1;
    }

    // Check if the arguments are all positive integers
    for(int i = 1; i < 7; i++){
        if(!is_positive_integer(argv[i])){
            printf("<ERROR> All arguments must be positive integers\n");
            return 1;
        }
    }

    #ifdef DEBUG
    printf("DEBUG# Saving user data\n");
    #endif
    // Read the arguments
    // Variable to save in the shared memory
    int initial_plafond = atoi(argv[1]);

    // Variables to be saved locally
    requests_left = atoi(argv[2]);
    int delta_video = atoi(argv[3]);
    int delta_music = atoi(argv[4]);
    int delta_social = atoi(argv[5]);
    int data_ammount = atoi(argv[6]);

    char message[200];
    sprintf(message, "MOBILE USER STARTING - USER ID: %d\n PLAFOND: %d\n MAX REQUESTS: %d\n DELTA VIDEO: %d\n DELTA MUSIC: %d\n DELTA SOCIAL: %d\n DATA AMMOUNT: %d\n", getpid(), initial_plafond, requests_left, delta_video, delta_music, delta_social, data_ammount);
    
    #ifdef DEBUG
    printf("DEBUG# Opening user pipe\n");
    #endif
    fd_user_pipe = open(USER_PIPE, O_WRONLY);
    if(fd_user_pipe == -1){
        perror("<ERROR> Could not open user pipe\n");
        return 1;
    }

    // Send the initial request
    #ifdef DEBUG
    printf("DEBUG# Sending initial request to register user\n");
    #endif
    if(send_initial_request(initial_plafond) != 0){
        perror("<ERROR> Could not register user\n");
        close(fd_user_pipe); // Close the pipe
        exit(1); // Exit the program, the only thing that needs to be cleaned up is the pipe
    }

    // Create threads to send requests
    // thread_args arg;
    // arg.data_ammount = data_ammount;
    // arg.max_requests = max_requests;

    // #ifdef DEBUG
    // printf("DEBUG# Creating video thread\n");
    // #endif
    // arg.delta = delta_video;
    // strcpy(arg.type, "VIDEO");
    // pthread_create(&video_thread, NULL, send_requests, (void*)&arg);

    // #ifdef DEBUG
    // printf("DEBUG# Creating music thread\n");
    // #endif
    // arg.delta = delta_music;
    // strcpy(arg.type, "MUSIC");
    // pthread_create(&music_thread, NULL, send_requests, (void*)&arg);

    // #ifdef DEBUG
    // printf("DEBUG# Creating social thread\n");
    // #endif
    // sleep(1);
    // arg.delta = delta_social;
    // strcpy(arg.type, "SOCIAL");
    
    // pthread_create(&social_thread, NULL, send_requests, (void*)&arg);
    char *types[3] = {"VIDEO", "MUSIC", "SOCIAL"};
    int deltas[3] = {delta_video, delta_music, delta_social};
    for(int i = 0; i < 3; i++){
        thread_args *arg = (thread_args*) malloc(sizeof(thread_args));
        arg->data_ammount = data_ammount;
        arg->delta = deltas[i];
        strcpy(arg->type, types[i]);
        pthread_create(&request_threads[i], NULL, send_requests, (void*)arg);
    }

    for(int i = 0; i < 3; i++){
        pthread_join(request_threads[i], NULL);
    }

    signal_handler(SIGINT);

    printf("%s", message);  

    return 0; 
}

void sleep_milliseconds(int milliseconds){
    struct timespec ts;
    // Get time in seconds
    ts.tv_sec = milliseconds / 1000;
    // Get the remaining milliseconds and convert them to nanoseconds
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    // Sleep for the specified time
    nanosleep(&ts, NULL);
}

int is_positive_integer(char *str) {
    while (*str) {
        // idigit is a function that checks if a character is a digit
        if (isdigit(*str) == 0) {
            return 0;
        }
        str++;
    }
    return 1;
}

int send_initial_request(int initial_plafond){
    #ifdef DEBUG
    printf("DEBUG# Sending initial request to register user\n");
    #endif

    char message[PIPE_BUFFER_SIZE];
    sprintf(message, "%d#%d", getpid(), initial_plafond);
    write(fd_user_pipe, message, strlen(message) + 1);

    // IMPLEMENTAR MAIS TARDE
    // Esperar por uma mensagem da message queue, caso seja aceite, devolve 1, caso contrário devolve 0

    #ifdef DEBUG
    printf("DEBUG# The user was registered, now proceeding\n");
    #endif
    return 0;
}

void *send_requests(void *arg){
    thread_args *args = (thread_args*) arg;
    char message[PIPE_BUFFER_SIZE];

    int delta = args->delta;
    #ifdef SLOWMOTION
    delta *= SLOWMOTION;
    #endif
    int data_ammount = args->data_ammount;
    char type[10];
    strcpy(type, args->type);

    printf("Thread %s starting, delta: %d, data ammount: %d\n", type, delta, data_ammount);

    while(!threads_should_exit){
        pthread_mutex_lock(&max_requests_mutex);
        if(requests_left > 0){
            #ifdef DEBUG
            printf("DEBUG# There are %d requests left\n", requests_left);
            #endif
            pthread_mutex_unlock(&max_requests_mutex);
            requests_left--; 
        }
        else{
            pthread_mutex_unlock(&max_requests_mutex);
            break;
        }

        #ifdef DEBUG
        printf("DEBUG# Thread %s sending request\n", type);
        #endif
        sprintf(message, "%d#%s#%d", getpid(), type, data_ammount);

        write(fd_user_pipe, message, strlen(message) + 1);
        
        // Sleep for delta milliseconds
        sleep_milliseconds(delta);
    }

    free(args);
    return NULL;
}

void signal_handler(int signal){
    if(signal == SIGINT){
        printf("<SIGNAL> SIGINT received\n");
        clean_up();
        exit(0);
    }
}

void clean_up(){
    // Send a message to remove the user from the shared memory [IMPLEMENT LATER]

    threads_should_exit = 1; 

    // Wait for the threads to exit
    #ifdef DEBUG
    printf("DEBUG# Waiting for threads to exit\n");
    #endif
    for(int i = 0; i < 3; i++){
        pthread_join(request_threads[i], NULL);
    }

    #ifdef DEBUG
    printf("DEBUG# Closing user pipe\n");
    #endif
    close(fd_user_pipe);

    #ifdef DEBUG
    printf("DEBUG# Destroying max_requests_mutex\n");
    #endif
    // Destroy the mutex
    pthread_mutex_destroy(&max_requests_mutex);


}