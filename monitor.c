// Compile using debug flag for error messages
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
void sig_int(int signo);
void sig_pipe(int signo);
void sig_alarm(int signo);
long int parse_long(char *str, int base);
int *childpid1, childpid4, nFiles;

int main(int argc, char *argv[]) {

	// Verify arguments validity
	if (argc < 4) {
		printf("Usage: %s <seconds> <query> <file to monitor> <...> .\n",
				argv[0]);
		exit (EXIT_FAILURE);
	}

	if (argc > 23) {
		printf("Too many files to monitor (Maximum = 20)\n");
		exit (EXIT_FAILURE);
	}

	long int timer;
	if ((timer = parse_long(argv[1], 10)) == LONG_MAX) {
		printf("Enter a valid timer number\n");
		exit (EXIT_FAILURE);
	}

	int status;
	nFiles = argc - 3;
	childpid1 = malloc(sizeof(int) * nFiles);
	// Create two arrays containing file descriptors needed to establish pipe connections
	int fd1[2]; // pipe connection between tail & grep (both separate monitor childs)
	int fd2[2]; // pipe connection between grep and monitor
	pid_t childpid2, childpid3;

	/* Set Signals handlers */
	// Prepare sig_quit handler
	signal(SIGQUIT, sig_quit);
	// Prepare sig_int handler
	signal(SIGINT, sig_int);
	// Prepare sig_alarm handler
	signal(SIGALRM, sig_alarm);
	// Set user defined countdown timer
	alarm(timer);
	// Prepare sig_pipe handler
	signal(SIGPIPE, sig_pipe);

	// fork and create as many aux process needed
	int i;
	for (i = 3; i < argc; i++) {

		childpid1[i - 3] = fork();
		if (childpid1[i - 3] == -1) {
			perror("Failed to fork childpid1");
			exit (EXIT_FAILURE);
		}

		/* parent code */
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

				/* file checker */
				if (childpid4 == 0) {
					while (nFiles > 0) {
						int i;
						for (i = 0; i < nFiles; i++) {
							if (access(argv[i + 3], F_OK) == -1) {
								perror(argv[i + 3]);
								printf("%s does not exist anymore\n",
										argv[i + 3]);
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
		/* stdout process */
		if (childpid1[i - 3] == 0) {
#ifdef DEBUG
			printf("PID of first child = %d; PPID = %d\n", getpid(), getppid());
#endif
			setpgrp();
			// create pipes
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

				close(fd1[READ]);
				close(fd2[WRITE]);
				dup2(fd2[READ], STDIN_FILENO);

				while (fgets(line, MAXLINE, stdin) != NULL) {
					n = strlen(line);
					write(fd1[WRITE], line, n);
					n = read(fd2[READ], line, MAXLINE); // waits for grep output
					if (n == 0) {
						perror("grep child closed pipe");
						break;
					}
					line[n - 1] = '"'; // substitute /n ?
					line[n] = 0; // end char

					// Display system time
					char timeBuffer[50];
					time_t now = time(0);

					strftime(timeBuffer, 50, "%Y-%m-%dT%H:%M:%S",
							localtime(&now));
					printf("%s - %s - \"%s\n", timeBuffer, argv[i], line);
				}
			}
			wait(&status);

			/* tail process */
			if (childpid2 == 0) {
#ifdef DEBUG
				printf("PID of tail child = %d; PPID = %d\n", getpid(), getppid());
#endif
// Create child3 responsible for executing grep command
				childpid3 = fork();
				if (childpid3 == -1) {
					perror("Failed to fork childpid3");
					exit (EXIT_FAILURE);
				}

				if (childpid3 > 0) {
// Redirect output through pipe 1 for grep
					close(fd1[READ]);
					if (fd1[WRITE] != STDOUT_FILENO) {
						if (dup2(fd1[WRITE], STDOUT_FILENO) != STDOUT_FILENO)
							perror("dup1 error to stdout in pipe 1");
					}
					close(fd1[WRITE]);

					execlp("tail", "tail", "-n 0", "-f", argv[i], (char*) 0);
					perror("tail execl error");
				}

				/* grep process */
				else if (childpid3 == 0) {
#ifdef DEBUG
					printf("PID of grep child = %d; PPID = %d\n", getpid(), getppid());
#endif
					// Receive tail input redirected through pipe 1
					close(fd1[WRITE]);
					dup2(fd1[READ], STDIN_FILENO);

					// redirect grep output to parent
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

	kill(childpid4, SIGUSR1);

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

	kill(childpid4, SIGUSR1);

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
