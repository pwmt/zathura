/* See LICENSE file for license and copyright information */

#ifndef COMPLETION_H
#define COMPLETION_H

#include <girara.h>

/**
 * Completion for the open command - Creates a list of accesible directories or
 * files
 *
 * @param session The used girara session
 * @param input The current input
 * @return The completion object or NULL if an error occured
 */
girara_completion_t* cc_open(girara_session_t* session, char* input);

#endif // COMPLETION_H
