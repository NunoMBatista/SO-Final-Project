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

            // close(fd_user_pipe);
            // if((fd_user_pipe = open(USER_PIPE, O_RDWR)) == -1){
            //     write_to_log("<ERROR OPENING USER_PIPE>");
            //     signal_handler(SIGINT);
            // }

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

            // close(fd_back_pipe);
            // if((fd_back_pipe = open(BACK_PIPE, O_RDWR)) == -1){
            //     write_to_log("<ERROR OPENING USER_PIPE>");
            //     signal_handler(SIGINT);
            // }
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

// Kill the auth engine [make it only kill extra later]
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


// Sends the messages to the queues, called by the receiver thread
// Also, deploys the extra auth engine if any of the queues is full
void parse_and_send(char *message){
    // JUST SEND TO VIDEO FOR NOW, IMPLEMENT PARSING LATTER

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
            #ifdef DEBUG
            printf("<RECEIVER>DEBUG# Sending message to video queue: %s\n", message_copy);
            #endif
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
            #ifdef DEBUG
            printf("<RECEIVER>DEBUG# Sending message to other queue: %s\n", message_copy);
            #endif
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