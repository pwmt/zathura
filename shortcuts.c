/* See LICENSE file for license and copyright information */

#include <girara.h>
#include <gtk/gtk.h>

#include "callbacks.h"
#include "shortcuts.h"
#include "zathura.h"
#include "render.h"
#include "utils.h"

bool
sc_abort(girara_session_t* session, girara_argument_t* argument, unsigned int t)
{
  g_return_val_if_fail(session != NULL, false);

  girara_mode_set(session, NORMAL);

  return false;
}

bool
sc_adjust_window(girara_session_t* session, girara_argument_t* argument, unsigned int t)
{
  return false;
}

bool
sc_change_buffer(girara_session_t* session, girara_argument_t* argument, unsigned int t)
{
  return false;
}

bool
sc_change_mode(girara_session_t* session, girara_argument_t* argument, unsigned int t)
{
  g_return_val_if_fail(session != NULL, false);

  girara_mode_set(session, argument->n);

  return false;
}

bool
sc_focus_inputbar(girara_session_t* session, girara_argument_t* argument, unsigned int t)
{
  g_return_val_if_fail(session != NULL, false);

  if (!(GTK_WIDGET_VISIBLE(GTK_WIDGET(session->gtk.inputbar)))) {
    gtk_widget_show(GTK_WIDGET(session->gtk.inputbar));
  }

  if (argument->data) {
    gtk_entry_set_text(session->gtk.inputbar, (char*) argument->data);
    gtk_widget_grab_focus(GTK_WIDGET(session->gtk.inputbar));
    gtk_editable_set_position(GTK_EDITABLE(session->gtk.inputbar), -1);
  }

  return false;
}

bool
sc_follow(girara_session_t* session, girara_argument_t* argument, unsigned int t)
{
  return false;
}

bool
sc_goto(girara_session_t* session, girara_argument_t* argument, unsigned int t)
{
  if (session == NULL || argument == NULL || Zathura.document == NULL) {
    return false;
  }

  if (argument->n == TOP) {
    girara_argument_t arg = { TOP, NULL };
    sc_scroll(session, &arg, 0);

    return false;
  } else {
    if (t == 0) {
      girara_argument_t arg = { BOTTOM, NULL };
      sc_scroll(session, &arg, 0);

      return true;
    }

    unsigned int number_of_pages = Zathura.document->number_of_pages;

    if (t > 0 && t <= number_of_pages)  {
      GtkAdjustment* adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(Zathura.UI.session->gtk.view));
      unsigned int offset = Zathura.document->pages[t - 1]->offset * Zathura.document->scale;
      gtk_adjustment_set_value(adjustment, offset);
    }
  }

  return false;
}

bool
sc_navigate(girara_session_t* session, girara_argument_t* argument, unsigned int t)
{
  if (session == NULL || argument == NULL || Zathura.document == NULL) {
    return false;
  }

  unsigned int number_of_pages = Zathura.document->number_of_pages;
  unsigned int new_page        = Zathura.document->current_page_number;

  if (argument->n == NEXT) {
    new_page = (new_page + 1) % number_of_pages;
  } else if (argument->n == PREVIOUS) {
    new_page = (new_page + number_of_pages - 1) % number_of_pages;
  }

  page_set(new_page);

  return false;
}

bool
sc_recolor(girara_session_t* session, girara_argument_t* argument, unsigned int t)
{
  return false;
}

bool
sc_reload(girara_session_t* session, girara_argument_t* argument, unsigned int t)
{
  return false;
}

bool
sc_rotate(girara_session_t* session, girara_argument_t* argument, unsigned int t)
{
  return false;
}

bool
sc_scroll(girara_session_t* session, girara_argument_t* argument, unsigned int t)
{
  GtkAdjustment* adjustment = NULL;
  if ( (argument->n == LEFT) || (argument->n == RIGHT) )
    adjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(Zathura.UI.session->gtk.view));
  else
    adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(Zathura.UI.session->gtk.view));

  gdouble view_size  = gtk_adjustment_get_page_size(adjustment);
  gdouble value      = gtk_adjustment_get_value(adjustment);
  gdouble max        = gtk_adjustment_get_upper(adjustment) - view_size;
  gdouble scroll_step = 40;

  gdouble new_value;
  switch(argument->n)
  {
    case FULL_UP:
      new_value = (value - view_size) < 0 ? 0 : (value - view_size);
      break;
    case FULL_DOWN:
      new_value = (value + view_size) > max ? max : (value + view_size);
      break;
    case HALF_UP:
      new_value = (value - (view_size / 2)) < 0 ? 0 : (value - (view_size / 2));
      break;
    case HALF_DOWN:
      new_value = (value + (view_size / 2)) > max ? max : (value + (view_size / 2));
      break;
    case LEFT:
    case UP:
      new_value = (value - scroll_step) < 0 ? 0 : (value - scroll_step);
      break;
    case RIGHT:
    case DOWN:
      new_value = (value + scroll_step) > max ? max : (value + scroll_step);
      break;
    case TOP:
      new_value = 0;
      break;
    case BOTTOM:
      new_value = max;
      break;
    default:
      new_value = 0;
  }

  gtk_adjustment_set_value(adjustment, new_value);

  return false;
}

