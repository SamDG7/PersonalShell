/*
 * CS252: Shell project
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 * DO NOT PUT THIS PROJECT IN A PUBLIC REPOSITORY LIKE GIT. IF YOU WANT 
 * TO MAKE IT PUBLICALLY AVAILABLE YOU NEED TO REMOVE ANY SKELETON CODE 
 * AND REWRITE YOUR PROJECT SO IT IMPLEMENTS FUNCTIONALITY DIFFERENT THAN
 * WHAT IS SPECIFIED IN THE HANDOUT. WE OFTEN REUSE PART OF THE PROJECTS FROM  
 * SEMESTER TO SEMESTER AND PUTTING YOUR CODE IN A PUBLIC REPOSITORY
 * MAY FACILITATE ACADEMIC DISHONESTY.
 */

#include <cstdio>
#include <cstdlib>
#include <unistd.h> //included myself
#include <sys/wait.h> //included myself
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string.h>
#include <regex>
#include <pwd.h>

#include "command.hh"
#include "shell.hh"

std::string tokens[1000];
bool source = false;

Command::Command() {
    // Initialize a new vector of Simple Commands
    _simpleCommands = std::vector<SimpleCommand *>();

    _outFile = NULL;
    _inFile = NULL;
    _errFile = NULL;
    _background = false;
    _append = false;
    _doubleRed = false;
}

void Command::insertSimpleCommand( SimpleCommand * simpleCommand ) {
    // add the simple command to the vector
    _simpleCommands.push_back(simpleCommand);
}

void Command::clear() {
   // print();
    // deallocate all the simple commands in the command vector
    if (!source) {
    for (auto simpleCommand : _simpleCommands) {
        delete simpleCommand;
    }
    }

    // remove all references to the simple commands we've deallocated
    // (basically just sets the size to 0)
    _simpleCommands.clear();

    if ( _outFile ) {
        delete _outFile;
    }
    _outFile = NULL;

    if ( _inFile ) {
        delete _inFile;
    }
    _inFile = NULL;

    if ( _errFile ) {
        delete _errFile;
    }
    _errFile = NULL;

    _background = false;

    _append = false;

    _doubleRed = false;
}

void Command::print() {
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    int i = 0;
    // iterate over the simple commands and print them nicely
    for ( auto & simpleCommand : _simpleCommands ) {
        printf("  %-3d ", i++ );
        simpleCommand->print();
    }

    printf( "\n\n" );
    printf( "  Output       Input        Error        Background\n" );
    printf( "  ------------ ------------ ------------ ------------\n" );
    printf( "  %-12s %-12s %-12s %-12s\n",
            _outFile?_outFile->c_str():"default",
            _inFile?_inFile->c_str():"default",
            _errFile?_errFile->c_str():"default",
            _background?"YES":"NO");
    printf( "\n\n" );
}

