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
#include "queue.h"

#define max(a, b) ((a) > (b) ? (a) : (b))

// Writes a message to the log file
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

// Reads the configuration file and store the values in the global config struct
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

// Listens to the named pipes and send the messages to the queues
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
        printf("<RECEIVER>DEBUG# Queue status:\n\tVideo: %d/%d\n\tOther: %d/%d\n\tExtra Engine: %d\n", video_queue->num_elements, video_queue->max_elements, other_queue->num_elements, other_queue->max_elements, extra_auth_engine); 
        #endif

        #ifdef PRETTY
        printf("VIDEO QUEUE: ");
        print_progress(video_queue->num_elements, video_queue->max_elements);
        printf("OTHER QUEUE: ");
        print_progress(other_queue->num_elements, other_queue->max_elements);
        #endif
    }

    return NULL;
}

// Sends the messages to the queues, called by the receiver thread
// Also, deploys the extra auth engine if any of the queues is full
void parse_and_send(char *message){
    // JUST SEND TO VIDEO FOR NOW, IMPLEMENT PARSING LATTER
    #ifdef DEBUG
    printf("<RECEIVER>DEBUG# Sending message to video queue: %s\n", message);
    #endif

    pthread_mutex_lock(&queues_mutex);

    if(is_full(video_queue)){
        char error_message[PIPE_BUFFER_SIZE];
        sprintf(error_message, "QUEUE IS FULL, DISCARDING MESSAGE: [%s]", message);
        write_to_log(error_message);     
    }
    else{
        push(video_queue, message);
        // If it's full after pushing the message, add a new auth engine
        if(is_full(video_queue) && extra_auth_engine == 0){
            deploy_extra_engine();
        }
    }

    #ifdef DEBUG
    printf("<RECEIVER>DEBUG# Notifying sender thread\n");
    #endif
    
    pthread_cond_signal(&sender_cond);
    pthread_mutex_unlock(&queues_mutex);    
}

// Deploys the extra auth engine if both queues are full, called by the parse_and_send function
void deploy_extra_engine(){
    #ifdef DEBUG
    printf("<RECEIVER>DEBUG# The queues are full and the extra auth engine is not active, deploying it...\n");
    #endif

    // Allocate memory for the new unammed pipe
    auth_engine_pipes[config->AUTH_SERVERS] = (int*) malloc(sizeof(int) * 2);
    if(pipe(auth_engine_pipes[config->AUTH_SERVERS]) == -1){
        write_to_log("<ERROR CREATING AUTH ENGINE PIPE>");
        return;
    }

    // Activate the extra engine flag
    extra_auth_engine = 1;

    extra_auth_pid = fork();
    if(extra_auth_pid == 0){
        // Child process (extra auth engine)
        
        // Signal handling
        signal(SIGTERM, kill_auth_engine);
        close(auth_engine_pipes[config->AUTH_SERVERS][1]); // Close write end of the pipe

        // Start the extra auth engine process with the last ID available
        auth_engine_process(config->AUTH_SERVERS);
        write_to_log("EXTRA AUTHORIZATION ENGINE DEPLOYED");
        exit(0);
    }
    else{
        // Parent process (ARM)
        close(auth_engine_pipes[config->AUTH_SERVERS][0]); // Close read end of the pipe

        // Activate the extra auth engine flag 
        extra_auth_engine = 1;
    }

    #ifdef DEBUG
    printf("<RECEIVER>DEBUG# Extra auth engine deployed\n");
    #endif
}

