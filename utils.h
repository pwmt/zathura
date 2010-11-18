/* See LICENSE file for license and copyright information */

#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>

bool file_exists(const char* path);
const char* file_get_extension(const char* path);

#endif // UTILS_H
