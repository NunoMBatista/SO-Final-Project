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
#include <ctype.h>
#include "global.h"
#include "system_functions.h"

/*
    Execution instructions:
    ./mobile_user {plafond} {max_requests} {delta_video} {delta_music} {delta_social} {data_ammount}
*/

int is_positive_integer(char *str);

int main(int argc, char *argv[]){
    MobileUserData* user_data;
    user_data = (MobileUserData*) malloc(sizeof(MobileUserData));
    
    // Check correct number of arguments
    if (argc != 7){
        printf("<INCORRECT NUMBER OF ARGUMENTS>\n Correct usage: ./%s {plafond} {max_requests} {delta_video} {delta_music} {delta_social} {data_ammount}\n", argv[0]);
        return 1;
    }

    // Check if the arguments are all positive integers
    for(int i = 1; i < 7; i++){
        if(!is_positive_integer(argv[i])){
            printf("<ERROR> All arguments must be positive integers\n");
            return 1;
        }
        
    }

    // Read the arguments
    user_data->user_id = getpid();
    user_data->plafond = atoi(argv[1]); 
    int max_requests = atoi(argv[2]);
    int delta_video = atoi(argv[3]);
    int delta_music = atoi(argv[4]);
    int delta_social = atoi(argv[5]);
    int data_ammount = atoi(argv[6]);

    char message[200];
    sprintf(message, "MOBILE USER STARTING - USER ID: %d\n PLAFOND: %d\n MAX REQUESTS: %d\n DELTA VIDEO: %d\n DELTA MUSIC: %d\n DELTA SOCIAL: %d\n DATA AMMOUNT: %d\n", user_data->user_id, user_data->plafond, max_requests, delta_video, delta_music, delta_social, data_ammount);
    
    printf("%s", message);  

    return 0; 
}

int is_positive_integer(char *str) {
    while (*str) {
        // idigit is a function that checks if a character is a digit
        if (isdigit(*str) == 0) {
            return 0;
        }
        str++;
    }
    return 1;
}