// Reads the messages from the queues, check for available auth engines and send the messages to them
// Also, deactivates the extra auth engine if both queues are at 50% capacity
void* sender_thread(){
    // Implement sender thread
    #ifdef DEBUG
    printf("<SENDER>DEBUG# Sender thread started\n");
    #endif
    write_to_log("THREAD SENDER CREATED");

    // JUST READ VIDEOQUEUE FOR NOW, IMPLEMENT THE OTHER QUEUE LATER
    while(1){
        // Check condition in mutual exclusion
        pthread_mutex_lock(&queues_mutex);
        #ifdef DEBUG
        printf("<SENDER>DEBUG# Sender thread checking queues in mutual exclusion\n");
        #endif

        while(is_empty(video_queue) && is_empty(other_queue)){
            // Wait to be notified and let other threads access the mutex
            #ifdef DEBUG
            printf("<SENDER>DEBUG# Both queues are empty, waiting for notification\n");
            #endif

            pthread_cond_wait(&sender_cond, &queues_mutex);
            #ifdef DEBUG
            printf("<SENDER>DEBUG# Sender thread notified\n");
            #endif
        }

        // Once notified, the sender will read the queues untill they are empty
        char *message;

        if(!is_empty(video_queue)){
            message = pop(video_queue);
            #ifdef DEBUG
            printf("<SENDER>DEBUG# Got [%s] from the video queue\n", message);
            #endif
        }  
        else if(!is_empty(other_queue)){
            message = pop(other_queue);
            #ifdef DEBUG
            printf("<SENDER>DEBUG# Got [%s] from the other queue\n", message);
            #endif
        }

        // Unlock receiver right after reading so it can keep pushing messages to the queue while the sender fetches an authorization engine
        pthread_mutex_unlock(&queues_mutex);

        #ifdef DEBUG
        int sem_value;
        sem_getvalue(engines_sem, &sem_value);
        if(sem_value == 0){
            printf("<SENDER>DEBUG# No auth engines available, waiting...\n");
        }
        else{
            printf("<SENDER>DEBUG# Engines semaphore value: %d\n", sem_value);
        }
        #endif
        // Wait until an auth engine is available
        sem_wait(engines_sem);
        
        #ifdef DEBUG
        printf("<SENDER>DEBUG# Searching for an available auth engine...\n");
        #endif
        // Check which auth engine is available
        sem_wait(aux_shm_sem);
        // If the extra auth engine is active, the for loop is increased by 1
        for(int id = 0; id < config->AUTH_SERVERS + extra_auth_engine; id++){
            #ifdef DEBUG
            printf("<SENDER>DEBUG# Checking if auth engine %d is available\n", id);
            #endif
            if(auxiliary_shm->active_auth_engines[id] == 0){
                #ifdef DEBUG
                printf("<SENDER>DEBUG# Auth engine %d is available, sending message\n", id);
                #endif
                // Send message to auth engine and mark it as busy
                auxiliary_shm->active_auth_engines[id] = 1;
                
                write(auth_engine_pipes[id][1], message, PIPE_BUFFER_SIZE);
                break;
            }

            #ifdef DEBUG
            printf("<SENDER>DEBUG# Auth engine %d is busy\n", id);
            #endif
        }
        sem_post(aux_shm_sem);

        #ifdef DEBUG    
        printf("<SENDER>DEBUG# Queue status:\n\tVideo: %d/%d\n\tOther: %d/%d\n\tExtra Engine: %d\n", video_queue->num_elements, video_queue->max_elements, other_queue->num_elements, other_queue->max_elements, extra_auth_engine); 
        #endif

        // If both queues reach 50% capacity, the extra auth engine is deactivated
        if((extra_auth_engine == 1) && (video_queue->num_elements <= config->QUEUE_POS / 2) && (other_queue->num_elements <= config->QUEUE_POS / 2)){
            #ifdef DEBUG
            printf("<SENDER>DEBUG# Both queues are at 50%% capacity, deactivating extra auth engine\n");
            #endif
           
            kill(extra_auth_pid, SIGTERM);
            extra_auth_engine = 0;
        }
    }
    return NULL;
}

// Creates the monitor engine process
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

// Creates and opens USER_PIPE and BACK_PIPE, called by the ARM
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

