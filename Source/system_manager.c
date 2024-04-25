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
#include "global.h"
#include "system_functions.h"

/*
    Execution instructions:
    ./5g_auth_platform {config_file}
*/

Config* config;
int shm_id;
//SharedMemory* shared_memory;
MobileUserData* shared_memory;
sem_t* log_semaphore;
sem_t* shared_memory_sem;

int main(int argc, char *argv[]){
    signal(SIGINT, signal_handler);

    // Allocate memory for the config struct
    config = (Config*) malloc(sizeof(Config));

    // Check correct number of arguments
    if(argc != 2){
        printf("<INCORRECT NUMBER OF ARGUMENTS>\n Correct usage: %s {config_file}\n", argv[0]);
        return 1;
    }

    char *config_file = argv[1];
    initialize_system(config_file);

    add_mobile_user(1, 100);
    add_mobile_user(2, 200);
    add_mobile_user(3, 300);

    print_shared_memory();

    clean_up();
    return 0; 
}