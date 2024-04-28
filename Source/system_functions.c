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

#include "system_functions.h"
#include "global.h"

#define max(a, b) ((a) > (b) ? (a) : (b))

void write_to_log(char *message){
    // Wait if there is any process using the log file
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

int read_config_file(char *filename){
    // Return 0 if the config file was read successfully and has valid values
    // Return 1 otherwise

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
    printf("DEBUG#Config values are valid:\n");
    printf("\tMOBILE_USERS: %d\n\tQUEUE_POS: %d\n\tAUTH_SERVERS: %d\n\tAUTH_PROC_TIME: %d\n\tMAX_VIDEO_WAIT: %d\n\tMAX_OTHERS_WAIT: %d\n",
           config->MOBILE_USERS, config->QUEUE_POS, config->AUTH_SERVERS, config->AUTH_PROC_TIME, config->MAX_VIDEO_WAIT, config->MAX_OTHERS_WAIT);
    #endif

    write_to_log("CONFIG FILE READ SUCCESSFULLY");

    return 0;
}

void parse_and_send(char *message){
    // JUST SEND TO VIDEO FOR NOW, IMPLEMENT PARSING LATTER
    queue_message msg; 
    msg.priority = 1;
    strcpy(msg.messaage, message);

    #ifdef DEBUG
    printf("<RECEIVER>DEBUG# Sending message to video queue: %s\n", message);
    #endif
    if(msgsnd(video_queue_id, &msg, sizeof(msg), 0) == -1){
        write_to_log("<ERROR SENDING MESSAGE TO VIDEO QUEUE>");
        signal_handler(SIGINT);
    }
}

void* receiver_thread(){
    // Implement receiver thread
    #ifdef DEBUG
    printf("<RECEIVER>DEBUG# Receiver thread started\n");
    #endif
    write_to_log("THREAD RECEIVER CREATED");

    // Read from user and back pipes using select
    fd_set read_set;
    int max_fd = max(fd_user_pipe, fd_back_pipe);
    int read_bytes;

    while(1){
        char buffer[PIPE_BUFFER_SIZE];
        FD_ZERO(&read_set);
        FD_SET(fd_user_pipe, &read_set);
        FD_SET(fd_back_pipe, &read_set);

        if(select(max_fd + 1, &read_set, NULL, NULL, NULL) == -1){
            write_to_log("<ERROR SELECTING PIPES>");
            signal_handler(SIGINT);
        }

        if(FD_ISSET(fd_user_pipe, &read_set)){
            if((read_bytes = read(fd_user_pipe, buffer, PIPE_BUFFER_SIZE)) == -1){
                write_to_log("<ERROR READING FROM USER_PIPE>");
                signal_handler(SIGINT);
            }
            buffer[read_bytes - 1] = '\0';

            #ifdef DEBUG
            printf("<RECEIVER>DEBUG# Received message from USER_PIPE: %s\n", buffer);
            #endif
        }

        if(FD_ISSET(fd_back_pipe, &read_set)){
            if((read_bytes = read(fd_back_pipe, buffer, PIPE_BUFFER_SIZE)) == -1){
                write_to_log("<ERROR READING FROM BACK_PIPE>");
                signal_handler(SIGINT); 
            }
            #ifdef DEBUG
            printf("<RECEIVER>DEBUG# Received message from BACK_PIPE: %s\n", buffer);
            #endif
        }

        // Parse and send the message to one of the queues
        parse_and_send(buffer);

        printf("%s\n", buffer);

        #ifdef DEBUG    
        printf("<RECEIVER>DEBUG# Read %d bytes\n", read_bytes);
        #endif
    }

    return NULL;
}

void* sender_thread(){
    // Implement sender thread

    #ifdef DEBUG
    printf("<SENDER>DEBUG# Sender thread started\n");
    #endif
    write_to_log("THREAD SENDER CREATED");

    // JUST READ VIDEOQUEUE FOR NOW, IMPLEMENT THE OTHER QUEUE LATER
    queue_message msg;
    while(1){
        if(msgrcv(video_queue_id, &msg, sizeof(msg), 0, 0) == -1){
            write_to_log("<ERROR RECEIVING MESSAGE FROM VIDEO QUEUE>");
            signal_handler(SIGINT);
        }

        #ifdef DEBUG
        printf("<SENDER>DEBUG# Received message from video queue: %s\n", msg.messaage);
        #endif
    }

    return NULL;
}

int create_monitor_engine(){   
    // Create a new process
 
    #ifdef DEBUG
    printf("<ME>DEBUG# Creating monitor engine...\n");
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

int create_pipes(){
    #ifdef DEBUG
    printf("<ARM>DEBUG# Creating named pipes...\n");
    #endif
    // Unlink pipes if they already exist
    unlink(USER_PIPE);
    unlink(BACK_PIPE);        
    if(mkfifo(USER_PIPE, O_CREAT | O_EXCL | 0666) == -1){
        write_to_log("<ERROR CREATING USER PIPE>");
        exit(1);
    }
    if(mkfifo(BACK_PIPE, O_CREAT | O_EXCL | 0666) == -1){
        write_to_log("<ERROR CREATING BACK PIPE>");
        exit(1);
    }

    #ifdef DEBUG
    printf("<ARM>DEBUG# Opening pipes...\n");
    #endif
    if((fd_user_pipe = open(USER_PIPE, O_RDWR)) == -1){
        write_to_log("<ERROR OPENING USER PIPE>");
        exit(1);
    }
    if((fd_back_pipe = open(BACK_PIPE, O_RDWR)) == -1){
        write_to_log("<ERROR OPENING BACK PIPE>");
        exit(1);
    }

    #ifdef DEBUG
    printf("<ARM>DEBUG# Pipes created and opened successfully\n");
    #endif

    return 0;
}

int create_auth_manager(){
    pid_t pid = fork();

    if(pid == -1){
        write_to_log("<ERROR CREATING AUTH MANAGER>");
        return 1;
    }

    if(pid == 0){
        // Child process
        write_to_log("PROCESS AUTHORIZATION_REQUEST_MANAGER CREATED");

        create_pipes();
        create_message_queues();

        // Create receiver and sender threads
        #ifdef DEBUG
        printf("<ARM>DEBUG# Creating receiver and sender threads...\n");
        #endif
        pthread_t receiver, sender;
        pthread_create(&receiver, NULL, receiver_thread, NULL);
        pthread_create(&sender, NULL, sender_thread, NULL);

        #ifdef DEBUG
        printf("<ARM>DEBUG# Threads created successfully\n");
        #endif

        // Wait for threads to finish
        pthread_join(receiver, NULL);
        pthread_join(sender, NULL);
        exit(0);
    }

    // Parent process
    return 0;
}   

int create_shared_memory(){  
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
            shared_memory[i].initial_plafond = plafond;
            shared_memory[i].spent_plafond = 0;
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
            printf("\tUser %d has plafond: %d\n", shared_memory[i].user_id, shared_memory[i].initial_plafond - shared_memory[i].spent_plafond);
        }
    }
}

