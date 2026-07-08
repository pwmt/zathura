/* SPDX-License-Identifier: Zlib */

#include "callbacks.h"

#include "commands.h"
#include "internal.h"
#include "session.h"
#include "shortcuts.h"

#include <girara/input-history.h>
#include <girara/datastructures.h>
#include <girara/log.h>
#include <girara/utils.h>
#include <glib/gi18n-lib.h>
#include <string.h>

static const guint ALL_ACCELS_MASK = GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_ALT_MASK;
static const guint MOUSE_MASK = GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_ALT_MASK | GDK_BUTTON1_MASK | GDK_BUTTON2_MASK |
                                GDK_BUTTON3_MASK | GDK_BUTTON4_MASK | GDK_BUTTON5_MASK;

static bool clean_mask(GtkEventControllerKey* controller, GdkModifierType state, guint* clean, guint* keyval) {
  GdkModifierType consumed = 0;
  GdkEvent* event          = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
  if (event != NULL && gdk_event_get_event_type(event) == GDK_KEY_PRESS) {
    consumed = gdk_key_event_get_consumed_modifiers(event);
  }

  if (clean != NULL) {
    *clean = state & ~consumed & ALL_ACCELS_MASK;
  }

  /* numpad numbers */
  switch (*keyval) {
  case GDK_KEY_KP_0:
    *keyval = GDK_KEY_0;
    break;
  case GDK_KEY_KP_1:
    *keyval = GDK_KEY_1;
    break;
  case GDK_KEY_KP_2:
    *keyval = GDK_KEY_2;
    break;
  case GDK_KEY_KP_3:
    *keyval = GDK_KEY_3;
    break;
  case GDK_KEY_KP_4:
    *keyval = GDK_KEY_4;
    break;
  case GDK_KEY_KP_5:
    *keyval = GDK_KEY_5;
    break;
  case GDK_KEY_KP_6:
    *keyval = GDK_KEY_6;
    break;
  case GDK_KEY_KP_7:
    *keyval = GDK_KEY_7;
    break;
  case GDK_KEY_KP_8:
    *keyval = GDK_KEY_8;
    break;
  case GDK_KEY_KP_9:
    *keyval = GDK_KEY_9;
    break;
  }

  return true;
}

/* callback implementation */
gboolean girara_callback_view_key_press_event(GtkEventControllerKey* controller, guint keyval_in, guint UNUSED(keycode),
                                              GdkModifierType state, girara_session_t* session) {
  g_return_val_if_fail(session != NULL, FALSE);

  guint clean  = 0;
  guint keyval = keyval_in;

  if (clean_mask(controller, state, &clean, &keyval) == false) {
    return false;
  }

  return girara_process_view_key(session, keyval, clean);
}

