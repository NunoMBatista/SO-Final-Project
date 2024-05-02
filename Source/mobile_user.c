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
#include <sys/msg.h>

#include "global.h"

#define EXIT_MESSAGE "DIE"

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
int initial_plafond; // Variable to save the initial plafond

void *send_requests(void *args);
void *message_receiver();
void print_arguments(int initial_plafond, int requests_left, int delta_video, int delta_music, int delta_social, int data_ammount);
void signal_handler(int signal);
void clean_up();


pthread_t request_threads[3];
//pthread_mutex_t gen_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t exit_signal_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t exit_signal = PTHREAD_COND_INITIALIZER;

pthread_t message_thread;
int started_threads = 0; // This value has to be 4 to start the threads (3 senders + 1 message receiver)

int requests_left;

int threads_should_exit = 0; // Flag to signal the threads to exit

int fd_user_pipe;
int user_msq_id;



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
    initial_plafond = atoi(argv[1]);

    // Variables to be saved locally
    requests_left = atoi(argv[2]);
    int delta_video = atoi(argv[3]);
    int delta_music = atoi(argv[4]);
    int delta_social = atoi(argv[5]);
    int data_ammount = atoi(argv[6]);
    
    print_arguments(initial_plafond, requests_left, delta_video, delta_music, delta_social, data_ammount);
    printf("Waiting for the system to accept registration request...\n");

    #ifdef DEBUG
    printf("DEBUG# Creating user message queue\n");
    #endif
    key_t queue_key = ftok(MESSAGE_QUEUE_KEY, 'a'); 
    if((user_msq_id = msgget(queue_key, 0777)) == -1){
        perror("<ERROR> Could not create message queue\n");
        return 1;
    }
    
    #ifdef DEBUG
    printf("DEBUG# Opening user pipe\n");
    #endif
    fd_user_pipe = open(USER_PIPE, O_WRONLY);
    if(fd_user_pipe == -1){
        perror("<ERROR> Could not open user pipe\n");
        return 1;
    }

    // Send the initial request to register the user
    if(send_initial_request(initial_plafond) != 0){
        perror("<ERROR> Could not register user\n");
        close(fd_user_pipe); // Close the pipe
        exit(1); // Exit the program, the only thing that needs to be cleaned up is the pipe
    }

    #ifdef DEBUG
    printf("DEBUG# Waiting for the initial request to be accepted\n");
    #endif
    QueueMessage qmsg;
    // Get messages with type equal to the user's pid
    if(msgrcv(user_msq_id, &qmsg, sizeof(QueueMessage), getpid(), 0) == -1){
        perror("<ERROR> Could not receive message from message queue\n");
        close(fd_user_pipe); // Close the pipe
        exit(1); // Exit the program, the only thing that needs to be cleaned up is the pipe
    }
    #ifdef DEBUG
    printf("DEBUG# Initial request response received: %s\n", qmsg.text);
    #endif
    if(qmsg.text[0] == 'R'){
        printf("<ERROR> The user was not accepted\n");
        
        pthread_mutex_lock(&exit_signal_mutex);

        threads_should_exit = 1;

        pthread_cond_signal(&exit_signal);
        pthread_mutex_unlock(&exit_signal_mutex);

        exit(1);
    }

    // Create a thread to continuously receive messages from the message queue
    pthread_create(&message_thread, NULL, message_receiver, NULL);

    printf("\n\n!!! USER ACCEPTED AND MESSAGE THREAD ACTIVE, START SENDING AUTH REQUESTS!!!\n\n");

    char *types[3] = {"VIDEO", "MUSIC", "SOCIAL"};
    int deltas[3] = {delta_video, delta_music, delta_social};
    for(int i = 0; i < 3; i++){
        thread_args *arg = (thread_args*) malloc(sizeof(thread_args));

        arg->data_ammount = data_ammount; // Pass the data amount per request
        arg->delta = deltas[i]; // Pass the delta time between each type of request
        strcpy(arg->type, types[i]); // Pass the type of request

        pthread_create(&request_threads[i], NULL, send_requests, (void*)arg);
    }

    #ifdef DEBUG
    printf("DEBUG# Waiting for message thread to exit\n");
    #endif
    // Wait for the exit signal
    pthread_mutex_lock(&exit_signal_mutex);
    while((!threads_should_exit) && (requests_left > 0)){
        pthread_cond_signal(&exit_signal);
        printf("\nLEBRON LOOP\n");
        pthread_cond_wait(&exit_signal, &exit_signal_mutex);
    }
    threads_should_exit = 1;
    pthread_mutex_unlock(&exit_signal_mutex);
    
    printf("\n\nLEBRON FREE\n\n");

    clean_up(); 
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

    #ifdef DEBUG
    printf("<%s SENDER>DEBUG# Starting, delta: %d, data ammount: %d\n", type, delta, data_ammount);
    #endif

    // threads_should_exit does not need to be checked in mutual exclusion because reading and writing ints is an atomic operation
    while(1){
        // Check if theres a signal to exit

        pthread_mutex_lock(&exit_signal_mutex); // Lock
        
        printf("<%s SENDER>DEBUG# THREADS SHOULD EXIT = %d\n", type, threads_should_exit);

        if(threads_should_exit == 1){
            pthread_mutex_unlock(&exit_signal_mutex);
            break;
        }

        #ifdef DEBUG
        printf("DEBUG# There are %d requests left\n", requests_left);
        #endif

        requests_left--; 

        #ifdef DEBUG
        printf("<%s SENDER>DEBUG# Thread sending request\n", type);
        #endif
        sprintf(message, "%d#%s#%d", getpid(), type, data_ammount);

        printf("\t(>>) Sending %s!\n", message);
        write(fd_user_pipe, message, PIPE_BUFFER_SIZE);
        
        // Notify monitor to check exit condition
        pthread_cond_signal(&exit_signal);
        pthread_mutex_unlock(&exit_signal_mutex); // Unlock

        // Sleep for delta milliseconds
        sleep_milliseconds(delta);
    }

    free(args);
    printf("%s SENDER THREAD EXITING\n", type);
    return NULL;
}

