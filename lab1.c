/* \file lab1.h
 * \author Jesse Joseph
 * \email jjoseph@hmc.edu
 * \ID 040161840
 */
#include "lab1.h"
#define FDIN_ 0
#define FDOUT_ 1
#define FDERR_ 2
#define TIMEOUT_ 20
#define BUFFERSIZE_ 512
/* Defining pipes:
 * We need two pipes - one for the parent to write to the
 * child, and one for the child to write to the parent.
 * The pipe that the parent will read from and the child
 * will write to is pipes[0], and the pipe that the parent
 * will write to and the child will read from will be
 * pipes[1].
 * source: https://jineshkj.wordpress.com/2006/12/22/how-to-capture-stdin-stdout-and-stderr-of-child-program/
 */
#define PARENT_READ_PIPE_ 0
#define PARENT_WRITE_PIPE_ 1

int pipes[2][2];

#define PARENT_READ_FD_ (pipes[PARENT_READ_PIPE_][FDIN_])
#define PARENT_WRITE_FD_ (pipes[PARENT_WRITE_PIPE_][FDOUT_])
#define CHILD_READ_FD_ (pipes[PARENT_WRITE_PIPE_][FDIN_])
#define CHILD_WRITE_FD_ (pipes[PARENT_READ_PIPE_][FDOUT_])

struct termios oldTerm;
int terminalModeChanged = 0;
int forked = 0;
pid_t pid;
int pidStatus;

int main(int argc, char **argv) {
  int rc;
  /* First, we register a cleanup function to make sure
   * that the terminal is returned to canonical input 
   * mode on program exit.
   */
  rc = atexit(exitCleanUp);
  if(rc != 0) {
    perror("In call to atexit(exitCleanUp): Failed to register cleanup routine");
    exit(1);
  }

  /* Parse arguments with getopt_long(4).
   * The only valid argument is --shell
   */
  int shell = 0;
  char opt;
  int optind;
  struct option longOptions[] = {
    {"shell", no_argument, &shell, 's'},
    {0, 0, 0, 0}};
  while((opt = getopt_long(argc, argv, "s", longOptions, &optind)) != -1) {
    switch(opt) {
      case 's':
				shell = 1;
      	break;
      case '?':
				exit(1);
      case 0:
        break;
      default:
				break;
    }
  }

  /* Save the old terminal settings */
  rc = tcgetattr(FDIN_, &oldTerm);
  if(rc != 0) {
    perror("In call to tcgetattr(FDIN_, &oldTerm):\r\n Failed to save original terminal settings.\r\n");
    exit(1);
  }
  
  if(shell) {
    /* Build our three pipes */
    pipe(pipes[PARENT_READ_PIPE_]);
    pipe(pipes[PARENT_WRITE_PIPE_]);
    
    pid = fork();
    if(pid < 0) {
      perror("In call to fork():\r\nFailed to fork process.\r\n");
      exit(1);
    }
    else if (pid == 0) {
			/* Child process */
      /* Connect the two pipes to the child's input and output. */
      if(dup2(CHILD_READ_FD_, FDIN_) < 0) {
				perror("In call to dup2(CHILD_READ_FD_, FDIN_):\r\nFailed to connect pipe to child's input.\r\n");
				exit(1);
      }
      if(dup2(CHILD_WRITE_FD_, FDOUT_) < 0) {
				perror("In call to dup2(CHILD_WRITE_FD_, FDOUT_):\r\nFailed to connect pipe to child's output.\r\n");
				exit(1);
      }
			if(dup2(CHILD_WRITE_FD_, FDERR_) < 0) {
				perror("In call to dup2(CHILD_WRITE_FD_, FDERR_):\r\nFailed to connect pipe to child's stderr.\r\n");
				exit(1);
			}

      
      /* Close file descriptors unneeded by child process */
      if(close(CHILD_READ_FD_) < 0) {
				perror("In call to close(CHILD_READ_FD_):\r\nFailed to close child's old input port.\r\n");
				exit(1);
      }
      if(close(CHILD_WRITE_FD_) < 0) {
				perror("In call to close(CHILD_WRITE_FD):\r\nFailed to close child's old output port.\r\n");
				exit(1);
      }
      if(close(PARENT_READ_FD_) < 0) {
				perror("In call to close(PARENT_READ_FD_):\r\nFailed to close parent's old input port.\r\n");
				exit(1);
      }
      if(close(PARENT_WRITE_FD_) < 0) {
				perror("In call to close(PARENT_WRITE_FD_):\r\nFailed to close parent's old output port.\r\n");
				exit(1);
      }

      char *args[] = {"/bin/bash", NULL};
      if(execv("/bin/bash", args) < 0) {
				perror("In call to execv(\"/bin/bash\", args):\r\nFailed to execute program bash.\r\n");
				exit(1);
      }
    }
    else {
      /* Parent process */
      forked = 1;
      signal(SIGCHLD, signalHandler);
			signal(SIGPIPE, signalHandler);
      signal(SIGINT, SIG_IGN);
      if(close(CHILD_READ_FD_) < 0) {
				perror("In call to close(CHILD_READ_FD_):\r\nFailed to close child pipe input port.\r\n");
				exit(1);
      }
      if(close(CHILD_WRITE_FD_) < 0) {
				perror("In call to close(CHILD_WRITE_FD_):\r\nFailed to close child pipe output port.\r\n");
				exit(1);
      }

      setTerminalToNonCanonicalInput();

      pthread_t readThread;
      pthread_t writeThread;
      if(pthread_create(&writeThread, NULL, readAndWritetoShell, NULL) < 0) {
				perror("In call to pthread_create(%writeThread, NULL, readAndWritetoShell, &PARENT_WRITE_FD_):\r\nFailed to create thread.\r\n");
				exit(1);
      }
      if(pthread_create(&readThread, NULL, readFromShell, NULL) < 0) {
				perror("In call to pthread_create(&readThread, NULL, readFromShell, &PARENT_READ_FD):\r\nFailed to create thread.\r\n");
				exit(1);
      }
      if(pthread_join(writeThread, NULL) != 0) {
				perror("In call to pthread_join(writeThread, NULL):\r\nThread did not terminate successfully.\r\n");
				exit(1);
      }
      if(pthread_join(readThread, NULL) != 0) {
				perror("In call to pthread_cancel(readThread):\r\nThread was not cancelled successfully.\r\n");
				exit(1);
      }
			sleep(10);
    }
    exit(0);
  }
  else {
    /* Change the terminal settings to non-canonical input */
    setTerminalToNonCanonicalInput();

    /* Continuously read from the terminal one byte at at time.
     * Continue until the EOF character ^D is given.
     */
    continuousNonCanonicalRead();
    
    /* Reset the terminal to the original settings. This is necessary
     * for normal execution of the program if it has not been forced 
     * to exit yet, as the cleanUp function only executes if exit(1)
     * has been called.
     */
    rc = tcsetattr(FDIN_, TCSANOW, &oldTerm);
    if(rc != 0) {
      perror("In call to tcsetattr(FDIN_, TCSANOW, &oldTerm):\r\nFailed to restore original terminal settings.\r\n");
      exit(1);
    }
    return 0;
  }
}


