/* SPDX-License-Identifier: Zlib */

#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdbool.h>
#include <girara/types.h>

/**
 * Create a bookmark
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occurred
 */
bool cmd_bookmark_create(girara_session_t* session, girara_list_t* argument_list);

/**
 * Delete a bookmark
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occurred
 */
bool cmd_bookmark_delete(girara_session_t* session, girara_list_t* argument_list);

/**
 * List bookmarks
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occurred
 */
bool cmd_bookmark_list(girara_session_t* session, girara_list_t* argument_list);

/**
 * Open a bookmark
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occurred
 */
bool cmd_bookmark_open(girara_session_t* session, girara_list_t* argument_list);

/**
 * Show recent jumps in jumplist
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occurred
 */
bool cmd_jumplist_list(girara_session_t* session, girara_list_t* argument_list);

/**
 * Close zathura
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occurred
 */
bool cmd_close(girara_session_t* session, girara_list_t* argument_list);

/**
 * Display document information
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occurred
 */
bool cmd_info(girara_session_t* session, girara_list_t* argument_list);

/**
 * Display help
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occurred
 */
bool cmd_help(girara_session_t* session, girara_list_t* argument_list);

/**
 * Shows current search results
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occurred
 */
bool cmd_hlsearch(girara_session_t* session, girara_list_t* argument_list);

/**
 * Opens a document file
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occurred
 */
bool cmd_open(girara_session_t* session, girara_list_t* argument_list);

/**
 * Print the current file
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occurred
 */
bool cmd_print(girara_session_t* session, girara_list_t* argument_list);

/**
 * Hides current search results
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occurred
 */
bool cmd_nohlsearch(girara_session_t* session, girara_list_t* argument_list);

/**
 * Close zathura
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occurred
 */
bool cmd_quit(girara_session_t* session, girara_list_t* argument_list);

/**
 * Save the current file
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occurred
 */
bool cmd_save(girara_session_t* session, girara_list_t* argument_list);

/**
 * Save the current file and overwrite existing files
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occurred
 */
bool cmd_savef(girara_session_t* session, girara_list_t* argument_list);

/**
 * Search the current file
 *
 * @param session The used girara session
 * @param input The current input
 * @param argument Passed argument
 * @return true if no error occurred
 */
bool cmd_search(girara_session_t* session, const char* input, girara_argument_t* argument);

/**
 * Save attachment to a file
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occurred
 */
bool cmd_export(girara_session_t* session, girara_list_t* argument_list);

/**
 * Execute command
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occurred
 */
bool cmd_exec(girara_session_t* session, girara_list_t* argument_list);

/**
 * Set page offset
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occurred
 */
bool cmd_offset(girara_session_t* session, girara_list_t* argument_list);

/**
 * Shows version information
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occurred
 */
bool cmd_version(girara_session_t* session, girara_list_t* argument_list);

/**
 * Source config file
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return true if no error occurred
 */
bool cmd_source(girara_session_t* session, girara_list_t* argument_list);

bool cmd_explorer(girara_session_t* session, girara_list_t* argument_list);

#endif // COMMANDS_H