gboolean girara_process_view_key(girara_session_t* session, guint keyval, guint clean) {
  g_return_val_if_fail(session != NULL, FALSE);

  girara_session_private_t* session_private = session->private_data;

  /* prepare event */
  for (size_t idx = 0; idx != girara_list_size(session->bindings.shortcuts); ++idx) {
    girara_shortcut_t* shortcut = girara_list_nth(session->bindings.shortcuts, idx);
    if (session_private->buffer.command != NULL) {
      break;
    }

    if (keyval == shortcut->key &&
        (clean == shortcut->mask || (shortcut->key >= 0x21 && shortcut->key <= 0x7E && clean == GDK_SHIFT_MASK)) &&
        (session->modes.current_mode == shortcut->mode || shortcut->mode == 0) && shortcut->function != NULL) {
      const int t = (session_private->buffer.n > 0) ? session_private->buffer.n : 1;
      for (int i = 0; i < t; i++) {
        if (shortcut->function(session, &(shortcut->argument), NULL, session_private->buffer.n) == false) {
          break;
        }
      }

      if (session->global.buffer != NULL) {
        g_string_free(session->global.buffer, TRUE);
        session->global.buffer = NULL;
      }

      session_private->buffer.n = 0;

      if (session->events.buffer_changed != NULL) {
        session->events.buffer_changed(session);
      }

      return TRUE;
    }
  }

  gunichar codepoint = gdk_keyval_to_unicode(keyval);

  /* update buffer */
  /* 0xff00 was chosen because every "special" keyval seems to be above it */
  if (keyval >= 0x21 && keyval < 0xff00 && codepoint) {
    /* overall buffer */
    if (session->global.buffer == NULL) {
      session->global.buffer = g_string_new("");
    }

    session->global.buffer = g_string_append_unichar(session->global.buffer, codepoint);

    if (session_private->buffer.command == NULL && keyval >= 0x30 && keyval <= 0x39) {
      if (((session_private->buffer.n * 10) + (keyval - '0')) < INT_MAX) {
        session_private->buffer.n = (session_private->buffer.n * 10) + (keyval - '0');
      }
    } else {
      if (session_private->buffer.command == NULL) {
        session_private->buffer.command = g_string_new("");
      }

      session_private->buffer.command = g_string_append_unichar(session_private->buffer.command, codepoint);
    }

    if (session->events.buffer_changed != NULL) {
      session->events.buffer_changed(session);
    }
  } else if (keyval == GDK_KEY_Escape) {
    if (session_private->buffer.command != NULL) {
      g_string_free(session_private->buffer.command, TRUE);
      session_private->buffer.command = NULL;
    }
    if (session->global.buffer != NULL) {
      g_string_free(session->global.buffer, TRUE);
      session->global.buffer = NULL;
    }
    if (session->events.buffer_changed != NULL) {
      session->events.buffer_changed(session);
    }
  }

  /* check for buffer command */
  if (session_private->buffer.command != NULL) {
    bool matching_command = false;

    for (size_t idx = 0; idx != girara_list_size(session->bindings.shortcuts); ++idx) {
      girara_shortcut_t* shortcut = girara_list_nth(session->bindings.shortcuts, idx);
      if (shortcut->buffered_command != NULL) {
        /* buffer could match a command */
        if (!strncmp(session_private->buffer.command->str, shortcut->buffered_command,
                     session_private->buffer.command->len)) {
          /* command matches buffer exactly */
          if (!g_strcmp0(session_private->buffer.command->str, shortcut->buffered_command) &&
              (session->modes.current_mode == shortcut->mode || shortcut->mode == 0)) {
            g_string_free(session_private->buffer.command, TRUE);
            g_string_free(session->global.buffer, TRUE);
            session_private->buffer.command = NULL;
            session->global.buffer          = NULL;

            if (session->events.buffer_changed != NULL) {
              session->events.buffer_changed(session);
            }

            int t = (session_private->buffer.n > 0) ? session_private->buffer.n : 1;
            for (int i = 0; i < t; i++) {
              if (shortcut->function(session, &(shortcut->argument), NULL, session_private->buffer.n) == false) {
                break;
              }
            }

            session_private->buffer.n = 0;
            return TRUE;
          }

          matching_command = true;
        }
      }
    }

    /* free buffer if buffer will never match a command */
    if (matching_command == false) {
      g_string_free(session_private->buffer.command, TRUE);
      g_string_free(session->global.buffer, TRUE);
      session_private->buffer.command = NULL;
      session->global.buffer          = NULL;
      session_private->buffer.n       = 0;

      if (session->events.buffer_changed != NULL) {
        session->events.buffer_changed(session);
      }
    }
  }

  return FALSE;
}

gboolean girara_callback_view_button_press_event(GtkGestureClick* gesture, gint n_press, gdouble x, gdouble y,
                                                 girara_session_t* session) {
  g_return_val_if_fail(session != NULL, false);

  girara_event_t event = {.x = x, .y = y};

  if (n_press == 2) {
    event.type = GIRARA_EVENT_2BUTTON_PRESS;
  } else if (n_press == 3) {
    event.type = GIRARA_EVENT_3BUTTON_PRESS;
  } else if (n_press == 1) {
    event.type = GIRARA_EVENT_BUTTON_PRESS;
  } else {
    event.type = GIRARA_EVENT_OTHER;
  }

  const guint gbutton  = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
  GdkModifierType mods = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(gesture));
  const guint state    = mods & MOUSE_MASK;
  girara_session_private_t* session_private = session->private_data;

  /* search registered mouse events */
  for (size_t idx = 0; idx != girara_list_size(session->bindings.mouse_events); ++idx) {
    girara_mouse_event_t* mouse_event = girara_list_nth(session->bindings.mouse_events, idx);
    if (mouse_event->function != NULL && gbutton == mouse_event->button && state == mouse_event->mask &&
        mouse_event->event_type == event.type &&
        (session->modes.current_mode == mouse_event->mode || mouse_event->mode == 0)) {
      mouse_event->function(session, &mouse_event->argument, &event, session_private->buffer.n);
      return true;
    }
  }

  return false;
}

