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
#include <ctype.h>
#include <sys/msg.h>

#include "system_functions.h"
#include "global.h"
#include "queue.h"

#define max(a, b) ((a) > (b) ? (a) : (b))



/*
    To implement:

    AE - Add user
    AE - Get stats
    AE - Reset stats

    ME - Get periodic stats
    ME - Condition variable

    SYS MAN - Finish tasks before exiting

    <ARM> - Verificar se posso ter video_queue_mutex e other_queue_mutex
*/

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
        // Wait until something is written to the pipes
        if(select(max_fd + 1, &read_set, NULL, NULL, NULL) == -1){
            write_to_log("<ERROR SELECTING PIPES>");
            signal_handler(SIGINT);
        }
        // Check USER_PIPE
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
        // Check BACK_PIPE
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


        #ifdef DEBUG    
        printf("<RECEIVER>DEBUG# Queue status:\n\tVideo: %d/%d\n\tOther: %d/%d\n\tExtra Engine: %d\n", video_queue->num_elements, video_queue->max_elements, other_queue->num_elements, other_queue->max_elements, extra_auth_engine); 
        #endif

        #ifdef QUEUE_PROGRESS_BAR
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

    // Parse the message and send it to the correct queue
    /*
        VIDEO QUEUE: 
            PID#VIDEO#INT - Video request
   
        OTHER QUEUE: 
            PID#INT - Initial request
            PID#[SOCIAL|MUSIC]#INT - Social or music request
            1#[data_stats|reset] - Backoffice request

        Therefore, we only need to check the first character of the second argument
    */

    // Copying thee message is necessary as tokenizing it modifies the initial string
    char message_copy[PIPE_BUFFER_SIZE];
    strcpy(message_copy, message);

    // Parsing process
    Request request;
    request.data_amount = 0; 
    request.initial_plafond = 0;
    request.start_time = -1; // IMPLEMENT LATER
    char *token = strtok(message, "#");
    if(token == NULL){
        write_to_log("<ERROR PARSING MESSAGE>");
        return;
    }

    // If it's a backoffice request
    if(atoi(token) == 1){
        request.user_id = 1; 
        token = strtok(NULL, "#");
        if(token == NULL){
            write_to_log("<ERROR PARSING MESSAGE>");
            return;
        }
        if(strcmp(token, "data_stats") == 0){
            // Get stats
            request.request_type = 'D';
        }
        else if(strcmp(token, "reset") == 0){
            // Reset stats
            request.request_type = 'R';
        }
        else{
            // Invalid request
            request.request_type = 'E';
        }
    }
    else{
        request.user_id = atoi(token);
        token = strtok(NULL, "#");
        if(token == NULL){
            write_to_log("<ERROR PARSING MESSAGE>");
            return;
        }
        // If it's a data request, there's another argument
        if(token[0] == 'V' || token[0] == 'M' || token[0] == 'S'){
            request.request_type = token[0];
            token = strtok(NULL, "#");
            if(token == NULL){
                write_to_log("<ERROR PARSING MESSAGE>");
                return;
            }
            request.data_amount = atoi(token);
        }
        // If it's an initial request, there are only 2 arguments
        else{
            request.request_type = 'I';
            request.initial_plafond = atoi(token);
        }
    }


    char error_message[PIPE_BUFFER_SIZE + 50];
    pthread_mutex_lock(&queues_mutex);

    // If it's a video request
    if(request.request_type == 'V'){
        // Send to video queue
        if(is_full(video_queue)){
            sprintf(error_message, "VIDEO QUEUE IS FULL, DISCARDING MESSAGE [%s]", message_copy);
            write_to_log(error_message);
        }
        else{
            push(video_queue, request);
        }
    }
    else{
        // Send to other queue
        if(is_full(other_queue)){
            sprintf(error_message, "OTHER QUEUE IS FULL, DISCARDING MESSAGE [%s]", message_copy);
            write_to_log(error_message);
        }
        else{
            push(other_queue, request);
        }
    }

    // Deploy extra auth engine if any of the queues is full and the extra auth engine is not active
    if((is_full(other_queue) || is_full(video_queue)) && (!extra_auth_engine)){
        deploy_extra_engine();
    }

    // Notify sender thread
    pthread_cond_signal(&sender_cond);
    pthread_mutex_unlock(&queues_mutex);

    #ifdef DEBUG
    printf("<RECEIVER>DEBUG# Notifying sender thread\n");
    #endif
    
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
        //char *message;
        Request message; 

        if(!is_empty(video_queue)){
            message = pop(video_queue);
            #ifdef DEBUG
            printf("<SENDER>DEBUG# Got [%d#%c#%d] from the video queue\n", message.user_id, message.request_type, message.data_amount);
            #endif
        }  
        else if(!is_empty(other_queue)){
            message = pop(other_queue);
            #ifdef DEBUG
            printf("<SENDER>DEBUG# Got [%d#%c#%d#%d] from the other queue\n", message.user_id, message.request_type, message.data_amount, message.initial_plafond);
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
                
                write(auth_engine_pipes[id][1], &message, sizeof(Request));
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
    printf("<AE%d>DEBUG# Auth engine started with PID %d\n", id, getpid());
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

        //char message[PIPE_BUFFER_SIZE];
        Request request;
        // Wait for message
        read(auth_engine_pipes[id][0], &request, sizeof(Request));
        
        #ifdef DEBUG
        printf("<AE%d>DEBUG# Auth engine is now processing a request:\n\tUSER: %d\n\tTYPE: %c\n\tDATA: %d\n\tINITPLAF: %d\n", id, request.user_id, request.request_type, request.data_amount, request.initial_plafond);
        #endif
    
        #ifdef SLOWMOTION
        sleep_milliseconds(config->AUTH_PROC_TIME * SLOWMOTION / 10);
        #endif
        sleep_milliseconds(config->AUTH_PROC_TIME);

        char log_message[PIPE_BUFFER_SIZE + 500]; // Write to the log
        char log_message_type[PIPE_BUFFER_SIZE]; // Used to write to the log the type of request processed
        char log_process_feedback[PIPE_BUFFER_SIZE]; // Used to write to the log the result of the request processing
       
       
        int return_code; // Identifies the return codes from functions
        char response[PIPE_BUFFER_SIZE]; // Send back to the user through the message queue
        int response_applicable = 0; // 1 if there needs to be a response sent to the message queue
        char type = request.request_type; // Make the code more readable
 
        sem_wait(shared_memory_sem); // Lock shared memory

        int user_index = get_user_index(request.user_id);
        
        // If the user is not asking to be added and does not exist
        if((request.request_type != 'I') && (user_index == -1)){
            // THE USER DOES NOT EXIST
            response_applicable = 1; 
            type = -1; // Skip if-else block

            sprintf(response, "DIE");

            sprintf(log_message_type, "INVALID REQUEST");
            sprintf(log_process_feedback, "USER %d DOES NOT EXIST", request.user_id);
        }

        // It's a registration request
        if(type == 'I'){
            sprintf(log_message_type, "USER REGISTRATION REQUEST");
            // Try to register user
            
            // There's a need to tell the user if the request was accepted or rejected  
            response_applicable = 1; 

            return_code = add_mobile_user(request.user_id, request.initial_plafond);
            if(return_code  == 0){
                sprintf(response, "ACCEPTED");
                //sprintf(log_message, "AUTHORIZATION_ENGINE %d: USER REGISTRATION REQUEST (ID = %d) PROCESSING COMPLETED -> USER ADDED WITH INITIAL PLAFFOND OF %d", id, request.user_id, request.initial_plafond);
                sprintf(log_process_feedback, "USER ADDED WITH INITIAL PLAFFOND OF %d", request.initial_plafond);
            }
            else if(return_code == 1){
                sprintf(response, "REJECTED");
                //sprintf(log_message, "AUTHORIZATION_ENGINE %d: USER REGISTRATION REQUEST (ID = %d) PROCESSING COMPLETED -> SHARED MEMORY IS FULL => USER NOT ADDED", id, request.user_id);
                sprintf(log_process_feedback, "SHARED MEMORY IS FULL => USER NOT ADDED");
            }
            else if(return_code == 2){
                sprintf(response, "REJECTED");
                //sprintf(log_message, "AUTHORIZATION_ENGINE %d: USER REGISTRATION REQUEST (ID = %d) PROCESSING COMPLETED -> USER ALREADY EXISTS => USER NOT ADDED", id, request.user_id);
                sprintf(log_process_feedback, "USER ALREADY EXISTS => USER NOT ADDED");
            }
        }

        // It's a data request
        if((type == 'V') || (type == 'M') || (type == 'S')){
            int add_stats = request.data_amount; // The amount of data to add to the user stats

            return_code = remove_from_user(request.user_id, add_stats);

            // There is still data left after removing
            if(return_code == 1){
                sprintf(log_process_feedback, "REMOVED %d FROM USER %d", request.data_amount, request.user_id);
            }
            // Theres no data left after removing
            if(return_code == 2){
                deactivate_user(request.user_id);
                sprintf(log_process_feedback, "USER %d HAS REACHED 0 PLAFOND AFTER REMOVING %d", request.user_id, request.data_amount);

    
                // SEND MESSAGE TELLING THE USER TO SHUTDOWN
                response_applicable = 1;
                sprintf(response, "DIE");
            }
            // The user didn't have enough data
            if(return_code == -1){
                
                // Don't add anything to the total stats
                add_stats = 0; 

                deactivate_user(request.user_id);
                sprintf(log_process_feedback, "USER %d DOES NOT HAVE ENOUGH PLAFOND TO GET %d, REMOVING USER", request.user_id, request.data_amount);

                // SEND MESSAGE TELLING THE USER TO SHUTDOWN
                response_applicable = 1; 
                sprintf(response, "DIE");               

            }
            // Set type in the log message

            // Get respective log message type and add the stats to the total
            switch(type){
                case 'V':
                    sprintf(log_message_type, "VIDEO AUTHORIZATION REQUEST");
                    shared_memory->spent_video += add_stats;
                    break;
                case 'M':
                    sprintf(log_message_type, "MUSIC AUTHORIZATION REQUEST");
                    shared_memory->spent_music += add_stats;
                    break;
                case 'S':
                    sprintf(log_message_type, "SOCIAL AUTHORIZATION REQUEST");
                    shared_memory->spent_social += add_stats;
                    break;
            }

            // NOTIFY MONITOR ENGINE
        }

        //     case 'D':
        //         // Try to send data to backoffice user
        //         sprintf(log_message, "AUTHORIZATION_ENGINE %d: DATA REQUEST FROM BACKOFFICE USER PROCESSING COMPLETED", id);
        //         break;
        //     case 'R':
        //         // Try to reset user data
        //         sprintf(log_message, "AUTHORIZATION_ENGINE %d: DATA RESET REQUEST FROM BACKOFFICE USER PROCESSING COMPLETED", id);
        //         break;
        //     case 'E':
        //         // Tell backoffice that the request is invalid
        //         sprintf(log_message, "AUTHORIZATION_ENGINE %d: INVALID REQUEST FROM BACKOFFICE USER PROCESSING COMPLETED", id);
        //         break;
        //     default:
        // }

        sem_post(shared_memory_sem); // Unlock shared memory 

        
        sprintf(log_message, "AUTHORIZATION_ENGINE %d: %s -> %s", id, log_message_type, log_process_feedback);
        write_to_log(log_message);

        if(response_applicable){
            #ifdef DEBUG
            printf("<AE%d>DEBUG# Sending response to user: %s\n", id, response);
            #endif
            QueueMessage response_message;
            // The type must be the user's id for it to be delivered to the correct user
            response_message.type = request.user_id; 
            strcpy(response_message.text, response);
            if(msgsnd(message_queue_id, &response_message, sizeof(response_message.text), 0) == -1){
                write_to_log("<ERROR SENDING RESPONSE MESSAGE>");
            }
        }

        #ifdef SHARED_MEMORY_DISPLAY
        print_shared_memory();
        #endif

        // Mark the auth engine as available
        sem_wait(aux_shm_sem);
        auxiliary_shm->active_auth_engines[id] = 0;
        sem_post(aux_shm_sem);

        // "Notify" the sender thread that an auth engine is available
        sem_post(engines_sem);

        #ifdef DEBUG
        printf("<AE%d>DEBUG# Auth engine finished processing request:\n\tUSER: %d\n\tTYPE: %c\n\tDATA: %d\n\tINITPLAF: %d\nIt's available again\n", id, request.user_id, request.request_type, request.data_amount, request.initial_plafond);
        #endif

    }

    return 0;
}

