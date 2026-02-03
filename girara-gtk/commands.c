/* SPDX-License-Identifier: Zlib */

#include "commands.h"

#include "internal.h"
#include "session.h"
#include "settings.h"
#include "shortcuts.h"
#include "utils.h"

#include <girara/datastructures.h>
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <string.h>

static void girara_cmd_display_shortcut(girara_session_t* session, const char* key_str, guint modifier, guint key,
                                        girara_mode_t mode) {
  girara_session_private_t* session_private = session->private_data;
  g_autoptr(GStrvBuilder) builder           = g_strv_builder_new();

  /* get mode name string */
  for (size_t idx = 0; idx != girara_list_size(session->modes.identifiers); ++idx) {
    girara_mode_string_t* mode_string = girara_list_nth(session->modes.identifiers, idx);
    if (mode != mode_string->index) {
      continue;
    }

    char* mode_str = g_strdup_printf("[%s]", mode_string->name);
    g_strv_builder_take(builder, mode_str);
  }

  /* add key string */
  if (key_str != NULL) {
    g_strv_builder_add(builder, " ");
    g_strv_builder_add(builder, key_str);
  }

  g_strv_builder_add(builder, ": ");

  /* find shortcut */
  girara_shortcut_t* shortcut = NULL;
  for (size_t idx = 0; idx != girara_list_size(session->bindings.shortcuts); ++idx) {
    girara_shortcut_t* shortcuts_it = girara_list_nth(session->bindings.shortcuts, idx);
    bool eq_mask                    = shortcuts_it->mask == modifier;
    bool eq_key                     = shortcuts_it->key == key;
    bool eq_mode                    = shortcuts_it->mode == mode || mode == 0;
    bool zero_mask_key              = (modifier != 0 || key != 0);

    if ((eq_mask && eq_key && eq_mode && zero_mask_key) == false) {
      continue;
    }

    shortcut = shortcuts_it;
    break;
  }

  if (shortcut == NULL) {
    g_strv_builder_add(builder, "no current mapping");
  } else {
    bool found_mapping = false;
    for (size_t idx = 0; idx != girara_list_size(session_private->config.shortcut_mappings); ++idx) {
      girara_shortcut_mapping_t* mapping = girara_list_nth(session_private->config.shortcut_mappings, idx);
      if (mapping->function != shortcut->function) {
        continue;
      }

      g_strv_builder_add(builder, mapping->identifier);
      found_mapping = true;
      break;
    }

    if (!found_mapping) {
      girara_warning("shortcut for key %d with mask %d and mode %d found in bindings, but not present in config",
                     shortcut->key, shortcut->mask, shortcut->mode);
    }

    if (shortcut->argument.data != NULL) {
      g_strv_builder_add(builder, " ");
      g_strv_builder_add(builder, shortcut->argument.data);
    }
  }

  GStrv array              = g_strv_builder_end(builder);
  g_autofree char* msg     = g_strjoinv("", array);
  g_autofree char* esc_msg = g_markup_escape_text(msg, -1);
  girara_notify(session, GIRARA_INFO, "%s", esc_msg);
  g_strfreev(array);
}

