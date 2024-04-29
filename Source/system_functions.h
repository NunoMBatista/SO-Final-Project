/*
  Authors: 
    Nuno Batista uc2022216127
    Miguel Castela uc2022212972
*/
#ifndef SYSTEM_MANAGER_STARTUP_H
#define SYSTEM_MANAGER_STARTUP_H


// Initialization (system_initialization.c)
int read_config_file(char *filename);
int initialize_system(char *config_file);

// Structures creation (structures_creation.c)
int create_pipes();
int create_auth_engines();
int create_message_queue();
int create_auth_manager();
int create_semaphores();
int create_fifo_queues();
int create_shared_memory();
int create_aux_semaphores();
int create_monitor_engine();

// ARM threads (arm_threads.c)
void* sender_thread();
void* receiver_thread();
void deploy_extra_engine();
void kill_auth_engine(int signal);
void parse_and_send(char *message);

// Authentication Engine functions (auth_engine.c)
int auth_engine_process(int id);
int add_mobile_user(int user_id, int plafond);
int get_user_index(int user_id);
int remove_from_user(int user_id, int amount);
int deactivate_user(int user_id);

// Cleanup (clean_up.c)
void clean_up();
void signal_handler(int signal);

// MISC (general_functions.c)
void write_to_log(char *message);
void print_progress(int current, int max);
void print_shared_memory();
void sleep_milliseconds(int milliseconds);

#endif