void *readAndWritetoShell() {
  /* Non-Canonical input:
   * read bytes continuously as they become available
   * and write them back continuously
   */
  int bytesRead = 0;
  char buff[BUFFERSIZE_];
  int rc = 0;
  while(!rc) {
    bytesRead = read(FDIN_, buff, BUFFERSIZE_);
    if(bytesRead < 0) {
      perror("In call to read(FDIN_, buff, BUFFERSIZE_):\r\nInput read failed.\r\n");
      exit(1);
    }
    else {
      rc = writeBack(buff, bytesRead) || sendToShell(buff, bytesRead);
    }
  }
	if(close(PARENT_WRITE_FD_) < 0) {
		perror("In call to close(fd):\r\nFailed to close pipe after normal shell termination.\r\n");
		exit(1);
	}
  return NULL;
}

void *readFromShell() {
  int bytesRead = 0;
  char buff[BUFFERSIZE_];
  int rc = 0;
  while(!rc) {
    bytesRead = read(PARENT_READ_FD_, buff, BUFFERSIZE_);
    if(bytesRead < 0) {
			collectShellStatus();
      exit(2);
    }
    else {
      rc = writeBack(buff, bytesRead);
    }
  }
	if(close(PARENT_READ_FD_) < 0) {
		perror("In call to close(*fd_int_ptr):\r\nFailed to close pipe after normal shell termination.\r\n");
		exit(1);
	}
  return NULL;
}