void *message_receiver(){
    // Message queue message
    QueueMessage qmsg;

    #ifdef DEBUG
    printf("<MESSAGE THREAD>DEBUG# Message thread started\n");
    #endif

    while(1){
        // Check if theres a signal to exit in mutual exclusion
        pthread_mutex_lock(&exit_signal_mutex);
        if(threads_should_exit){
            pthread_mutex_unlock(&exit_signal_mutex);
            break;
        }
        pthread_mutex_unlock(&exit_signal_mutex);

        #ifdef DEBUG
        printf("<MESSAGE THREAD>DEBUG# Waiting for message from message queue\n");
        #endif
        // Get messages with type equal to the user's pid
        if(msgrcv(user_msq_id, &qmsg, sizeof(QueueMessage), getpid(), 0) == -1){
            perror("<ERROR> Could not receive message from message queue<\n");
            
            // Signal threads to exit 
            pthread_mutex_lock(&exit_signal_mutex);

            threads_should_exit = 1;

            pthread_cond_signal(&exit_signal);
            pthread_mutex_unlock(&exit_signal_mutex);
        }

        // If the message is a number
        if(atoi(qmsg.text) != 0){
            printf("\n\n\t!!! THE USER %d HAS SPENT %d%% OF THE PLAFOND !!!\n\n\n", getpid(), atoi(qmsg.text));
        }

        if((atoi(qmsg.text) == 100) || (strcmp(qmsg.text, EXIT_MESSAGE) == 0)){
            #ifdef DEBUG
            printf("<MESSAGE THREAD>DEBUG# Received exit message\n");
            #endif
            break;
        }    
    }

    // Signal the threads to exit in mutual exclusion
    pthread_mutex_lock(&exit_signal_mutex);

    threads_should_exit = 1;

    pthread_cond_signal(&exit_signal); // Notify the monitor thread
    pthread_mutex_unlock(&exit_signal_mutex);

    printf("MESSAGE THREAD EXITING\n");

    return NULL;
}

void print_arguments(int initial_plafond, int requests_left, int delta_video, int delta_music, int delta_social, int data_ammount) {
    printf("\n");
    printf("************************************\n");
    printf("* Mobile User Data                 *\n");
    printf("************************************\n");
    printf("* Initial Plafond: %15d *\n", initial_plafond);
    printf("* Requests Left:   %15d *\n", requests_left);
    printf("* Delta Video:     %15d *\n", delta_video);
    printf("* Delta Music:     %15d *\n", delta_music);
    printf("* Delta Social:    %15d *\n", delta_social);
    printf("* Data Amount:     %15d *\n", data_ammount);
    printf("************************************\n");
    printf("\n");
}

void clean_up(){
    #ifdef DEBUG
    printf("DEBUG# Signaling the threads to exit in mutual exclusion\n");
    #endif

    // Send a message to the message queue to signal the message thread to exit
    #ifdef DEBUG
    printf("DEBUG# Sending message to message queue\n");
    #endif
    QueueMessage qmsg;
    qmsg.type = getpid();
    strcpy(qmsg.text, EXIT_MESSAGE);
    if(msgsnd(user_msq_id, &qmsg, sizeof(QueueMessage), 0) == -1){
        perror("<ERROR> Could not send message to message queue\n");
        threads_should_exit = 1;
    }

    // Send a message to remove the user from the shared memory [IMPLEMENT LATER]
    char kill_message[PIPE_BUFFER_SIZE];
    // This forces the user to be removed lmfao
    sprintf(kill_message, "%d#KILL", getpid(), initial_plafond + 1);
    write(fd_user_pipe, kill_message, PIPE_BUFFER_SIZE);

    // Wait for the threads to exit
    #ifdef DEBUG
    printf("DEBUG# Waiting for threads to exit\n");
    #endif
    for(int i = 0; i < 3; i++){
        pthread_join(request_threads[i], NULL);
    }
    pthread_join(message_thread, NULL);

    #ifdef DEBUG
    printf("DEBUG# Closing user pipe\n");
    #endif

    // Close pipes
    if(close(fd_user_pipe) == -1){
        perror("<ERROR> Could not close user pipe\n");
    }

    // NO NEED TO REMOVE MESSAGE QUEUE

    #ifdef DEBUG
    printf("DEBUG# Destroying exit_signal_mutex\n");
    #endif
    // Destroy the mutex
    pthread_mutex_destroy(&exit_signal_mutex);

    #ifdef DEBUG
    printf("DEBUG# Destroying exit_signal condition variable\n");
    #endif
    // Destroy the cond var
    pthread_cond_destroy(&exit_signal);

    #ifdef DEBUG
    printf("DEBUG# Exiting\n");
    #endif
}

void signal_handler(int signal){
    if(signal == SIGINT){
        printf("<SIGNAL> SIGINT received\n");
        

        pthread_mutex_lock(&exit_signal_mutex);

        threads_should_exit = 1;

        pthread_cond_signal(&exit_signal);
        pthread_mutex_unlock(&exit_signal_mutex);


    }
}
