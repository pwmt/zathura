/* SPDX-License-Identifier: Zlib */

#ifndef GIRARA_CONFIG_H
#define GIRARA_CONFIG_H

#include "types.h"

/**
 * Parses and evaluates a configuration file
 *
 * @param session The used girara session
 * @param path Path to the configuration file
 */
void girara_config_parse(girara_session_t* session, const char* path) ;

/**
 * Adds an additional config handler
 *
 * @param session The girara session
 * @param identifier Identifier of the handle
 * @param handle Handle
 * @return true if no error occurred, otherwise false
 */
bool girara_config_handle_add(girara_session_t* session, const char* identifier,
                              girara_command_function_t handle) ;

#endif
