#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    // Parse command line arguments
    if (argc != 7) {
        printf("Usage: %s plafond num_requests video_interval music_interval social_interval data_to_reserve\n", argv[0]);
        return 1;
    }
    int plafond = atoi(argv[1]), num_requests = atoi(argv[2]), video_interval = atoi(argv[3]), music_interval = atoi(argv[4]), social_interval = atoi(argv[5]), data_to_reserve = atoi(argv[6]);
    if (plafond <= 0 || num_requests <= 0 || video_interval <= 0 || music_interval <= 0 || social_interval <= 0 || data_to_reserve <= 0) {
        printf("All arguments must be positive integers!\n");
        return 1;
    }
    printf("Mobile process started with PID %d\n", getpid());
    printf("The arguments are: plafond = %d, num_requests = %d, video_interval = %d, music_interval = %d, social_interval = %d, data_to_reserve = %d\n", plafond, num_requests, video_interval, music_interval, social_interval, data_to_reserve);

    return 0;
}