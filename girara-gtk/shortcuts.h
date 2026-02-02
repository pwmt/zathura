/* SPDX-License-Identifier: Zlib */

#ifndef GIRARA_SHORTCUTS_H
#define GIRARA_SHORTCUTS_H

#include "types.h"
#include <glib.h>

/**
 * Adds a shortcut
 *
 * Note: currently argument_data accepts only a character string
 *
 * @param session The used girara session
 * @param modifier The modifier
 * @param key The key
 * @param buffer Buffer command
 * @param function Executed function
 * @param mode Available modes
 * @param argument_n Argument identifier
 * @param argument_data Argument data
 * @return true No error occurred
 * @return false An error occurred
 */
bool girara_shortcut_add(girara_session_t* session, guint modifier, guint key, const char* buffer,
                         girara_shortcut_function_t function, girara_mode_t mode, int argument_n, void* argument_data);

/**
 * Removes a shortcut
 *
 * @param session The used girara session
 * @param modifier The modifier
 * @param key The key
 * @param buffer Buffer command
 * @param mode Available modes
 * @return true No error occurred
 * @return false An error occurred
 */
bool girara_shortcut_remove(girara_session_t* session, guint modifier, guint key, const char* buffer,
                            girara_mode_t mode);

/**
 * Adds an inputbar shortcut
 *
 * @param session The used girara session
 * @param modifier The modifier
 * @param key The key
 * @param function Executed function
 * @param argument_n Argument identifier
 * @param argument_data Argument data
 * @return true No error occurred
 * @return false An error occurred
 */
bool girara_inputbar_shortcut_add(girara_session_t* session, guint modifier, guint key,
                                  girara_shortcut_function_t function, int argument_n, void* argument_data);

/**
 * Removes an inputbar shortcut
 *
 * @param session The used girara session
 * @param modifier The modifier
 * @param key The key
 * @return true No error occurred
 * @return false An error occurred
 */
bool girara_inputbar_shortcut_remove(girara_session_t* session, guint modifier, guint key);

/**
 * Default shortcut function to focus the inputbar
 *
 * @param session The used girara session
 * @param argument The argument
 * @param event Girara event
 * @param t Number of executions
 * @return true No error occurred
 * @return false An error occurred (abort execution)
 */
bool girara_sc_focus_inputbar(girara_session_t* session, girara_argument_t* argument, girara_event_t* event,
                              unsigned int t);

/**
 * Default shortcut function to abort
 *
 * @param session The used girara session
 * @param argument The argument
 * @param event Girara event
 * @param t Number of executions
 * @return true No error occurred
 * @return false An error occurred (abort execution)
 */
bool girara_sc_abort(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Toggles the visibility of the inputbar
 *
 * @param session The used girara session
 * @param argument The argument
 * @param event Girara event
 * @param t Numbr of execution
 * @return true No error occurred
 * @return false An error occurred (abort execution)
 */
bool girara_sc_toggle_inputbar(girara_session_t* session, girara_argument_t* argument, girara_event_t* event,
                               unsigned int t);

/**
 * Toggles the visibility of the statusbar
 *
 * @param session The used girara session
 * @param argument The argument
 * @param event Girara event
 * @param t Numbr of execution
 * @return true No error occurred
 * @return false An error occurred (abort execution)
 */
bool girara_sc_toggle_statusbar(girara_session_t* session, girara_argument_t* argument, girara_event_t* event,
                                unsigned int t);

/**
 * Passes the argument to the set command
 *
 * @param session The used girara session
 * @param argument The argument
 * @param event Girara event
 * @param t Number ofexecutions
 * @return true No error occurred
 * @return false An error occurred (abort execution)
 */
bool girara_sc_set(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Default shortcut function to activate
 *
 * @param session The used girara session
 * @param argument The argument
 * @param event Girara event
 * @param t Number of executions
 * @return true No error occurred
 * @return false An error occurred (abort execution)
 */
bool girara_isc_activate(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Default inputbar shortcut to abort
 *
 * @param session The used girara session
 * @param argument The argument
 * @param event Girara event
 * @param t Number of executions
 * @return true No error occurred
 * @return false An error occurred (abort execution)
 */
bool girara_isc_abort(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Default inputbar shortcut that completes the given input
 * in the statusbar
 *
 * @param session The used girara session
 * @param argument The argument
 * @param event Girara event
 * @param t Number of executions
 * @return true No error occurred
 * @return false An error occurred (abort execution)
 */
bool girara_isc_completion(girara_session_t* session, girara_argument_t* argument, girara_event_t* event,
                           unsigned int t);

/**
 * Default inputbar shortcut to manipulate the inputbar string
 *
 * @param session The used girara session
 * @param argument The argument
 * @param event Girara event
 * @param t Number of executions
 * @return true No error occurred
 * @return false An error occurred (abort execution)
 */
bool girara_isc_string_manipulation(girara_session_t* session, girara_argument_t* argument, girara_event_t* event,
                                    unsigned int t);

/**
 * Default inputbar shortcut to navigate through the command history
 *
 * @param session The used girara session
 * @param argument The argument
 * @param event Girara event
 * @param t Number of executions
 * @return true No error occurred
 * @return false An error occurred (abort execution)
 */
bool girara_isc_command_history(girara_session_t* session, girara_argument_t* argument, girara_event_t* event,
                                unsigned int t);

/**
 * Creates a mapping between a shortcut function and an identifier and is used
 * to evaluate the mapping command
 *
 * @param session The girara session
 * @param identifier Optional identifier
 * @param function The function that should be mapped
 * @return true if no error occurred
 */
bool girara_shortcut_mapping_add(girara_session_t* session, const char* identifier,
                                 girara_shortcut_function_t function);

/**
 * Creates a mapping between a shortcut argument and an identifier and is used
 * to evalue the mapping command
 *
 * @param session The girara session
 * @param identifier The identifier
 * @param value The value that should be represented
 * @return true if no error occurred
 */
bool girara_argument_mapping_add(girara_session_t* session, const char* identifier, int value);

/**
 * Adds a mouse event
 *
 * @param session The used girara session
 * @param mask The mask
 * @param button Pressed button
 * @param function Executed function
 * @param mode Available mode
 * @param event_type Event type
 * @param argument_n Argument identifier
 * @param argument_data Argument data
 * @return true No error occurred
 * @return false An error occurred
 */
bool girara_mouse_event_add(girara_session_t* session, guint mask, guint button, girara_shortcut_function_t function,
                            girara_mode_t mode, girara_event_type_t event_type, int argument_n, void* argument_data);

/**
 * Removes a mouse event
 *
 * @param session The used girara session
 * @param mask The mask
 * @param button Pressed button
 * @param mode Available mode
 * @return true No error occurred
 * @return false An error occurred
 */
bool girara_mouse_event_remove(girara_session_t* session, guint mask, guint button, girara_mode_t mode);

#endif
