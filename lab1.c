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
	// getopt_long handles the error, no need to write perror
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
    perror("In call to tcgetattr(FDIN_, &oldTerm):\n Failed to save original terminal settings.\n");
    exit(1);
  }
  
  if(shell) {
    /* Build our three pipes */
    pipe(pipes[PARENT_READ_PIPE_]);
    pipe(pipes[PARENT_WRITE_PIPE_]);
    
    pid = fork();
    if(pid < 0) {
      perror("In call to fork():\nFailed to fork process.\n");
      exit(1);
    }
    else if (pid == 0) {
      /* Child process - for testing, set up an alarm here so it 
       * quits eventually.
       */
      /* Connect the two pipes to the child's input and output. */
      if(dup2(CHILD_READ_FD_, FDIN_) < 0) {
	perror("In call to dup2(CHILD_READ_FD_, FDIN_):\nFailed to connect pipe to child's input.\n");
	exit(1);
      }
      if(dup2(CHILD_WRITE_FD_, FDOUT_) < 0) {
	perror("In call to dup2(CHILD_WRITE_FD_, FDOUT_):\nFailed to connect pipe to child's output.\n");
	exit(1);
      }
      
      /* Close file descriptors unneeded by child process */
      if(close(CHILD_READ_FD_) < 0) {
	perror("In call to close(CHILD_READ_FD_):\nFailed to close child's old input port.\n");
	exit(1);
      }
      if(close(CHILD_WRITE_FD_) < 0) {
	perror("In call to close(CHILD_WRITE_FD):\nFailed to close child's old output port.\n");
	exit(1);
      }
      if(close(PARENT_READ_FD_) < 0) {
	perror("In call to close(PARENT_READ_FD_):\nFailed to close parent's old input port.\n");
	exit(1);
      }
      if(close(PARENT_WRITE_FD_) < 0) {
	perror("In call to close(PARENT_WRITE_FD_):\nFailed to close parent's old output port.\n");
	exit(1);
      }

      char *args[] = {"/bin/bash", NULL};
      if(execv("/bin/bash", args) < 0) {
	perror("In call to execv(\"/bin/bash\", args):\nFailed to execute program bash.\n");
	exit(1);
      }
    }
    else {
      /* Parent process */
      forked = 1;
      signal(SIGCHLD, signalHandler);
      signal(SIGINT, SIG_IGN);
      if(close(CHILD_READ_FD_) < 0) {
	perror("In call to close(CHILD_READ_FD_):\nFailed to close child pipe input port.\n");
	exit(1);
      }
      if(close(CHILD_WRITE_FD_) < 0) {
	perror("In call to close(CHILD_WRITE_FD_):\nFailed to close child pipe output port.\n");
	exit(1);
      }

      setTerminalToNonCanonicalInput();

      pthread_t readThread;
      pthread_t writeThread;
      if(pthread_create(&writeThread, NULL, readAndWritetoShell, &PARENT_WRITE_FD_) < 0) {
	perror("In call to pthread_create(%writeThread, NULL, readAndWritetoShell, &PARENT_WRITE_FD_):\nFailed to create thread.\n");
	exit(1);
      }
      if(pthread_create(&readThread, NULL, readFromShell, &PARENT_READ_FD_) < 0) {
	perror("In call to pthread_create(&readThread, NULL, readFromShell, &PARENT_READ_FD):\nFailed to create thread.\n");
	exit(1);
      }
      if(pthread_join(writeThread, NULL) != 0) {
	perror("In call to pthread_join(writeThread, NULL):\nThread did not terminate successfully.\n");
	exit(1);
      }
      if(pthread_cancel(readThread) != 0) {
	perror("In call to pthread_cancel(readThread):\nThread was not cancelled successfully.\n");
	exit(1);
      }
      if(close(PARENT_WRITE_FD_) < 0) {
	perror("In call to close(PARENT_WRITE_FD_):\nFailed to close pipe after normal shell termination.\n");
	exit(1);
      }
      if(close(PARENT_READ_FD_) < 0) {
	perror("In call to close(PARENT_READ_FD_):\nFailed to close pipe after normal shell termination.\n");
	exit(1);
      }
    }
    return 0;
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
      perror("In call to tcsetattr(FDIN_, TCSANOW, &oldTerm):\nFailed to restore original terminal settings..\n");
      exit(1);
    }
    return 0;
  }
}


