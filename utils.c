/* See LICENSE file for license and copyright information */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "utils.h"

#define BLOCK_SIZE 64

bool
file_exists(const char* path)
{
  if(!access(path, F_OK)) {
    return true;
  } else {
    return false;
  }
}

const char*
file_get_extension(const char* path)
{
  if(!path) {
    return NULL;
  }

  unsigned int i = strlen(path);
  for(; i > 0; i--)
  {
    if(*(path + i) != '.')
      continue;
    else
      break;
  }

  if(!i) {
    return NULL;
  }

  return path + i + 1;
}

bool
execute_command(char* const argv[], char** output)
{
  if(!output) {
    return false;
  }

  int p[2];
  if(pipe(p))
    return -1;

  pid_t pid = fork();

  if(pid == -1) { // failure
    return false;
  }
  else if(pid == 0) { // child
    dup2(p[1], 1);
    close(p[0]);

    if(execvp(argv[0], argv) == -1) {
      return false;
    }
  }
  else { // parent
    dup2(p[0], 0);
    close(p[1]);

    /* read output */
    unsigned int bc = BLOCK_SIZE;
    unsigned int i  = 0;
    char* buffer    = malloc(sizeof(char) * bc);
    *output = NULL;

    if(!buffer) {
      close(p[0]);
      return false;
    }

    char c;
    while(1 == read(p[0], &c, 1)) {
      buffer[i++] = c;

      if(i == bc) {
        bc += BLOCK_SIZE;
        char* tmp = realloc(buffer, sizeof(char) * bc);

        if(!tmp) {
          free(buffer);
          close(p[0]);
          return false;
        }

        buffer = tmp;
      }
    }

    char* tmp = realloc(buffer, sizeof(char) * (bc + 1));
    if(!tmp) {
      free(buffer);
      close(p[0]);
      return false;
    }

    buffer = tmp;
    buffer[i] = '\0';

    *output = buffer;

    /* wait for child to terminate */
    waitpid(pid, NULL, 0);
    close(p[0]);
  }

  return true;
}
