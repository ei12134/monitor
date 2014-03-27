// compile using debug flag for error messages
//
// gcc monitor.c -o monitor -Wall -DDEBUG

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

#define READ 0
#define WRITE 1
#define TAIL "/usr/bin/tail"
#define GREP "/bin/grep"

static unsigned long int parse_ulong(char *str, int base);

int main(int argc, char *argv[]) {

  unsigned long int timer;

  // Verify arguments validity
  if (argc < 4) {
    printf("Usage: %s <seconds> <query> <file to monitor> <...> .\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  if ((timer = parse_ulong(argv[1], 10)) == ULONG_MAX)
    exit(EXIT_FAILURE);

  // Set user defined countdown timer
  alarm(timer);

  // Create two arrays containing file descriptors needed to establish pipe connections
  int fd1[2]; // pipe connection between tail & grep (both separate monitor childs)
  int fd2[2]; // pipe connection between grep and monitor
  pid_t childpid1, childpid2;

  // First create pipes
  if(pipe (fd1) == -1)
    perror("Failed to create the pipe");

  // Followed by fork
  // Create child1 responsible for executing tail command
  childpid1 = fork();
  if (childpid1 == -1){
    perror("Failed to fork");
    exit(EXIT_FAILURE);
  }

  /* parent code*/
  //for ( ; ;);

  /* child1 code */
  if (childpid1 == 0) {
   
    // Create child2 responsible for executing grep command
    childpid2 = fork();
    if (childpid2 == -1){
      perror("Failed to fork");
      exit(EXIT_FAILURE);
    }
   
    /* child2 code */
    if (childpid2 == 0) {
      if ( execl(GREP, "grep", "--line-buffered", "-w", "adeus", "ler", (char*) 0 < 0) < 0 );
      perror("execl error");
    }
    else{
      if ( execl(TAIL,"tail", "ler", (char*) 0 ) < 0 )
	perror("execl error");
    }
  }
  exit(EXIT_SUCCESS);
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
  //printf("strtoul() returned %lu\n", timer);
  
  /* Successful conversion*/
  return timer; 
}
