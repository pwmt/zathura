/* See LICENSE file for license and copyright information */

#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdbool.h>
#include <girara.h>

/**
 * Create a bookmark
 *
 * @param session The used girara session
 * @param argc Number of arguments
 * @param argv Value of the arguments
 * @return true if no error occured
 */
bool cmd_bookmark_create(girara_session_t* session, int argc, char** argv);

/**
 * Delete a bookmark
 *
 * @param session The used girara session
 * @param argc Number of arguments
 * @param argv Value of the arguments
 * @return true if no error occured
 */
bool cmd_bookmark_delete(girara_session_t* session, int argc, char** argv);

/**
 * Open a bookmark
 *
 * @param session The used girara session
 * @param argc Number of arguments
 * @param argv Value of the arguments
 * @return true if no error occured
 */
bool cmd_bookmark_open(girara_session_t* session, int argc, char** argv);

/**
 * Close zathura
 *
 * @param session The used girara session
 * @param argc Number of arguments
 * @param argv Value of the arguments
 * @return true if no error occured
 */
bool cmd_close(girara_session_t* session, int argc, char** argv);

/**
 * Display document information
 *
 * @param session The used girara session
 * @param argc Number of arguments
 * @param argv Value of the arguments
 * @return true if no error occured
 */
bool cmd_info(girara_session_t* session, int argc, char** argv);

/**
 * Print the current file
 *
 * @param session The used girara session
 * @param argc Number of arguments
 * @param argv Value of the arguments
 * @return true if no error occured
 */
bool cmd_print(girara_session_t* session, int argc, char** argv);

/**
 * Save the current file
 *
 * @param session The used girara session
 * @param argc Number of arguments
 * @param argv Value of the arguments
 * @return true if no error occured
 */
bool cmd_save(girara_session_t* session, int argc, char** argv);

#endif // COMMANDS_H
