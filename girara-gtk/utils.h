/* SPDX-License-Identifier: Zlib */

#ifndef GIRARA_GTK_UTILS_H
#define GIRARA_GTK_UTILS_H

#include <girara/utils.h>

#include "types.h"

int girara_list_cmpstr(const void* lhs, const void* rhs);

/**
 * Execute command from argument list
 *
 * @param session The used girara session
 * @param argument_list The argument list
 * @return true if no error occurred
 */
bool girara_exec_with_argument_list(girara_session_t* session, girara_list_t* argument_list);

#endif
