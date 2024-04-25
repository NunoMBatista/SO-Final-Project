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
    shm_id = shmget(IPC_PRIVATE, sizeof(MobileUserData) * config->MOBILE_USERS, IPC_CREAT | 0777);
    if(shm_id == -1){
        write_to_log("<ERROR CREATING SHARED MEMORY>");
        return 1;
    }

    // Attach shared memory to shared_memory pointer
    shared_memory = (MobileUserData*) shmat(shm_id, NULL, 0);
    if(shared_memory == (void*) -1){
        write_to_log("<ERROR ATTACHING SHARED MEMORY>");
        return 1;
    }

    for(int i = 0; i < config->MOBILE_USERS; i++){
        shared_memory[i].isActive = 0;
    }

    #ifdef DEBUG
    printf("DEBUG# Shared memory created successfully\n");
    #endif

    return 0;
}

int create_semaphores(){
    #ifdef DEBUG
    printf("DEBUG# Creating semaphores...\n");
    #endif

    // Clean up sempahores if they already exist
    sem_close(log_semaphore);
    sem_unlink(LOG_SEMAPHORE);
    sem_close(shared_memory_sem);
    sem_unlink(SHARED_MEMORY_SEMAPHORE);

    #ifdef DEBUG
    printf("DEBUG# Creating log semaphore...\n");
    #endif

    // Create log semaphore
    log_semaphore = sem_open(LOG_SEMAPHORE, O_CREAT | O_EXCL, 0777, 1);
    if(log_semaphore == SEM_FAILED){
        write_to_log("<ERROR CREATING LOG SEMAPHORE>");
        return 1;
    }

    #ifdef DEBUG
    printf("DEBUG# Log semaphore created successfully\n");
    printf("DEBUG# Creating shared memory semaphore...\n");
    #endif

    // Create shared memory semaphore
    shared_memory_sem = sem_open(SHARED_MEMORY_SEMAPHORE, O_CREAT | O_EXCL, 0777, 1);
    if(shared_memory_sem == SEM_FAILED){
        write_to_log("<ERROR CREATING SHARED MEMORY SEMAPHORE>");
        return 1;
    }

    #ifdef DEBUG
    printf("DEBUG# Shared memory semaphore created successfully\n");
    #endif

    return 0;
}

int add_mobile_user(int user_id, int plafond){
    #ifdef DEBUG
    printf("DEBUG# Adding user %d to shared memory\n", user_id);
    #endif
    
    sem_wait(shared_memory_sem); // Lock shared memory semaphore

    for(int i = 0; i <= config->MOBILE_USERS; i++){

        // If the loop reaches the end of the shared memory, it means it's full
        if(i == config->MOBILE_USERS){
            write_to_log("<ERROR ADDING USER TO SHARED MEMORY> Shared memory full");
            return 1;
        }

        if(shared_memory[i].isActive == 0){
            // Check if there's a user with the same id already in the shared memory
            if(shared_memory[i].user_id == user_id){ 
                write_to_log("<ERROR ADDING USER TO SHARED MEMORY> User already exists");
                return 1;
            }

            shared_memory[i].isActive = 1; // Set user as active
            shared_memory[i].user_id = user_id;
            shared_memory[i].plafond = plafond;
            break;
        }
    }
    
    sem_post(shared_memory_sem); // Unlock shared memory semaphore

    #ifdef DEBUG
    printf("DEBUG# User %d added to shared memory\n", user_id);
    #endif

    char message[100];
    sprintf(message, "USER %d ADDED TO SHARED MEMORY", user_id);
    write_to_log(message);

    return 0;
}

void print_shared_memory(){
    #ifdef DEBUG
    printf("DEBUG# Printing current state of the shared memory...\n");
    #endif

    printf("Shared memory:\n");
    for(int i = 0; i < config->MOBILE_USERS; i++){
        if(shared_memory[i].isActive == 1){
            printf("\tUser %d has plafond: %d\n", shared_memory[i].user_id, shared_memory[i].plafond);
        }
    }
}

void clean_up(){
    if(shared_memory != NULL){    
        // Detach and delete shared memory
        shmdt(shared_memory);
        shmctl(shm_id, IPC_RMID, NULL);
    }

    // Free config memory
    if(config != NULL){
        free(config);
    }
    
    write_to_log("5G_AUTH_PLATFORM SIMULATOR CLOSING");

    // Close and unlink semaphores
    sem_close(log_semaphore);
    sem_unlink(LOG_SEMAPHORE);
    sem_close(shared_memory_sem);
    sem_unlink(SHARED_MEMORY_SEMAPHORE);
}

void signal_handler(int signal){
    if(signal == SIGINT){
        write_to_log("SIGNAL SIGINT RECEIVED");
        clean_up();
        exit(0);
    }
}

int initialize_system(char* config_file){
    #ifdef DEBUG
    printf("DEBUG# Initializing system...\n");
    #endif

    // Create semaphores
    if(create_semaphores() != 0){
        return 1;
    }

    // Read the config file
    if(read_config_file(config_file) != 0){
        return 1;
    }

    // Create monitor engine
    if(create_monitor_engine() != 0){
        return 1;
    }

    // Create auth manager
    if(create_auth_manager() != 0){
        return 1;
    }

    // Create shared memory
    if(create_shared_memory() != 0){
        return 1;
    }

    #ifdef DEBUG
    printf("DEBUG# System initialized successfully\n");
    #endif

    return 0;

}