// Adds a mobile user to the shared memory, called by the auth engines

int add_mobile_user(int user_id, int plafond){
    /*
        Returns:
            0 - User added successfully
            1 - Shared memory is full
            2 - User already exists
    */
    #ifdef DEBUG
    printf("DEBUG# Adding user %d to shared memory\n", user_id);
    #endif

    // Check if the shared memory is full
    if(shared_memory->num_users == config->MOBILE_USERS){
        return 1;
    }

    for(int i = 0; i <= config->MOBILE_USERS; i++){
        if(shared_memory->users[i].isActive == 0){
            // Check if there's a user with the same id already in the shared memory
            if(shared_memory->users[i].user_id == user_id){ 
                write_to_log("<ERROR ADDING USER TO SHARED MEMORY> User already exists");
                return 2;
            }
            
            shared_memory->users[i].isActive = 1; // Set user as active
            shared_memory->users[i].user_id = user_id; // Set user id
            shared_memory->users[i].initial_plafond = plafond; // Set initial plafond
            shared_memory->users[i].spent_plafond = 0; // Set spent plafond
            break;
        }
    }
    
    #ifdef DEBUG
    printf("DEBUG# User %d added to shared memory\n", user_id);
    #endif

    char message[100];
    sprintf(message, "USER %d ADDED TO SHARED MEMORY", user_id);
    write_to_log(message);
    return 0;
}