static void girara_cmd_display_mouse_event(girara_session_t* session, const char* button_str, const char* event_str,
                                           guint mask, guint button, girara_mode_t mode,
                                           girara_event_type_t event_type) {
  girara_session_private_t* session_private = session->private_data;
  g_autoptr(GStrvBuilder) builder           = g_strv_builder_new();

  /* get mode name string */
  for (size_t idx = 0; idx != girara_list_size(session->modes.identifiers); ++idx) {
    girara_mode_string_t* mode_string = girara_list_nth(session->modes.identifiers, idx);
    if (mode != mode_string->index) {
      continue;
    }

    char* mode_str = g_strdup_printf("[%s]", mode_string->name);
    g_strv_builder_take(builder, mode_str);
  }

  if (button_str != NULL) {
    g_strv_builder_add(builder, " ");
    g_strv_builder_add(builder, button_str);
  }

  if (event_str != NULL) {
    g_strv_builder_add(builder, " ");
    g_strv_builder_add(builder, event_str);
  }

  g_strv_builder_add(builder, ": ");

  /* find mouse event */
  girara_mouse_event_t* mouse_event = NULL;
  for (size_t idx = 0; idx != girara_list_size(session->bindings.mouse_events); ++idx) {
    girara_mouse_event_t* me_it = girara_list_nth(session->bindings.mouse_events, idx);
    bool eq_mask                = me_it->mask == mask;
    bool eq_button              = me_it->button == button;
    bool eq_mode                = me_it->mode == mode;
    bool eq_event               = me_it->event_type == event_type;
    if ((eq_mask && eq_button && eq_mode && eq_event) == false) {
      continue;
    }

    mouse_event = me_it;
    break;
  }

  if (mouse_event == NULL) {
    g_strv_builder_add(builder, "no current mapping");
  } else {
    bool found_mapping = false;
    for (size_t idx = 0; idx != girara_list_size(session_private->config.shortcut_mappings); ++idx) {
      girara_shortcut_mapping_t* mapping = girara_list_nth(session_private->config.shortcut_mappings, idx);
      if (mapping->function != mouse_event->function) {
        continue;
      }

      g_strv_builder_add(builder, mapping->identifier);
      found_mapping = true;
      break;
    }

    if (!found_mapping) {
      girara_warning(
          "mouse event for button %d with mask %d, mode %d, event_type %d found in bindings, but not present in config",
          mouse_event->button, mouse_event->mask, mouse_event->mode, mouse_event->event_type);
    }

    if (mouse_event->argument.data != NULL) {
      g_strv_builder_add(builder, " ");
      g_strv_builder_add(builder, mouse_event->argument.data);
    }
  }

  GStrv array              = g_strv_builder_end(builder);
  g_autofree char* msg     = g_strjoinv("", array);
  g_autofree char* esc_msg = g_markup_escape_text(msg, -1);
  girara_notify(session, GIRARA_INFO, "%s", esc_msg);
  g_strfreev(array);
}

const gdk_keyboard_button_t gdk_keyboard_buttons[48] = {
    {"BackSpace", GDK_KEY_BackSpace},
    {"CapsLock", GDK_KEY_Caps_Lock},
    {"NumLock", GDK_KEY_Num_Lock},
    {"ScrollLock", GDK_KEY_Scroll_Lock},
    {"Down", GDK_KEY_Down},
    {"Esc", GDK_KEY_Escape},
    {"F10", GDK_KEY_F10},
    {"F11", GDK_KEY_F11},
    {"F12", GDK_KEY_F12},
    {"F1", GDK_KEY_F1},
    {"F2", GDK_KEY_F2},
    {"F3", GDK_KEY_F3},
    {"F4", GDK_KEY_F4},
    {"F5", GDK_KEY_F5},
    {"F6", GDK_KEY_F6},
    {"F7", GDK_KEY_F7},
    {"F8", GDK_KEY_F8},
    {"F9", GDK_KEY_F9},
    {"Left", GDK_KEY_Left},
    {"PageDown", GDK_KEY_Page_Down},
    {"PageUp", GDK_KEY_Page_Up},
    {"Home", GDK_KEY_Home},
    {"End", GDK_KEY_End},
    {"Return", GDK_KEY_Return},
    {"Right", GDK_KEY_Right},
    {"Space", GDK_KEY_space},
    {"Super", GDK_KEY_Super_L},
    {"Tab", GDK_KEY_Tab},
    {"ShiftTab", GDK_KEY_ISO_Left_Tab},
    {"Up", GDK_KEY_Up},
    {"Print", GDK_KEY_Print},
    {"KPLeft", GDK_KEY_KP_Left},
    {"KPRight", GDK_KEY_KP_Right},
    {"KPUp", GDK_KEY_KP_Up},
    {"KPDown", GDK_KEY_KP_Down},
    {"KPBegin", GDK_KEY_KP_Begin},
    {"KPPrior", GDK_KEY_KP_Prior},
    {"KPNext", GDK_KEY_KP_Next},
    {"KPPageUp", GDK_KEY_KP_Prior},
    {"KPPageDown", GDK_KEY_KP_Next},
    {"KPEnd", GDK_KEY_KP_End},
    {"KPHome", GDK_KEY_KP_Home},
    {"KPInsert", GDK_KEY_KP_Insert},
    {"KPDelete", GDK_KEY_KP_Delete},
    {"KPMultiply", GDK_KEY_KP_Multiply},
    {"KPAdd", GDK_KEY_KP_Add},
    {"KPSubtract", GDK_KEY_KP_Subtract},
    {"KPDivide", GDK_KEY_KP_Divide},
};

