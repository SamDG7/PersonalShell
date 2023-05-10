
/*
 *
 * CS-252 
 * shell.l: lexical analyzer for shell
 * You have to extend it.
 *
 */

%{

#include <cstring>
#include <algorithm>
#include <sys/wait.h>
#include <filesystem>
#include <unistd.h>
#include "y.tab.hh"

static  void yyunput (int c,char *buf_ptr  );

void myunputc(int c) {
  unput(c);
}

extern "C" char * read_line();

int mygetc(FILE * f) {
  static char *p;
  char ch;

  if (!isatty(0)) {
    return getc(f);
  }

  if (p==NULL || *p == 0) {
    char * s = read_line();
    p =s;
  }
  ch = *p;
  p++;

  return ch;
}

#undef getc
#define getc(f) mygetc(f)

%}

%option noyywrap

%%

\n {
  return NEWLINE;
}

\`[^\n]*\` {
  printf("subshell time");
}

~[^ \n\t]* {
  std::string str = std::string(yytext);
  std::string home = getenv("HOME");
  if (str.size() == 1) {
    yylval.cpp_string = new std::string(home);
  } 
  else {
    if (str[1] == '/') {
      str = home + str.substr(1, -1);
    } 
    else {
      str = std::string("/homes/") + str.substr(1,-1);
    }
    yylval.cpp_string = new std::string(str);
  }
  return WORD;
}

["$"]["{"][^ \t\n][^ \t\n]* {
  //environment expansion
  // \$\{[A-Za-z0-9!_?]+\} 
  //yylval.cpp_string = new std::string(yytext);
  //yylval.cpp_string->erase(0, 2);
  //yylval.cpp_string->erase(yylval.cpp_string->size() - 1);

  std::string temp = yytext;

  //fprintf(stderr, "VAR CAUGHT: %s\n", temp.c_str());

  while (temp.find('$') != std::string::npos) {
    int index_of_dollar = temp.find('$');
    int index_of_close = temp.find('}');

    std::string to_sub = temp.substr(index_of_dollar, index_of_close - index_of_dollar + 1);
    to_sub.erase(0,2);
    to_sub.erase(to_sub.size()-1);

    //fprintf(stderr, "TOSUB: %s\n", to_sub.c_str());
    if (to_sub == "SHELL") {
      std::filesystem::path path = std::filesystem::canonical("/proc/self/exe");
      
      //char * path = realpath( (char *) std::filesystem::current_path(), NULL);
      to_sub = std::string(path);
    }
    else if (to_sub == "$") {
      //fprintf(stderr, "At least in here\n");
      int p = getpid();
      char * mypid = (char*) malloc(6);
      sprintf(mypid, "%d", p);

      
      to_sub = std::string(mypid);
      //fprintf(stderr, "%s\n", p);
    }
    else {
      char **p = environ;
      while (*p != NULL) {
        std::string var(*p);
        int index = var.find("=", 0);
        std::string sub = var.substr(0, index);
        if (sub == to_sub) {
          to_sub = var.substr(index+1, var.size()-1);
          break;
        }
        p++;
      }
    }
    temp.replace(index_of_dollar, index_of_close - index_of_dollar + 1, to_sub);
    //fprintf(stderr, "TEMP AFTER REPLACE: %s\n", temp.c_str());

  }

  yylval.cpp_string = new std::string(temp.c_str());

  //fprintf(stderr, "%s\n", yylval.cpp_string->c_str());


  return WORD;
}

\".*?\" {
  //remove quotes
  yylval.cpp_string = new std::string(yytext);
  yylval.cpp_string->erase(0, 1);
  yylval.cpp_string->erase(yylval.cpp_string->size() - 1);
  return WORD;
}



[ \t] {
  /* Discard spaces and tabs */
}

">" {
  return GREAT;
}

">>" {
  return GREATGREAT;
}

"|" {
  return PIPE;
}

"&" {
  return AMPERSAND;
}

"<" {
  return LESS;
}

">&" {
  return GREATAMPERSAND;
}

">>&" {
  return GREATGREATAMPERSAND;
}

"2>" {
  return TWOGREAT;
}

"exit" {
  printf("Good bye!!\n");
  exit(1);
  return EXIT;
}

[^ \t\n|><]*\\[^ \t\n|><]*.+ {
  yylval.cpp_string = new std::string(yytext);
  //printf("%s\n", yylval.cpp_string->c_str());

  int index = yylval.cpp_string->find("\\", 0);

  while (index != std::string::npos) {
    yylval.cpp_string->erase(index, 1);
    index = yylval.cpp_string->find("\\", index + 1);
    //printf("%s\n", yylval.cpp_string->c_str());
  }

  return WORD;
}

\$\([^\n]*\) {
  //printf("subshell time");
  
  std::string subshell_cmd = yytext;

  subshell_cmd.erase(0,2);
  subshell_cmd.erase(subshell_cmd.size()- 1);


  int pin[2], pout[2];
  pipe(pin);
  pipe(pout);

  int defIn = dup(0);
  int defOut = dup(1);

  dup2(pin[0], 0);
  dup2(pout[1], 1);

  close(pin[0]);
  close(pout[1]);
  //fprintf(stderr, "SUBSHELL COMMAND: %s\n", subshell_cmd.c_str());

  write(pin[1], subshell_cmd.c_str(), strlen(subshell_cmd.c_str()));
  write(pin[1], "\n", 1);
  write(pin[1], "exit\n",5);
  close(pin[1]);

  int ret = fork();

  if (ret == 0) {
    execvp("/proc/self/exe", NULL);
    _exit(1);
  } else if (ret < 0) {
    perror("fork issue\n");
    _exit(2);
  }

  //waitpid(ret, NULL, 0);

  dup2(defIn, 0);
  dup2(defOut, 1);
  close(defIn);
  close(defOut);

  //output of subshell now in pout[0]
  char * charout = (char *) malloc(5000);
  char ch;

  int ln = read(pout[0], &ch, 1);

  //printf("%d\n", ln);

  int idx = 0;
  
  while (ln != 0) {
    charout[idx] = ch;

    idx++;

    ln = read(pout[0], &ch, 1);
  }

  //fprintf(stderr, "CHAROUT:\n%s\n", charout);


  for (int i = idx - 12; i >= 0; i--) {
    if (charout[i] == '\n') {
      charout[i] = ' ';
    }
    myunputc(charout[i]);
  }
  delete(charout);
}

[^ \t\n|><][^ \t\n|><]*  {

  //right now  not including $
  /* Assume that file names have only alpha chars */
   yylval.cpp_string = new std::string(yytext);

   //printf("CURRENT WORD: %s\n", yylval.cpp_string->c_str());

  return WORD;
}
