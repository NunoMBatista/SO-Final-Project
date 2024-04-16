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
SharedMemory* shared_memory;
sem_t* log_semaphore;

int main(int argc, char *argv[]){
    signal(SIGINT, signal_handler);
    // Clean up log semaphore if it already exists
    sem_close(log_semaphore);
    sem_unlink("log_semaphore");
    
    // Create the log semaphore
    log_semaphore = sem_open("log_semaphore", O_CREAT | O_EXCL, 0777, 1);
    if(log_semaphore == SEM_FAILED){
        printf("<ERROR CREATING LOG SEMAPHORE>\n");
        return 1;
    }
    write_to_log("5G_AUTH_PLATFORM SIMULATOR STARTING");

    // Allocate memory for the config struct
    config = (Config*) malloc(sizeof(Config));

    // Check correct number of arguments
    if(argc != 2){
        printf("<INCORRECT NUMBER OF ARGUMENTS>\n Correct usage: %s {config_file}\n", argv[0]);
        return 1;
    }

    // Read the config file
    if(read_config_file(argv[1]) != 0){
        //printf("<ERROR READING CONFIG FILE>\n");
        return 1;
    }
    
    if(create_monitor_engine() != 0){
        //printf("<ERROR CREATING MONITOR ENGINE>\n");
        return 1;
    }
    wait(NULL);

    if(create_auth_manager() != 0){
        //printf("<ERROR CREATING AUTH ENGINE>\n");
        return 1;
    }
    wait(NULL);

    if(create_shared_memory() != 0){
        //printf("<ERROR CREATING SHARED MEMORY>\n");
        return 1;
    }

    add_mobile_user(1, 100);
    add_mobile_user(2, 200);
    add_mobile_user(3, 300);

    print_shared_memory();


    clean_up();
    return 0; 
}