typedef struct gdk_mouse_button_s {
  const char* identifier;
  int button;
} gdk_mouse_button_t;

static const gdk_mouse_button_t gdk_mouse_buttons[] = {
    {"Button1", GIRARA_MOUSE_BUTTON1}, {"Button2", GIRARA_MOUSE_BUTTON2}, {"Button3", GIRARA_MOUSE_BUTTON3},
    {"Button4", GIRARA_MOUSE_BUTTON4}, {"Button5", GIRARA_MOUSE_BUTTON5}, {"Button6", GIRARA_MOUSE_BUTTON6},
    {"Button7", GIRARA_MOUSE_BUTTON7}, {"Button8", GIRARA_MOUSE_BUTTON8}, {"Button9", GIRARA_MOUSE_BUTTON9},
};

typedef struct event_type_s {
  const char* identifier;
  int event;
} event_type_t;

static const event_type_t event_types[] = {
    {"motion", GIRARA_EVENT_MOTION_NOTIFY},      {"scroll_up", GIRARA_EVENT_SCROLL_UP},
    {"scroll_down", GIRARA_EVENT_SCROLL_DOWN},   {"scroll_left", GIRARA_EVENT_SCROLL_LEFT},
    {"scroll_right", GIRARA_EVENT_SCROLL_RIGHT},
};

typedef struct mouse_event_s {
  const char* identifier;
  int event;
} mouse_event_t;

static const mouse_event_t mouse_events[] = {
    {"button-pressed", GIRARA_EVENT_BUTTON_PRESS},
    {"2-button-pressed", GIRARA_EVENT_2BUTTON_PRESS},
    {"3-button-pressed", GIRARA_EVENT_2BUTTON_PRESS},
    {"button-released", GIRARA_EVENT_BUTTON_RELEASE},
};

