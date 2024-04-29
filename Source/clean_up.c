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

    GEN - Might need stdout semaphore

    AE - Get stats
    AE - Reset stats

    ME - Get periodic stats
    ME - Condition variable

    SYS MAN - Remove nested aux shared memory
    SYS MAN - Finish tasks before exiting

    <ARM> - Verificar se posso ter video_queue_mutex e other_queue_mutex
*/

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