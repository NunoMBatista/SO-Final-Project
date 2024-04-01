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
int create_monitor_engine();
int create_auth_manager();
int create_shared_memory();
int add_mobile_user(int user_id, int plafond);
void print_shared_memory();
void write_to_log(char *message);
void clean_up();
void signal_handler(int signal);

#endif