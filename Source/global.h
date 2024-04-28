/*
  Authors: 
    Nuno Batista uc2022216127
    Miguel Castela uc2022212972
*/
#ifndef GLOBAL_H
#define GLOBAL_H

#define SLOWMOTION 100 // Comment this line to remove slow motion, it's value is the delta coefficient
#define DEBUG // Comment this line to remove debug messages
#define LOG_SEMAPHORE "log_semaphore"
#define SHARED_MEMORY_SEMAPHORE "shared_memory_semaphore"
#define PIPE_BUFFER_SIZE 100
#define USER_PIPE "/tmp/USER_PIPE"
#define BACK_PIPE "/tmp/BACK_PIPE"

#include <semaphore.h>
#include "queue.h"

typedef struct{
    int isActive;
    pid_t user_id;
    int initial_plafond;
    int spent_plafond;
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

extern int fd_user_pipe;
extern int fd_back_pipe;

extern Queue *video_queue;
extern Queue *other_queue;
extern int message_queue_id;

extern pthread_mutex_t queues_mutex;
extern pthread_cond_t sender_cond;

extern int extra_auth_engine; // 0 if it's not active, 1 if it was activated by the video queue, 2 if it was activated by the other queue

#endif
