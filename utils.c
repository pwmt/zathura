/* See LICENSE file for license and copyright information */

#include <stdlib.h>
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
  return NULL;
}
