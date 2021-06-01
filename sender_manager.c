//
// Created by matteo on 29/05/21.
//

#include "err_exit.h"
#include "defines.h"
#include "shared_memory.h"
#include "semaphore.h"
#include "fifo.h"
#include "pipe.h"
#include <sys/shm.h>
#include <sys/msg.h>
#include <fcntl.h>

bool wait_time = true;

void sigHandler(int sig){
  if(sig == SIGUSR1){
    sleep(5);
  }

  if(sig == SIGCONT){
    wait_time = false;
  }

}

int main(int argc, char * argv[]) {

  struct ipc historical[10] = {};
  //=================================================================
  //impostazione per la gestione dei segnali

  // set of signals (N.B. it is not initialized!)
  sigset_t mySet, prevSet;
  // initialize mySet to contain all signals
  sigfillset(&mySet);
  // remove SIGINT from mySet
  sigdelset(&mySet, SIGTERM);
  sigdelset(&mySet, SIGUSR1);
  sigdelset(&mySet, SIGCONT);

  // blocking all signals but SIGTERM, SIGUSR1, SIGCONT
  sigprocmask(SIG_SETMASK, &mySet, NULL);

  if(signal(SIGUSR1, sigHandler) == SIG_ERR){
    ErrExit("signal failed");
  }

  bool *check_time = &wait_time; //modifica il wait_time

 //===================================================================
 //creazione msg queue

  struct mymsg{
      long mtype;
      struct msg message;
  }m;

  key_t msgKey = 01110001;
  int msqid = msgget(msgKey, IPC_CREAT | S_IRUSR | S_IWUSR);

  if(msqid == -1){
    ErrExit("msgget failed");
  }

  ssize_t mSize = sizeof(struct mymsg) - sizeof(long);

  sprintf(historical[0].idKey, "%x", msgKey);
  strcpy(historical[0].ipc, "Q");
  strcpy(historical[0].creator, "SM");
  historical[0] = get_time(historical[0], 'c');

  //==================================================================================
  //creazione della message queue tra Sender e Hackler

  struct signal sigInc; //MQ per IncreaseDelay

  key_t mqInc_Key = ftok("receiver_manager.c", 'D');
  int mqInc_id = msgget(mqInc_Key, IPC_CREAT | S_IRUSR | S_IWUSR);

  if(mqInc_id == -1){
    ErrExit("internal msgget failed");
  }

  sprintf(historical[1].idKey, "%x", mqInc_Key);
  strcpy(historical[1].ipc, "Q-INC");
  strcpy(historical[1].creator, "SM");
  historical[1] = get_time(historical[1], 'c');

  struct signal sigRmv; //MQ per RemoveMSG


  key_t mqRmv_Key = ftok("receiver_manager.c", 'E');
  int mqRmv_id = msgget(mqRmv_Key, IPC_CREAT | S_IRUSR | S_IWUSR);

  if(mqRmv_id == -1){
    ErrExit("internal msgget failed");
  }

  sprintf(historical[2].idKey, "%x", mqRmv_Key);
  strcpy(historical[2].ipc, "Q-RMV");
  strcpy(historical[2].creator, "SM");
  historical[2] = get_time(historical[2], 'c');


  struct signal sigSnd; //MQ per SendMSG


  key_t mqSnd_Key = ftok("receiver_manager.c", 'F');
  int mqSnd_id = msgget(mqSnd_Key, IPC_CREAT | S_IRUSR | S_IWUSR);

  if(mqSnd_id == -1){
    ErrExit("internal msgget failed");
  }

  sprintf(historical[3].idKey, "%x", mqSnd_Key);
  strcpy(historical[3].ipc, "Q-SND");
  strcpy(historical[3].creator, "SM");
  historical[3] = get_time(historical[3], 'c');

 //==================================================================
 //creazione shared memory (SH) tra Senders & Receiver

  key_t shmKey = 01101101;
  int shmid;
  shmid = alloc_shared_memory(shmKey, sizeof(struct message));
  struct message *messageSH = (struct message *)get_shared_memory(shmid, 0);

  sprintf(historical[4].idKey, "%x", shmKey);
  strcpy(historical[4].ipc, "SH");
  strcpy(historical[4].creator, "SM");
  historical[4] = get_time(historical[4], 'c');

  //==================================================================
  //creazione shared memory per la condivisione del puntatore

  key_t shmKey2 = ftok("receiver_manager.c", 'A');

  int shmid2;
  shmid2 = alloc_shared_memory(shmKey2, sizeof(struct address));
  struct address *address = (struct address *)get_shared_memory(shmid2, 0);

  address->ptr = messageSH;

  sprintf(historical[5].idKey, "%x", shmKey2);
  strcpy(historical[5].ipc, "SH2");
  strcpy(historical[5].creator, "SM");
  historical[5] = get_time(historical[5], 'c');

  //==================================================================================
  //creazione del semaforo per la scrittura sulla SH

  key_t semKey = 01110011;
  int semid = semget(semKey, 1, IPC_CREAT | S_IRUSR | S_IWUSR);

  if(semid == -1){
    ErrExit("semget failed");
  }

  unsigned short semInitVal[] = {1};
  union semun arg;
  arg.array = semInitVal;

  if(semctl(semid, 0, SETALL, arg) == -1){
    ErrExit("semctl failed");
  }

  sprintf(historical[6].idKey, "%x", semKey);
  strcpy(historical[6].ipc, "SEM");
  strcpy(historical[6].creator, "SM");
  historical[6] = get_time(historical[6], 'c');

  //=================================================================================
  //creazione del semaforo che permetterà di scrivere i file F8 e F9 prima di essere letti

  key_t semKey2 = ftok("receiver_manager.c", 'B');
  int semid2 = semget(semKey2, 1, IPC_CREAT | S_IRUSR | S_IWUSR);

  if(semid2 == -1){
    ErrExit("semget failed SEMAPHORE 2 (SM)");
  }

  unsigned short semInitVal2[] = {2};
  union semun arg2;
  arg2.array = semInitVal2;

  if(semctl(semid2, 0, SETALL, arg2) == -1){
    ErrExit("semctl failed");
  }

  sprintf(historical[7].idKey, "%x", semKey2);
  strcpy(historical[7].ipc, "SEM2");
  strcpy(historical[7].creator, "SM");
  historical[7] = get_time(historical[7], 'c');


  //=================================================================================
  //creazione fifo

  int fifo;

  do{
    fifo = open("OutputFiles/my_fifo.txt", O_WRONLY);
  }while(fifo == -1);

  //=================================================================================
  //creazione pipe e allocazione delle variabili necessarie

  int pipe1[2];
  int pipe2[2];
  char pipe_1[5] = "";
  char pipe_2[5] = "";


  create_pipe(pipe1);
  create_pipe(pipe2);

  fcntl(pipe1[0], F_SETFL, O_NONBLOCK);
  fcntl(pipe2[0], F_SETFL, O_NONBLOCK);

  strcpy(historical[8].ipc, "PIPE1");
  strcpy(historical[9].ipc, "PIPE2");
  strcpy(historical[8].creator, "SM");
  strcpy(historical[9].creator, "SM");

  sprintf(pipe_1, "%d", pipe1[0]);
  strcpy(historical[8].idKey, pipe_1);
  sprintf(pipe_1, "%d", pipe1[1]);
  strcat(historical[8].idKey, "/");
  strcat(historical[8].idKey, pipe_1);

  sprintf(pipe_2, "%d", pipe2[0]);
  strcpy(historical[9].idKey, pipe_2);
  sprintf(pipe_2, "%d", pipe2[1]);
  strcat(historical[9].idKey, "/");
  strcat(historical[9].idKey, pipe_2);

  historical[8] = get_time(historical[8], 'c');
  historical[9] = get_time(historical[9], 'c');



  int F1 = open("OutputFiles/F1.csv", O_RDWR | O_CREAT, S_IRWXU); //creiamo F1.csv con permessi di lettura scrittura

  if(F1 == -1){
    ErrExit("open F1 failed");
  }

  int F2 = open("OutputFiles/F2.csv", O_RDWR | O_CREAT, S_IRWXU); //creiamo F1.csv con permessi di lettura scrittura

  if(F2 == -1){
    ErrExit("open F2 failed");
  }

  int F3 = open("OutputFiles/F3.csv", O_RDWR | O_CREAT, S_IRWXU); //creiamo F3.csv con permessi di lettura scrittura

  if(F3 == -1){
    ErrExit("open F3 failed");
  }

  int F10 = open("OutputFiles/F10.csv", O_RDWR | O_CREAT, S_IRWXU);

  if(F10 == -1){
    ErrExit("open F10 failed");
  }

  pid_t pid_S[3] = {0, 0, 0}; //@param contiene i pid dei processi
  int process = 0;

  //intestazione dei file F1, F2, F3
  const char heading[] = "Id;Message;IDSender;IDReceiver;TimeArrival;TimeDeparture\n";

  pid_t pid;
  pid = fork(); //creo S1

  if(pid != 0){ //sono nel padre
    pid_S[0] = pid; //salvo pid di S1
  }else if(pid == 0){ //sono in S1
    process = 1;
  }else{
    ErrExit("Error on child S1");
  }

  if(pid != 0){//sono nel padre
    pid = fork(); //creo S2
    if(pid != 0) {
      pid_S[1] = pid; //salvo pid di S2
    }else if(pid == 0){ //sono in S2
      process = 2;
    }else{
      ErrExit("Error on child S2");
    }
  }

  if(pid != 0){//sono nel padre
    pid = fork(); //creo S3
    if(pid != 0) {
      pid_S[2] = pid; //salvo pid di S3
    }else if(pid == 0){ //sono in S3
      process = 3;
    }else{
      ErrExit("Error on child S3");
    }
  }


  if(process == 1){


    //dichiarazione variabili di S1;
    ssize_t numRead;
    ssize_t numWrite;
    ssize_t nBys;
    struct container msgFile = {};
    struct msg message = {};
    char tmp;
    int i = 0;
    bool finish = false;
    bool check = false;


    if(close(pipe1[0]) == -1){
      ErrExit("Close pipe1 read-end failed");
    }

    getcwd(path, PATH_SZ);
    strcat(path, argv[1]);

    int F0 = open(path, O_RDONLY, S_IRUSR | S_IRGRP | S_IROTH); //apriamo F0.csv in sola lettura

    if(F0 == -1){
      ErrExit("open F0 failed");
    }

    lseek(F0, strlen(heading) - 3, SEEK_CUR); //ci spostiamo dopo l'intestazione (heading)

    numWrite = write(F1, heading, strlen(heading));

    if(numWrite != strlen(heading)){
      ErrExit("write F1 failed");
    }

    while(1){
      if(finish == false) {
        numRead = read(F0, &tmp, sizeof(char));
        if(numRead == 0){
          if(tmp != '\n'){
            i++;
            tmp = '\n';
          }
          else{
            check = true;
          }
          finish = true;
        }
        if(check == false) {

          if (tmp != '\n') {
            buffer[i] = tmp;
            i++;
          } else {
            buffer[i] = '\0';

            i = 0;
            msgFile = get_time_arrival();
            message = fill_structure(buffer);

            pid = fork();

            if (pid == -1) {
              ErrExit("Fork failed! Child of S1 not created");
            }

            if (pid > 0) {
              //mandare il pid sulla MQ e cambio mtype
              sigInc.pid = pid;
              sigInc.mtype = 1;

              if (msgsnd(mqInc_id, &sigInc, sizeof(struct signal) - sizeof(long), 0) == -1) {
                if(errno != EINVAL) {
                  ErrExit("Sending pid to Hackler failed (S1 - INC)");
                }
              }

              sigRmv.pid = pid;
              sigRmv.mtype = 1;

              if (msgsnd(mqRmv_id, &sigRmv, sizeof(struct signal) - sizeof(long), 0) == -1) {
                if(errno != EINVAL ) {
                  ErrExit("Sending pid to Hackler failed (S1 - RMV)");
                }
              }

              sigSnd.pid = pid;
              sigSnd.mtype = 1;

              if (msgsnd(mqSnd_id, &sigSnd, sizeof(struct signal) - sizeof(long), 0) == -1) {
                if(errno != EINVAL ) {
                  ErrExit("Sending pid to Hackler failed (S1 - SND)");
                }
              }
            }
            if (pid == 0) {

              int sec;

              if ((sec = sleep(atoi(message.delS1))) > 0 && wait_time == true) {
                sleep(sec);
                *check_time = false;
              }

              msgFile = get_time_departure(msgFile);
              writeFile(msgFile, message, F1);

              if (strcmp(message.idSender, "S1") == 0) {

                if (strcmp(message.type, "Q") == 0) {
                  m.message = message;

                  if (strcmp(message.idReceiver, "R1") == 0) {
                    m.mtype = 1;
                  } else if (strcmp(message.idReceiver, "R2") == 0) {
                    m.mtype = 2;
                  } else if (strcmp(message.idReceiver, "R3") == 0) {
                    m.mtype = 3;
                  }

                  if (msgsnd(msqid, &m, mSize, 0) == -1) {
                    if(errno != EINVAL ) {
                      ErrExit("Message Send Failed[S1]");
                    }
                  }
                }

                if (strcmp(message.type, "SH") == 0) {
                  printf("Sender 1");
                  semOp(semid, 0, -1);  //entro nella sezione critica se il semaforo è a 1 altrimenti aspetto

                  messageSH = address->ptr;


                  //scrivo sulla shared memory il  messaggio
                  strcpy(messageSH->id, message.id);
                  strcpy(messageSH->message, message.message);
                  strcpy(messageSH->idSender, message.idSender);
                  strcpy(messageSH->idReceiver, message.idReceiver);
                  strcpy(messageSH->delS1, message.delS1);
                  strcpy(messageSH->delS2, message.delS2);
                  strcpy(messageSH->delS3, message.delS3);
                  strcpy(messageSH->type, message.type);


                  messageSH++;
                  address->ptr = messageSH;

                  //esco dalla sezione critica e rimetto il semaforo a 1 (libero)
                  printf("Sender 2\n");
                  semOp(semid, 0, 1);

                }
              } else {
                nBys = write(pipe1[1], &message, sizeof(message));
                if (nBys != sizeof(message)) {
                  if(errno != EPIPE && errno != EAGAIN) {
                    ErrExit("write to pipe1 failed");
                  }
                }
              }

              exit(0);
            }
          }
        }
      }
    }
    while(wait(NULL) != -1);


    exit(0);
  }else if(process == 2){

    if(close(pipe1[1]) == -1){
      ErrExit("Close pipe1 write-end failed");
    }
    if(close(pipe2[0]) == -1){
      ErrExit("Close pipe2 read-end failed");
    }

    ssize_t numRead;
    ssize_t numWrite;
    ssize_t nBys;
    struct container msgFile = {};
    struct msg message;

    numWrite = write(F2, heading, strlen(heading));

    if(numWrite != strlen(heading)){
      ErrExit("write F2 failed");
    }

    while(1){
      numRead = read(pipe1[0], &message, sizeof(struct msg));
      if(numRead == -1){
        if(errno != EPIPE && errno != EAGAIN) {
          ErrExit("Read from pipe1 failed");
        }
      }
      if(numRead > 0) {

        if (numRead == sizeof(struct msg)) {
          msgFile = get_time_arrival();
          pid = fork();

          if (pid == -1) {
            ErrExit("Fork failed! Child of S2 not created");
          }
          if(pid > 0){
            //mandare il pid sulla MQ e cambio mtype
            sigInc.pid = pid;
            sigInc.mtype = 2;

            if(msgsnd(mqInc_id, &sigInc, sizeof(struct signal) - sizeof(long), 0) == -1){
              if(errno != EINVAL ) {
                ErrExit("Sending pid to Hackler failed (S2 - INC)");
              }
            }

            sigRmv.pid = pid;
            sigRmv.mtype = 2;

            if(msgsnd(mqRmv_id, &sigRmv, sizeof(struct signal) - sizeof(long), 0) == -1){
              if(errno != EINVAL ) {
                ErrExit("Sending pid to Hackler failed (S2 - RMV)");
              }
            }

            sigSnd.pid = pid;
            sigSnd.mtype = 2;

            if(msgsnd(mqSnd_id, &sigSnd, sizeof(struct signal) - sizeof(long), 0) == -1){
              if(errno != EINVAL ) {
                ErrExit("Sending pid to Hackler failed (S2 - SND)");
              }
            }
          }

          if (pid == 0) {
            int sec;

            if ((sec = sleep(atoi(message.delS2))) > 0 && wait_time == true) {
              sleep(sec);
              *check_time = false;
            }

            msgFile = get_time_departure(msgFile);
            writeFile(msgFile, message, F2);

            if (strcmp(message.idSender, "S2") == 0) {

              if (strcmp(message.type, "Q") == 0) {
                m.message = message;

                if (strcmp(message.idReceiver, "R1") == 0) {
                  m.mtype = 1;
                } else if (strcmp(message.idReceiver, "R2") == 0) {
                  m.mtype = 2;
                } else if (strcmp(message.idReceiver, "R3") == 0) {
                  m.mtype = 3;
                }

                if (msgsnd(msqid, &m, mSize, 0) == -1) {
                  if(errno != EINVAL ) {
                    ErrExit("Message Send Failed[S2]");
                  }
                }
              }

              if (strcmp(message.type, "SH") == 0) {
                printf("Sender 3\n");
                semOp(semid, 0, -1);  //entro nella sezione critica se il semaforo è a 1 altrimenti aspetto

                messageSH = address->ptr;


                //scrivo sulla shared memory il  messaggio
                strcpy(messageSH->id, message.id);
                strcpy(messageSH->message, message.message);
                strcpy(messageSH->idSender, message.idSender);
                strcpy(messageSH->idReceiver, message.idReceiver);
                strcpy(messageSH->delS1, message.delS1);
                strcpy(messageSH->delS2, message.delS2);
                strcpy(messageSH->delS3, message.delS3);
                strcpy(messageSH->type, message.type);


                messageSH++;
                address->ptr = messageSH;

                //esco dalla sezione critica e rimetto il semaforo a 1 (libero)
                printf("Sender 4\n");
                semOp(semid, 0, 1);

              }
            } else if (strcmp(message.idSender, "S3") == 0) {
              nBys = write(pipe2[1], &message, sizeof(message));
              if (nBys != sizeof(message)) {
                if(errno != EPIPE && errno != EAGAIN) {
                  ErrExit("write to pipe2 failed");
                }
              }
            }
            exit(0);
          }
        } else {
          ErrExit("Read from pipe1 failed");
        }
      }
    }

    while(wait(NULL) != -1);


    exit(0);
  }else if(process == 3){

    if(close(pipe2[1]) == -1){
      ErrExit("Close pipe2 write-end failed");
    }

    ssize_t numRead;
    ssize_t numWrite;
    struct container msgFile = {};
    struct msg message;



    numWrite = write(F3, heading, strlen(heading));

    if(numWrite != strlen(heading)){
      ErrExit("write F3 failed");
    }

    while(1) {
      numRead = read(pipe2[0], &message, sizeof(struct msg));
      if(numRead ==-1) {
        if (errno != EPIPE && errno != EAGAIN) {
          ErrExit("Read from pipe2 failed");
        }
      }

      if(numRead > 0) {
        if (numRead == sizeof(struct msg)) {
          msgFile = get_time_arrival();
          pid = fork();

          if (pid == -1) {
            ErrExit("Fork failed! Child of S2 not created");
          }
          if(pid > 0){
            //mandare il pid sulla MQ e cambio mtype
            sigInc.pid = pid;
            sigInc.mtype = 3;

            if(msgsnd(mqInc_id, &sigInc, sizeof(struct signal) - sizeof(long), 0) == -1){
              if(errno != EINVAL ) {
                ErrExit("Sending pid to Hackler failed (S3 - INC)");
              }
            }

            sigRmv.pid = pid;
            sigRmv.mtype = 3;

            if(msgsnd(mqRmv_id, &sigRmv, sizeof(struct signal) - sizeof(long), 0) == -1){
              if(errno != EINVAL ) {
                ErrExit("Sending pid to Hackler failed (S3 - RMV)");
              }
            }

            sigSnd.pid = pid;
            sigSnd.mtype = 3;

            if(msgsnd(mqSnd_id, &sigSnd, sizeof(struct signal) - sizeof(long), 0) == -1){
              if(errno != EINVAL ) {
                ErrExit("Sending pid to Hackler failed (S3 - SND)");
              }
            }
          }
          if (pid == 0) {
            int sec;

            if ((sec = sleep(atoi(message.delS3))) > 0 && wait_time == true) {
              sleep(sec);
              *check_time = false;
            }

            msgFile = get_time_departure(msgFile);
            writeFile(msgFile, message, F3);

            if (strcmp(message.type, "Q") == 0) {
              m.message = message;

              if (strcmp(message.idReceiver, "R1") == 0) {
                m.mtype = 1;
              } else if (strcmp(message.idReceiver, "R2") == 0) {
                m.mtype = 2;
              } else if (strcmp(message.idReceiver, "R3") == 0) {
                m.mtype = 3;
              }

              if (msgsnd(msqid, &m, mSize, 0) == -1) {
                if(errno != EINVAL ) {
                  ErrExit("Message Send Failed[S3]");
                }
              }
            } else if (strcmp(message.type, "SH") == 0) {
              printf("Sender 5\n");
              semOp(semid, 0, -1);  //entro nella sezione critica se il semaforo è a 1 altrimenti aspetto

              messageSH = address->ptr;


              //scrivo sulla shared memory il  messaggio
              strcpy(messageSH->id, message.id);
              strcpy(messageSH->message, message.message);
              strcpy(messageSH->idSender, message.idSender);
              strcpy(messageSH->idReceiver, message.idReceiver);
              strcpy(messageSH->delS1, message.delS1);
              strcpy(messageSH->delS2, message.delS2);
              strcpy(messageSH->delS3, message.delS3);
              strcpy(messageSH->type, message.type);


              messageSH++;
              address->ptr = messageSH;

              //esco dalla sezione critica e rimetto il semaforo a 1 (libero)
              printf("Sender 6\n");
              semOp(semid, 0, 1);
            } else {
              ssize_t numWrite = write(fifo, &message, sizeof(struct msg));
              if(numWrite != sizeof(struct msg)){
                ErrExit("Write on fifo failed");
              }
            }
            exit(0);
          }
        }
      }
    }
    while(wait(NULL) != -1);

    exit(0);

  }else{

    writeF8(pid_S);
    printf("Sender 8\n");
    semOp(semid2, 0, -1);

    int status;

    while((pid = wait(&status)) != -1) {
      if (pid == pid_S[0]) {
        printf("S1 %d exited, status = %d\n", pid, WEXITSTATUS(status)); //qui sta eseguendo sicuramente il padre che ha nella variabile pid il pid reale del figlio che ha creato
      }else if(pid == pid_S[1]){
        printf("S2 %d exited, status = %d\n", pid, WEXITSTATUS(status));
      }else{
        printf("S3 %d exited, status = %d\n", pid, WEXITSTATUS(status));
      }
    }

  }

  if(close(F1) == -1){
    ErrExit("Close F1 failed");
  }

  if(close(F2) == -1){
    ErrExit("Close F2 failed");
  }

  if(close(F3) == -1){
    ErrExit("Close F3 failed");
  }

  if(close(fifo) == -1){
    ErrExit("Close fifo failed by sender");
  }

  historical[0] = get_time(historical[0], 'd');
  if(msgctl(msqid, 0, IPC_RMID) == -1){
    ErrExit("msgctl failed");
  }

  historical[1] = get_time(historical[1], 'd');
  if(msgctl(mqInc_id, 0, IPC_RMID) == -1){
    ErrExit("msgctl failed");
  }

  historical[2] = get_time(historical[2], 'd');
  if(msgctl(mqRmv_id, 0, IPC_RMID) == -1){
    ErrExit("msgctl failed");
  }

  historical[3] = get_time(historical[3], 'd');
  if(msgctl(mqSnd_id, 0, IPC_RMID) == -1){
    ErrExit("msgctl failed");
  }

  historical[4] = get_time(historical[4], 'd');
  free_shared_memory(messageSH);
  remove_shared_memory(shmid);

  historical[5] = get_time(historical[5], 'd');
  free_shared_memory(address);
  remove_shared_memory(shmid2);

  historical[6] = get_time(historical[6], 'd');
  if(semctl(semid, 0, IPC_RMID) == -1){
    ErrExit("semctl failed");
  }

  historical[7] = get_time(historical[7], 'd');
  if(semctl(semid2, 0, IPC_RMID) == -1){
    ErrExit("semctl failed");
  }

  historical[8] = get_time(historical[8], 'd');
  if(close(pipe1[0]) == -1){
    ErrExit("Close pipe1 read-end failed");
  }

  if(close(pipe1[1]) == -1){
    ErrExit("Close pipe1 write-end failed");
  }

  historical[9] = get_time(historical[9], 'd');
  if(close(pipe2[0]) == -1){
    ErrExit("Close pipe2 read-end failed");
  }

  if(close(pipe2[1]) == -1){
    ErrExit("Close pipe2 write-end failed");
  }

  for(int i = 0; i < 10; i++){
    writeF10(historical[i], F10);
  }

  sigprocmask(SIG_SETMASK, &prevSet, NULL);

  return 0;
}

