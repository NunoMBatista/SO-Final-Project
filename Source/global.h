#ifndef GLOBAL_H
#define GLOBAL_H

#define DEBUG // remove this line to remove debug messages
#define MAX_USER_CAPACITY 10

typedef struct
{
    int userID;
    int plafond;
    int deltaVideo;
    int deltaMusic;
    int deltaSocial;
} MobileUserData;

// Shared memory for mobile user data
typedef struct
{
    int numUsers;
    MobileUserData data[MAX_USER_CAPACITY];
} SharedMemory;

// vê início da página 9 para entenderes
typedef struct{
    int QUEUE_POS; // Number of queue slots
    int AUTH_SERVERS_MAX; // Max number of auth engines
    int AUTH_PROC_TIM; // Time takne by auth engines to process requests
    int MAX_VIDEO_WAIT; // Max time a a video request can wait before being processed
    int MAX_OTHERS_WAIT; // Max time a a non-video request can wait before being processed
} Config;
#endif