#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

// STDOU_FILENO
#include <sys/stat.h>

// parse unsigned long
#include <limits.h>

// error report
#include <errno.h>

// wait
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

static unsigned long int parse_ulong(char *str, int base);
void alarm_handler(int sign);

int main(int argc, char *argv[]) {

  unsigned long int timer;
  int file;

  if (argc < 4) {
    printf("Usage: %s <seconds> <query> <file1> <file...>.\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  if ((timer = parse_ulong(argv[1], 10)) == ULONG_MAX)
    exit(EXIT_FAILURE);

  
  file = open(argv[3], O_RDONLY);
  if (file == -1) {
    perror(argv[3]);
    exit(EXIT_FAILURE);
  }
  close(file);
 
  pid_t childpid;  
  int fd[2];
  if(pipe (fd) == -1)
    perror("Failed to create the pipe");

  childpid = fork();

  if (childpid == -1){
    perror("Failed to fork");
    exit(EXIT_FAILURE);
  }

  /* parent code*/
  if (childpid){
    close(fd[0]);
    dup2(fd[1], STDOUT_FILENO);
    close(fd[1]);
    execlp("tail", "tail", "ler", NULL);
  }

  /* child code */
  else{
    execlp("grep", "grep", "--line-buffered", "-w", "adeus", STDOUT_FILENO, NULL);
  }

  exit(EXIT_SUCCESS);
}



void alarm_handler(int sign) {
  printf("Alarm received...\n");
}

static unsigned long int parse_ulong(char *str, int base) {

  char *endptr;
  unsigned long int timer = strtoul(str, &endptr, base);

 /* Check for various possible errors */
  if((errno == ERANGE && timer == ULONG_MAX) || (errno != 0 && timer == 0)){
    perror ("strtoul conversion error");
    return ULONG_MAX;
  }

  if (endptr == str){
#ifdef DEBUG  
  fprintf(stderr, "No digits were found\n");
#endif
    return ULONG_MAX;
  }

  if (*endptr != '\0'){
#ifdef DEBUG  
  fprintf(stderr, "Non digit char found\n");
#endif
  return ULONG_MAX;
  }
  printf("strtoul() returned %lu\n", timer);

  /* Successful conversion*/
  return timer; 
}


  /* struct sigaction action; */
/*   sigset_t sigmask; */
/*   // install SIGALRM handler */
/*   action.sa_handler = alarm_handler; */
/*   sigemptyset(&action.sa_mask); //all signals are delivered */
/*   action.sa_flags = 0; */
/*   sigaction(SIGALRM,&action,NULL); */

/*   // prepare mask for 'sigsuspend' */
/*   sigfillset(&sigmask); //all signals blocked ... */
/*   sigdelset(&sigmask,SIGALRM); //...except SIGALRM */
/*   alarm(timer); */
/*   printf("Pausing ...\n"); */
  
/*   //while (!alarmflag) pause(); //REPLACED BY 'sigsuspend' */
/*   sigsuspend(&sigmask); */