/* default commands implementation */
static bool girara_cmd_map_unmap(girara_session_t* session, girara_list_t* argument_list, bool unmap) {
  const size_t number_of_arguments = girara_list_size(argument_list);

  unsigned int limit = 1;
  if (number_of_arguments < limit) {
    girara_warning("Invalid number of arguments passed: %zu instead of at least %u", number_of_arguments, limit);
    girara_notify(session, GIRARA_ERROR, _("Invalid number of arguments passed: %zu instead of at least %u"),
                  number_of_arguments, limit);
    return false;
  }

  int shortcut_mask                            = 0;
  int shortcut_key                             = 0;
  int shortcut_mouse_button                    = 0;
  girara_mode_t shortcut_mode                  = session->modes.normal;
  int shortcut_argument_n                      = 0;
  g_autofree char* shortcut_buffer_command     = NULL;
  girara_event_type_t event_type               = GIRARA_EVENT_BUTTON_PRESS;
  girara_shortcut_function_t shortcut_function = NULL;
  bool mouse_event                             = false;

  size_t current_command = 0;
  char* tmp              = girara_list_nth(argument_list, current_command);
  size_t tmp_length      = g_utf8_strlen(tmp, -1);
  size_t tmp_size        = strlen(tmp);

  /* Check first argument for mode */
  bool is_mode = false;
  if (tmp_length >= 3 && tmp[0] == '[' && tmp[tmp_size - 1] == ']') {
    g_autofree char* tmp_inner = g_strndup(tmp + 1, tmp_size - 2);

    for (size_t idx = 0; idx != girara_list_size(session->modes.identifiers); ++idx) {
      girara_mode_string_t* mode = girara_list_nth(session->modes.identifiers, idx);
      if (!g_strcmp0(tmp_inner, mode->name)) {
        shortcut_mode = mode->index;
        is_mode       = true;
        break;
      }
    }

    if (is_mode == false) {
      girara_warning("Unregistered mode specified: %s", tmp_inner);
      girara_notify(session, GIRARA_ERROR, _("Unregistered mode specified: %s"), tmp_inner);
      return false;
    }
  }

  if (is_mode == true) {
    if (number_of_arguments == 1) {
      girara_warning("Invalid number of arguments passed");
      girara_notify(session, GIRARA_ERROR, _("Invalid number of arguments passed"));
      return false;
    }

    tmp        = girara_list_nth(argument_list, ++current_command);
    tmp_length = g_utf8_strlen(tmp, -1);
    tmp_size   = strlen(tmp);
  }

  const char* key_str = girara_list_nth(argument_list, current_command);

  /* Check for multi key shortcut */
  if (tmp_length >= 3 && tmp[0] == '<' && tmp[tmp_size - 1] == '>') {
    g_autofree char* tmp_inner = g_strndup(tmp + 1, tmp_size - 2);
    size_t tmp_inner_length    = g_utf8_strlen(tmp_inner, -1);

    /* Multi key shortcut */
    if (strchr(tmp_inner, '-') != NULL && g_utf8_get_char(g_utf8_offset_to_pointer(tmp_inner, 1)) == '-' &&
        tmp_inner_length > 2) {
      switch (g_utf8_get_char(tmp_inner)) {
      case 'S':
        shortcut_mask = GDK_SHIFT_MASK;
        break;
      case 'A':
      case 'M':
        shortcut_mask = GDK_MOD1_MASK;
        break;
      case 'C':
        shortcut_mask = GDK_CONTROL_MASK;
        break;
      default:
        girara_warning("Invalid modifier in %s", tmp_inner);
        girara_notify(session, GIRARA_ERROR, _("Invalid modifier in %s"), tmp_inner);
        return false;
      }

      /* Single key */
      if (tmp_inner_length == 3) {
        shortcut_key = gdk_unicode_to_keyval(g_utf8_get_char(g_utf8_offset_to_pointer(tmp_inner, 2)));
        /* Possible special key */
      } else {
        bool found = false;
        for (unsigned int i = 0; i < LENGTH(gdk_keyboard_buttons); i++) {
          if (g_strcmp0(tmp_inner + 2, gdk_keyboard_buttons[i].identifier) == 0) {
            shortcut_key = gdk_keyboard_buttons[i].keyval;
            found        = true;
            break;
          }
        }

        for (unsigned int i = 0; i < LENGTH(gdk_mouse_buttons); i++) {
          if (!g_strcmp0(tmp_inner + 2, gdk_mouse_buttons[i].identifier)) {
            shortcut_mouse_button = gdk_mouse_buttons[i].button;
            mouse_event           = true;
            found                 = true;
            break;
          }
        }

        for (unsigned int i = 0; i < LENGTH(event_types); i++) {
          if (!g_strcmp0(tmp_inner + 2, event_types[i].identifier)) {
            event_type  = event_types[i].event;
            mouse_event = true;
            found       = true;
            break;
          }
        }

        if (found == false) {
          girara_warning("Invalid special key value or mode: %s", tmp_inner);
          girara_notify(session, GIRARA_ERROR, _("Invalid special key value for %s"), tmp_inner);
          return false;
        }
      }
      /* Possible special key */
    } else {
      bool found = false;
      for (unsigned int i = 0; i < LENGTH(gdk_keyboard_buttons); i++) {
        if (g_strcmp0(tmp_inner, gdk_keyboard_buttons[i].identifier) == 0) {
          shortcut_key = gdk_keyboard_buttons[i].keyval;
          found        = true;
          break;
        }
      }

      for (unsigned int i = 0; i < LENGTH(gdk_mouse_buttons); i++) {
        if (!g_strcmp0(tmp_inner, gdk_mouse_buttons[i].identifier)) {
          shortcut_mouse_button = gdk_mouse_buttons[i].button;
          mouse_event           = true;
          found                 = true;
          break;
        }
      }

      for (unsigned int i = 0; i < LENGTH(event_types); i++) {
        if (!g_strcmp0(tmp_inner, event_types[i].identifier)) {
          event_type  = event_types[i].event;
          mouse_event = true;
          found       = true;
          break;
        }
      }

      if (found == false) {
        girara_warning("Invalid special key value or mode: %s", tmp_inner);
        girara_notify(session, GIRARA_ERROR, _("Invalid special key value or mode %s"), tmp_inner);
        return false;
      }
    }
    /* Single key shortcut */
  } else if (tmp_length == 1) {
    shortcut_key = gdk_unicode_to_keyval(g_utf8_get_char(tmp));
    /* Buffer command */
  } else {
    shortcut_buffer_command = g_strdup(tmp);
  }

  if (unmap == true) {
    if (mouse_event == false) {
      girara_shortcut_remove(session, shortcut_mask, shortcut_key, shortcut_buffer_command, shortcut_mode);
    } else {
      girara_mouse_event_remove(session, shortcut_mask, shortcut_mouse_button, shortcut_mode);
    }
    return true;
  }

  /* check for mouse mode */
  bool mouse_mode       = false;
  const char* event_str = NULL;
  if (current_command + 1 < number_of_arguments) {
    tmp        = girara_list_nth(argument_list, current_command + 1);
    tmp_length = g_utf8_strlen(tmp, -1);
    tmp_size   = strlen(tmp);

    if (tmp_length >= 3 && tmp[0] == '[' && tmp[tmp_size - 1] == ']') {
      mouse_mode = true;
      event_str  = girara_list_nth(argument_list, ++current_command);

      if (mouse_event == false) {
        girara_warning("Mode passed on non-mouse event: %s", tmp);
        return false;
      }

      g_autofree char* tmp_inner = g_strndup(tmp + 1, tmp_size - 2);

      bool found = false;
      for (unsigned int i = 0; i < LENGTH(mouse_events); i++) {
        if (!g_strcmp0(tmp_inner, mouse_events[i].identifier)) {
          event_type = mouse_events[i].event;
          found      = true;
          break;
        }
      }

      if (found == false) {
        girara_warning("Invalid mouse event mode has been passed: %s", tmp_inner);
        return false;
      }
    }
  }

  /* display mapping if no shortcut is supplied */
  if (current_command + 1 == number_of_arguments) {
    if (mouse_mode) {
      girara_cmd_display_mouse_event(session, key_str, event_str, shortcut_mask, shortcut_mouse_button, shortcut_mode,
                                     event_type);
    } else {
      girara_cmd_display_shortcut(session, key_str, shortcut_mask, shortcut_key, shortcut_mode);
    }
    return true;
  }

  tmp = girara_list_nth(argument_list, ++current_command);

  girara_session_private_t* session_private = session->private_data;

  /* Check for passed shortcut command */
  bool found_mapping = false;
  for (size_t idx = 0; idx != girara_list_size(session_private->config.shortcut_mappings); ++idx) {
    girara_shortcut_mapping_t* mapping = girara_list_nth(session_private->config.shortcut_mappings, idx);
    if (!g_strcmp0(tmp, mapping->identifier)) {
      shortcut_function = mapping->function;
      found_mapping     = true;
      break;
    }
  }

  if (found_mapping == false) {
    girara_warning("Not a valid shortcut function: %s", tmp);
    girara_notify(session, GIRARA_ERROR, _("Not a valid shortcut function: %s"), tmp);
    return false;
  }

  /* Check for passed argument */
  char* shortcut_argument_data = NULL;
  if (++current_command < number_of_arguments) {
    tmp = girara_list_nth(argument_list, current_command);

    for (size_t idx = 0; idx != girara_list_size(session_private->config.argument_mappings); ++idx) {
      girara_argument_mapping_t* mapping = girara_list_nth(session_private->config.argument_mappings, idx);
      if (!g_strcmp0(tmp, mapping->identifier)) {
        shortcut_argument_n = mapping->value;
        break;
      }
    }

    /* If no known argument is passed we save it in the data field */
    if (shortcut_argument_n == 0) {
      shortcut_argument_data = tmp;
      /* If a known argument is passed and there are still more arguments,
       * we save the next one in the data field */
    } else if (++current_command < number_of_arguments) {
      shortcut_argument_data = girara_list_nth(argument_list, current_command);
    }
  }

  if (mouse_event == false) {
    girara_shortcut_add(session, shortcut_mask, shortcut_key, shortcut_buffer_command, shortcut_function, shortcut_mode,
                        shortcut_argument_n, shortcut_argument_data);
  } else {
    girara_mouse_event_add(session, shortcut_mask, shortcut_mouse_button, shortcut_function, shortcut_mode, event_type,
                           shortcut_argument_n, shortcut_argument_data);
  }

  return true;
}

