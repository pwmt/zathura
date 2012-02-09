/* See LICENSE file for license and copyright information */

#ifndef COMPLETION_H
#define COMPLETION_H

#include <girara/types.h>

/**
 * Completion for the open command - Creates a list of accesible directories or
 * files
 *
 * @param session The used girara session
 * @param input The current input
 * @return The completion object or NULL if an error occured
 */
girara_completion_t* cc_open(girara_session_t* session, const char* input);

/**
 * Completion for the bmarks command - Creates a list of bookmarks
 *
 * @param session The used girara session
 * @param input The current input
 * @return The completion object or NULL if an error occured
 */
girara_completion_t* cc_bookmarks(girara_session_t* session, const char* input);

/**
 * Completion for the export command - Creates a list of attachments
 *
 * @param session the girara session
 * @param input the current input
 * @return completion object, NULL on error
 */
girara_completion_t* cc_export(girara_session_t* session, const char* input);


#endif // COMPLETION_H
