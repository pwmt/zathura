/* SPDX-License-Identifier: Zlib */

#ifndef MARKS_H
#define MARKS_H

#include <girara/types.h>

#include "zathura.h"

/**
 * Saves a mark
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_mark_add(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Evaluates a mark
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_mark_evaluate(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Mark current location within the web page
 *
 * @param session The girara session
 * @param argument_list Argument list
 * @return true if no error occurred otherwise false
 */
bool cmd_marks_add(girara_session_t* session, girara_list_t* argument_list);

/**
 * Delete the specified marks
 *
 * @param session The girara session
 * @param argument_list Argument list
 * @return true if no error occurred otherwise false
 */
bool cmd_marks_delete(girara_session_t* session, girara_list_t* argument_list);

/**
 * Load and set quickmarks from database
 *
 * @param zathura The zathura instance
 * @param file Open file
 * @return true if no error occurred otherwise false
 */
bool zathura_quickmarks_load(zathura_t* zathura, const gchar* file);

#endif // MARKS_H
