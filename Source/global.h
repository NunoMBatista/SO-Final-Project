/*
  Authors: 
    Nuno Batista uc2022216127
    Miguel Castela uc2022212972
*/
#ifndef GLOBAL_H
#define GLOBAL_H

//#define DEBUG // Comment this line to remove debug messages

typedef struct{
    int isActive;
    int user_id;
    int plafond;
} MobileUserData;

// Shared memory for mobile user data
typedef struct{
    int numUsers;
    MobileUserData* data; // Later to be allocated with MAX_USER_CAPACITY
} SharedMemory;

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
extern SharedMemory* shared_memory;
extern int shm_id;
extern sem_t* log_semaphore;

#endif
