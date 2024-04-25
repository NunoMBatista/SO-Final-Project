/*
  Authors: 
    Nuno Batista uc2022216127
    Miguel Castela uc2022212972
*/
#ifndef GLOBAL_H
#define GLOBAL_H

#define DEBUG // Comment this line to remove debug messages
#define LOG_SEMAPHORE "log_semaphore"
#define SHARED_MEMORY_SEMAPHORE "shared_memory_semaphore"

typedef struct{
    int isActive;
    int user_id;
    int plafond;
} MobileUserData;

typedef struct{
    int MOBILE_USERS; // Max number of users
    int QUEUE_POS; // Number of queue slots
    int AUTH_SERVERS; // Max number of auth engines
    int AUTH_PROC_TIME; // Time taken by auth engines to process requests
    int MAX_VIDEO_WAIT; // Max time that a video request can wait before being processed
    int MAX_OTHERS_WAIT; // Max time that a non-video request can wait before being processed
} Config;

// External declaration to be used in other files
extern Config* config;
extern MobileUserData* shared_memory;
extern int shm_id;
extern sem_t* log_semaphore;
extern sem_t* shared_memory_sem;

#endif