// Gets user index in shared memory, called by the auth engines
int get_user_index(int user_id){
    /*
        Returns:
            -1 - User not found
            i - User index
            config->MOBILE_USERS - backoffice user
    */
    
    if(user_id == 1){
        return config->MOBILE_USERS;
    }
    
    for(int i = 0; i < config->MOBILE_USERS; i++){
        // Check if the user is active and has the same id
        if((shared_memory->users[i].isActive == 1) && (shared_memory->users[i].user_id == user_id)){
            return i;
        }
    }
    return -1;
}

// Removes an ammount from the user's plafond, called by the auth engines
int remove_from_user(int user_id, int amount){
    /*
        Returns:
            1 - Success and remaining > 0
            2 - Sucess and remaining = 0
            -1 - User does not have enough plafond
    */

    // Returns the ammount taken from the user
    for(int i = 0; i < config->MOBILE_USERS; i++){
        if(shared_memory->users[i].user_id == user_id){
            shared_memory->users[i].spent_plafond += amount;
            int remaining = shared_memory->users[i].initial_plafond - shared_memory->users[i].spent_plafond;
            if(remaining == 0){
                return 2;
            }
            if(remaining < 0){                
                return -1;
            }
            if(remaining > 0){
                return 1;
            }
        }
    }       
    return -2; // User not found
}

