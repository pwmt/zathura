/* SPDX-License-Identifier: Zlib */

#include <stdlib.h>
#include <string.h>
#include <girara/session.h>
#include <girara/callbacks.h>
#include <girara/datastructures.h>

#include "callbacks.h"
#include "database.h"
#include "document.h"
#include "marks.h"
#include "render.h"
#include "utils.h"

static void mark_add(zathura_t* zathura, int key);
static void mark_evaluate(zathura_t* zathura, int key);

static gboolean cb_marks_view_key_press_event_add(GtkWidget* UNUSED(widget), GdkEventKey* event, gpointer user_data) {
  g_return_val_if_fail(user_data != NULL, FALSE);
  girara_session_t* session = user_data;
  g_return_val_if_fail(session->gtk.view != NULL, FALSE);
  g_return_val_if_fail(session->global.data != NULL, FALSE);
  zathura_t* zathura = session->global.data;

  /* reset signal handler */
  g_signal_handler_disconnect(G_OBJECT(session->gtk.view), session->signals.view_key_pressed);
  session->signals.view_key_pressed = g_signal_connect(G_OBJECT(session->gtk.view), "key-press-event",
                                                       G_CALLBACK(girara_callback_view_key_press_event), session);

  /* evaluate key */
  if (((event->keyval >= '0' && event->keyval <= '9') || (event->keyval >= 'a' && event->keyval <= 'z') ||
       (event->keyval >= 'A' && event->keyval <= 'Z')) == false) {
    return FALSE;
  }

  mark_add(zathura, event->keyval);

  return TRUE;
}

static gboolean cb_marks_view_key_press_event_evaluate(GtkWidget* UNUSED(widget), GdkEventKey* event,
                                                       gpointer user_data) {
  g_return_val_if_fail(user_data != NULL, FALSE);
  girara_session_t* session = user_data;
  g_return_val_if_fail(session->gtk.view != NULL, FALSE);
  g_return_val_if_fail(session->global.data != NULL, FALSE);
  zathura_t* zathura = session->global.data;

  /* reset signal handler */
  g_signal_handler_disconnect(G_OBJECT(session->gtk.view), session->signals.view_key_pressed);
  session->signals.view_key_pressed = g_signal_connect(G_OBJECT(session->gtk.view), "key-press-event",
                                                       G_CALLBACK(girara_callback_view_key_press_event), session);

  /* evaluate key */
  if (((event->keyval >= '0' && event->keyval <= '9') || (event->keyval >= 'a' && event->keyval <= 'z') ||
       (event->keyval >= 'A' && event->keyval <= 'Z')) == false) {
    return TRUE;
  }

  mark_evaluate(zathura, event->keyval);

  return TRUE;
}

bool sc_mark_add(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event),
                 unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->gtk.view != NULL, false);

  /* redirect signal handler */
  g_signal_handler_disconnect(G_OBJECT(session->gtk.view), session->signals.view_key_pressed);
  session->signals.view_key_pressed = g_signal_connect(G_OBJECT(session->gtk.view), "key-press-event",
                                                       G_CALLBACK(cb_marks_view_key_press_event_add), session);

  return true;
}

bool sc_mark_evaluate(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event),
                      unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->gtk.view != NULL, false);

  /* redirect signal handler */
  g_signal_handler_disconnect(G_OBJECT(session->gtk.view), session->signals.view_key_pressed);
  session->signals.view_key_pressed = g_signal_connect(G_OBJECT(session->gtk.view), "key-press-event",
                                                       G_CALLBACK(cb_marks_view_key_press_event_evaluate), session);

  return true;
}

bool cmd_marks_add(girara_session_t* session, girara_list_t* argument_list) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = (zathura_t*)session->global.data;

  if (girara_list_size(argument_list) < 1) {
    return false;
  }

  const char* key_string = girara_list_nth(argument_list, 0);

  if (key_string == NULL || strlen(key_string) != 1) {
    return false;
  }

  const char key = key_string[0];
  if (((key >= 0x41 && key <= 0x5A) || (key >= 0x61 && key <= 0x7A)) == false) {
    return false;
  }

  mark_add(zathura, key);

  return false;
}