int create_message_queues(){
    #ifdef DEBUG
    printf("<ARM>DEBUG# Creating video queue...\n");
    #endif
    if((video_queue_id = msgget(IPC_PRIVATE, IPC_CREAT | 0777)) == -1){
        write_to_log("<ERROR CREATING VIDEO QUEUE>");
        return 1;
    }

    #ifdef DEBUG
    printf("<ARM>DEBUG# Creating other queue...\n");
    #endif
    if((other_queue_id = msgget(IPC_PRIVATE, IPC_CREAT | 0777)) == -1){
        write_to_log("<ERROR CREATING OTHER QUEUE>");
        return 1;
    }

    return 0; 
}

int initialize_system(char* config_file){
    #ifdef DEBUG
    printf("<SYS MAN>DEBUG# Initializing system...\n");
    #endif

    #ifdef DEBUG
    printf("<SYS MAN>DEBUG# Setting up signal handler...\n");
    #endif
    // Signal handling
    signal(SIGINT, signal_handler);
    
    // The semaphores need to be set up before any other feature because they are used to write to the log file
    #ifdef DEBUG
    printf("<SYS MAN>DEBUG# Creating semaphores...\n");
    #endif
    if(create_semaphores() != 0){
        return 1;
    }

    write_to_log("5G_AUTH_PLATFORM SIMULATOR STARTING");
    write_to_log("PROCESS SYSTEM_MANAGER CREATED");
    
    #ifdef DEBUG
    printf("<SYS MAN>DEBUG# Setting up config struct\n");
    #endif    
    // Allocate memory for the config struct
    config = (Config*) malloc(sizeof(Config));
    // Read the config file
    if(read_config_file(config_file) != 0){
        return 1;
    }

    #ifdef DEBUG
    printf("<SYS MAN>DEBUG# Creating monitor engine...\n");
    #endif
    if(create_monitor_engine() != 0){
        return 1;
    }

    #ifdef DEBUG
    printf("<SYS MAN>DEBUG# Creating auth manager...\n");
    #endif
    if(create_auth_manager() != 0){
        return 1;
    }

    #ifdef DEBUG
    printf("<SYS MAN>DEBUG# Creating shared memory...\n");
    #endif
    if(create_shared_memory() != 0){
        return 1;
    }

    #ifdef DEBUG
    printf("<SYS MAN>DEBUG# System initialized successfully\n");
    #endif

    return 0;
}

void clean_up(){
    write_to_log("5G_AUTH_PLATFORM SIMULATOR CLOSING");

    #ifdef DEBUG
    printf("<SYS MAN>DEBUG# Detatching and deleting the shared memory\n");
    #endif
    if(shared_memory != NULL){    
        // Detach and delete shared memory
        shmdt(shared_memory);
        shmctl(shm_id, IPC_RMID, NULL);
    }

    // Free config memory
    if(config != NULL){
        free(config);
    }

    #ifdef DEBUG
    printf("<SYS MAN>DEBUG# Closing and unlinking pipes\n");
    #endif
    // Close and unlink pipes
    close(fd_user_pipe);
    close(fd_back_pipe);
    unlink(USER_PIPE);
    unlink(BACK_PIPE);
    
    #ifdef DEBUG
    printf("<SYS MAN>DEBUG# Closing and unlinking semaphores\n");
    #endif
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