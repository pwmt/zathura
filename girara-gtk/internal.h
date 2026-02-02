/* SPDX-License-Identifier: Zlib */

#ifndef GIRARA_INTERNAL_H
#define GIRARA_INTERNAL_H

#include <glib.h>
#include <gtk/gtk.h>

#include "types.h"

#define FORMAT_COMMAND "<b>%s</b>"
#define FORMAT_DESCRIPTION "<i>%s</i>"

#define UNUSED(x) GIRARA_UNUSED(x)

#define LENGTH(x) (sizeof(x) / sizeof((x)[0]))

/**
 * Free girara_setting_t struct
 *
 * @param setting The setting to free.
 */
void girara_setting_free(girara_setting_t* setting);

void girara_config_handle_free(girara_config_handle_t* handle);

void girara_shortcut_mapping_free(girara_shortcut_mapping_t* mapping);

void girara_shortcut_free(girara_shortcut_t* shortcut);

void girara_argument_mapping_free(girara_argument_mapping_t* argument_mapping);

void girara_command_free(girara_command_t* command);

void girara_config_load_default(girara_session_t* session);

void widget_add_class(GtkWidget* widget, const char* styleclass);

void widget_remove_class(GtkWidget* widget, const char* styleclass);

/**
 * Default complection function for the settings
 *
 * @param session The used girara session
 * @param input The current input
 */
girara_completion_t* girara_cc_set(girara_session_t* session, const char* input);

/**
 * Default command to map sortcuts
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return TRUE No error occurred
 * @return FALSE An error occurred
 */
bool girara_cmd_map(girara_session_t* session, girara_list_t* argument_list);

/**
 * Default command to unmap sortcuts
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return TRUE No error occurred
 * @return FALSE An error occurred
 */
bool girara_cmd_unmap(girara_session_t* session, girara_list_t* argument_list);

/**
 * Default command to quit the application
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return TRUE No error occurred
 * @return FALSE An error occurred
 */
bool girara_cmd_quit(girara_session_t* session, girara_list_t* argument_list);

/**
 * Default command to set the value of settings
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return TRUE No error occurred
 * @return FALSE An error occurred
 */
bool girara_cmd_set(girara_session_t* session, girara_list_t* argument_list);

/**
 * Execute an external command
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return TRUE No error occurred
 * @return FALSE An error occurred
 */
bool girara_cmd_exec(girara_session_t* session, girara_list_t* argument_list);

/**
 * Dump current settings to a JSON file
 *
 * @param session The used girara session
 * @param argument_list List of passed arguments
 * @return TRUE No error occurred
 * @return FALSE An error occurred
 */
bool girara_cmd_dump_config(girara_session_t* session, girara_list_t* argument_list);

/**
 * Process argument as a sequence of keys that were typed by the user
 *
 * @param session The session
 * @param argument The argument
 * @param event Event type
 * @param t Number of times
 * @return true No error occurred
 * @return false An error occurred
 */
bool girara_sc_feedkeys(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

void css_template_fill_font(GiraraTemplate* csstemplate, const char* font);

/**
 * Structure of a command
 */
struct girara_command_s {
  char* command;                           /**< Name of the command */
  char* abbr;                              /**< Abbreviation of the command */
  girara_command_function_t function;      /**< Function */
  girara_completion_function_t completion; /**< Completion function */
  char* description;                       /**< Description of the command */
};

struct girara_mode_string_s {
  char* name;          /**< Name of the mode object */
  girara_mode_t index; /**< Index */
};

/**
 * Shortcut mapping
 */
struct girara_shortcut_mapping_s {
  char* identifier;                    /**> Identifier string */
  girara_shortcut_function_t function; /** Shortcut function */
};

/**
 * Argument mapping
 */
struct girara_argument_mapping_s {
  char* identifier; /**> Identifier string */
  int value;        /**> Value */
};

/**
 * Structure of a shortcut
 */
struct girara_shortcut_s {
  guint mask;                          /**< Mask */
  guint key;                           /**< Key */
  char* buffered_command;              /**< Buffer command */
  girara_argument_t argument;          /**< Given argument */
  girara_shortcut_function_t function; /**< The correspondending function */
  girara_mode_t mode;                  /**< Mode identifier */
};

/**
 * Structure of a inputbar shortcut
 */
struct girara_inputbar_shortcut_s {
  guint mask;                          /**< Mask */
  guint key;                           /**< Key */
  girara_shortcut_function_t function; /**< Function */
  girara_argument_t argument;          /**< Given argument */
};

/**
 * Structure of a special command
 */
struct girara_special_command_s {
  girara_inputbar_special_function_t function; /**< Function */
  girara_argument_t argument;                  /**< Argument */
  char identifier;                             /**< Identifier */
  bool always;                                 /**< Evalute on every change of the input */
};

/**
 * Structure of a mouse event
 */
struct girara_mouse_event_s {
  guint mask;                          /**< Mask */
  guint button;                        /**< Button */
  girara_shortcut_function_t function; /**< Function */
  girara_mode_t mode;                  /**< Allowed modes */
  girara_event_type_t event_type;      /**< Event type */
  girara_argument_t argument;          /**< Given argument */
};

/**
 * Config handle
 */
struct girara_config_handle_s {
  char* identifier;
  girara_command_function_t handle;
};

/**
 * Structure of a statusbar item
 */
struct girara_statusbar_item_s {
  GtkWidget* box; /**< Event box */
  GtkLabel* text; /**< Text label */
};

/**
 * Private data of the girara session
 */
struct girara_session_private_s {
  GMutex feedkeys_mutex;

  /**
   * Used in session-specific paths
   */
  char* session_name;

  /**
   * List of all settings
   */
  girara_list_t* settings;

  /**
   * Template enginge for CSS.
   */
  GiraraTemplate* csstemplate;

  struct {
    GtkWidget* overlay; /**< So we can overlay bottom_box on top of view */
    GtkBox* bottom_box; /**< Box grouping input, status and notification */
    GtkCssProvider* cssprovider;
  } gtk;

  struct {
    girara_list_t* statusbar_items; /**< List of statusbar items */
  } elements;

  struct {
    GString* command; /**< Command in buffer */
    int n;            /**< Numeric buffer */
  } buffer;

  struct {
    girara_list_t* handles;
    girara_list_t* shortcut_mappings;
    girara_list_t* argument_mappings;
  } config;

  struct {
    GList* entries;
    GList* entries_current;
    char* previous_command;
    char* previous_parameter;
    size_t previous_length;
    bool command_mode;
  } completion;
};

typedef struct gdk_keyboard_button_s {
  const char* identifier;
  int keyval;
} gdk_keyboard_button_t;

extern const gdk_keyboard_button_t gdk_keyboard_buttons[48];

#endif