void *readAndWritetoShell(void *fd) {
  /* Non-Canonical input:
   * read bytes continuously as they become available
   * and write them back continuously
   */
  int bytesRead = 0;
  char buff[BUFFERSIZE_];
  int *fd_int_ptr = (int *)fd;
  int rc = 0;
  while(!rc) {
    bytesRead = read(FDIN_, buff, BUFFERSIZE_);
    if(bytesRead < 0) {
      perror("In call to read(FDIN_, buff, BUFFERSIZE_):\nInput read failed.\n");
      exit(1);
    }
    else {
      rc = writeBack(buff, bytesRead) || sendToShell(buff, bytesRead, *fd_int_ptr);
    }
  }
  return NULL;
}

void *readFromShell(void *fd) {
  int bytesRead = 0;
  char buff[BUFFERSIZE_];
  int *fd_int_ptr = (int *)fd;
  int rc = 0;
  while(!rc) {
    bytesRead = read(*fd_int_ptr, buff, BUFFERSIZE_);
    if(bytesRead < 0) {
      perror("In call to read(*fd_int_ptr, buff, BUFFERSIZE_):\nRead from shell failed.\n");
      exit(2);
    }
    else {
      rc = writeBack(buff, bytesRead);
    }
  }
  return NULL;
}

int sendToShell(char *buff, int nBytes, int fd) {
  char writeByte;
  char lf = '\n';
  for(int i = 0; i < nBytes; ++i) {
    writeByte = buff[i];
    if(writeByte == '\n' || writeByte == '\r') {
      if(write(fd, &lf, 1) < 0) {
	perror("In call to write(fd, &lf, 1):\nWrite to shell failed.\n");
	exit(1);
      }
    }
    else if(writeByte == 0x004) {
      if(write(fd, &writeByte, 1) < 0) {
	perror("In call to write(fd, &writeByte, 1):\nFailed to send EOF to shell.\n");
	exit(1);
      }
      return 1;
    }
    else if(writeByte == 0x003) {
      if(kill(pid, SIGINT) < 0) {
        perror("In call to kill(pid, SIGINT):\nFailed to send signal to child process.\n");
	exit(1);
      }
    }
    else {
      if(write(fd, &writeByte, 1) < 0) {
	fprintf(stderr, "In call to write(fd, &writeByte, 1):\nFailed to write char %s to shell.\n", &writeByte);
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
	perror("In call to write(FDOUT_, crlf, 2):\nFailed to write to terminal.\n");
	exit(1);
      }
    } 
    else if(writeByte == 0x004){
      return 1;
    }
    else if(writeByte == 0x003) {
      // Do nothing
    }
    else {
      if(write(FDOUT_, &writeByte, 1) < 0) {
	perror("In call to write(FDOUT_, &writeByte, 1):\nFailed to write to terminal.\n");
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
    perror("In call to tcflush(FDIN_, TCIFLUSH):\nFailed to flush terminal input.\n");
    exit(1);
  }
  rc = tcsetattr(FDIN_, TCSANOW, &newTerm);
  if(rc != 0) {
    perror("In call to tcsetattr(FDIN_, TCSANOW, &newTerm):\nFailed to set terminal to non-canonical input.\n");
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
      perror("In call to read(FDIN_, buff, BUFFERSIZE_):\nInput read failed.\n");
      exit(1);
    }
    else {
      rc = writeBack(buff, bytesRead);
    }
  }
}

void exitCleanUp() {
  if(terminalModeChanged) {
    int rc = tcsetattr(FDIN_, TCSANOW, &oldTerm);
    if(rc != 0) {
      perror("In call to tcsetattr(FDIN_, TCSANOW, &oldTerm):\nFailed to restore terminal to canonical input.\n");
    }
  }
  if(forked) {
    int pidStatus;
    if(waitpid(pid, &pidStatus, WNOHANG) < 0) {
      perror("In call to waitpid(pid, &status, 0):\nwaitpid failed due to signal in calling process.\n");
    }
    int pidExitStatus = WEXITSTATUS(pidStatus);
    int pidStopSignal = WSTOPSIG(pidStatus);
    fprintf(stdout, "\nSHELL EXIT SIGNAL=%d STATUS=%d\n", pidExitStatus, pidStopSignal);
  }
}

void signalHandler(int SIGNUM) {
  if(SIGNUM == SIGPIPE) {
    exit(2);
  }
  if(SIGNUM == SIGCHLD) {
    exit(2);
  }
  else {
    fprintf(stdout, "received signal: %d\n", SIGNUM);
    exit(1);
  }
}

