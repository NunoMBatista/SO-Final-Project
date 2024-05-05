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

/*
    To implement:
    GEN - Might need stdout semaphore (the log_sem is the only one actually needed, we can add a color parameter to write_to_log to differentiate between the different processes, use enums)

    ME - Get periodic stats
    ME - Condition variable

    SYS MAN - Remove nested aux shared memory
    SYS MAN - Finish tasks before exiting

    ARM - Verificar se posso ter video_queue_mutex e other_queue_mutex

    ARM - Close and free unnamed pipes
*/

// Cleans up the system, called by the signal handler
void clean_up(){
    signal(SIGINT, SIG_IGN); // Ignore SIGINT while cleaning up

    //printf("\033[31m\tTHIS PID: %d\n\tARM PID: %d\n\tPARENT PID: %d\n\033[0m\n\n", getpid(), arm_pid, parent_pid);
    
    // Notify ARM to exit
    #ifdef DEBUG
    printf("<SYS MAN>DEBUG# Notifying ARM to exit\n");
    #endif
    kill(arm_pid, SIGTERM);

    write_to_log("Waiting for ARM process to finish");
    int status;
    // Wait for ARM to exit
    waitpid(arm_pid, &status, 0);
    printf("LEFT WITH STATUS %d\n", WEXITSTATUS(status));


    // Notify monitor engine to exit
    #ifdef DEBUG
    printf("<SYS MAN>DEBUG# Notifying monitor engine to exit\n");
    #endif
    kill(monitor_pid, SIGTERM);

    write_to_log("Waiting for monitor engine to finish");
    waitpid(monitor_pid, &status, 0);

    // REMOVER MESSAGE_QUEUE_KEY FILE

    write_to_log("5G_AUTH_PLATFORM SIMULATOR CLOSING");

    // Kill extra auth engine if it's active   
    if(extra_auth_engine){
        kill(extra_auth_pid, SIGKILL);
    }

    // Destroy mutexes and condition variables
    pthread_mutex_destroy(&queues_mutex);
    pthread_cond_destroy(&sender_cond);

    #ifdef DEBUG
    printf("<SYS MAN>DEBUG# Detatching and deleting the main shared memory\n");
    #endif
    // Deleted nested shared memory
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
    sem_close(aux_shm_sem);
    sem_unlink(AUXILIARY_SHM_SEMAPHORE);
    sem_close(engines_sem);
    sem_unlink(ENGINES_SEMAPHORE);

    // Unlink lockfiles
    unlink(MAIN_LOCKFILE);
    unlink(BACKOFFICE_LOCKFILE);
}

void clean_up_arm(){     
    // Notify ARM threads to exit
    notify_arm_threads();

    pthread_join(sender_t, NULL);
    pthread_join(receiver_t, NULL);

    // Let the auth engines know they should exit
    for(int i = 0; i < config->AUTH_SERVERS + 1; i++){
        if(i == config->AUTH_SERVERS){ // The last auth engine is the extra one
            if(!extra_auth_engine){ // Only kill it if it's active
                continue;
            }
            kill(extra_auth_pid, SIGTERM);
        }
        else{
            kill(auth_engine_pids[i], SIGTERM);
        }

        // Write dummy request in case the auth engine is waiting for a message in the pipe
        Request dummy;
        dummy.request_type = 'K';
        write(auth_engine_pipes[i][1], &dummy, sizeof(Request));
    }


    // Wait for the auth engines to leave
    while(wait(NULL) > 0);
    
    // Close and free every pipe
    #ifdef DEBUG
    printf("<ARM>DEBUG# Closing and freeing pipes\n");
    #endif        
    // IMPLEMENT LATER
    
    #ifdef DEBUG
    printf("<ARM>DEBUG# Freeing auth engine pids\n");
    #endif
    free(auth_engine_pids);

    #ifdef DEBUG
    printf("<ARM>DEBUG# Freeing queues\n");
    #endif
    free(video_queue);
    free(other_queue);

    #ifdef DEBUG
    printf("<ARM>DEBUG# ARM process exiting\n");
    #endif
}

// Ask ARM threads to exit
void notify_arm_threads(){
    #ifdef DEBUB
    printf("<ARM>DEBUG# Notifying ARM threads to exit\n");
    #endif
    arm_threads_exit = 1;

    #ifdef DEBUG
    printf("<ARM>DEBUG# Notifying sender thread\n");
    #endif

    // Notify sender thread in case it's waiting for a signal
    pthread_mutex_lock(&queues_mutex);
    pthread_cond_signal(&sender_cond);
    pthread_mutex_unlock(&queues_mutex);

    #ifdef DEBUG
    printf("<ARM>DEBUG# Sending exit message to receiver thread\n");
    #endif

    // Send message to receiver thread in case it's waiting to read from a pipe
    char exit_message[PIPE_BUFFER_SIZE] = "EXIT";
    if(write(fd_user_pipe, exit_message, PIPE_BUFFER_SIZE) == -1){
        write_to_log("<ERROR SENDING EXIT MESSAGE TO RECEIVER THREAD>");
    }
}

void kill_auth_engine(int signal){
    #ifdef DEBUG
    printf("<AE%d>DEBUG# Received signal %d\n", auth_engine_index, signal);
    #endif

    if(signal == SIGTERM){
        // Notify ARM threads to exit after the last work cycle
        auth_engine_exit = 1;     

    }
}

// Signal handler for SIGINT
void signal_handler(int signal){
    if(signal == SIGINT){
        if(getpid() == parent_pid){
            write_to_log("SIGNAL SIGINT RECEIVED");
            clean_up();
        }
        exit(0);
    }
    if(signal == SIGTERM){ 
        if(getpid() == monitor_pid){
            #ifdef DEBUG
            printf("<ME>DEBUG# Received SIGTERM\n");
            #endif

            // CANCEL THREAD AND WAIT FOR IT TO JOIN (SLEEP IS A CANCELATION POINT)


            exit(0);
        }
        if(getpid() == arm_pid){
            #ifdef DEBUG
            printf("<ARM>DEBUG# Received SIGTERM\n");
            #endif
            clean_up_arm();
            exit(0);
        }
    }
}