gboolean girara_callback_view_button_release_event(GtkGestureClick* gesture, gint UNUSED(n_press), gdouble x, gdouble y,
                                                   girara_session_t* session) {
  g_return_val_if_fail(session != NULL, false);

  girara_event_t event = {.type = GIRARA_EVENT_BUTTON_RELEASE, .x = x, .y = y};

  const guint gbutton  = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
  GdkModifierType mods = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(gesture));
  const guint state    = mods & MOUSE_MASK;
  girara_session_private_t* session_private = session->private_data;

  /* search registered mouse events */
  for (size_t idx = 0; idx != girara_list_size(session->bindings.mouse_events); ++idx) {
    girara_mouse_event_t* mouse_event = girara_list_nth(session->bindings.mouse_events, idx);
    if (mouse_event->function != NULL && gbutton == mouse_event->button && state == mouse_event->mask &&
        mouse_event->event_type == GIRARA_EVENT_BUTTON_RELEASE &&
        (session->modes.current_mode == mouse_event->mode || mouse_event->mode == 0)) {
      mouse_event->function(session, &(mouse_event->argument), &event, session_private->buffer.n);
      return true;
    }
  }

  return false;
}

static gboolean dispatch_mouse_motion(girara_session_t* session, double x, double y, GdkModifierType mods) {
  g_return_val_if_fail(session != NULL, false);

  girara_event_t event = {.type = GIRARA_EVENT_MOTION_NOTIFY, .x = x, .y = y};

  const guint state                         = mods & MOUSE_MASK;
  girara_session_private_t* session_private = session->private_data;

  /* search registered mouse events */
  for (size_t idx = 0; idx != girara_list_size(session->bindings.mouse_events); ++idx) {
    girara_mouse_event_t* mouse_event = girara_list_nth(session->bindings.mouse_events, idx);
    if (mouse_event->function != NULL && state == mouse_event->mask && mouse_event->event_type == event.type &&
        (session->modes.current_mode == mouse_event->mode || mouse_event->mode == 0)) {
      mouse_event->function(session, &(mouse_event->argument), &event, session_private->buffer.n);
      return true;
    }
  }

  return false;
}

gboolean girara_callback_view_button_motion_notify_event(GtkEventControllerMotion* controller, gdouble x, gdouble y,
                                                         girara_session_t* session) {
  GdkModifierType mods = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(controller));
  return dispatch_mouse_motion(session, x, y, mods);
}

bool girara_has_mouse_event(girara_session_t* session, girara_event_type_t type, guint button, GdkModifierType state) {
  g_return_val_if_fail(session != NULL, false);

  const guint masked = state & MOUSE_MASK;
  for (size_t idx = 0; idx != girara_list_size(session->bindings.mouse_events); ++idx) {
    girara_mouse_event_t* mouse_event = girara_list_nth(session->bindings.mouse_events, idx);
    if (mouse_event->function != NULL && mouse_event->button == button && masked == mouse_event->mask &&
        mouse_event->event_type == type &&
        (session->modes.current_mode == mouse_event->mode || mouse_event->mode == 0)) {
      return true;
    }
  }

  return false;
}

/* recover the pointer position of a scroll event in widget coordinates (gtk3 scroll->x/y parity) */
static void scroll_event_position(GtkEventControllerScroll* controller, double* x, double* y) {
  *x = 0.0;
  *y = 0.0;

  GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));
  GdkEvent* event   = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
  if (widget == NULL || event == NULL) {
    return;
  }

  double sx = 0.0, sy = 0.0;
  if (gdk_event_get_position(event, &sx, &sy) == FALSE) {
    return;
  }

  GtkNative* native = gtk_widget_get_native(widget);
  if (native == NULL) {
    return;
  }

  double nx = 0.0, ny = 0.0;
  gtk_native_get_surface_transform(native, &nx, &ny);
  const graphene_point_t point = GRAPHENE_POINT_INIT((float)(sx - nx), (float)(sy - ny));
  graphene_point_t out         = {0};
  if (gtk_widget_compute_point(GTK_WIDGET(native), widget, &point, &out) == TRUE) {
    *x = out.x;
    *y = out.y;
  }
}

