/* Compile using debug flag [-DDEBUG] for error messages  */

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

#define READ 0
#define WRITE 1
#define MAXLINE 4096
int *childpid1, childpid4, nFiles;

void sig_pipe(int signo) {
#ifdef DEBUG
  printf("SIGPIPE caught\n");
#endif
  exit (EXIT_FAILURE);
}

void sig_quit(int signo) {
#ifdef DEBUG
  printf("SIGQUIT caught\n");
#endif
  kill(-getpgrp(), SIGUSR1);
}

void sig_alarm(int signo) {
#ifdef DEBUG
  printf("SIGALARM caught\n");
#endif
  printf("User set timer expired - monitor stopped\n");
  int i;
  for (i = 0; i < nFiles; i++)
    kill(childpid1[i], SIGQUIT);

  kill(childpid4, SIGQUIT);

  free(childpid1);
  exit (EXIT_SUCCESS);
}

void sig_int(int signo) {
#ifdef DEBUG
  printf("SIGINT caught\n");
#endif
  int i;
  for (i = 0; i < nFiles; i++)
    kill(childpid1[i], SIGQUIT);

  kill(childpid4, SIGQUIT);

  free(childpid1);
  exit (EXIT_SUCCESS);
}

long int parse_long(char *str, int base) {

  char *endptr;
  long int timer = strtol(str, &endptr, base);

  /* Check for various possible errors */
  if ((errno == ERANGE && (timer == LONG_MAX || timer == LONG_MIN))
      || (errno != 0 && timer == 0)) {
    perror("strtol");
    return LONG_MAX;
  }

  if (endptr == str) {
#ifdef DEBUG
    fprintf(stderr, "No digits were found\n");
#endif
    return LONG_MAX;
  }

  if (*endptr != '\0') {
#ifdef DEBUG
    fprintf(stderr, "Non digit char found\n");
#endif
    return LONG_MAX;
  }

  if (timer <= 0) {
#ifdef DEBUG
    fprintf(stderr, "Non positive number\n");
#endif
    return LONG_MAX;
  }
#ifdef DEBUG
  printf("strtol() successfuly returned %ld\n", timer);
#endif

  /* Successful conversion */
  return timer;
}