// Deactivates a user, called by the auth engines
int deactivate_user(int user_id){
    /*  
        Returns:
            0 - User deactivated successfully
            -1 - Couldn't find an active user with provided id
    */
    
    for(int i = 0; i < config->MOBILE_USERS; i++){
        // Check if the user is active and has the same id
        if(shared_memory->users[i].isActive == 1 && shared_memory->users[i].user_id == user_id){
            shared_memory->users[i].isActive = 0;
            return 0;
        }
    }
    return -1;
}

void print_shared_memory(){
    #ifdef DEBUG
    printf("DEBUG# Printing current state of the shared memory...\n");
    #endif

    printf("\n-> Current state of the shared memory <-\n");
    
    
    printf("Spent Video: %d\n", shared_memory->spent_video);
    printf("Spent Music: %d\n", shared_memory->spent_music);
    printf("Spent Social: %d\n", shared_memory->spent_social);
    printf("\n");

    for(int i = 0; i < config->MOBILE_USERS; i++){
        if(shared_memory->users[i].isActive == 1){
            int initial = shared_memory->users[i].initial_plafond;
            int spent = shared_memory->users[i].spent_plafond;
            int remaining = initial - spent;

            printf("User %d:\n", shared_memory->users[i].user_id);
            printf("\tInitial Plafond: %d\n", initial);
            printf("\tSpent Plafond: %d\n", spent);
            printf("\tRemaining Plafond: %d\n", remaining);

            printf("\tProgress: [");
            int progress = (int)(((double)spent / initial) * 50);
            for(int j = 0; j < 50; j++){
                if(j < progress){
                    printf("#");
                } else {
                    printf("-");
                }
            }
            printf("]\n\n");
        }
    }
}

// Creates the message queue
int create_message_queue(){
    // Generate a key based on the file name
    
    // Create temporary file
    FILE* temp = fopen(MESSAGE_QUEUE_KEY, "w");
    if(temp == NULL){
        write_to_log("<ERROR CREATING MESSAGE QUEUE KEY>");
        return 1;
    }
    fclose(temp);

    // Generate a key
    key_t queue_key = ftok(MESSAGE_QUEUE_KEY, 'a');
    if((message_queue_id = msgget(queue_key, IPC_CREAT | 0777)) == -1){
        perror("msgget");
        write_to_log("<ERROR CREATING MESSAGE QUEUE>");
        return 1;
    }
    return 0;
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

    #ifdef DEBUG
    printf("<SYS MAN>DEBUG# Creating message queue...\n");
    #endif
    if(create_message_queue() != 0){
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

// Kill the auth engines
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

        // MIGHT CHANGE THIS IN ORDER TO JUST CHANGE A FLAG AND LET THE LAST LOOP ITERATION HAPPEN
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
    // REMOVER MESSAGE_QUEUE_KEY FILE
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

    #ifdef DEBUG
    printf("<SYS MAN>DEBUG# Destroying message queue\n");
    #endif
    // Destroy message queue
    if(msgctl(message_queue_id, IPC_RMID, NULL) == -1){
        write_to_log("<ERROR DESTROYING MESSAGE QUEUE>");
    }
    
    #ifdef DEBUG
    printf("<SYS MAN>DEBUG# Removing message queue key file\n");
    #endif
    // Remove message queue key file
    if(remove(MESSAGE_QUEUE_KEY) == -1){
        write_to_log("<ERROR REMOVING MESSAGE QUEUE KEY FILE>");
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