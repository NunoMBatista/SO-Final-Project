#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <semaphore.h>
#include <fcntl.h>
#include <pthread.h>

typedef struct Config {
    int MAX_MOBILE_USERS;
    int QUEUE_POS;
    int AUTH_SERVERS_MAX;
    int AUTH_PROC_TIME;
    int MAX_VIDEO_WAIT;
    int MAX_OTHERS_WAIT;
} Config;

typedef struct client_info {
    int client_id;
    int plafond;
} client_info;

client_info * shared_memory;
int shmid;
Config config;
sem_t * log_semaphore;

void write_log_file(char *message);
void read_config_file(char *file_path);
void * sender_code(void * arg);
void * receiver_code(void * arg);
void create_processes();
int create_message_queue();
int create_shared_memory();
void sigint_handler(int sig_num);
void cleanup();

void write_log_file(char *message) {
    sem_wait(log_semaphore);
    FILE *file = fopen("log.txt", "a");
    if (file == NULL) {
        fprintf(stderr, "Error opening log file\n");
        write_log_file("ERROR OPENING LOG FILE\n");
        return;
    }
    time_t rawtime;
    struct tm* timeinfo;
    char buffer[80];

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, 80, "%d-%m-%Y %I:%M:%S", timeinfo);
    fprintf(file, "%s - %s", buffer, message);
    printf("%s - %s", buffer, message);
    fclose(file);
    sem_post(log_semaphore);
    return;
}

void read_config_file(char *file_path) {
    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        fprintf(stderr, "Error opening file\n");
        write_log_file("ERROR OPENING CONFIG FILE\n");
        exit(1);
    }
    fseek(file, 0, SEEK_END);
    if (ftell(file) == 0) {
        fprintf(stderr, "Empty file\n");
        write_log_file("EMPTY CONFIG FILE\n");
        exit(1);
    }
    rewind(file);
    int num_reads = fscanf(file, "%d\n%d\n%d\n%d\n%d\n%d", &config.MAX_MOBILE_USERS, &config.QUEUE_POS, &config.AUTH_SERVERS_MAX, &config.AUTH_PROC_TIME, &config.MAX_VIDEO_WAIT, &config.MAX_OTHERS_WAIT);
    if (num_reads != 6 || config.MAX_MOBILE_USERS <= 0 || config.QUEUE_POS <= 0 || config.AUTH_SERVERS_MAX <= 0 || config.AUTH_PROC_TIME <= 0 || config.MAX_VIDEO_WAIT <= 0 || config.MAX_OTHERS_WAIT <= 0) {
        fprintf(stderr, "Invalid configuration values\n");
        write_log_file("ERROR READING CONFIG FILE\n");
        exit(1);
    }
    fclose(file);
    return;
    
}

void * sender_code(void * arg) {
    arg = arg;
    printf("THREAD SENDER STARTED\n");
    write_log_file("THREAD SENDER STARTED\n");
    return NULL;
}

void * receiver_code(void * arg) {
    arg = arg;
    printf("THREAD RECEIVER STARTED\n");
    write_log_file("THREAD RECEIVER STARTED\n");
    return NULL;
}

void create_processes() {
    pid_t pid = fork();
    
    if (pid < 0) {
        fprintf(stderr, "Fork failed");
        write_log_file("ERROR CREATING AUTHORIZATION_REQUEST_MANAGER\n");
        return;
    }

    if (pid == 0) {
        pthread_t sender, receiver;
        printf("PROCESS AUTHORIZATION_REQUEST_MANAGER CREATED\n");
        write_log_file("PROCESS AUTHORIZATION_REQUEST_MANAGER CREATED\n");

        pthread_create(&sender, NULL, sender_code, NULL);
        pthread_create(&receiver, NULL, receiver_code, NULL);
        pthread_join(sender, NULL);
        pthread_join(receiver, NULL);
        exit(0);
    }
    pid = fork();

    if (pid < 0) {
        fprintf(stderr, "Fork failed");
        write_log_file("ERROR CREATING MONITOR ENGINE\n");
        return;
    }

    if (pid == 0) {
        printf("Monitor engine %d\n", getpid());
        write_log_file("PROCESS MONITOR_ENGINE CREATED\n");
        exit(0);
    }
    printf("System manager %d\n", getpid());
    return;
}

int create_message_queue() {
    // Replace with actual message queue creation code
    return 0;
}

int create_shared_memory() {
	shmid = shmget(IPC_PRIVATE, sizeof(client_info) * config.MAX_MOBILE_USERS, IPC_CREAT | 0777); 
    if (shmid == -1) {
        fprintf(stderr, "Error creating shared memory\n");
        write_log_file("ERROR CREATING SHARED MEMORY\n");
        return 1;
    }
    shared_memory = (client_info *) shmat(shmid, NULL, 0);  
    if (shared_memory == (void *) -1) {
        fprintf(stderr, "Error attaching shared memory\n");
        write_log_file("ERROR ATTACHING SHARED MEMORY\n");
        return 1;
    } 
    printf("Shared memory created\n");
    return 0;
}

void sigint_handler(int sig_num) {
    if (sig_num == SIGINT) {
        write_log_file("SIGINT RECEIVED\n");
        write_log_file("SYSTEM_MANAGER CLOSING\n");
        cleanup();
        exit(0);
    }
    exit(0);
}

void authorization_requests_manager() {
    pthread_t sender_tid, receiver_tid;
    pthread_create(&sender_tid, NULL, sender_code, NULL);
    pthread_create(&receiver_tid, NULL, receiver_code, NULL);
    pthread_join(sender_tid, NULL);
    pthread_join(receiver_tid, NULL);
}

void monitor_engine() {
    // Monitor engine code here
}


void cleanup() {
    shmdt(shared_memory);
    shmctl(shmid, IPC_RMID, 0);
    write_log_file("5G_AUTH_PLATFORM SIMULATOR CLOSING\n");
    sem_close(log_semaphore);   
    sem_unlink("log_semaphore");
    
}

void clear_log_file() {
    FILE *file = fopen("log.txt", "w");
    if (file == NULL) {
        fprintf(stderr, "Error opening log file\n");
        return;
    }
    fclose(file);
}


int main(int argc, char *argv[]) {
    clear_log_file();
    log_semaphore = sem_open("log_semaphore", 0);

    if (log_semaphore != SEM_FAILED) {
        sem_close(log_semaphore);
        sem_unlink("log_semaphore");
    }

    log_semaphore = sem_open("log_semaphore", O_CREAT, 0777, 1);
1    if (log_semaphore == SEM_FAILED) {
        fprintf(stderr, "Error creating semaphore\n");
        write_log_file("ERROR CREATING SEMAPHORE\n");
        return 1;
    }

    signal(SIGINT, sigint_handler);
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <config_file>\n", argv[0]);
        write_log_file("ERROR: INVALID ARGUMENTS\n");
        return 1;
    }    
    write_log_file("5G_AUTH_PLATFORM SIMULATOR STARTING\n");
    write_log_file("PROCESS SYSTEM_MANAGER CREATED\n");
    
    read_config_file(argv[1]);

    create_processes();
    create_message_queue();
    create_shared_memory();

    int status;
    for (int i = 0; i < 2; ++i){
        pid_t pid = wait(&status);
        printf("waited for process %d\n", pid);
    }
    write_log_file("SYSTEM_MANAGER CLOSING\n");
    cleanup();
    return 0;
}
