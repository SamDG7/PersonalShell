#include <cstdio>

#include "shell.hh"
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

int yyparse(void);

void Shell::prompt() {
  if (isatty(0)) {
     printf("myshell>");
  }
  fflush(stdout);
}



void sighandler(int sig) {
  if (sig == SIGINT) {
    int yer = 1;
    printf("\n");
    if (Shell::_currentCommand._simpleCommands.empty() && Shell::_currentCommand._background == false) {
      Shell::prompt();
    }
  }
}

void zombhandler(int signum) {
  int pid = waitpid(-1, NULL, 0);

  while (pid > 0) {
    pid = waitpid(-1, NULL, WNOHANG);
   }
}

int main() {

  struct sigaction act;
  act.sa_handler = sighandler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_RESTART;
  sigaction(SIGINT, &act, NULL);


  struct sigaction zombie_act;
  zombie_act.sa_handler = zombhandler;
  sigemptyset(&zombie_act.sa_mask);
  sigaction(SIGCHLD, &zombie_act, NULL);

  if (isatty(0)) {
     Shell::prompt();
  }
  yyparse();

}

Command Shell::_currentCommand;
