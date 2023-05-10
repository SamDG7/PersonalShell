
/*
 * CS-252
 * shell.y: parser for shell
 *
 * This parser compiles the following grammar:
 *
 *	cmd [arg]* [> filename]
 *
 * you must extend it to understand the complete shell grammar
 *
 */

%code requires 
{
#include <string>

#if __cplusplus > 199711L
#define register      // Deprecated in C++11 so remove the keyword
#endif
#define MAXFILENAME 1024
}

%union
{
  char        *string_val;
  // Example of using a c++ type in yacc
  std::string *cpp_string;
}

%token <cpp_string> WORD
%token NOTOKEN GREAT NEWLINE LESS GREATGREAT PIPE AMPERSAND GREATAMPERSAND GREATGREATAMPERSAND TWOGREAT EXIT

%{
//#define yylex yylex

#include <cstdio>
#include <cstring>
#include <regex.h>
#include <dirent.h>
#include <algorithm>
#include <string.h>
#include <stdio.h>

#include "shell.hh"

void yyerror(const char * s);
int yylex();
void expandWildcardsIfNecessary(std::string * s);
void expandWildcard(char *pre, char * suff);
bool cmpfunction (char * i, char * j);
static std::vector<char *> _sortArgument = std::vector<char *>();

%}

%%
goal: command_list;

arg_list:
  arg_list argument
  | /*empty string*/
  ;

argument:
  WORD {
    //Command::_currentSimpleCommand->insertArgument( $1 );
    //expandWildcardsIfNecessary( $1 ); 
    char *p = (char *)"";
    expandWildcard(p, (char *)$1->c_str());
    std::sort(_sortArgument.begin(), _sortArgument.end(), cmpfunction);
    for (auto a: _sortArgument) {
      if (!strchr(a, '*')) {
        std::string * argToInsert = new std::string(a);
        Command::_currentSimpleCommand->insertArgument(argToInsert);
      }
    }
    _sortArgument.clear();
  }
  ;

cmd_and_args:
  commandword arg_list {
    Shell::_currentCommand.
    insertSimpleCommand( Command::_currentSimpleCommand );
  }
  ;

commandword:
  WORD {
    //printf("   Yacc: insert command \"%s\"\n", $1->c_str());
    Command::_currentSimpleCommand = new SimpleCommand();
    Command::_currentSimpleCommand->insertArgument( $1 );
  }
  ;

pipe_list:
  cmd_and_args
  | pipe_list PIPE cmd_and_args
  ;

io_modifier:
  GREATGREAT WORD {
    //printf("   Yacc: insert output \"%s\"\n", $2->c_str());
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._append = true;
  }
  | GREAT WORD {
    //printf("   Yacc: insert output \"%s\"\n", $2->c_str());
    if (Shell::_currentCommand._outFile != NULL) {
      Shell::_currentCommand._doubleRed = true;
    }
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._append = false;
  }
  | GREATGREATAMPERSAND WORD {
    //printf("   Yacc: insert output \"%s\"\n", $2->c_str());
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._errFile = $2;
    Shell::_currentCommand._append = true;
  }
  | GREATAMPERSAND WORD {
    //printf("   Yacc: insert output \"%s\"\n", $2->c_str());
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._errFile = $2;
    Shell::_currentCommand._append = false;
  }
  | LESS WORD {
    //printf("   Yacc: insert output \"%s\"\n", $2->c_str());
    Shell::_currentCommand._inFile = $2;
    Shell::_currentCommand._append = false;
  }
  | TWOGREAT WORD {
    //printf("   Yacc: insert output \"%s\"\n", $2->c_str());
    Shell::_currentCommand._errFile = $2;
    Shell::_currentCommand._append = false;
  }
  ;

io_modifier_list:
  io_modifier_list io_modifier
  | /*empty*/
  ;

background_optional:
  AMPERSAND {
    Shell::_currentCommand._background = true;
  }
  | /*empty*/
  ;

command_line:
    pipe_list io_modifier_list
  background_optional NEWLINE {
    //printf("   Yacc: Execute command\n");
    Shell::_currentCommand.execute();
    }
  | NEWLINE /*accept empty cmd line*/
  | error NEWLINE{yyerrok;}
  ;         /*error recovery*/

command_list :
  command_line |
  command_list command_line
  ;/* command loop*/

%%

void
yyerror(const char * s)
{
  fprintf(stderr,"%s", s);
}
bool cmpfunction (char * i, char * j) { return strcmp(i,j)<0; }

void expandWildcard(char * prefix, char * suffix) {

  if (suffix[0] == 0) {
    _sortArgument.push_back(strdup(prefix));
    return;
  }

  char Prefix[MAXFILENAME];

  if (prefix[0] == 0) {
    if (suffix[0] == '/') {
      suffix += 1;
      sprintf(Prefix, "%s/", prefix);
    } else {
      strcpy(Prefix, prefix);
    }

  } else {
    sprintf(Prefix, "%s/", prefix);
  }

  char * s = strchr(suffix, '/');
  char component[MAXFILENAME];

  if (s != NULL) {
    strncpy(component, suffix, s-suffix);
    component[s-suffix] = 0;
    suffix = s + 1;
  }
  else {
    strcpy(component, suffix);
    suffix = suffix + strlen(suffix);
  }

  char newPrefix[MAXFILENAME];

  if (strchr(component,'?')==NULL & strchr(component,'*')==NULL) {
    if (Prefix[0] == 0) {
      strcpy(newPrefix, component);
    }
    else {
      sprintf(newPrefix, "%s/%s", prefix, component);
    }
    expandWildcard(newPrefix, suffix);
    return;
  }
  
  char * reg = (char*)malloc(2*strlen(component)+10);
  char * r = reg;
  *r = '^'; r++;
  int i = 0;
  while (component[i]) {
    if (component[i] == '*') {*r='.'; r++; *r='*'; r++;}
    else if (component[i] == '?') {*r='.'; r++;}
    else if (component[i] == '.') {*r='\\'; r++; *r='.'; r++;}
    else {*r=component[i]; r++;}
    i++;
  }
  *r='$'; r++; *r=0;

  regex_t re;
  int expbuf = regcomp(&re, reg, REG_EXTENDED|REG_NOSUB);
  
  char * dir;
  if (Prefix[0] == 0) dir = (char*)"."; else dir = Prefix;
  DIR * d = opendir(dir);
  if (d == NULL) {
    return;
  }

  struct dirent * ent;
  bool find = false;
  while ((ent = readdir(d)) != NULL) {
    if(regexec(&re, ent->d_name, 1, NULL, 0) == 0) {
      find = true;
      if (Prefix[0] == 0) {
        strcpy(newPrefix, ent->d_name);
      }
      else {
        sprintf(newPrefix, "%s/%s", prefix, ent->d_name);
      }
      if (reg[1] == '.') {
        if (ent->d_name[0] != '.') expandWildcard(newPrefix, suffix);
      } else {
        expandWildcard(newPrefix, suffix);
      }
    }
  }

  if (!find) {
    if (Prefix[0] == 0) strcpy(newPrefix, component);
    else sprintf(newPrefix, "%s/%s", prefix, component);
    expandWildcard(newPrefix, suffix);
  }


  closedir(d);
  regfree(&re);
  free(reg);
}

#if 0
main()
{
  yyparse();
}
#endif
