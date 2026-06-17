/* SPDX-License-Identifier: Zlib */

#include "marks.h"

#include <stdlib.h>
#include <string.h>
#include <girara-gtk/session.h>
#include <girara-gtk/callbacks.h>
#include <girara/datastructures.h>

#include "callbacks.h"
#include "database.h"
#include "document.h"
#include "document-widget.h"
#include "render.h"
#include "utils.h"

static void mark_add(zathura_t* zathura, int key);
static void mark_evaluate(zathura_t* zathura, int key);

static gboolean cb_marks_one_shot(GtkEventControllerKey* controller, guint keyval, guint UNUSED(keycode),
                                  GdkModifierType UNUSED(state), gpointer user_data) {
  girara_session_t* session = user_data;
  g_return_val_if_fail(session != NULL && session->global.data != NULL, FALSE);
  zathura_t* zathura = session->global.data;

  GtkEventController* ctrl = GTK_EVENT_CONTROLLER(controller);
  gboolean evaluate        = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(ctrl), "evaluate"));
  GtkWidget* win           = gtk_event_controller_get_widget(ctrl);

  /* remove the controller from its own callback so it only fires once */
  gtk_widget_remove_controller(win, ctrl);

  if (((keyval >= '0' && keyval <= '9') || (keyval >= 'a' && keyval <= 'z') || (keyval >= 'A' && keyval <= 'Z')) ==
      false) {
    return TRUE;
  }

  if (evaluate) {
    mark_evaluate(zathura, keyval);
  } else {
    mark_add(zathura, keyval);
  }
  return TRUE;
}

/* set up a one-shot controller, evaluate selects between add and evaluate */
static void marks_install_one_shot(girara_session_t* session, gboolean evaluate) {
  GtkEventController* ctrl = gtk_event_controller_key_new();
  gtk_event_controller_set_propagation_phase(ctrl, GTK_PHASE_CAPTURE);
  g_object_set_data(G_OBJECT(ctrl), "evaluate", GINT_TO_POINTER(evaluate));
  g_signal_connect(ctrl, "key-pressed", G_CALLBACK(cb_marks_one_shot), session);
  gtk_widget_add_controller(session->gtk.window, ctrl);
}

bool sc_mark_add(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event),
                 unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->gtk.view != NULL, false);

  marks_install_one_shot(session, FALSE);
  return true;
}

bool sc_mark_evaluate(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event),
                      unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->gtk.view != NULL, false);

  marks_install_one_shot(session, TRUE);
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

  return true;
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

      adjust_view(zathura);
      zathura_document_widget_render_all(zathura->ui.document_widget);

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
