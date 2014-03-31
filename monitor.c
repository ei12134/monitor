// Compile using debug flag for error messages
//
// gcc monitor.c -o monitor -Wall -DDEBUG

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>

#define READ 0
#define WRITE 1
#define MAXLINE 4096
#define TAIL "/usr/bin/tail"
#define GREP "/bin/grep"
#define STAT "/usr/bin/stat"

void sig_pipe(int signo);
void sig_alarm(int signo);
long int parse_long(char *str, int base);
pid_t parent_pid;

int main(int argc, char *argv[]) {

  long int timer;
  int status;

  // Verify arguments validity
  if (argc < 4) {
    printf("Usage: %s <seconds> <query> <file to monitor> <...> .\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  if ((timer = parse_long(argv[1], 10)) == LONG_MAX) {
    printf("Enter a valid timer number\n");
    exit(EXIT_FAILURE);
  }

  // Set user defined countdown timer
  signal(SIGALRM,sig_alarm);
  alarm(timer);

  // Create two arrays containing file descriptors needed to establish pipe connections
  int fd1[2]; // pipe connection between tail & grep (both separate monitor childs)
  int fd2[2]; // pipe connection between grep and monitor
  int n;
  pid_t childpid1, childpid2;
  char line[MAXLINE];

  //  execl("stat", "stat", "--printf='%y'", "ler", (char*) 0);
  //  execl("find", "find", ".", "-name ler -printf '%TY-%Tm-%TdT%TH:%TM:%TS'", (char*) 0);

  // sig_pipe handler
  signal(SIGPIPE, sig_pipe);

  /* time_t t = time(NULL); */
  /* struct tm tm = *localtime(&t); */
    
  /* printf("%d-%d-%d %d:%d:%d - %s.txt - ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, argv[3]); 
   fflush(stdout); */

  // First create pipes
  if (pipe (fd1) == -1)
    perror("Failed to create the pipe 1");

  if (pipe (fd2) == -1)
    perror("Failed to create the pipe 2");

  // Followed by fork
  // Create child1 responsible for executing tail command
  childpid1 = fork();
  if (childpid1 == -1){
    perror("Failed to fork");
    exit(EXIT_FAILURE);
  }

  /* parent code*/ 
  if (childpid1 > 0) {
    printf("PID of parent = %d; PPID = %d\n", getpid(), getppid());
    close(fd1[READ]);
    close(fd2[WRITE]);
    dup2(fd2[READ],STDIN_FILENO);
    
    while (fgets(line, MAXLINE,stdin) != NULL) {
	n=strlen(line);
	write(fd1[WRITE],line,n);
	n=read(fd2[READ],line,MAXLINE); // waits for grep output
	if (n==0) {
	    perror("child closed pipe");
	    break;
	} 
	line[n]=0; // null ending char is not received, so "add" it
	
	fflush(stdout);
	printf("%s.txt - ", argv[3]);
	printf("%s", line);
    }    
    wait(&status);  
  }
  /* Child1 code */
  if (childpid1 == 0) {
    printf("PID of tail child = %d; PPID = %d\n", getpid(), getppid());
    // Create child2 responsible for executing grep command
    childpid2 = fork();
      
    if (childpid2 == -1){
      perror("Failed to fork");
      exit(EXIT_FAILURE);
    }
	
    if (childpid2 > 0){
      // Redirect output through pipe 1 for grep
      close(fd1[READ]);
      if (fd1[WRITE] != STDOUT_FILENO) {
	if (dup2(fd1[WRITE],STDOUT_FILENO) != STDOUT_FILENO)
	  perror("dup1 error to stdout in pipe 1");
      }
      close(fd1[WRITE]);
      
      execl(TAIL,"tail","-n 0", "-f" , argv[3], (char*) 0 );
      perror("tail execl error");
    }
    
    /* Child2 code */
    else if (childpid2 == 0) {
      printf("PID of grep child = %d; PPID = %d\n", getpid(), getppid());
      // Receive tail input redirected through pipe 1
      close(fd1[WRITE]);
      dup2(fd1[READ],STDIN_FILENO);

      // redirect grep output to parent
      close(fd2[READ]);
      if (dup2(fd2[WRITE],STDOUT_FILENO) != STDOUT_FILENO)
	perror("dup2 error to stdout in pipe 2");
      close(fd2[WRITE]);
     
      execl(GREP, "grep", "--line-buffered", "-w", argv[2], (char*) 0);
      perror("grep execl error");
    }
  } 
  exit(EXIT_SUCCESS);
}

void sig_pipe(int signo) {
  printf("SIGPIPE caught\n");
  exit(EXIT_FAILURE);
}

void sig_alarm(int signo) {
#ifdef DEBUG
  printf("SIGALARM caught\n");
#endif
  printf("Monitor user set timer expired\n");
  kill(-parent_pid, SIGQUIT);
  exit(EXIT_SUCCESS);
}
 
void sigquit_handler (int sig) {
  assert(sig == SIGQUIT);
  pid_t self = getpid();
  if (parent_pid != self) _exit(EXIT_SUCCESS);
}


long int parse_long(char *str, int base) {
  
  char *endptr;
  long int timer = strtol(str, &endptr, base);

  /* Check for various possible errors */
  
  if ((errno == ERANGE && (timer == LONG_MAX || timer == LONG_MIN)) || (errno != 0 && timer == 0)) {
    perror("strtol");
    return LONG_MAX;
  }

  if (endptr == str){
#ifdef DEBUG  
    fprintf(stderr, "No digits were found\n");
#endif
    return LONG_MAX;
  }
  
  if (*endptr != '\0'){
#ifdef DEBUG 
    fprintf(stderr, "Non digit char found\n");
#endif
    return LONG_MAX;
  }

  if (timer < 0) {
#ifdef DEBUG
    fprintf(stderr, "Negative number\n");
#endif
    return LONG_MAX;
  }
  
#ifdef DEBUG  
  printf("strtol() returned %ld\n", timer);
#endif

  /* Successful conversion*/
  return timer; 
}
