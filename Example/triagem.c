/*Autores:
  João Filipe Sequeira Montenegro Nº 2016228672
  João Miguel Rainho Mendes Nº 2016230975
*/
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <sys/mman.h>
#include "triagem.h"
#include "global.h"
#include "log.h"

//Operaçoes triagem

Globals globalVars;

void* triaPaciente(void* t){
  Paciente paciente;
  printf("Thread %ld criada\n", pthread_self());
  //Escreve no log
  char message[MAX_LOG_MESSAGE];
  sprintf(message, "Thread %ld criada\n", pthread_self());
  #ifdef DEBUG
  printf("Message = %s\n", message);
  #endif
  write_to_log(message);

  while(1){
    if(msgrcv(globalVars.mq_id_thread, &paciente, sizeof(Paciente)-sizeof(long), MTYPE, 0) < 0){
      perror("Erro ao receber da queue em triaPaciente\n");
    }

    //Pára o contador do tempo antes da triagem e Calcula o tempo
    struct timespec cont_tempo;
    clock_gettime(CLOCK_REALTIME, &cont_tempo);
    int64_t temp = cont_tempo.tv_nsec;
    temp-=paciente.before_triage;

    //Coloca o tempo no total de microsegundos
    pthread_mutex_lock(&globalVars.mutex_doctor);
    (*globalVars.total_time_before_triage)+=temp;
    #ifdef DEBUG
    printf("Total ms antes triagem: %ld\n", *globalVars.total_time_before_triage);
    fflush(stdout);
    #endif
    pthread_mutex_unlock(&globalVars.mutex_doctor);

    printf("Thread [%ld] recebeu paciente %s\n", pthread_self(), paciente.nome);

    //Espera pelo tempo de triagem
    usleep(paciente.triage_time);

    //Inicia o contador de tempo entre a triagem e ser atendido
    clock_gettime(CLOCK_REALTIME, &cont_tempo);
    paciente.before_atend = cont_tempo.tv_nsec;

    if(msgsnd(globalVars.mq_id_doctor, &paciente, sizeof(Paciente)-sizeof(long), 0) < 0){
      perror("Erro ao enviar para a queue em triaPaciente\n");
    }
    else{
      printf("Thread [%ld] enviou paciente %s\n", pthread_self(), paciente.nome);
      //aumenta o numero de pacientes triados
      pthread_mutex_lock(&globalVars.mutex_doctor);
      (*globalVars.n_pacientes_triados)++;
      pthread_mutex_unlock(&globalVars.mutex_doctor);
    }
  }
}
