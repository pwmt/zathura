/* SPDX-License-Identifier: Zlib */

#ifndef GIRARA_COMPLETION_H
#define GIRARA_COMPLETION_H

#include "types.h"

/**
 * Creates an girara completion object
 *
 * @return Completion object
 * @return NULL An error occurred
 */
girara_completion_t* girara_completion_init(void) ;

/**
 * Creates an girara completion group
 *
 * @return Completion object
 * @return NULL An error occurred
 */
girara_completion_group_t* girara_completion_group_create(girara_session_t* session, const char* name) ;

/**
 * Frees a completion group
 *
 * @param group The group
 */
void girara_completion_group_free(girara_completion_group_t* group) ;

/**
 * Adds an group to a completion object
 *
 * @param completion The completion object
 * @param group The completion group
 */
void girara_completion_add_group(girara_completion_t* completion, girara_completion_group_t* group) ;

/**
 * Frees an completion and all of its groups and elements
 *
 * @param completion The completion
 */
void girara_completion_free(girara_completion_t* completion) ;

/**
 * Adds an element to a completion group
 *
 * @param group The completion group
 * @param value Value of the entry
 * @param description Description of the entry
 */
void girara_completion_group_add_element(girara_completion_group_t* group, const char* value,
                                         const char* description) ;

#endif