bool girara_cmd_map(girara_session_t* session, girara_list_t* argument_list) {
  return girara_cmd_map_unmap(session, argument_list, false);
}

bool girara_cmd_unmap(girara_session_t* session, girara_list_t* argument_list) {
  return girara_cmd_map_unmap(session, argument_list, true);
}

bool girara_cmd_set(girara_session_t* session, girara_list_t* argument_list) {
  const size_t number_of_arguments = girara_list_size(argument_list);

  if (number_of_arguments == 0) {
    girara_warning("Not enough arguments for :set.");
    girara_notify(session, GIRARA_ERROR, _("Not enough arguments."));
    return false;
  }
  if (number_of_arguments > 2) {
    girara_warning("Too many arguments for :set.");
    girara_notify(session, GIRARA_ERROR, _("Too many arguments."));
    return false;
  }

  /* search for existing setting */
  char* name = girara_list_nth(argument_list, 0);
  if (name == NULL) {
    return false;
  }

  girara_setting_t* setting = girara_setting_find(session, name);
  if (setting == NULL) {
    girara_warning("Unknown option: %s", name);
    girara_notify(session, GIRARA_ERROR, _("Unknown option: %s"), name);
    return false;
  }

  if (number_of_arguments == 1) {
    /* display setting*/
    switch (girara_setting_get_type(setting)) {
    case BOOLEAN: {
      /* for compatibility reasons: toogle the setting */
      bool value = false;
      girara_setting_get_value(setting, &value);
      bool tmp = !value;
      girara_setting_set_value(session, setting, &tmp);
      girara_notify(session, GIRARA_INFO, "%s: %s", name, tmp ? _("true") : _("false"));
      break;
    }
    case FLOAT: {
      float value = 0;
      girara_setting_get_value(setting, &value);
      girara_notify(session, GIRARA_INFO, "%s: %f", name, value);
      break;
    }
    case INT: {
      int value = 0;
      girara_setting_get_value(setting, &value);
      girara_notify(session, GIRARA_INFO, "%s: %i", name, value);
      break;
    }
    case STRING: {
      g_autofree char* str = NULL;
      girara_setting_get_value(setting, &str);
      girara_notify(session, GIRARA_INFO, "%s: %s", name, str ? str : "(NULL)");
      break;
    }
    default:
      return false;
    }
  } else {
    const char* value = girara_list_nth(argument_list, 1);
    if (value == NULL) {
      girara_warning("No value defined for option: %s", name);
      girara_notify(session, GIRARA_ERROR, _("No value defined for option: %s"), name);
      return false;
    }

    /* update value */
    switch (girara_setting_get_type(setting)) {
    case BOOLEAN:
      if (g_strcmp0(value, "false") == 0 || g_strcmp0(value, "0") == 0) {
        bool b = false;
        girara_setting_set_value(session, setting, &b);
      } else if (g_strcmp0(value, "true") == 0 || g_strcmp0(value, "1") == 0) {
        bool b = true;
        girara_setting_set_value(session, setting, &b);
      } else {
        girara_warning("Unknown value for option: %s", name);
        girara_notify(session, GIRARA_ERROR, _("Unknown value for option: %s"), name);
      }
      break;
    case FLOAT: {
      float f = g_ascii_strtod(value, NULL);
      girara_setting_set_value(session, setting, &f);
      break;
    }
    case INT: {
      int i = atoi(value);
      girara_setting_set_value(session, setting, &i);
      break;
    }
    case STRING:
      girara_setting_set_value(session, setting, value);
      break;
    default:
      return false;
    }
  }

  return true;
}

