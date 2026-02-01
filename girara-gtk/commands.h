/* SPDX-License-Identifier: Zlib */

#ifndef GIRARA_COMMANDS_H
#define GIRARA_COMMANDS_H

#include "types.h"

/**
 * Adds an inputbar command
 *
 * @param session The used girara session
 * @param command The name of the command
 * @param abbreviation The abbreviation of the command
 * @param function Executed function
 * @param completion Completion function
 * @param description Description of the command
 * @return TRUE No error occurred
 * @return FALSE An error occurred
 */
bool girara_inputbar_command_add(girara_session_t* session, const char* command, const char* abbreviation,
                                 girara_command_function_t function, girara_completion_function_t completion,
                                 const char* description) ;

/**
 * Adds a special command
 *
 * @param session The used girara session
 * @param identifier Char identifier
 * @param function Executed function
 * @param always If the function should executed on every change of the input
 *        (e.g.: incremental search)
 * @param argument_n Argument identifier
 * @param argument_data Argument data
 * @return TRUE No error occurred
 * @return FALSE An error occurred
 */
bool girara_special_command_add(girara_session_t* session, char identifier, girara_inputbar_special_function_t function,
                                bool always, int argument_n, void* argument_data) ;

/**
 * Parse input and execute the command
 *
 * @param session The used girara session
 * @param input User input
 * @return TRUE No error occurred
 * @return FALSE An error occured */
bool girara_command_run(girara_session_t* session, const char* input) ;

#endif
