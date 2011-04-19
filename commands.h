/* See LICENSE file for license and copyright information */

#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdbool.h>
#include <girara.h>

/**
 * Create a bookmark
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occured
 */
bool cmd_bookmark_create(girara_session_t* session, girara_list_t* argument_list);

/**
 * Delete a bookmark
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occured
 */
bool cmd_bookmark_delete(girara_session_t* session, girara_list_t* argument_list);

/**
 * Open a bookmark
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occured
 */
bool cmd_bookmark_open(girara_session_t* session, girara_list_t* argument_list);

/**
 * Close zathura
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occured
 */
bool cmd_close(girara_session_t* session, girara_list_t* argument_list);

/**
 * Display document information
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occured
 */
bool cmd_info(girara_session_t* session, girara_list_t* argument_list);

/**
 * Print the current file
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occured
 */
bool cmd_print(girara_session_t* session, girara_list_t* argument_list);

/**
 * Save the current file
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occured
 */
bool cmd_save(girara_session_t* session, girara_list_t* argument_list);

#endif // COMMANDS_H
