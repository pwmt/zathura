/* See LICENSE file for license and copyright information */

#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <gtk/gtk.h>

bool file_exists(const char* path);
const char* file_get_extension(const char* path);
bool execute_command(char* const argv[], char** output);
GdkPixbuf* page_blank(unsigned int width, unsigned int height);

#endif // UTILS_H