int sendToShell(char *buff, int nBytes) {
  char writeByte;
  char lf = '\n';
  for(int i = 0; i < nBytes; ++i) {
    writeByte = buff[i];
    if(writeByte == '\n' || writeByte == '\r') {
      if(write(PARENT_WRITE_FD_, &lf, 1) < 0) {
				perror("In call to write(fd, &lf, 1):\r\nWrite to shell failed.\r\n");
				exit(1);
      }
    }
    else if(writeByte == 0x004) {
      return 1;
    }
    else if(writeByte == 0x003) {
      if(kill(pid, SIGINT) < 0) {
				perror("In call to kill(pid, SIGINT):\r\nFailed to send signal to child process.\r\n");
				exit(1);
      }
    }
    else {
      if(write(PARENT_WRITE_FD_, &writeByte, 1) < 0) {
				fprintf(stderr, "In call to write(fd, &writeByte, 1):\r\nFailed to write char %s to shell.\r\n", &writeByte);
				perror("");
				exit(1);
      }
    }
  }
  return 0;
}

int writeBack(char *buff, int nBytes) {
  char writeByte;
  char crlf[2] = {'\r', '\n'};
  for(int i = 0; i < nBytes; ++i ) {
    writeByte = buff[i];
    if(writeByte == '\n' || writeByte == '\r') {
      if(write(FDOUT_, crlf,  2) < 0) {
				perror("In call to write(FDOUT_, crlf, 2):\r\nFailed to write to terminal.\r\n");
				exit(1);
      }
    } 
    else if(writeByte == 0x004){
      return 1;
    }
    else if(writeByte == 0x003) {
    }
    else {
      if(write(FDOUT_, &writeByte, 1) < 0) {
				perror("In call to write(FDOUT_, &writeByte, 1):\r\nFailed to write to terminal.\r\n");
				exit(1);
      }
    }
  }
  return 0;
}

void setTerminalToNonCanonicalInput() {
  int rc = 0; 
  /* Create a new termios object to define our desired 
   * terminal settings.
   */
  struct termios newTerm;
  newTerm.c_iflag = IUTF8 | ISTRIP;
  newTerm.c_oflag = 0;
  newTerm.c_lflag = 0;
  rc = tcflush(FDIN_, TCIFLUSH);
  if(rc != 0) {
    perror("In call to tcflush(FDIN_, TCIFLUSH):\r\nFailed to flush terminal input.\r\n");
    exit(1);
  }
  rc = tcsetattr(FDIN_, TCSANOW, &newTerm);
  if(rc != 0) {
    perror("In call to tcsetattr(FDIN_, TCSANOW, &newTerm):\r\nFailed to set terminal to non-canonical input.\r\n");
    exit(1);
  }
  terminalModeChanged = 1;
}

void continuousNonCanonicalRead() {
  /* Non-Canonical input:
   * read bytes continuously as they become available
   * and write them back continuously
   */
  int bytesRead = 0;
  char buff[BUFFERSIZE_];
  int rc = 0;
  while(!rc) {
    bytesRead = read(FDIN_, buff, BUFFERSIZE_);
    if(bytesRead < 0) {
      perror("In call to read(FDIN_, buff, BUFFERSIZE_):\r\nInput read failed.\r\n");
      exit(1);
    }
    else {
      rc = writeBack(buff, bytesRead);
    }
  }
}

void signalHandler(int SIGNUM) {
  if(SIGNUM == SIGPIPE) {
		collectShellStatus();
    exit(2);
  }
	else if(SIGNUM == SIGCHLD) {
		collectShellStatus();
		exit(0);
  }
  else {
    fprintf(stdout, "received signal: %d\r\n", SIGNUM);
    exit(1);
  }
}

void collectShellStatus() {
	if(wait(&pidStatus) < 0) {
		perror("in call to wait(&pidStatus):\r\nwaitpid failed.\r\n");
		exit(1);
	}
	int pidStopSignal = pidStatus & 0x00FF;
	int pidExitStatus = pidStatus >> 8;
	fprintf(stdout, "\r\nSHELL EXIT SIGNAL=%d STATUS=%d\r\n", pidStopSignal, pidExitStatus);
}

void exitCleanUp() {
  if(terminalModeChanged) {
    int rc = tcsetattr(FDIN_, TCSANOW, &oldTerm);
    if(rc != 0) {
      perror("In call to tcsetattr(FDIN_, TCSANOW, &oldTerm):\r\nFailed to restore terminal to canonical input.\r\n");
    }
  }
}