gboolean girara_callback_view_scroll_event(GtkEventControllerScroll* controller, gdouble dx, gdouble dy,
                                           girara_session_t* session) {
  g_return_val_if_fail(session != NULL, false);

  girara_event_t event = {.x = 0, .y = 0};
  scroll_event_position(controller, &event.x, &event.y);

  /* only wheel units are discrete steps so touchpad deltas stay smooth */
  if (gtk_event_controller_scroll_get_unit(controller) != GDK_SCROLL_UNIT_WHEEL) {
    event.type = GIRARA_EVENT_SCROLL_BIDIRECTIONAL;
#ifdef __APPLE__
    /* Apple has much higher deltas */
    const double surface_to_wheel = 50.0;
#else
    /* gtk4 sends raw pixels */
    const double surface_to_wheel = 10.0;
#endif
    event.x = dx / surface_to_wheel;
    event.y = dy / surface_to_wheel;
  } else if (dx == 0.0 && dy < 0.0) {
    event.type = GIRARA_EVENT_SCROLL_UP;
  } else if (dx == 0.0 && dy > 0.0) {
    event.type = GIRARA_EVENT_SCROLL_DOWN;
  } else if (dy == 0.0 && dx < 0.0) {
    event.type = GIRARA_EVENT_SCROLL_LEFT;
  } else if (dy == 0.0 && dx > 0.0) {
    event.type = GIRARA_EVENT_SCROLL_RIGHT;
  } else {
    event.type = GIRARA_EVENT_SCROLL_BIDIRECTIONAL;
    event.x    = dx;
    event.y    = dy;
#ifdef __APPLE__
    /* Apple has much higher deltas */
    event.x /= 50;
    event.y /= 50;
#endif
  }

  GdkModifierType mods = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(controller));
  const guint state    = mods & MOUSE_MASK;
  girara_session_private_t* session_private = session->private_data;

  /* search registered mouse events */
  /* TODO: Filter correct event */
  for (size_t idx = 0; idx != girara_list_size(session->bindings.mouse_events); ++idx) {
    girara_mouse_event_t* mouse_event = girara_list_nth(session->bindings.mouse_events, idx);
    if (mouse_event->function != NULL && state == mouse_event->mask && mouse_event->event_type == event.type &&
        (session->modes.current_mode == mouse_event->mode || mouse_event->mode == 0)) {
      mouse_event->function(session, &(mouse_event->argument), &event, session_private->buffer.n);
      return true;
    }
  }

  return false;
}

gboolean girara_callback_inputbar_activate(GtkEntry* entry, girara_session_t* session) {
  g_return_val_if_fail(session != NULL, FALSE);

  /* a custom handler has been installed (e.g. by girara_dialog) */
  if (session->signals.inputbar_custom_activate != NULL) {
    gboolean return_value = session->signals.inputbar_custom_activate(entry, session->signals.inputbar_custom_data);

    /* disconnect custom handler */
    session->signals.inputbar_custom_activate        = NULL;
    session->signals.inputbar_custom_key_press_event = NULL;
    session->signals.inputbar_custom_data            = NULL;

    if (session->gtk.inputbar_dialog != NULL && session->gtk.inputbar_entry != NULL) {
      gtk_label_set_markup(session->gtk.inputbar_dialog, "");
      gtk_widget_set_visible(GTK_WIDGET(session->gtk.inputbar_dialog), FALSE);
      if (session->global.autohide_inputbar == true) {
        gtk_widget_set_visible(GTK_WIDGET(session->gtk.inputbar), FALSE);
      }
      gtk_entry_set_visibility(session->gtk.inputbar_entry, TRUE);
      girara_isc_abort(session, NULL, NULL, 0);
      return true;
    }

    return return_value;
  }

  g_autofree gchar* input = gtk_editable_get_chars(GTK_EDITABLE(entry), 1, -1);
  if (input == NULL) {
    girara_isc_abort(session, NULL, NULL, 0);
    return false;
  }

  if (strlen(input) == 0) {
    girara_isc_abort(session, NULL, NULL, 0);
    return false;
  }

  /* append to command history */
  const char* command = gtk_editable_get_text(GTK_EDITABLE(entry));
  girara_input_history_append(session->command_history, command);

  /* special commands */
  g_autofree char* identifier_s = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, 1);
  if (identifier_s == NULL) {
    return false;
  }

  const char identifier = identifier_s[0];
  girara_debug("Processing special command with identifier '%c'.", identifier);
  for (size_t idx = 0; idx != girara_list_size(session->bindings.special_commands); ++idx) {
    girara_special_command_t* special_command = girara_list_nth(session->bindings.special_commands, idx);
    if (special_command->identifier == identifier) {
      girara_debug("Found special command.");
      if (special_command->always != true) {
        special_command->function(session, input, &(special_command->argument));
      }

      girara_isc_abort(session, NULL, NULL, 0);
      return true;
    }
  }

  return girara_command_run(session, input);
}

