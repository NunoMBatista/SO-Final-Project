#include <stdio.h>
#include <unistd.h>


int main() {
    printf("Backoffice process started with PID %d\n", getpid());
    // Handle Backoffice tasks here

    return 0;
}