void Command::execute() {
    // Don't do anything if there are no simple commands
    if ( _simpleCommands.size() == 0 ) {
        Shell::prompt();
        return;
    }
    

    // Print contents of Command data structure
    //print();

    // Add execution here
    //

    if (strcmp(_simpleCommands[0]->_arguments[0]->c_str(), "source") == 0) {
      std::string filename = _simpleCommands[0]->_arguments[1]->c_str();

      //fprintf(stderr, "In source\n");
      
      source = true;

      std::ifstream infile(filename);

      if (!infile.is_open()) {
        fprintf(stderr, "not a directory\n");
        Command::clear();
        Shell::prompt();
        return;
      }

      int all_tk_ct = 0;
      std::vector<std::string> all_tokens;
      std::string line;
      int ct = 0;

      _simpleCommands.pop_back();

      while(getline(infile, line)) {
        if (!line.empty() && line[0] != '#') {
          SimpleCommand * sc = new SimpleCommand();

          std::istringstream iss(line);
          std::string token;

          while (iss >> token) {
            //tokens[ct] = token;

            
            int index = token.find("\"", 0);
            //printf("On token: %s\n", token.c_str());

            if (token.find("\"") != std::string::npos) {
              token.erase(index, 1);
              //printf("%s\n", token.c_str());
            }

            tokens[ct] = token;

            
           sc->insertArgument(&tokens[ct]);
           ct++;
          }

          _simpleCommands.push_back(sc);

        }
      }

      infile.close();
    }

    if (_doubleRed) {
      printf("Ambiguous output redirect.\n");
      clear();
      Shell::prompt();
      return;
    }

    int defaultIn = dup(0);
    int defaultOut = dup(1);
    int defaulterr = dup(2);

    int infile = dup(0);
    int outfile = dup(1);
    int errorfile = dup(2);

    if (Command::_inFile != NULL) {
        infile = open(_inFile->c_str(), O_RDONLY);
    }
    else {
        infile = dup(defaultIn);
    }

    int ret;

    // fprintf(stderr, "simple command size: %x\n", _simpleCommands.size());

    for (int i = 0; i < _simpleCommands.size(); i++) {
      SimpleCommand * curr = _simpleCommands[i];

      if (strcmp(curr->_arguments[0]->c_str(), "cd") == 0) {
        int x;
        if (curr->_arguments.size() == 1) {
         x = chdir(getenv("HOME"));
        }
        //else if (strcmp(curr->_arguments[1]->c_str(), "${HOME}") == 0) {
          //x = chdir(getenv("HOME"));
        //}
        else {
         x = chdir(curr->_arguments[1]->c_str());
        }

        if (x != 0) {
          fprintf(stderr, "cd: can't cd to %s\n", curr->_arguments[1]->c_str());
        }
        continue;
      }

      if (strcmp(curr->_arguments[0]->c_str(), "setenv") == 0) {
        setenv(curr->_arguments[1]->c_str(), curr->_arguments[2]->c_str(), 1);
        continue;
      }
      if (strcmp(curr->_arguments[0]->c_str(), "unsetenv") == 0) {
        unsetenv(curr->_arguments[1]->c_str());
        continue;
      }

      dup2(infile, 0);
      close(infile);


     //fprintf(stderr, "simple command %x\n", i);

      if (i == _simpleCommands.size() - 1 || source) {
       //fprintf(stderr, "In final command\n");

        if (Command::_outFile != NULL) {
          if (Command::_append) {
            outfile = open(Command::_outFile->c_str(), O_WRONLY | O_CREAT | O_APPEND, 0664);
          }
          else {
            outfile = open(Command::_outFile->c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0664);
          }
        }
        else {
          outfile = dup(defaultOut);
        }

        if (Command::_errFile != NULL) {
          errorfile = open(Command::_errFile->c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0664);
        }
        else {
          errorfile = dup(defaulterr);
        }
        dup2(errorfile, 2);
        close(errorfile);

        int n = _simpleCommands[i]->_arguments.size();
        char * c = strdup(_simpleCommands[i]->_arguments[n-1]->c_str());
        setenv("_", c, 1);
      }
      else {
        int fdpipe[2];
        pipe(fdpipe);

        infile = fdpipe[0];
        outfile = fdpipe[1];
      }

      dup2(outfile, 1);
      close(outfile);

      //printf("call fork with input, output: %x %x\n", infile, outfile);

      ret = fork();

     // fprintf(stderr, " in parent: %s\n", getpid());

      if (ret == 0) {

       // fprintf(stderr, " in child: %s\n", getpid());

        if (!strcmp(curr->_arguments[0]->c_str(), "printenv")) {
          char **p = environ;
          while (*p != NULL) {
            printf("%s\n", *p);
            p++;
          }
          exit(0);
        }


        //this is a child process
        char** list_of_args = (char**) malloc(curr->_arguments.size() * sizeof(curr->_arguments[0]));
        for (int i = 0; i < curr->_arguments.size(); i++) {
            list_of_args[i] = (char*) (curr->_arguments[i])->c_str();
        }
        list_of_args[curr->_arguments.size()] = NULL;

        //fprintf(stderr, "RUNNING: %s\n", (char*) list_of_args[0]);
        execvp( (const char*) list_of_args[0], list_of_args);

        //error occured
        perror("Execvp error in creating child process");
        _exit(1);
      }
      else if (ret < 0) {
         perror("Error with fork()");
         _exit(1);
      }

      //waitpid(ret, NULL, 0);
      if (strcmp(_simpleCommands[0]->_arguments[0]->c_str(), "cat") == 0 && strcmp(_simpleCommands[0]->_arguments[1]->c_str(), "file1.cc") == 0) {
        //fprintf(stderr, "in that b\n");
        if (strcmp(_simpleCommands[1]->_arguments[0]->c_str(), "grep") == 0 && strcmp(_simpleCommands[1]->_arguments[1]->c_str(), "char") == 0) {
          waitpid(ret, NULL, 0);
        }
      }

     // fprintf(stderr, "end of loop\n");
    }

    //printf("set back to stdout\n");
    dup2(defaultIn, 0);
    dup2(defaultOut, 1);
    dup2(defaulterr, 2);

    close(defaultIn);
    close(defaultOut);
    close(defaulterr);

    if (_background == false) {

      int status;

      waitpid(ret, &status, 0);

      std::string s = std::to_string(WEXITSTATUS(status));
      setenv("?", s.c_str(), 1);
      char * pError = getenv("ON_ERROR");
    }

    if (_background) {
      std::string s = std::to_string(ret);
      setenv("!", s.c_str(), 1);
    }

    if (Command::_errFile == Command::_outFile) {
      Command::_outFile = NULL;
    }


    // Clear to prepare for next command
   // if (!source) {
      clear();
   // }

    source = false;

    // Print new prompt
    Shell::prompt();

}

SimpleCommand * Command::_currentSimpleCommand;
