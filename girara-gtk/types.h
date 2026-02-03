/* SPDX-License-Identifier: Zlib */

#ifndef GIRARA_GTK_TYPES_H
#define GIRARA_GTK_TYPES_H

#include <girara/types.h>
#include <girara/macros.h>
#include <stdbool.h>
#include <glib-object.h>

typedef struct girara_setting_s girara_setting_t;
typedef struct girara_session_s girara_session_t;
typedef struct girara_session_private_s girara_session_private_t;
typedef struct girara_command_s girara_command_t;
typedef struct girara_mouse_event_s girara_mouse_event_t;
typedef struct girara_config_handle_s girara_config_handle_t;
typedef struct girara_mode_string_s girara_mode_string_t;
typedef struct girara_tab_s girara_tab_t;
typedef struct girara_statusbar_item_s girara_statusbar_item_t;
typedef struct girara_argument_s girara_argument_t;
typedef struct girara_shortcut_mapping_s girara_shortcut_mapping_t;
typedef struct girara_argument_mapping_s girara_argument_mapping_t;
typedef struct girara_completion_element_s girara_completion_element_t;
typedef struct girara_completion_s girara_completion_t;
typedef struct girara_completion_group_s girara_completion_group_t;
typedef struct girara_shortcut_s girara_shortcut_t;
typedef struct girara_inputbar_shortcut_s girara_inputbar_shortcut_t;
typedef struct girara_special_command_s girara_special_command_t;
typedef struct girara_event_s girara_event_t;

/**
 * This structure defines the possible argument identifiers
 */
enum {
  GIRARA_HIDE = 1,             /**< Hide the completion list */
  GIRARA_NEXT,                 /**< Next entry */
  GIRARA_PREVIOUS,             /**< Previous entry */
  GIRARA_NEXT_GROUP,           /**< Next group in the completion list */
  GIRARA_PREVIOUS_GROUP,       /**< Previous group in the completion list */
  GIRARA_HIGHLIGHT,            /**< Highlight the entry */
  GIRARA_NORMAL,               /**< Set to the normal state */
  GIRARA_DELETE_LAST_WORD,     /**< Delete the last word */
  GIRARA_DELETE_LAST_CHAR,     /**< Delete the last character */
  GIRARA_NEXT_CHAR,            /**< Go to the next character */
  GIRARA_PREVIOUS_CHAR,        /**< Go to the previous character */
  GIRARA_DELETE_TO_LINE_START, /**< Delete the line to the start */
  GIRARA_DELETE_TO_LINE_END,   /**< Delete the line to the end */
  GIRARA_DELETE_CURR_CHAR,     /**< Delete current char */
  GIRARA_GOTO_START,           /**< Go to start of the line */
  GIRARA_GOTO_END              /**< Go to end of the line */
};

/**
 * Mode identifier
 */
typedef int girara_mode_t;

/**
 * Function declaration of a function that generates a completion group
 *
 * @param session The current girara session
 * @param input The current input
 * @return The completion group
 */
typedef girara_completion_t* (*girara_completion_function_t)(girara_session_t* session, const char* input);

/**
 * Function declaration of a inputbar special function
 *
 * @param session The current girara session
 * @param input The current input
 * @param argument The given argument
 * @return TRUE No error occurred
 * @return FALSE Error occurred
 */
typedef bool (*girara_inputbar_special_function_t)(girara_session_t* session, const char* input,
                                                   girara_argument_t* argument);

/**
 * Function declaration of a command function
 *
 * @param session The current girara session
 * @param argument_list Arguments
 */
typedef bool (*girara_command_function_t)(girara_session_t* session, girara_list_t* argument_list);

/**
 * Function declaration of a shortcut function
 *
 * If a numeric value has been written into the buffer, this function gets as
 * often executed as the value defines or until the function returns false the
 * first time.
 */
typedef bool (*girara_shortcut_function_t)(girara_session_t*, girara_argument_t*, girara_event_t*, unsigned int);

/**
 * This structure defines the possible types that a setting value can have
 */
typedef enum girara_setting_type_e {
  BOOLEAN = G_TYPE_BOOLEAN, /**< Boolean type */
  FLOAT   = G_TYPE_FLOAT,   /**< Floating number */
  INT     = G_TYPE_INT,     /**< Integer */
  STRING  = G_TYPE_STRING,  /**< String */
  UNKNOWN = 0xFFFF          /**< Unknown type */
} girara_setting_type_t;

/**
 * Function declaration for a settings callback
 *
 * @param session The current girara session
 * @param name The name of the affected setting
 * @param type The type of the affected setting
 * @param value Pointer to the new value
 * @param data User data
 */
typedef void (*girara_setting_callback_t)(girara_session_t* session, const char* name, girara_setting_type_t type,
                                          const void* value, void* data);

/**
 * Definition of an argument of a shortcut or buffered command
 */
struct girara_argument_s {
  void* data; /**< Data */
  int n;      /**< Identifier */
};

/**
 * Define mouse buttons
 */
typedef enum girara_mouse_button_e {
  GIRARA_MOUSE_BUTTON1 = 1, /**< Button 1 */
  GIRARA_MOUSE_BUTTON2 = 2, /**< Button 2 */
  GIRARA_MOUSE_BUTTON3 = 3, /**< Button 3 */
  GIRARA_MOUSE_BUTTON4 = 4, /**< Button 4 */
  GIRARA_MOUSE_BUTTON5 = 5, /**< Button 5 */
  GIRARA_MOUSE_BUTTON6 = 6, /**< Button 6 */
  GIRARA_MOUSE_BUTTON7 = 7, /**< Button 7 */
  GIRARA_MOUSE_BUTTON8 = 8, /**< Button 8 */
  GIRARA_MOUSE_BUTTON9 = 9  /**< Button 9 */
} girara_mouse_button_t;

/**
 * Describes the types of a girara
 */
typedef enum girara_event_type_e {
  GIRARA_EVENT_BUTTON_PRESS,        /**< Single click */
  GIRARA_EVENT_2BUTTON_PRESS,       /**< Double click */
  GIRARA_EVENT_3BUTTON_PRESS,       /**< Triple click */
  GIRARA_EVENT_BUTTON_RELEASE,      /**< Button released */
  GIRARA_EVENT_MOTION_NOTIFY,       /**< Cursor moved */
  GIRARA_EVENT_SCROLL_UP,           /**< Scroll event */
  GIRARA_EVENT_SCROLL_DOWN,         /**< Scroll event */
  GIRARA_EVENT_SCROLL_LEFT,         /**< Scroll event */
  GIRARA_EVENT_SCROLL_RIGHT,        /**< Scroll event */
  GIRARA_EVENT_OTHER,               /**< Unknown event */
  GIRARA_EVENT_SCROLL_BIDIRECTIONAL /**< Scroll event that carries extra data
                                     *   in girara_argument_t with motion
                                     *   information as double[2].
                                     *   First component is horizontal shift,
                                     *   second - vertical.
                                     */
} girara_event_type_t;

/**
 * Describes a girara event
 */
struct girara_event_s {
  double x; /**< X coordinates where the event occurred */
  double y; /**< Y coordinates where the event occurred */

  girara_event_type_t type; /**< The event type */
};

#endif
