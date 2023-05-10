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

#include "command.hh"
#include "shell.hh"


Command::Command() {
    // Initialize a new vector of Simple Commands
    _simpleCommands = std::vector<SimpleCommand *>();

    _outFile = NULL;
    _inFile = NULL;
    _errFile = NULL;
    _background = false;
    _append = false;
}

void Command::insertSimpleCommand( SimpleCommand * simpleCommand ) {
    // add the simple command to the vector
    _simpleCommands.push_back(simpleCommand);
}

void Command::clear() {
    // deallocate all the simple commands in the command vector
    for (auto simpleCommand : _simpleCommands) {
        delete simpleCommand;
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
    print();

    // Add execution here

    int defaultIn = dup(0);
    int defaultOut = dup(1);
    int defaulterr = dup(2);
    int infile;
    int outfile;
    int errorfile;

    if (Command::_inFile != NULL) {
        infile = open(_inFile->c_str(), O_RDONLY);
    }
    else {
        infile = dup(defaultIn);
    }


    for (int i = 0; i < _simpleCommands.size(); i++) {
      SimpleCommand * curr = _simpleCommands[i];

      dup2(infile, 0);
      close(infile);
      
      if (i == _simpleCommands.size() - 1) {

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


      }
      else {
        int fdpipe[2];
        pipe(fdpipe);

        infile = dup(fdpipe[0]);
        outfile = dup(fdpipe[1]);

        close(fdpipe[0]);
        close(fdpipe[1]);
      }

      dup2(errorfile, 2);
      close(errorfile);


      dup2(outfile, 1);
      close(outfile);

      int ret = fork();

      if (ret == 0) {
        //this is a child process
        char** list_of_args = (char**) malloc(curr->_arguments.size() * sizeof(curr->_arguments[0]));
        for (int i = 0; i < curr->_arguments.size(); i++) {
            list_of_args[i] = (char*) (curr->_arguments[i])->c_str();
        }
        list_of_args[curr->_arguments.size()] = NULL;

        execvp( (const char*) list_of_args[0], list_of_args);

        //error occured
        perror("Execvp error in creating child process");
        exit(1);
      }
      else if (ret < 0) {
         perror("Error with fork()");
         exit(2);
      }
      else {
        //parent process waits for child to exit
        if (Command::_background == false) {
          waitpid(ret, NULL, 0);
        }
      }
    }

    dup2(defaultIn, 0);
    dup2(defaultOut, 1);
    dup2(defaulterr, 2);

    close(defaultIn);
    close(defaultOut);
    close(defaulterr);

    if (Command::_errFile == Command::_outFile) {
      Command::_outFile = NULL;
    }

    // Clear to prepare for next command
    clear();

    // Print new prompt
    Shell::prompt();
}

SimpleCommand * Command::_currentSimpleCommand;
