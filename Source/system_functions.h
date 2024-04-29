/*
  Authors: 
    Nuno Batista uc2022216127
    Miguel Castela uc2022212972
*/
#ifndef SYSTEM_MANAGER_STARTUP_H
#define SYSTEM_MANAGER_STARTUP_H

int read_config_file(char *filename);
void* receiver_thread();
void* sender_thread();
int initialize_system(char *config_file);
int create_monitor_engine();

int create_pipes();
int create_auth_manager();
int auth_engine_process(int id);

int create_shared_memory();
int create_semaphores();
int create_aux_semaphores();

int create_auth_engines();
int add_mobile_user(int user_id, int plafond);
int create_fifo_queues();
void parse_and_send(char *message);
void print_shared_memory();
void write_to_log(char *message);
void clean_up();
void signal_handler(int signal);

int add_mobile_user(int user_id, int plafond);
int get_user_index(int user_id);
int remove_from_user(int user_id, int amount);
int deactivate_user(int user_id);

int create_message_queue();

void deploy_extra_engine();
void print_progress(int current, int max);

void kill_auth_engine(int signal);

void sleep_milliseconds(int milliseconds);
#endif