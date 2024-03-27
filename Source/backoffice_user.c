#include <sys/shm.h>
#include <semaphore.h> // POSIX 
#include <fcntl.h> // O_CREAT, O_EXCL
#include <unistd.h> // fork
#include <stdio.h>
#include <stdlib.h> // exit
#include <sys/wait.h> // wait
#include <sys/types.h>
#include <semaphore.h>
#include <signal.h>
#include "global.h"
#include "system_functions.h"
/*
    Execution instructions:
    ./backoffice_user
*/

int main(){
    char message[100];
    sprintf(message, "BACKOFFICE USER STARTING - PROCESS ID: %d", getpid());
    printf("%s\n", message);
    //write_to_log(message);
}