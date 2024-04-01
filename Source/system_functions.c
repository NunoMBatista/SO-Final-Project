/*
  Authors: 
    Nuno Batista uc2022216127
    Miguel Castela uc2022212972
*/
#include <sys/shm.h>
#include <semaphore.h> // POSIX 
#include <fcntl.h> // O_CREAT, O_EXCL
#include <unistd.h> // fork
#include <stdio.h>
#include <stdlib.h> // exit
#include <sys/wait.h> // wait
#include <sys/types.h>
#include <semaphore.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include "global.h"

void write_to_log(char *message){
    // Check if there is any process using the log file
    sem_wait(log_semaphore); 

    // Open log file in append mode
    FILE* log_file = fopen("log.txt", "a");
    if(log_file == NULL){
        printf("<ERROR OPENING LOG FILE>\n");
        sem_post(log_semaphore);
        return;
    }

    time_t current_time;
    // Struct to store time information
    struct tm* time_info;

    // Get current time
    time(&current_time);

    // Get time information
    time_info = localtime(&current_time);

    char time_str[100];

    // Write message to log file
    strftime(time_str, 100, "%Y-%m-%d %H:%M:%S", time_info);

    // Print message to console
    printf("[%s] %s\n", time_str, message);

    // Print message to log file
    fprintf(log_file, "[%s] %s\n", time_str, message);
    fclose(log_file);

    // Unlock the log semaphore
    sem_post(log_semaphore);

    return;
}

// Return 0 if the config file was read successfully and has valid values
// Return 1 otherwise
int read_config_file(char* filename){
    FILE* config_file = fopen(filename, "r");
    if(config_file == NULL){
        write_to_log("<ERROR OPENING CONFIG FILE>");
        exit(1);
    }

    #ifdef DEBUG
    printf("Reading config file...\n");
    #endif

    // There should be 6 positive integers in the config file separated by \n
    int config_values = fscanf(config_file, "%d\n%d\n%d\n%d\n%d\n%d", &config->MOBILE_USERS, &config->QUEUE_POS, &config->AUTH_SERVERS, &config->AUTH_PROC_TIME, &config->MAX_VIDEO_WAIT, &config->MAX_OTHERS_WAIT);

    if(config_values != 6){
        write_to_log("<INCORRECT NUMBER OF CONFIG VALUES> There should be 6 configuration values");
        fclose(config_file);
        return 1;
    }

    // Check if there are more than 6 values
    int extra_values = 0;
    // Read until EOF
    while(fgetc(config_file) != EOF){
        extra_values++;
    }

    if(extra_values > 0){
        write_to_log("<EXTRA CONFIG VALUES> There are more than 6 configuration values in the config file, only the first 6 will be considered");
    }

    #ifdef DEBUG
    printf("Config values read successfully, checking validity...\n");
    #endif
    
    // Check if all values are positive integers
    if(config->MOBILE_USERS <= 0 || config->QUEUE_POS <= 0 || config->AUTH_SERVERS <= 0 || config->AUTH_PROC_TIME <= 0 || config->MAX_VIDEO_WAIT <= 0 || config->MAX_OTHERS_WAIT <= 0){
        write_to_log("<INVALID CONFIG VALUES> Every configuration value should be a positive integer");
        fclose(config_file);
        return 1;
    }

    #ifdef DEBUG
    printf("Config values read successfully:\n");
    printf("\tMOBILE_USERS: %d\n\tQUEUE_POS: %d\n\tAUTH_SERVERS: %d\n\tAUTH_PROC_TIME: %d\n\tMAX_VIDEO_WAIT: %d\n\tMAX_OTHERS_WAIT: %d\n",
           config->MOBILE_USERS, config->QUEUE_POS, config->AUTH_SERVERS, config->AUTH_PROC_TIME, config->MAX_VIDEO_WAIT, config->MAX_OTHERS_WAIT);
    #endif

    write_to_log("CONFIG FILE READ SUCCESSFULLY");

    return 0;
}

void* receiver_thread(){
    // Implement receiver thread

    #ifdef DEBUG
    printf("DEBUG# Receiver thread started\n");
    #endif

    write_to_log("THREAD RECEIVER CREATED");

    return NULL;
}

void* sender_thread(){
    // Implement sender thread

    #ifdef DEBUG
    printf("DEBUG# Sender thread started\n");
    #endif

    write_to_log("THREAD SENDER CREATED");

    return NULL;
}

