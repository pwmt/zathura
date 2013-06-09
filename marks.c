/* See LICENSE file for license and copyright information */

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
  float scale; /**> Zoom level */
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

bool
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
  if (((event->keyval >= 0x41 && event->keyval <= 0x5A) || (event->keyval >=
       0x61 && event->keyval <= 0x7A)) == false) {
    return false;
  }

  mark_add(zathura, event->keyval);

  return true;
}

bool cb_marks_view_key_press_event_evaluate(GtkWidget* UNUSED(widget), GdkEventKey*
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
  if (((event->keyval >= 0x41 && event->keyval <= 0x5A) || (event->keyval >=
       0x61 && event->keyval <= 0x7A)) == false) {
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

  char* key_string = girara_list_nth(argument_list, 0);

  if (key_string == NULL) {
    return false;
  }

  if (strlen(key_string) < 1 || strlen(key_string) > 1) {
    return false;
  }

  char key = key_string[0];

  if (((key >= 0x41 && key <= 0x5A) || (key >=
                                        0x61 && key <= 0x7A)) == false) {
    return false;
  }

  mark_add(zathura, key);

  return false;
}

bool
cmd_marks_delete(girara_session_t* session, girara_list_t* argument_list)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = (zathura_t*) session->global.data;

  if (girara_list_size(argument_list) < 1) {
    return false;
  }

  if (girara_list_size(zathura->global.marks) == 0) {
    return false;
  }

  GIRARA_LIST_FOREACH(argument_list, char*, iter, key_string)
  if (key_string == NULL) {
    continue;
  }

  for (unsigned int i = 0; i < strlen(key_string); i++) {
    char key = key_string[i];
    if (((key >= 0x41 && key <= 0x5A) || (key >=
                                          0x61 && key <= 0x7A)) == false) {
      continue;
    }

    /* search for existing mark */
    girara_list_iterator_t* mark_iter = girara_list_iterator(zathura->global.marks);
    do {
      zathura_mark_t* mark = (zathura_mark_t*) girara_list_iterator_data(mark_iter);
      if (mark == NULL) {
        continue;
      }

      if (mark->key == key) {
        girara_list_remove(zathura->global.marks, mark);
        continue;
      }
    } while (girara_list_iterator_next(mark_iter) != NULL);
    girara_list_iterator_free(mark_iter);
  }
  GIRARA_LIST_FOREACH_END(argument_list, char*, iter, key_string);

  return true;
}

void
mark_add(zathura_t* zathura, int key)
{
  if (zathura == NULL || zathura->document == NULL || zathura->global.marks == NULL) {
    return;
  }

  GtkScrolledWindow *window = GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view);
  GtkAdjustment* v_adjustment = gtk_scrolled_window_get_vadjustment(window);
  GtkAdjustment* h_adjustment = gtk_scrolled_window_get_hadjustment(window);

  if (v_adjustment == NULL || h_adjustment == NULL) {
    return;
  }

  double position_x = gtk_adjustment_get_value(h_adjustment);
  double position_y = gtk_adjustment_get_value(v_adjustment);
  float scale       = zathura_document_get_scale(zathura->document);

  /* search for existing mark */
  GIRARA_LIST_FOREACH(zathura->global.marks, zathura_mark_t*, iter, mark)
  if (mark->key == key) {
    mark->position_x = position_x;
    mark->position_y = position_y;
    mark->scale      = scale;
    return;
  }
  GIRARA_LIST_FOREACH_END(zathura->global.marks, zathura_mark_t*, iter, mark);

  /* add new mark */
  zathura_mark_t* mark = g_malloc0(sizeof(zathura_mark_t));

  mark->key        = key;
  mark->position_x = position_x;
  mark->position_y = position_y;
  mark->scale      = scale;

  girara_list_append(zathura->global.marks, mark);
}

void
mark_evaluate(zathura_t* zathura, int key)
{
  if (zathura == NULL || zathura->global.marks == NULL) {
    return;
  }

  /* search for existing mark */
  GIRARA_LIST_FOREACH(zathura->global.marks, zathura_mark_t*, iter, mark)
  if (mark != NULL && mark->key == key) {
    zathura_document_set_scale(zathura->document, mark->scale);
    render_all(zathura);

    zathura_jumplist_add(zathura);
    position_set(zathura, mark->position_x, mark->position_y);
    zathura_jumplist_add(zathura);

    cb_view_vadjustment_value_changed(NULL, zathura);

    zathura->global.update_page_number = true;
    return;
  }
  GIRARA_LIST_FOREACH_END(zathura->global.marks, zathura_mark_t*, iter, mark);
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