// Creates the ARM process, which will create the auth engines and the receiver/sender threads
int create_auth_manager(){
    pid_t pid = fork();

    if(pid == -1){
        write_to_log("<ERROR CREATING AUTH MANAGER>");
        return 1;
    }

    if(pid == 0){
        // Child process
        #ifdef DEBUG
        printf("<ARM>DEBUG# Authorization Requests Manager has PID %d\n", getpid());
        #endif

        write_to_log("PROCESS AUTHORIZATION_REQUEST_MANAGER CREATED");
        
        create_auth_engines();

        create_pipes();
        create_fifo_queues();


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

// Creates the shared memory and auxiliary shared memory
int create_shared_memory(){  
    // Create new shared memory with size of SharedMemory struct and permissions 0777 (rwx-rwx-rwx)
    // shm_id = shmget(IPC_PRIVATE, sizeof(MobileUserData) * config->MOBILE_USERS, IPC_CREAT | 0777);
    // if(shm_id == -1){
    //     write_to_log("<ERROR CREATING SHARED MEMORY>");
    //     return 1;
    // }

    // Attach shared memory to shared_memory pointer
    //shared_memory = (MobileUserData*) shmat(shm_id, NULL, 0);
    
    // Initialize shared memory struct
    shm_id = shmget(IPC_PRIVATE, sizeof(SharedMemory), IPC_CREAT | 0777);
    if(shm_id == -1){
        write_to_log("<ERROR CREATING SHARED MEMORY>");
        return 1;
    }
    shared_memory = (SharedMemory*) shmat(shm_id, NULL, 0);
    if(shared_memory == (void*) -1){
        write_to_log("<ERROR ATTACHING SHARED MEMORY>");
        return 1;
    }
    
    // Allocate memory for the users
    shm_id_users = shmget(IPC_PRIVATE, sizeof(MobileUserData) * config->MOBILE_USERS, IPC_CREAT | 0777);
    if(shm_id_users == -1){
        write_to_log("<ERROR CREATING SHARED MEMORY FOR USERS>");
        return 1;
    }
    shared_memory->users = (MobileUserData*) shmat(shm_id_users, NULL, 0);
    if(shared_memory->users == (void*) -1){
        write_to_log("<ERROR ATTACHING SHARED MEMORY FOR USERS>");
        return 1;
    }
    // Initialize the shared memory values
    shared_memory->num_users = 0;
    shared_memory->spent_video = 0;
    shared_memory->spent_social = 0;
    shared_memory->spent_music = 0;
    for(int i = 0; i < config->MOBILE_USERS; i++){
        shared_memory->users[i].isActive = 0;
    }


    // Initialize auxiliary shared memory
    aux_shm_id = shmget(IPC_PRIVATE, sizeof(AuxiliaryShm), IPC_CREAT | 0777);
    if(aux_shm_id == -1){
        write_to_log("<ERROR CREATING AUXILIARY SHARED MEMORY>");
        return 1;
    }
    auxiliary_shm = (AuxiliaryShm*) shmat(aux_shm_id, NULL, 0);
    if(auxiliary_shm == (void*) -1){
        write_to_log("<ERROR ATTACHING AUXILIARY SHARED MEMORY>");
        return 1;
    }

    // Allocate memory for engines
    engines_shm_id = shmget(IPC_PRIVATE, sizeof(int) * (config->AUTH_SERVERS + 1), IPC_CREAT | 0777); // +1 for the extra engine
    if(engines_shm_id == -1){
        write_to_log("<ERROR CREATING AUXILIARY SHARED MEMORY FOR ENGINES>");
        return 1;
    }
    auxiliary_shm->active_auth_engines = (int*) shmat(engines_shm_id, NULL, 0);
    if(auxiliary_shm->active_auth_engines == (void*) -1){
        write_to_log("<ERROR ATTACHING AUXILIARY SHARED MEMORY FOR ENGINES>");
        return 1;
    }
    for(int i = 0; i < config->AUTH_SERVERS + 1; i++){
        // Set all auth engines as unavailable
        auxiliary_shm->active_auth_engines[i] = 1;
    }
    


    #ifdef DEBUG
    printf("DEBUG# Shared memory created successfully\n");
    #endif

    return 0;
}

// Creates the log semaphore and the shared memory semaphore
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

// Creates the auxiliary shared memory semaphore and the engines semaphore
int create_aux_semaphores(){
    sem_close(aux_shm_sem);
    sem_unlink(AUXILIARY_SHM_SEMAPHORE);
    sem_close(engines_sem);
    sem_unlink(ENGINES_SEMAPHORE);

    #ifdef DEBUG
    printf("DEBUG# Creating auxiliary shared memory semaphore...\n");
    #endif

    // Create auxiliary shared memory semaphore
    aux_shm_sem = sem_open(AUXILIARY_SHM_SEMAPHORE, O_CREAT | O_EXCL, 0777, 1);
    if(aux_shm_sem == SEM_FAILED){
        write_to_log("<ERROR CREATING AUXILIARY SHM SEMAPHORE>");
        return 1;
    }

    #ifdef DEBUG
    printf("DEBUG# Auxiliary shared memory semaphore created successfully\n");
    printf("DEBUG# Creating engines semaphore...\n");
    #endif

    // Create engines semaphore
    engines_sem = sem_open(ENGINES_SEMAPHORE, O_CREAT | O_EXCL, 0777, 0); // Start as locked, will be unlocked when the auth engines are created
    if(engines_sem == SEM_FAILED){
        write_to_log("<ERROR CREATING ENGINES SEMAPHORE>");
        return 1;
    }

    #ifdef DEBUG
    printf("DEBUG# Engines semaphore created successfully\n");
    #endif
    return 0; 
}

// Creates the initial auth engines
int create_auth_engines(){
    auth_engine_pipes = (int**) malloc(sizeof(int*) * (config->AUTH_SERVERS + 1)); // +1 for the extra engine
    if(auth_engine_pipes == NULL){
        write_to_log("<ERROR CREATING AUTH ENGINE PIPES>");
        return 1;
    }

    #ifdef DEBUG
    printf("<ARM>DEBUG# Creating auth engines...\n");
    #endif

    for(int i = 0; i < config->AUTH_SERVERS; i++){
        // Open pipe before forking
        auth_engine_pipes[i] = (int*) malloc(sizeof(int) * 2);

        if(pipe(auth_engine_pipes[i]) == -1){
            write_to_log("<ERROR CREATING AUTH ENGINE PIPE>");
            return 1;
        }

        pid_t engine_pid = fork();
        if(engine_pid == -1){
            write_to_log("<ERROR CREATING AUTH ENGINE>");
            return 1;
        }

        if(engine_pid == 0){
            // Auth engine
            close(auth_engine_pipes[i][1]); // Close write end of the pipe

            auth_engine_process(i);

            write_to_log("PROCESS AUTHORIZATION_ENGINE CREATED");
            exit(0);
        }
        else{
            // Parent process (ARM)
            close(auth_engine_pipes[i][0]); // Close read end of the pipe

        }
    }

    return 0; 
}

// Authorization engine process
int auth_engine_process(int id){
    #ifdef DEBUG
    printf("<AE>DEBUG# Auth engine %d started with PID %d\n", id, getpid());
    #endif

    // Mark as available
    sem_wait(aux_shm_sem);
    auxiliary_shm->active_auth_engines[id] = 0;
    sem_post(aux_shm_sem);

    // "Notify" the sender thread that an auth engine is available
    sem_post(engines_sem);

    while(1){
        #ifdef DEBUG
        printf("<AE%d>DEBUG# Auth engine is ready to receive a request\n", id);
        #endif

        char message[PIPE_BUFFER_SIZE];
        // Wait for message
        read(auth_engine_pipes[id][0], message, PIPE_BUFFER_SIZE);
        
        #ifdef DEBUG
        printf("<AE%d>DEBUG# Auth engine is now processing a request: %s\n", id, message);
        #endif

        sleep_milliseconds(config->AUTH_PROC_TIME);

        // Mark the auth engine as available
        sem_wait(aux_shm_sem);
        auxiliary_shm->active_auth_engines[id] = 0;
        sem_post(aux_shm_sem);

        // "Notify" the sender thread that an auth engine is available
        sem_post(engines_sem);

        #ifdef DEBUG
        printf("<AE%d>DEBUG# Auth engine finished processing request: %s, it's available again\n", id, message);
        #endif

    }

    return 0;
}

// Adds a mobile user to the shared memory, called by the auth engines
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

        if(shared_memory->users[i].isActive == 0){
            // Check if there's a user with the same id already in the shared memory
            if(shared_memory->users[i].user_id == user_id){ 
                write_to_log("<ERROR ADDING USER TO SHARED MEMORY> User already exists");
                return 1;
            }

            shared_memory->users[i].isActive = 1; // Set user as active
            shared_memory->users[i].user_id = user_id;
            shared_memory->users[i].initial_plafond = plafond;
            shared_memory->users[i].spent_plafond = 0;
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
        if(shared_memory->users[i].isActive == 1){
            printf("\tUser %d has plafond: %d\n", shared_memory->users[i].user_id, shared_memory->users[i].initial_plafond - shared_memory->users[i].spent_plafond);
        }
    }
}

// Creates the video and other queues
int create_fifo_queues(){
    #ifdef DEBUG
    printf("<ARM>DEBUG# Creating video queue...\n");
    #endif
    video_queue = create_queue(config->QUEUE_POS);
    
    #ifdef DEBUG
    printf("<ARM>DEBUG# Creating other queue...\n");
    #endif
    other_queue = create_queue(config->QUEUE_POS);
    
    return 0; 
}

// Initializes the system, called by the main function
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


    // The auxiliary semaphores must be created after reading the config file
    // Because the engines semaphore depends on the number of auth servers
    #ifdef DEBUG
    printf("Creating auxiliary semaphores...\n");
    #endif
    if(create_aux_semaphores() != 0){
        return 1;
    }

    #ifdef DEBUG
    printf("<SYS MAN>DEBUG# Creating shared memory...\n");
    #endif
    if(create_shared_memory() != 0){
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
    printf("<SYS MAN>DEBUG# System initialized successfully\n");
    #endif

    return 0;
}

// Sleeps for the specified amount of milliseconds
void sleep_milliseconds(int milliseconds){
    struct timespec ts;
    // Get time in seconds
    ts.tv_sec = milliseconds / 1000;
    // Get the remaining milliseconds and convert them to nanoseconds
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    // Sleep for the specified time
    nanosleep(&ts, NULL);
}

void kill_auth_engine(int signal){
    if(signal == SIGTERM){
        write_to_log("<AE>SIGTERM RECEIVED, KILLING AUTH ENGINE");
        if(getpid() == extra_auth_pid){
            // Free extra pipe memory
            free(auth_engine_pipes[config->AUTH_SERVERS]);
        }
        else{
            for(int i = 0; i < config->AUTH_SERVERS; i++){
                free(auth_engine_pipes[i]);
            }
            free(auth_engine_pipes);
        }
        exit(0);
    }
}

void print_progress(int current, int max){
    int barWidth = 70;

    printf("[");
    int pos = barWidth * current / max;
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) printf("=");
        else if (i == pos) printf(">");
        else printf(" ");
    }
    printf("] %d%%\n", (int)(current * 100.0 / max));
}