int create_monitor_engine(){   
    // Create a new process
 
    #ifdef DEBUG
    printf("DEBUG# Creating monitor engine...\n");
    #endif
 
    pid_t pid = fork();

    if(pid == -1){
        write_to_log("<ERROR CREATING MONITOR ENGINE>");
        return 1;
    }

    if(pid == 0){
        // Child process
        write_to_log("PROCESS MONITOR_ENGINE CREATED");
        exit(0);
    }

    // Parent process
    return 0;
}

int create_auth_manager(){
    // Create a new process
    #ifdef DEBUG
    printf("DEBUG# Creating auth manager...\n");
    #endif

    pid_t pid = fork();

    if(pid == -1){
        write_to_log("<ERROR CREATING AUTH MANAGER>");
        return 1;
    }

    if(pid == 0){
        // Child process
        write_to_log("PROCESS AUTHORIZATION_REQUEST_MANAGER CREATED");

        // Create receiver and sender threads
        #ifdef DEBUG
        printf("DEBUG# Creating receiver and sender threads...\n");
        #endif

        pthread_t receiver, sender;
        pthread_create(&receiver, NULL, receiver_thread, NULL);
        pthread_create(&sender, NULL, sender_thread, NULL);

        // Wait for threads to finish
        pthread_join(receiver, NULL);
        pthread_join(sender, NULL);
        exit(0);
    }

    // Parent process
    return 0;
}   

int create_shared_memory(){
    #ifdef DEBUG
    printf("DEBUG# Creating shared memory...\n");
    #endif
    
    // Create new shared memory with size of SharedMemory struct and permissions 0777 (rwx-rwx-rwx)
    shm_id = shmget(IPC_PRIVATE, sizeof(SharedMemory), IPC_CREAT | 0777);
    if(shm_id == -1){
        write_to_log("<ERROR CREATING SHARED MEMORY>");
        return 1;
    }

    // Attach shared memory to shared_memory pointer
    shared_memory = (SharedMemory*) shmat(shm_id, NULL, 0);
    if(shared_memory == (void*) -1){
        write_to_log("<ERROR ATTACHING SHARED MEMORY>");
        return 1;
    }

    // Initialize shared memory values
    shared_memory->numUsers = 0;
    // Allocate memory for the mobile user data
    shared_memory->data = (MobileUserData*) malloc(config->MOBILE_USERS * sizeof(MobileUserData));

    #ifdef DEBUG
    printf("DEBUG# Shared memory created successfully\n");
    #endif

    return 0;
}

int add_mobile_user(int user_id, int plafond){
    // Check if the user is already in the shared memory
    for(int i = 0; i < shared_memory->numUsers; i++){
        if(shared_memory->data[i].user_id == user_id){
            write_to_log("<ERROR ADDING USER TO SHARED MEMORY> User already exists");
            return 1;
        }
    }

    // Check if the shared memory is full
    if(shared_memory->numUsers == config->MOBILE_USERS){
        write_to_log("<ERROR ADDING USER TO SHARED MEMORY> Shared memory full");
        return 1;
    }

    // Add user to shared memory 
    MobileUserData user_data;
    user_data.user_id = user_id;
    user_data.plafond = plafond;
    shared_memory->data[shared_memory->numUsers] = user_data;
    shared_memory->numUsers++;
    
    #ifdef DEBUG
    printf("DEBUG# User %d added to shared memory\n", user_id);
    #endif

    char message[100];
    sprintf(message, "USER %d ADDED TO SHARED MEMORY", user_id);
    write_to_log(message);

    return 0;
}

void print_shared_memory(){
    printf("Shared memory:\n");
    printf("\tNumber of users: %d\n", shared_memory->numUsers);
    for(int i = 0; i < shared_memory->numUsers; i++){
        printf("\tUser %d: %d\n", shared_memory->data[i].user_id, shared_memory->data[i].plafond);
    }
}

void clean_up(){
    if(shared_memory != NULL){    
        // Free shared memory
        free(shared_memory->data);
        // Detach and delete shared memory
        shmdt(shared_memory);
        // Delete shared memory
        shmctl(shm_id, IPC_RMID, NULL);
    }

    // Free config memory
    if(config != NULL){
        free(config);
    }
    
    write_to_log("5G_AUTH_PLATFORM SIMULATOR CLOSING");

    // Close and unlink log semaphore
    sem_close(log_semaphore);
    sem_unlink("log_semaphore");
}

void signal_handler(int signal){
    if(signal == SIGINT){
        write_to_log("SIGNAL SIGINT RECEIVED");
        clean_up();
        exit(0);
    }
}