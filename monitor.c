// Compile using debug flag for error messages
//
// gcc monitor.c -o monitor -Wall -DDEBUG

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

void sig_quit(int signo);
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

  // Create two arrays containing file descriptors needed to establish pipe connections
  int fd1[2]; // pipe connection between tail & grep (both separate monitor childs)
  int fd2[2]; // pipe connection between grep and monitor
  int file, n;
  pid_t childpid1, childpid2, childpid3;
  char line[MAXLINE];

  /* Set Signals handlers */
  // Prepare sig_quit handler
  signal(SIGQUIT,sig_quit);
  parent_pid = getpid();
  // Prepare sig_alarm handler
  signal(SIGALRM,sig_alarm);
  // Set user defined countdown timer
  alarm(timer);
  // Prepare sig_pipe handler
  signal(SIGPIPE, sig_pipe);

  // Create pipes
  if (pipe (fd1) == -1)
    perror("Failed to create the pipe 1");

  if (pipe (fd2) == -1)
    perror("Failed to create the pipe 2");

  // Followed by fork
  // Create child1 responsible for executing tail command
  childpid1 = fork();
  if (childpid1 == -1){
    perror("Failed to fork childpid1");
    exit(EXIT_FAILURE);
  }

  /* parent code */
  if (childpid1 > 0) {
#ifdef DEBUG    
    printf("PID of parent = %d; PPID = %d\n", getpid(), getppid());
#endif
    close(fd1[READ]);
    close(fd2[WRITE]);
    dup2(fd2[READ],STDIN_FILENO);
    
    while (fgets(line, MAXLINE,stdin) != NULL) {
      n=strlen(line);
      write(fd1[WRITE],line,n);
      n=read(fd2[READ],line,MAXLINE); // waits for grep output
      if (n==0) {
	perror("grep child closed pipe");
	break;
      } 
      line[n-1]='"'; // substitute /n ?
      line[n]=0; // end char

      // Display system time
      char timeBuffer[50];
      time_t now = time (0);
      
      strftime ( timeBuffer, 50, "%Y-%m-%dT%H:%M:%S", localtime (&now));
      printf ("%s - %s - \"%s\n", timeBuffer,  argv[3], line);
    }    
    wait(&status);  
  }
  
  // Child1 code - file checker
  if (childpid1 == 0){
#ifdef DEBUG
    printf("PID of first child = %d; PPID = %d\n", getpid(), getppid());
#endif
    setpgrp();
    childpid2 = fork();
    if (childpid2 == -1){
      perror("Failed to fork childpid2");
      exit(EXIT_FAILURE);
    }
    if (childpid2 > 0){
      while (1){
	file = open(argv[3], O_RDONLY);
	if (file == -1) {
	  perror(argv[3]);
	  printf("%s does not exist anymore\n",argv[3]);
	  kill(-getpgrp(), SIGUSR1);
	}
	sleep(5);
      }
      wait(&status);
    }
    if (childpid2 == 0){
      /* Child2 code - tail */
      if (childpid2 == 0) {
#ifdef DEBUG
	printf("PID of tail child = %d; PPID = %d\n", getpid(), getppid());
#endif
	// Create child3 responsible for executing grep command
	childpid3 = fork();
	if (childpid3 == -1){
	  perror("Failed to fork childpid3");
	  exit(EXIT_FAILURE);
	}    
	
	if (childpid3 > 0){
	  // Redirect output through pipe 1 for grep
	  close(fd1[READ]);
	  if (fd1[WRITE] != STDOUT_FILENO) {
	    if (dup2(fd1[WRITE],STDOUT_FILENO) != STDOUT_FILENO)
	      perror("dup1 error to stdout in pipe 1");
	  }
	  close(fd1[WRITE]);
      
	  execlp("tail","tail","-n 0", "-f" , argv[3], (char*) 0 );
	  perror("tail execl error");
	}
    
	/* Child3 code  - grep */
	else if (childpid3 == 0) {
#ifdef DEBUG	
	  printf("PID of grep child = %d; PPID = %d\n", getpid(), getppid());
#endif
	  // Receive tail input redirected through pipe 1
	  close(fd1[WRITE]);
	  dup2(fd1[READ],STDIN_FILENO);

	  // redirect grep output to parent
	  close(fd2[READ]);
	  if (dup2(fd2[WRITE],STDOUT_FILENO) != STDOUT_FILENO)
	    perror("dup2 error to stdout in pipe 2");
	  close(fd2[WRITE]);
     
	  execlp("grep", "grep", "--line-buffered", argv[2], (char*) 0);
	  perror("grep execl error");
	}
      } 
    }
  }
  exit(EXIT_SUCCESS);
}

void sig_pipe(int signo) {
#ifdef DEBUG
  printf("SIGPIPE caught\n");
#endif
  exit(EXIT_FAILURE);
}

void sig_alarm(int signo) {
#ifdef DEBUG
  printf("SIGALARM caught\n");
#endif
  printf("User set timer expired - monitor stopped\n");
  kill(-parent_pid, SIGUSR1);
  exit(EXIT_SUCCESS);
}
 
void sig_quit(int signo) {
#ifdef DEBUG
  printf("SIGQUIT caught\n");
#endif
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
  printf("strtol() successfuly returned %ld\n", timer);
#endif

  /* Successful conversion*/
  return timer; 
}