// Cleans up the system, called by the signal handler
void clean_up(){
    // ADICIONAR MÁSCARAS AO SIGINT
    // CLEAN NAMED PIPES AND FREE auth_engine_pipes
    // FREE THE QUEUES 
    // DON'T FORGET TO LOOK FOR AND FREE EVERY MALLOC

    write_to_log("5G_AUTH_PLATFORM SIMULATOR CLOSING");

    #ifdef DEBUG
    printf("<SYS MAN>DEBUG# Detatching and deleting the shared memory\n");
    #endif
    if(shared_memory->users != NULL){
        if(shmdt(shared_memory->users) == -1){
            write_to_log("<ERROR DETATCHING USERS SHARED MEMORY>");
        }
        if(shmctl(shm_id_users, IPC_RMID, NULL) == -1){
            write_to_log("<ERROR DELETING USERS SHARED MEMORY>");
        }
    }
    if(shared_memory != NULL){    
        // Detach and delete shared memory
        if(shmdt(shared_memory) == -1){
            write_to_log("<ERROR DETATCHING SHARED MEMORY>");
        }
        if(shmctl(shm_id, IPC_RMID, NULL) == -1){
            write_to_log("<ERROR DELETING SHARED MEMORY>");
        }
    }

    #ifdef DEBUG
    printf("<SYS MAN>DEBUG# Detatching and deleting the auxiliary shared memory\n");
    #endif
    if(auxiliary_shm->active_auth_engines != NULL){
        if(shmdt(auxiliary_shm->active_auth_engines) == -1){
            write_to_log("<ERROR DETATCHING ENGINES SHARED MEMORY>");
        }
        if(shmctl(engines_shm_id, IPC_RMID, NULL) == -1){
            write_to_log("<ERROR DELETING ENGINES SHARED MEMORY>");
        }
    }
    if(auxiliary_shm != NULL){
        // Detach and delete auxiliary shared memory
        if(shmdt(auxiliary_shm) == -1){
            write_to_log("<ERROR DETATCHING AUXILIARY SHARED MEMORY>");
        }
        if(shmctl(aux_shm_id, IPC_RMID, NULL) == -1){
            write_to_log("<ERROR DELETING AUXILIARY SHARED MEMORY>");
        }
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

// Signal handler for SIGINT
void signal_handler(int signal){
    if(signal == SIGINT){
        write_to_log("SIGNAL SIGINT RECEIVED");
        clean_up();
        exit(0);
    }
}