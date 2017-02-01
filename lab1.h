/* \file lab1.h
 * \author Jesse Joseph
 * \email jjoseph@hmc.edu
 * \ID 040161840
 * \brief Interface for lab1.c
 *
 * The lab1a program converts the terminal to non-canonical input
 * and echos written characters back.
 *
 * Adding the --shell option opens a bash shell in a child process,
 * and writing commands into the lab1a program interface functions
 * similarly to writing them into the bash prompt. Output from the 
 * bash shell is sent back to the terminal.
 *
 * To quit the program:
 * Send the EOF character with ctrl-D.
 *
 * Sending ctrl-c sends a SIGINT to the child shell, which forces
 * the child shell to exit. I don't believe this is what was 
 * intended by the assignment
 */

#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <sys/wait.h>

int main(int argc, char **argv);

/* \brief Writes contents of buff, up to buff[nBytes], to stdout
 * \return 0 if successful, 1 for special EOF character.
 */
int writeBack(char *buff, int nBytes);

/* \brief Cleanup function to restore terminal settings and collect shell exit status
 */
void exitCleanUp();

/* \brief Signal handler designed to catch SIGCHLD and SIGPIPE 
 */
void signalHandler(int SIGNUM);

/* \brief For non-shell execution - simply echo back input, mapping \n or \r to \r\n
 */
void continuousNonCanonicalRead();

/* \brief Changes terminal into byte-at-a-time non canonical input mode.
 */
void setTerminalToNonCanonicalInput();

/* \brief Similar to writeBack, but sends to output specified by file descriptor fd. Also has specific handling of special characters.
 */
int sendToShell(char *buff, int nBytes, int fd);

/* \brief Executes in a pthread; takes keyboard input and sends it to output specified by file descriptor fd.
 */
void *readAndWritetoShell(void *fd);

/* \brief Executes in a pthread; reads input source specified by fd and writes it to the terminal
 */
void *readFromShell(void *fd);
