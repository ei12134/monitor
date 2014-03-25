#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

// parse unsigned long
#include <limits.h>

// error report
#include <errno.h>

// wait
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

void alarm_handler(int sign);

int main(int argc, char *argv[]){

  unsigned long int timer;
  char *endptr, *str;
  int base, file;

  if (argc < 4) {
    printf("Usage: %s <seconds> <word> <file1> <file2> <file...>.\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  str = argv[1];
  base = (argc > 2) ? atoi(argv[2]) : 10;
  timer = strtoul(str, &endptr, base);

  /* Check for various possible errors */

  if((errno == ERANGE && timer == ULONG_MAX) || (errno != 0 && timer == 0)){
    perror ("strtol");
    exit(EXIT_FAILURE);
  }

  if (endptr == str){
    fprintf(stderr, "No digits were found\n");
    exit(EXIT_FAILURE);
  }

  if (*endptr != '\0'){
    fprintf(stderr, "Non digit characters found\n");
    exit(EXIT_FAILURE);
  }

  //  printf("strtol() returned %lu\n", timer);
  
  file = open(argv[3], O_RDONLY);
  if (file == -1) {
    perror(argv[3]);
    exit(EXIT_FAILURE);
  }

  struct sigaction action;
  sigset_t sigmask;
  // install SIGALRM handler
  action.sa_handler = alarm_handler;
  sigemptyset(&action.sa_mask); //all signals are delivered
  action.sa_flags = 0;
  sigaction(SIGALRM,&action,NULL);

  // prepare mask for 'sigsuspend'
  sigfillset(&sigmask); //all signals blocked ...
  sigdelset(&sigmask,SIGALRM); //...except SIGALRM
  alarm(timer);
  printf("Pausing ...\n");

  execlp("tail","tail","ler"," | grep -w adeus" ,NULL); //| grep -w adeus", NULL); 

  //while (!alarmflag) pause(); //REPLACED BY 'sigsuspend'
  sigsuspend(&sigmask);

  printf ("done");  

  exit(EXIT_SUCCESS);
}



void alarm_handler(int sign)
{
  // signal(SIGALRM, pause_handler);
  printf("Alarm received...\n");
  //signal(SIGINT,pause_handler);
}
