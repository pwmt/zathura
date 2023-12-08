/* SPDX-License-Identifier: Zlib */

#include <stdlib.h>
#include <string.h>
#include <girara/session.h>
#include <girara/callbacks.h>
#include <girara/datastructures.h>

#include "callbacks.h"
#include "marks.h"
#include "document.h"
#include "render.h"
#include "utils.h"

static void mark_add(zathura_t* zathura, int key);
static void mark_evaluate(zathura_t* zathura, int key);
static bool cb_marks_view_key_press_event_add(GtkWidget* widget, GdkEventKey*
    event, girara_session_t* session);
static bool cb_marks_view_key_press_event_evaluate(GtkWidget* widget,
    GdkEventKey* event, girara_session_t* session);

struct zathura_mark_s {
  int key; /**> Marks key */
  double position_x; /**> Horizontal adjustment */
  double position_y; /**> Vertical adjustment */
  unsigned int page; /**> Page number */
  double zoom; /**> Zoom level */
};

bool
sc_mark_add(girara_session_t* session, girara_argument_t* UNUSED(argument),
            girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL,           FALSE);
  g_return_val_if_fail(session->gtk.view != NULL, FALSE);

  /* redirect signal handler */
  g_signal_handler_disconnect(G_OBJECT(session->gtk.view), session->signals.view_key_pressed);
  session->signals.view_key_pressed = g_signal_connect(G_OBJECT(session->gtk.view), "key-press-event",
                                      G_CALLBACK(cb_marks_view_key_press_event_add), session);

  return true;
}

bool
sc_mark_evaluate(girara_session_t* session, girara_argument_t* UNUSED(argument),
                 girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL,           FALSE);
  g_return_val_if_fail(session->gtk.view != NULL, FALSE);

  /* redirect signal handler */
  g_signal_handler_disconnect(G_OBJECT(session->gtk.view), session->signals.view_key_pressed);
  session->signals.view_key_pressed = g_signal_connect(G_OBJECT(session->gtk.view), "key-press-event",
                                      G_CALLBACK(cb_marks_view_key_press_event_evaluate), session);

  return true;
}

static bool
cb_marks_view_key_press_event_add(GtkWidget* UNUSED(widget), GdkEventKey* event,
                                  girara_session_t* session)
{
  g_return_val_if_fail(session != NULL,              FALSE);
  g_return_val_if_fail(session->gtk.view != NULL,    FALSE);
  g_return_val_if_fail(session->global.data != NULL, FALSE);
  zathura_t* zathura = (zathura_t*) session->global.data;

  /* reset signal handler */
  g_signal_handler_disconnect(G_OBJECT(session->gtk.view), session->signals.view_key_pressed);
  session->signals.view_key_pressed = g_signal_connect(G_OBJECT(session->gtk.view), "key-press-event",
                                      G_CALLBACK(girara_callback_view_key_press_event), session);

  /* evaluate key */
  if (((event->keyval >= '0' && event->keyval <= '9') ||
       (event->keyval >= 'a' && event->keyval <= 'z') ||
       (event->keyval >= 'A' && event->keyval <= 'Z')
       ) == false) {
    return false;
  }

  mark_add(zathura, event->keyval);

  return true;
}

static bool
cb_marks_view_key_press_event_evaluate(GtkWidget* UNUSED(widget), GdkEventKey*
                                       event, girara_session_t* session)
{
  g_return_val_if_fail(session != NULL,              FALSE);
  g_return_val_if_fail(session->gtk.view != NULL,    FALSE);
  g_return_val_if_fail(session->global.data != NULL, FALSE);
  zathura_t* zathura = (zathura_t*) session->global.data;

  /* reset signal handler */
  g_signal_handler_disconnect(G_OBJECT(session->gtk.view), session->signals.view_key_pressed);
  session->signals.view_key_pressed = g_signal_connect(G_OBJECT(session->gtk.view), "key-press-event",
                                      G_CALLBACK(girara_callback_view_key_press_event), session);

  /* evaluate key */
  if (((event->keyval >= '0' && event->keyval <= '9') ||
       (event->keyval >= 'a' && event->keyval <= 'z') ||
       (event->keyval >= 'A' && event->keyval <= 'Z')
       ) == false) {
    return true;
  }

  mark_evaluate(zathura, event->keyval);

  return true;
}

bool
cmd_marks_add(girara_session_t* session, girara_list_t* argument_list)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = (zathura_t*) session->global.data;

  if (girara_list_size(argument_list) < 1) {
    return false;
  }

  const char* key_string = girara_list_nth(argument_list, 0);

  if (key_string == NULL || strlen(key_string) != 1) {
    return false;
  }

  const char key = key_string[0];
  if (((key >= 0x41 && key <= 0x5A) ||
       (key >= 0x61 && key <= 0x7A)) == false) {
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
  if (zathura == NULL || zathura->document == NULL || zathura->global.marks == NULL) {
    return;
  }

  unsigned int page_id = zathura_document_get_current_page_number(zathura->document);
  double position_x    = zathura_document_get_position_x(zathura->document);
  double position_y    = zathura_document_get_position_y(zathura->document);

  double zoom = zathura_document_get_zoom(zathura->document);

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
      zathura_document_set_zoom(zathura->document, zathura_correct_zoom_value(zathura->ui.session, mark->zoom));
      render_all(zathura);

      zathura_jumplist_add(zathura);
      page_set(zathura, mark->page);
      position_set(zathura, mark->position_x, mark->position_y);
      zathura_jumplist_add(zathura);

      return;
    }
  }
}

void
mark_free(void* data)
{
  if (data == NULL) {
    return;
  }

  zathura_mark_t* mark = (zathura_mark_t*) data;

  g_free(mark);
}
