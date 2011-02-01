/* See LICENSE file for license and copyright information */

#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <gtk/gtk.h>

/**
 * Checks if the given file exists
 *
 * @param path Path to the file
 * @return true if the file exists, otherwise false
 */
bool file_exists(const char* path);

/**
 * Returns the file extension of a path
 *
 * @param path Path to the file
 * @return The file extension or NULL
 */
const char* file_get_extension(const char* path);

/**
 * Executes a system command and saves its output into output
 *
 * @param argv The command
 * @param output Pointer where the output will be saved
 * @return true if no error occured, otherwise false
 */
bool execute_command(char* const argv[], char** output);

/**
 * Creates a blank page
 *
 * @param width The width of the page
 * @param height The height of the page
 * @return The widget of the page or NULL if an error occured
 */
GtkWidget* page_blank(unsigned int width, unsigned int height);

#endif // UTILS_H