bool
sc_search(girara_session_t* session, girara_argument_t* argument, unsigned int t)
{
  return false;
}

bool
sc_navigate_index(girara_session_t* session, girara_argument_t* argument, unsigned int t)
{
  return false;
}

bool
sc_toggle_index(girara_session_t* session, girara_argument_t* argument, unsigned int t)
{
  if (session == NULL || Zathura.document == NULL) {
    return false;
  }

  girara_tree_node_t* document_index = NULL;
  GtkWidget* treeview                = NULL;
  GtkTreeModel* model                = NULL;
  GtkCellRenderer* renderer          = NULL;

  if (Zathura.UI.index == NULL) {
    /* create new index widget */
    Zathura.UI.index = gtk_scrolled_window_new(NULL, NULL);

    if (Zathura.UI.index == NULL) {
      goto error_ret;
    }

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Zathura.UI.index),
      GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    /* create index */
    document_index = zathura_document_index_generate(Zathura.document);
    if (document_index == NULL) {
      // TODO: Error message
      goto error_free;
    }

    model = GTK_TREE_MODEL(gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_POINTER));
    if (model == NULL) {
      goto error_free;
    }

    treeview = gtk_tree_view_new_with_model(model);
    if (treeview == NULL) {
      goto error_free;
    }

    g_object_unref(model);

    renderer = gtk_cell_renderer_text_new();
    if (renderer == NULL) {
      goto error_free;
    }

    document_index_build(model, NULL, document_index);
    girara_node_free(document_index);

    /* setup widget */
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (treeview), 0, "Title", renderer, "markup", 0, NULL);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    g_object_set(G_OBJECT(gtk_tree_view_get_column(GTK_TREE_VIEW(treeview), 0)), "expand", TRUE, NULL);
    gtk_tree_view_set_cursor(GTK_TREE_VIEW(treeview), gtk_tree_path_new_first(), NULL, FALSE);
    /*g_signal_connect(G_OBJECT(treeview), "row-activated", G_CALLBACK(cb_index_row_activated), NULL); TODO*/

    gtk_container_add(GTK_CONTAINER(Zathura.UI.index), treeview);
    gtk_widget_show(treeview);
  }

  if (GTK_WIDGET_VISIBLE(GTK_WIDGET(Zathura.UI.index))) {
    girara_set_view(session, Zathura.UI.page_view);
    gtk_widget_hide(GTK_WIDGET(Zathura.UI.index));
  } else {
    girara_set_view(session, Zathura.UI.index);
    gtk_widget_show(GTK_WIDGET(Zathura.UI.index));
  }

  return false;

error_free:

  if (Zathura.UI.index != NULL) {
    g_object_unref(Zathura.UI.index);
    Zathura.UI.index = NULL;
  }

  if (document_index != NULL) {
    girara_node_free(document_index);
  }
  
error_ret:

  return false;
}

bool
sc_toggle_inputbar(girara_session_t* session, girara_argument_t* argument, unsigned int t)
{
  if (session == NULL) {
    return false;
  }

  if (GTK_WIDGET_VISIBLE(GTK_WIDGET(session->gtk.inputbar))) {
    gtk_widget_hide(GTK_WIDGET(session->gtk.inputbar));
  } else {
    gtk_widget_show(GTK_WIDGET(session->gtk.inputbar));
  }

  return false;
}

bool
sc_toggle_fullscreen(girara_session_t* session, girara_argument_t* argument, unsigned int t)
{
  if (session == NULL) {
    return false;
  }

  static bool fullscreen = false;

  if (fullscreen) {
    gtk_window_unfullscreen(GTK_WINDOW(session->gtk.window));
  } else {
    gtk_window_fullscreen(GTK_WINDOW(session->gtk.window));
  }

  fullscreen = fullscreen ? false : true;

  return false;
}

bool
sc_toggle_statusbar(girara_session_t* session, girara_argument_t* argument, unsigned int t)
{
  if (session == NULL) {
    return false;
  }

  if (GTK_WIDGET_VISIBLE(GTK_WIDGET(session->gtk.statusbar))) {
    gtk_widget_hide(GTK_WIDGET(session->gtk.statusbar));
  } else {
    gtk_widget_show(GTK_WIDGET(session->gtk.statusbar));
  }

  return false;
}

bool
sc_quit(girara_session_t* session, girara_argument_t* argument, unsigned int t)
{
  girara_argument_t arg = { GIRARA_HIDE, NULL };
  girara_isc_completion(session, &arg, 0);

  cb_destroy(NULL, NULL);

  gtk_main_quit();

  return false;
}

bool
sc_zoom(girara_session_t* session, girara_argument_t* argument, unsigned int t)
{
  if (session == NULL || argument == NULL || Zathura.document == NULL) {
    return false;
  }

  /* retreive zoom step value */
  int* value = girara_setting_get(Zathura.UI.session, "zoom-step");
  if (value == NULL) {
    return false;
  }

  float zoom_step = *value / 100.0f;

  if (argument->n == ZOOM_IN) {
    Zathura.document->scale += zoom_step;
  } else if (argument->n == ZOOM_OUT) {
    Zathura.document->scale -= zoom_step;
  } else {
    Zathura.document->scale = 1.0;
  }

  render_all();

  return false;
}