bool girara_inputbar_command_add(girara_session_t* session, const char* command, const char* abbreviation,
                                 girara_command_function_t function, girara_completion_function_t completion,
                                 const char* description) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(command != NULL, false);
  g_return_val_if_fail(function != NULL, false);

  /* search for existing binding */
  for (size_t idx = 0; idx != girara_list_size(session->bindings.commands); ++idx) {
    girara_command_t* commands_it = girara_list_nth(session->bindings.commands, idx);
    if (g_strcmp0(commands_it->command, command) == 0) {
      g_free(commands_it->abbr);
      g_free(commands_it->description);

      commands_it->abbr        = g_strdup(abbreviation);
      commands_it->function    = function;
      commands_it->completion  = completion;
      commands_it->description = g_strdup(description);

      return true;
    }
  };

  /* add new inputbar command */
  girara_command_t* new_command = g_malloc0(sizeof(girara_command_t));

  new_command->command     = g_strdup(command);
  new_command->abbr        = g_strdup(abbreviation);
  new_command->function    = function;
  new_command->completion  = completion;
  new_command->description = g_strdup(description);
  girara_list_append(session->bindings.commands, new_command);

  return true;
}

bool girara_special_command_add(girara_session_t* session, char identifier, girara_inputbar_special_function_t function,
                                bool always, int argument_n, void* argument_data) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(function != NULL, false);

  girara_argument_t argument = {.n = argument_n, .data = argument_data};

  /* search for existing special command */
  for (size_t idx = 0; idx != girara_list_size(session->bindings.special_commands); ++idx) {
    girara_special_command_t* scommand_it = girara_list_nth(session->bindings.special_commands, idx);
    if (scommand_it->identifier == identifier) {
      scommand_it->function = function;
      scommand_it->always   = always;
      scommand_it->argument = argument;

      return true;
    }
  };

  /* create new special command */
  girara_special_command_t* special_command = g_malloc0(sizeof(girara_special_command_t));

  special_command->identifier = identifier;
  special_command->function   = function;
  special_command->always     = always;
  special_command->argument   = argument;
  girara_list_append(session->bindings.special_commands, special_command);

  return true;
}

