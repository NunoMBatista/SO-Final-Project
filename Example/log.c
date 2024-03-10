/*Autores:
  João Filipe Sequeira Montenegro Nº 2016228672
  João Miguel Rainho Mendes Nº 2016230975
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>

#include "global.h"
#include "sinais.h"

Globals globalVars;

void write_to_log(char* message){
  #ifdef DEBUG
  printf("A entrar em write_to_log...\n");
  #endif
  if(sem_wait(globalVars.semLog) != 0){
    perror("Erro ao decrementar semLog em write_to_log\n");
    cleanup(2);
  }
  #ifdef DEBUG
  printf("A escrever no log...\n");
  #endif
  //Copia os bytes de message[0] para a posiçao no mmapped file correspondente a log_ptr + ptr_pos
  memcpy(globalVars.log_ptr+globalVars.ptr_pos, message, strlen(message));
  #ifdef DEBUG
  printf("Passou do memcpy...\n");
  #endif
  globalVars.ptr_pos+=strlen(message);
  if(sem_post(globalVars.semLog) != 0){
    perror("Erro ao incrementar semLog em write_to_log\n");
    cleanup(2);
  }
}
