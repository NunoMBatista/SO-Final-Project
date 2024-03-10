/*Autores:
  João Filipe Sequeira Montenegro Nº 2016228672
  João Miguel Rainho Mendes Nº 2016230975
*/
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/time.h>

#include "ficheiro.h"
#include "triagem.h"
#include "global.h"
#include "doutor.h"
#include "sinais.h"
#include "log.h"

int check_str_triage(char*);
int check_sigusr1(char*);

void show_stats(int signum);
void ignore_signal(int signum);

void trataPaciente_tempDoctor();
void* createTempDoctor();

Globals globalVars;
char buf[MAX_BUFFER];
int contPaciente=1;

int main(int argc, char** argv){
  Dados dados;
  FILE *fileptr = fopen("config.txt", "r");

  signal(SIGINT, cleanup);
  signal(SIGUSR1, show_stats);

  //Ignora os outros sinais, excepto SIGINT(2), SIGSEGV(11), e SIGUSR1(10)
  for(int i=1; i<31; i++){
    if((i!=2) && (i!=10) && (i!=11)){
      signal(i, ignore_signal);
    }
  }

  //Lê dados do ficheiro
  dados = readFile(fileptr);
  globalVars.TRIAGE=dados.triage;
  globalVars.DOCTORS=dados.doctors;
  globalVars.SHIFT_LENGTH=dados.shift_length;
  globalVars.MQ_MAX=dados.mq_max;

  globalVars.thread_triage = malloc(sizeof(pthread_t)*globalVars.TRIAGE);
  long ids[globalVars.TRIAGE];

  srand(time(NULL));

  //Cria as zonas de memoria partilhada
  if((globalVars.shmid = shmget(IPC_PRIVATE, MEM_SIZE, 0666 | IPC_CREAT)) < 0){
    perror("Erro ao criar segmento de memoria partilhada\n");
    cleanup(2);
  }
  if((globalVars.dadosPartilhados = shmat(globalVars.shmid, NULL, 0)) < 0){
    perror("Erro ao fazer attach da memoria partilhada\n");
    cleanup(2);
  }

  //Renomeia as zonas de memoria partilhada
  globalVars.n_pacientes_triados = globalVars.dadosPartilhados;
  globalVars.n_pacientes_atendidos = globalVars.dadosPartilhados+1;
  globalVars.total_time_before_triage = globalVars.dadosPartilhados+2;
  globalVars.total_time_before_atend = globalVars.dadosPartilhados+3;
  globalVars.total_time = globalVars.dadosPartilhados+4;

  //Cria mmf
  globalVars.log_fd = open("log.txt", O_RDWR|O_CREAT, 0600);

  lseek(globalVars.log_fd, LOG_SIZE-1, SEEK_SET);
  write(globalVars.log_fd, "", 1);

  globalVars.log_ptr = mmap(0, LOG_SIZE, PROT_WRITE|PROT_READ, MAP_SHARED, globalVars.log_fd, 0);
  globalVars.ptr_pos = 0;

  //Cria e abre named pipe
  #ifdef DEBUG
  printf("A criar e abrir FIFO\n");
  fflush(stdout);
  #endif
  if(mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0700) < 0){
    perror("Erro ao criar named pipe\n");
    cleanup(2);
  }
  if((globalVars.named_fd = open(PIPE_NAME, O_RDONLY)) < 0){
    perror("Erro ao abrir named pipe\n");
    cleanup(2);
  }

  //Cria as filas de mensagens
  if((globalVars.mq_id_thread = msgget(IPC_PRIVATE, O_CREAT|0700)) < 0){
    perror("Erro ao criar message queue triage\n");
    cleanup(2);
  }

  if((globalVars.mq_id_doctor = msgget(IPC_PRIVATE, O_CREAT|0700)) < 0){
    perror("Erro ao criar message queue doctor\n");
    cleanup(2);
  }

  //Inicializa dados partilhados
  globalVars.numDadosPartilhados = 5;
  for(int i=0; i<globalVars.numDadosPartilhados; i++){
    globalVars.dadosPartilhados[i] = 0;
  }

  sem_unlink("SemLog");
  sem_unlink("SemMQ");
  sem_unlink("SemSHM");

  //Inicializa semaphore
  #ifdef DEBUG
  printf("A inicializar semaforos\n");
  fflush(stdout);
  #endif
  if((globalVars.semLog = sem_open("SemLog", O_CREAT|O_EXCL, 0600, 1)) == SEM_FAILED){
    perror("Erro ao Inicializar SemLog\n");
    cleanup(SIGINT);
  }
  if((globalVars.semMQ = sem_open("SemMQ", O_CREAT, 0600, 1)) == SEM_FAILED){
    perror("Erro ao inicializar SemMQ\n");
    cleanup(SIGINT);
  }
  if((globalVars.semSHM = sem_open("SemSHM", O_CREAT, 0600, 1)) == SEM_FAILED){
    perror("Erro ao inicializar SemSHM\n");
    cleanup(SIGINT);
  }

  //Inicializa variaveis de condiçao e mutexes
  #ifdef DEBUG
  printf("A inicializar variaveis de condiçao e mutexes\n");
  fflush(stdout);
  #endif
  pthread_mutex_init(&globalVars.mutex_doctor, NULL);
  pthread_cond_init(&globalVars.cond_var_doctor, NULL);

  globalVars.checkRequestedDoctor = 0;
  globalVars.requestDoctor = 0;

  //Cria a thread que vai criar processos doutor
  if(pthread_create(&globalVars.thread_doctors, NULL, createDoctors, 0)!=0){
    perror("Erro ao criar thread thread_doctors\n");
    cleanup(2);
  }

  //Thread que vai criar o doutor temporario, se for preciso
  if(pthread_create(&globalVars.temp_doctor_thread, 0, createTempDoctor, NULL)!=0){
    perror("Erro ao criar thread temp_doctor_thread\n");
    cleanup(2);
  }

  globalVars.newTriage = -1;

  usleep(100);

  //Cria as threads de triagem
  for(int i=0; i<globalVars.TRIAGE; i++){
    if(pthread_create(&globalVars.thread_triage[i], NULL, triaPaciente, &ids[i]) != 0) printf("Erro ao criar thread!\n");
  }

  //Recebe os pacientes pelo named pipe
  while(1){
    Paciente paciente;
    paciente.mtype = MTYPE;
    int temp;
    char* tokens, *ptr, *bufTemp;
    int nread = read(globalVars.named_fd, buf, sizeof(buf));
    buf[nread-1] = '\0'; //\n
    printf("Recebeu: %s\n", buf);

    //Verifica se o formato é num num num num
    if(buf[0] >= '0' && buf[0] <= '9'){
      #ifdef DEBUG
      printf("Entrou no if de num num num num\n");
      #endif
      //Varios pacientes, faz um for com o primeiro token
      bufTemp = strdup(buf);
      tokens = strtok(bufTemp, " ");
      sscanf(tokens, "%d", &temp);
      for(int i=0; i<temp; i++){
        bufTemp = strdup(buf);
        tokens = strtok(bufTemp, " ");
        //Trata os dados do paciente
        //Cria um nome para o paciente, usando a data e um contador
        struct tm* tm;
        time_t t = time(NULL);
        tm = localtime(&t); //localtime() preenche o struct tm usando o numero de segundos em tempo Unix
        sprintf(paciente.nome, "%d%d%d-%d", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, contPaciente);

        paciente.arrival_time = time(NULL);
        tokens = strtok(NULL, " "); //Tempo de triagem
        paciente.triage_time = strtoimax(tokens, &ptr, 10);
        tokens = strtok(NULL, " "); //Tempo de atendimento
        paciente.atend_time = strtoimax(tokens, &ptr, 10);
        tokens = strtok(NULL, " "); //prioridade
        paciente.prioridade = strtoimax(tokens, &ptr, 10);
        contPaciente++;

        //Inicia o contador do tempo antes da triagem e o tempo total
        struct timespec cont_tempo;
        clock_gettime(CLOCK_REALTIME, &cont_tempo);
        paciente.before_triage = cont_tempo.tv_nsec; //Segundo atual em precisao de nanosegundo
        paciente.total_time = cont_tempo.tv_nsec;

        //Envia para a message queue
        msgsnd(globalVars.mq_id_thread, &paciente, sizeof(Paciente)-sizeof(long), 0);
      }
    }

    //Não é do formato num num num num
    else if((buf[0] >= 'A' && buf[0] <= 'z') && (check_str_triage(buf) != 1) && (check_sigusr1(buf) != 1)){
      #ifdef DEBUG
      printf("Entrou no else if de nome num num num\n");
      #endif
      //È do formato nome num num num
      tokens = strtok(buf, " "); //Nome
      strcpy(paciente.nome, tokens);
      paciente.arrival_time = time(NULL);
      tokens = strtok(NULL, " "); //Tempo de triagem
      paciente.triage_time = strtoimax(tokens, &ptr, 10);
      tokens = strtok(NULL, " "); //Tempo de atendimento
      paciente.atend_time = strtoimax(tokens, &ptr, 10);
      tokens = strtok(NULL, " "); //prioridade
      paciente.prioridade = strtoimax(tokens, &ptr, 10);
      contPaciente++;

      //Inicia o contador do tempo
      struct timespec cont_tempo;
      clock_gettime(CLOCK_REALTIME, &cont_tempo);
      paciente.before_triage = cont_tempo.tv_nsec;

      //Envia para a message queue
      msgsnd(globalVars.mq_id_thread, &paciente, sizeof(Paciente)-sizeof(long), 0);
    }

    //Não é do formato nome num num num
    else if(check_str_triage(buf)){
      #ifdef DEBUG
      printf("Entrou no else if TRIAGE=x\n");
      #endif
      //É do formato TRIAGE=??
      tokens = strtok(buf, "="); tokens = strtok(NULL, "=");
      int temp = strtoimax(tokens, &ptr, 10);
      #ifdef DEBUG
      printf("Temp = %d\n", temp);
      #endif
      int oldTriage = (globalVars.newTriage==-1?0:globalVars.newTriage);
      #ifdef DEBUG
      printf("oldTriage = %d\n", oldTriage);
      #endif
      if(temp>globalVars.newTriage){
        globalVars.newTriage = temp;
        //Só permite incrementar uma vez as triagens
        if(oldTriage == 0){
          #ifdef DEBUG
          printf("Entrou no if oldTriage == 0\n");
          #endif
          globalVars.new_thread_triage = malloc(sizeof(pthread_t)*(globalVars.newTriage-globalVars.TRIAGE)); //Para apenas adicionar ao total
          for(int i=0; i<globalVars.newTriage - globalVars.TRIAGE; i++){ //Para apenas criar o numero necessario de threads
            if(pthread_create(&globalVars.new_thread_triage[i], NULL, triaPaciente, &ids[i]) != 0) printf("Erro ao criar thread!\n");
          }
        }
        else{
          printf("Já foram adicionadas novas triagens\n");
        }
      }
      else{
        printf("Numero de novas triagens tem de ser superior ao que já existe\n");
      }
    }

    else if(check_sigusr1(buf)){
      #ifdef DEBUG
      printf("Entrou no else if STATS\n");
      #endif
      //É do formato STATS, envia sinal SIGUSR1
      kill(getpid(), SIGUSR1);
    }
    //É um formato desconhecido
    else{
      printf("Formato desconhecido!\n");
    }

    globalVars.named_fd = open(PIPE_NAME, O_RDONLY);
  }
}

int check_str_triage(char* str){ //Verifica se str é TRIAGE
  return
    str[0] == 'T' &&
    str[1] == 'R' &&
    str[2] == 'I' &&
    str[3] == 'A' &&
    str[4] == 'G' &&
    str[5] == 'E';
}

int check_sigusr1(char* str){ //Verifica se str é STATS
  return
    str[0] == 'S' &&
    str[1] == 'T' &&
    str[2] == 'A' &&
    str[3] == 'T' &&
    str[4] == 'S';
}
