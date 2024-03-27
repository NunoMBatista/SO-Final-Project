#include <sys/shm.h>
#include <semaphore.h> // POSIX 
#include <fcntl.h> // O_CREAT, O_EXCL
#include <unistd.h> // fork
#include <stdio.h>
#include <stdlib.h> // exit
#include <sys/wait.h> // wait
#include "global.h"

sem_t *mobileUserMutex;
int shmID;

void addUserToSharedMemory(SharedMemory* shMem, int userID, int plafond)
{
    if (shMem->numUsers < MAX_USER_CAPACITY)
    {
        shMem->data[shMem->numUsers].userID = userID;
        shMem->data[shMem->numUsers].plafond = plafond;
        shMem->numUsers++;
    }
}

void printSharedMemory(SharedMemory* shMem)
{
    printf("Number of users: %d\n", shMem->numUsers);
    for (int i = 0; i < shMem->numUsers; i++)
    {
        printf("User %d: plafond=%d\n",
               shMem->data[i].userID, shMem->data[i].plafond);
    }
}

int main(void){
    SharedMemory* shMem; 

    // Create shared memory segment with permissions 0666
    shmID = shmget(IPC_PRIVATE, sizeof(SharedMemory), 0666 | IPC_CREAT);
    
    #ifdef DEBUG
    printf("Created shared memory segment with ID: %d\n", shmID);
    #endif
    if (shmID < 0)

    {
        printf("Error creating shared memory segment\n");
        return 1;
    }
   
    // Attach shared memory segment to the address space of the calling process
    shMem = (SharedMemory*) shmat(shmID, NULL, 0);
    
    #ifdef DEBUG
    printf("DEBUG# Attached shared memory segment to address: %p\n", shMem);
    #endif
    
    if (shMem == (void*)-1)
    {
        printf("Error attaching shared memory segment\n");
        return 1;
    }

    // Initialize shared memory
    shMem->numUsers = 0;

    // Initialize semaphore
    sem_unlink("MOBILE_USER_MUTEX"); // remove any previous semaphore with the same name
    mobileUserMutex = sem_open("MOBILE_USER_MUTEX", O_CREAT | O_EXCL, 0666, 1); // create a new semaphore with initial value 1
    if (mobileUserMutex == SEM_FAILED)
    {
        printf("Error creating semaphore\n");
        return 1;
    }

    if(fork() == 0){ 
        // Child process
        sem_wait(mobileUserMutex); // wait for the semaphore to be available
        addUserToSharedMemory(shMem, 3, 300); // add user to shared memory
    
        #ifdef DEBUG
        printf("DEBUG# Process %d added user to shared memory\mn", getpid());
        #endif
 
        sem_post(mobileUserMutex); // release the semaphore
        exit(0); // exit the child process
    } else {
        // Parent process
        sem_wait(mobileUserMutex); // wait for the semaphore to be available
        addUserToSharedMemory(shMem, 4, 400); // add user to shared memory

        #ifdef DEBUG 
        printf("DEBUG# Process %d added user to shared memory\n", getpid());
        #endif

        sem_post(mobileUserMutex); // release the semaphore
    }

    // Wait for child process to finish
    wait(NULL);
    printSharedMemory(shMem);    

    return 0; 
}