void girara_command_free(girara_command_t* command) {
  if (command != NULL) {
    g_free(command->description);
    g_free(command->abbr);
    g_free(command->command);
    g_free(command);
  }
}

bool girara_command_run(girara_session_t* session, const char* input) {
  /* parse input */
  gchar** argv = NULL;
  gint argc    = 0;

  if (g_shell_parse_argv(input, &argc, &argv, NULL) == FALSE) {
    girara_debug("Failed to parse argument.");
    return false;
  }

  gchar* cmd = argv[0];

  /* search commands */
  for (size_t idx = 0; idx != girara_list_size(session->bindings.commands); ++idx) {
    girara_command_t* binding_command = girara_list_nth(session->bindings.commands, idx);
    if ((g_strcmp0(cmd, binding_command->command) == 0) || (g_strcmp0(cmd, binding_command->abbr) == 0)) {
      g_autoptr(girara_list_t) argument_list = girara_list_new();
      if (argument_list == NULL) {
        g_strfreev(argv);
        return false;
      }

      for (int i = 1; i < argc; i++) {
        girara_list_append(argument_list, argv[i]);
      }

      binding_command->function(session, argument_list);

      g_strfreev(argv);

      girara_isc_abort(session, NULL, NULL, 0);

      if (session->global.autohide_inputbar == true) {
        gtk_widget_hide(GTK_WIDGET(session->gtk.inputbar));
      }
      gtk_widget_hide(GTK_WIDGET(session->gtk.inputbar_dialog));
      return true;
    }
  }

  /* check for unknown command event handler */
  if (session->events.unknown_command != NULL) {
    if (session->events.unknown_command(session, input) == true) {
      g_strfreev(argv);
      girara_isc_abort(session, NULL, NULL, 0);

      if (session->global.autohide_inputbar == true) {
        gtk_widget_hide(GTK_WIDGET(session->gtk.inputbar));
      }
      gtk_widget_hide(GTK_WIDGET(session->gtk.inputbar_dialog));

      return true;
    }
  }

  /* unhandled command */
  girara_notify(session, GIRARA_ERROR, _("Not a valid command: %s"), cmd);
  g_strfreev(argv);
  girara_isc_abort(session, NULL, NULL, 0);

  return false;
}
