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

// Prints the current state of the shared memory
void print_shared_memory(){
    #ifdef DEBUG
    printf("DEBUG# Printing current state of the shared memory...\n");
    #endif

    printf("\n-> Current state of the shared memory <-\n");
    
    
    printf("Spent Video: %d\n", shared_memory->spent_video);
    printf("Spent Music: %d\n", shared_memory->spent_music);
    printf("Spent Social: %d\n", shared_memory->spent_social);
    
    printf("Requests Video: %d\n", shared_memory->reqs_video);
    printf("Requests Music: %d\n", shared_memory->reqs_music);
    printf("Requests Social: %d\n", shared_memory->reqs_social);
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

// Prints the queues progress
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