gboolean girara_callback_inputbar_key_press_event(GtkEventControllerKey* controller, guint keyval_in,
                                                  guint UNUSED(keycode), GdkModifierType state,
                                                  girara_session_t* session) {
  g_return_val_if_fail(session != NULL, false);

  GtkWidget* entry = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));

  /* a custom handler has been installed (e.g. by girara_dialog) */
  gboolean custom_ret = false;
  if (session->signals.inputbar_custom_key_press_event != NULL) {
    girara_debug("Running custom key press event handler.");
    GdkEvent* event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
    custom_ret = session->signals.inputbar_custom_key_press_event(entry, event, session->signals.inputbar_custom_data);
    if (custom_ret == true) {
      girara_isc_abort(session, NULL, NULL, 0);

      if (session->global.autohide_inputbar == true) {
        gtk_widget_set_visible(GTK_WIDGET(session->gtk.inputbar), FALSE);
      }
      gtk_widget_set_visible(GTK_WIDGET(session->gtk.inputbar_dialog), FALSE);
    }
  }

  guint keyval = keyval_in;
  guint clean  = 0;
  if (clean_mask(controller, state, &clean, &keyval) == false) {
    girara_debug("clean_mask returned false.");
    return false;
  }
  girara_debug("Proccessing key %u with mask %x.", keyval, clean);

  if (custom_ret == false) {
    for (size_t idx = 0; idx != girara_list_size(session->bindings.inputbar_shortcuts); ++idx) {
      girara_inputbar_shortcut_t* inputbar_shortcut = girara_list_nth(session->bindings.inputbar_shortcuts, idx);
      if (inputbar_shortcut->key == keyval && inputbar_shortcut->mask == clean) {
        girara_debug("found shortcut for key %u and mask %x", keyval, clean);
        if (inputbar_shortcut->function != NULL) {
          inputbar_shortcut->function(session, &(inputbar_shortcut->argument), NULL, 0);
        }

        return true;
      }
    }
  }

  if ((session->gtk.results != NULL) && (gtk_widget_get_visible(GTK_WIDGET(session->gtk.results)) == TRUE) &&
      (keyval == GDK_KEY_space)) {
    gtk_widget_set_visible(GTK_WIDGET(session->gtk.results), FALSE);
  }

  return custom_ret;
}

gboolean girara_callback_inputbar_changed_event(GtkEditable* entry, girara_session_t* session) {
  g_return_val_if_fail(session != NULL, false);

  /* special commands */
  g_autofree char* identifier_s = gtk_editable_get_chars(entry, 0, 1);
  if (identifier_s == NULL) {
    return false;
  }

  char identifier = identifier_s[0];
  for (size_t idx = 0; idx != girara_list_size(session->bindings.special_commands); ++idx) {
    girara_special_command_t* special_command = girara_list_nth(session->bindings.special_commands, idx);
    if ((special_command->identifier == identifier) && (special_command->always == true)) {
      g_autofree gchar* input = gtk_editable_get_chars(GTK_EDITABLE(entry), 1, -1);
      special_command->function(session, input, &(special_command->argument));
      return true;
    }
  }

  return false;
}
