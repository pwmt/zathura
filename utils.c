/* See LICENSE file for license and copyright information */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

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
