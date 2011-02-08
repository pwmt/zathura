/* See LICENSE file for license and copyright information */

#ifndef COMPLETION_H
#define COMPLETION_H

#include <girara.h>

/**
 * Completion for the print command. Creates a list of available printers
 *
 * @param session The used girara session
 * @param input The current input
 * @return The completion object or NULL if an error occured
 */
girara_completion_t* cc_print(girara_session_t* session, char* input);

#endif // COMPLETION_H
