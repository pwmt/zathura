/* See LICENSE file for license and copyright information */

#include <girara.h>
#include <gtk/gtk.h>

#include "callbacks.h"
#include "shortcuts.h"
#include "document.h"
#include "zathura.h"
#include "render.h"
#include "utils.h"

bool
sc_abort(girara_session_t* session, girara_argument_t* UNUSED(argument),
    unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);

  girara_mode_set(session, session->modes.normal);

  return false;
}

bool
sc_adjust_window(girara_session_t* session, girara_argument_t* UNUSED(argument),
    unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);

  return false;
}

bool
sc_change_mode(girara_session_t* session, girara_argument_t* argument, unsigned
    int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);

  girara_mode_set(session, argument->n);

  return false;
}

bool
sc_focus_inputbar(girara_session_t* session, girara_argument_t* argument,
    unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);

  if (!(gtk_widget_get_visible(GTK_WIDGET(session->gtk.inputbar)))) {
    gtk_widget_show(GTK_WIDGET(session->gtk.inputbar));
  }

  if (gtk_widget_get_visible(GTK_WIDGET(session->gtk.notification_area))) {
    gtk_widget_hide(GTK_WIDGET(session->gtk.notification_area));
  }

  if (argument->data) {
    gtk_entry_set_text(session->gtk.inputbar, (char*) argument->data);
    gtk_widget_grab_focus(GTK_WIDGET(session->gtk.inputbar));
    gtk_editable_set_position(GTK_EDITABLE(session->gtk.inputbar), -1);
  }

  return false;
}

bool
sc_follow(girara_session_t* session, girara_argument_t* UNUSED(argument),
    unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);

  return false;
}

bool
sc_goto(girara_session_t* session, girara_argument_t* argument, unsigned int t)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(zathura->document != NULL, false);

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

    page_set(zathura, t - 1);
  }

  return false;
}

bool
sc_navigate(girara_session_t* session, girara_argument_t* argument, unsigned int
    UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(zathura->document != NULL, false);

  unsigned int number_of_pages = zathura->document->number_of_pages;
  unsigned int new_page        = zathura->document->current_page_number;

  if (argument->n == NEXT) {
    new_page = (new_page + 1) % number_of_pages;
  } else if (argument->n == PREVIOUS) {
    new_page = (new_page + number_of_pages - 1) % number_of_pages;
  }

  page_set(zathura, new_page);

  return false;
}

bool
sc_recolor(girara_session_t* session, girara_argument_t* UNUSED(argument),
    unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  zathura->global.recolor = !zathura->global.recolor;
  render_all(zathura);

  return false;
}

bool
sc_reload(girara_session_t* session, girara_argument_t* UNUSED(argument),
    unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);

  return false;
}

bool
sc_rotate(girara_session_t* session, girara_argument_t* UNUSED(argument),
    unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(zathura->document != NULL, false);

  /* update rotate value */
  zathura->document->rotate  = (zathura->document->rotate + 90) % 360;

  /* render all pages again */
  render_all(zathura);

  return false;
}

bool
sc_scroll(girara_session_t* session, girara_argument_t* argument, unsigned int
    UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  if (zathura->document == NULL) {
    return false;
  }

  GtkAdjustment* adjustment = NULL;
  if ( (argument->n == LEFT) || (argument->n == RIGHT) )
    adjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));
  else
    adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));

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
sc_search(girara_session_t* session, girara_argument_t* argument, unsigned int
    UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(zathura->document != NULL, false);

  return false;
}

bool
sc_navigate_index(girara_session_t* session, girara_argument_t* argument,
    unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(zathura->document != NULL, false);

  return false;
}

bool
sc_toggle_index(girara_session_t* session, girara_argument_t* argument, unsigned
    int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(zathura->document != NULL, false);

  girara_tree_node_t* document_index = NULL;
  GtkWidget* treeview                = NULL;
  GtkTreeModel* model                = NULL;
  GtkCellRenderer* renderer          = NULL;

  if (zathura->ui.index == NULL) {
    /* create new index widget */
    zathura->ui.index = gtk_scrolled_window_new(NULL, NULL);

    if (zathura->ui.index == NULL) {
      goto error_ret;
    }

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(zathura->ui.index),
      GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    /* create index */
    document_index = zathura_document_index_generate(zathura->document);
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

    gtk_container_add(GTK_CONTAINER(zathura->ui.index), treeview);
    gtk_widget_show(treeview);
  }

  if (gtk_widget_get_visible(GTK_WIDGET(zathura->ui.index))) {
    girara_set_view(session, zathura->ui.page_view);
    gtk_widget_hide(GTK_WIDGET(zathura->ui.index));
  } else {
    girara_set_view(session, zathura->ui.index);
    gtk_widget_show(GTK_WIDGET(zathura->ui.index));
  }

  return false;

error_free:

  if (zathura->ui.index != NULL) {
    g_object_ref_sink(zathura->ui.index);
    zathura->ui.index = NULL;
  }

  if (document_index != NULL) {
    girara_node_free(document_index);
  }

error_ret:

  return false;
}

bool
sc_toggle_fullscreen(girara_session_t* session, girara_argument_t*
    UNUSED(argument), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);

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
sc_quit(girara_session_t* session, girara_argument_t* UNUSED(argument), unsigned
    int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);

  girara_argument_t arg = { GIRARA_HIDE, NULL };
  girara_isc_completion(session, &arg, 0);

  cb_destroy(NULL, NULL);

  gtk_main_quit();

  return false;
}

bool
sc_zoom(girara_session_t* session, girara_argument_t* argument, unsigned int
    UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(zathura->document != NULL, false);

  /* retreive zoom step value */
  int* value = girara_setting_get(zathura->ui.session, "zoom-step");
  if (value == NULL) {
    return false;
  }

  float zoom_step = *value / 100.0f;

  if (argument->n == ZOOM_IN) {
    zathura->document->scale += zoom_step;
  } else if (argument->n == ZOOM_OUT) {
    zathura->document->scale -= zoom_step;
  } else {
    zathura->document->scale = 1.0;
  }

  render_all(zathura);

  return false;
}
