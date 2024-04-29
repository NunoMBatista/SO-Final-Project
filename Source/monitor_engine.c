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

pthread_t periodic_notifications_thread; // Implement later

// Monitor Engine main function
// Will notify users once they have reached 80%, 90% and 100% of their plafond
void monitor_engine_process(){
    

    while(1){
        
        // Wait for an authorization request to notify
        pthread_mutex_lock(&auxiliary_shm->monitor_engine_mutex);
        pthread_cond_wait(&auxiliary_shm->monitor_engine_cond, &auxiliary_shm->monitor_engine_mutex);

        // Read the whole shared memory and find possible notifications to send



        pthread_mutex_unlock(&auxiliary_shm->monitor_engine_mutex);
    }


}