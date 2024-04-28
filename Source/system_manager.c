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
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>

#include "global.h"
#include "system_functions.h"
#include "queue.h"

/*
    Execution instructions:
    ./5g_auth_platform {config_file}
*/

Config* config;
int shm_id;
MobileUserData* shared_memory;
sem_t* log_semaphore;
sem_t* shared_memory_sem;

int fd_user_pipe; 
int fd_back_pipe;

Queue *video_queue;
Queue *other_queue;
int message_queue_id;

pthread_mutex_t queues_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t sender_cond = PTHREAD_COND_INITIALIZER;

int extra_auth_engine = 0;

int main(int argc, char *argv[]){
    // Check if another instance of the program is running by checking if the semaphore already exists
    if((sem_open(LOG_SEMAPHORE, O_CREAT | O_EXCL, 0666, 1) == SEM_FAILED) && (errno == EEXIST)){
        printf("!!! ANOTHER INSTANCE OF THE PROGRAM IS ALREADY RUNNING !!!\n");
        return 1;
    }

    // Check correct number of arguments
    if(argc != 2){
        printf("<INCORRECT NUMBER OF ARGUMENTS>\n Correct usage: %s {config_file}\n", argv[0]);
        return 1;
    }

    char *config_file = argv[1];

    initialize_system(config_file);

    // Wait for all child processes to finish
    while(wait(NULL) > 0);

    add_mobile_user(1, 100);
    add_mobile_user(2, 200);
    add_mobile_user(3, 300);

    print_shared_memory();

    clean_up();
    return 0; 
}