int main(int argc, char *argv[]) {

  /* Verify arguments validity */
  if (argc < 4) {
    printf("Usage: %s <seconds> <query> <file to monitor> <...> .\n",
	   argv[0]);
    exit (EXIT_FAILURE);
  }

  /* Verify maximum of threads permitted to the user */
  struct rlimit rlim;
  getrlimit(RLIMIT_NPROC, &rlim);

  /* Set number of files */
  nFiles = argc - 3;

  /* + 2 because of additional monitor and aux process - does not take into account any other user processes running :( */
  if (nFiles*3 + 2 >= rlim.rlim_cur) {
    printf("Number of user allowed threads exceeded: Maximum = %d\n",
	   (int)rlim.rlim_cur);
    exit (EXIT_FAILURE);
  }
       
  /* Safely convert seconds string argument to a long integer */
  long int timer;
  if ((timer = parse_long(argv[1], 10)) == LONG_MAX) {
    printf("Enter a valid timer number\n");
    exit (EXIT_FAILURE);
  }

  /* Alocate memory for childpid's array */
  childpid1 = malloc(sizeof(int) * nFiles);
  if (childpid1 == NULL) {
    fprintf(stderr,
	    "Memory exhausted (malloc of %d bytes) - Use less files to monitor\n",
	    nFiles);
    exit (EXIT_FAILURE);
  }

  /* Create two arrays containing file descriptors needed to establish pipe connections */
  int fd1[2]; /* pipe connection between tail & grep (both separate monitor childs) */
  int fd2[2]; /* pipe connection between grep and monitor */
  int status;
  pid_t childpid2, childpid3;

  /* Signal handlers */
  signal(SIGQUIT, sig_quit);
  signal(SIGINT, sig_int);
  signal(SIGPIPE, sig_pipe);
  signal(SIGALRM, sig_alarm);
  alarm(timer);

  /* Fork for each file */
  int i;
  for (i = 3; i < argc; i++) {

    childpid1[i - 3] = fork();
    if (childpid1[i - 3] == -1) {
      perror("Failed to fork childpid1");
      exit (EXIT_FAILURE);
    }

    /* Parent code */
    if (childpid1[i - 3] > 0) {
#ifdef DEBUG
      printf("PID of parent = %d; PPID = %d\n", getpid(), getppid());
#endif	
      int z;
      if (i >= argc - 1) {
	childpid4 = fork();
	if (childpid4 == -1) {
	  perror("Failed to fork childpid4");
	  exit (EXIT_FAILURE);
	}
	if (childpid4 > 0)
	  for (z = 0; z < nFiles; z++)
	    wait(&status);

	/* File checker */
	if (childpid4 == 0) {
	  while (nFiles > 0) {
	    int i;
	    for (i = 0; i < nFiles; i++) {
	      if (access(argv[i + 3], F_OK) == -1) {
		perror(argv[i + 3]);
		kill(childpid1[i], SIGQUIT);
		if (nFiles > 1) {
		  int c, d;
		  for (c = i; c < nFiles - 1; c++)
		    childpid1[c] = childpid1[c + 1];

		  for (d = i + 3; d < argc - 1; d++)
		    argv[d] = argv[d + 1];
		}
		nFiles--;
	      }
	    }
	    sleep(5);
	  }
	}
      } else
	waitpid(-1, &status, WNOHANG);
    }
    /* Output formatter process */
    if (childpid1[i - 3] == 0) {
#ifdef DEBUG
      printf("PID of first child = %d; PPID = %d\n", getpid(), getppid());
#endif
      setpgrp();
      /* Create pipes */
      if (pipe(fd1) == -1)
	perror("Failed to create the pipe 1");

      if (pipe(fd2) == -1)
	perror("Failed to create the pipe 2");

      childpid2 = fork();
      if (childpid2 == -1) {
	perror("Failed to fork childpid2");
	exit (EXIT_FAILURE);
      }
      if (childpid2 > 0) {
	int n;
	char line[MAXLINE];

	close(fd2[WRITE]);
	dup2(fd2[READ], STDIN_FILENO);

	/* Reads grep output */
	while ( (n = read(fd2[READ], line, MAXLINE)) > 0 ) {
	  /* Substitute /n ? */
	  line[n - 1] = '"';
	  /* End char */
	  line[n] = 0;

	  /* Display system time */
	  char timeBuffer[50];
	  time_t now = time(0);

	  strftime(timeBuffer, 50, "%Y-%m-%dT%H:%M:%S",
		   localtime(&now));
	  printf("%s - %s - \"%s\n", timeBuffer, argv[i], line);
	}
      }
      wait(&status);

      /* Tail process */
      if (childpid2 == 0) {
#ifdef DEBUG
	printf("PID of tail child = %d; PPID = %d\n", getpid(), getppid());
#endif
	/* Create child3 responsible for executing grep command */
	childpid3 = fork();
	if (childpid3 == -1) {
	  perror("Failed to fork childpid3");
	  exit (EXIT_FAILURE);
	}

	if (childpid3 > 0) {
	  /* Redirect output through pipe 1 for grep */
	  close(fd1[READ]);
	  if (fd1[WRITE] != STDOUT_FILENO) {
	    if (dup2(fd1[WRITE], STDOUT_FILENO) != STDOUT_FILENO)
	      perror("dup1 error to stdout in pipe 1");
	  }
	  close(fd1[WRITE]);

	  execlp("tail", "tail", "-n 0", "-f", argv[i], (char*) 0);
	  perror("tail execl error");
	}

	/* Grep process */
	else if (childpid3 == 0) {
#ifdef DEBUG
	  printf("PID of grep child = %d; PPID = %d\n", getpid(), getppid());
#endif
	  /* Receive tail input redirected through pipe 1 */
	  close(fd1[WRITE]);
	  dup2(fd1[READ], STDIN_FILENO);

	  /* Redirect grep output to parent */
	  close(fd2[READ]);
	  if (dup2(fd2[WRITE], STDOUT_FILENO) != STDOUT_FILENO)
	    perror("dup2 error to stdout in pipe 2");
	  close(fd2[WRITE]);

	  execlp("grep", "grep", "--line-buffered", argv[2],
		 (char*) 0);
	  perror("grep execl error");
	}
      }
    }
  }
  free(childpid1);
  exit (EXIT_SUCCESS);
}