bool cmd_marks_delete(girara_session_t* session, girara_list_t* argument_list) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = (zathura_t*)session->global.data;

  if (girara_list_size(argument_list) < 1) {
    return false;
  }

  if (girara_list_size(zathura->global.marks) == 0) {
    return false;
  }

  for (size_t idx = 0; idx != girara_list_size(argument_list); ++idx) {
    char* key_string = girara_list_nth(argument_list, idx);
    if (key_string == NULL) {
      continue;
    }

    for (unsigned int i = 0; i < strlen(key_string); i++) {
      char key = key_string[i];
      if (((key >= 0x41 && key <= 0x5A) || (key >= 0x61 && key <= 0x7A)) == false) {
        continue;
      }

      /* search for existing mark */
      for (size_t inner_idx = girara_list_size(zathura->global.marks); inner_idx; --inner_idx) {
        zathura_mark_t* mark = girara_list_nth(zathura->global.marks, inner_idx - 1);
        if (mark == NULL) {
          continue;
        }

        if (mark->key == key) {
          girara_list_remove(zathura->global.marks, mark);
        }
      }
    }
  }

  return true;
}

static void mark_add(zathura_t* zathura, int key) {
  if (zathura_has_document(zathura) == false || zathura->global.marks == NULL) {
    return;
  }

  zathura_document_t* document = zathura_get_document(zathura);
  unsigned int page_id         = zathura_document_get_current_page_number(document);
  double position_x            = zathura_document_get_position_x(document);
  double position_y            = zathura_document_get_position_y(document);

  double zoom = zathura_document_get_zoom(document);

  /* search for existing mark */
  for (size_t idx = 0; idx != girara_list_size(zathura->global.marks); ++idx) {
    zathura_mark_t* mark = girara_list_nth(zathura->global.marks, idx);
    if (mark->key == key) {
      mark->page       = page_id;
      mark->position_x = position_x;
      mark->position_y = position_y;
      mark->zoom       = zoom;
      return;
    }
  }

  /* add new mark */
  zathura_mark_t* mark = g_try_malloc0(sizeof(zathura_mark_t));
  if (mark == NULL) {
    return;
  }

  mark->key        = key;
  mark->page       = page_id;
  mark->position_x = position_x;
  mark->position_y = position_y;
  mark->zoom       = zoom;

  girara_list_append(zathura->global.marks, mark);
}

static void mark_evaluate(zathura_t* zathura, int key) {
  if (zathura == NULL || zathura->global.marks == NULL) {
    return;
  }

  /* search for existing mark */
  for (size_t idx = 0; idx != girara_list_size(zathura->global.marks); ++idx) {
    zathura_mark_t* mark = girara_list_nth(zathura->global.marks, idx);
    if (mark != NULL && mark->key == key) {
      zathura_document_set_zoom(zathura_get_document(zathura),
                                zathura_correct_zoom_value(zathura->ui.session, mark->zoom));
      render_all(zathura);

      zathura_jumplist_add(zathura);
      page_set(zathura, mark->page);
      position_set(zathura, mark->position_x, mark->position_y);
      zathura_jumplist_add(zathura);

      return;
    }
  }
}

bool zathura_quickmarks_load(zathura_t* zathura, const gchar* file) {
  g_return_val_if_fail(zathura, false);
  g_return_val_if_fail(file, false);

  if (zathura->database == NULL) {
    return false;
  }

  girara_list_t* marks = zathura_db_load_quickmarks(zathura->database, file);
  if (marks == NULL) {
    return false;
  }

  girara_list_free(zathura->global.marks);
  zathura->global.marks = marks;

  return true;
}