/* See LICENSE file for license and copyright information */

#include <girara/session.h>
#include <girara/settings.h>
#include <girara/datastructures.h>
#include <girara/shortcuts.h>
#include <gtk/gtk.h>

#include "callbacks.h"
#include "shortcuts.h"
#include "document.h"
#include "zathura.h"
#include "render.h"
#include "utils.h"

bool
sc_abort(girara_session_t* session, girara_argument_t* UNUSED(argument),
    girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);

  girara_mode_set(session, session->modes.normal);

  return false;
}

bool
sc_adjust_window(girara_session_t* session, girara_argument_t* argument,
    girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);

  unsigned int* pages_per_row = girara_setting_get(session, "pages-per-row");

  if (zathura->ui.page_view == NULL || zathura->document == NULL || pages_per_row == NULL) {
    goto error_ret;
  }

  /* get window size */
  /* TODO: Get correct size of the view widget */
  gint width;
  gint height;
  gtk_window_get_size(GTK_WINDOW(session->gtk.window), &width, &height);

  /* calculate total width and max-height */
  double total_width = 0;
  double max_height = 0;

  for (unsigned int page_id = 0; page_id < *pages_per_row; page_id++) {
    if (page_id == zathura->document->number_of_pages) {
      break;
    }

    zathura_page_t* page = zathura->document->pages[page_id];
    if (page == NULL) {
      goto error_free;
    }

    if (page->height > max_height) {
      max_height = page->height;
    }

    total_width += page->width;
  }

  if (argument->n == ADJUST_WIDTH) {
    zathura->document->scale = width / total_width;
  } else if (argument->n == ADJUST_BESTFIT) {
    zathura->document->scale = height / max_height;
  } else {
    goto error_free;
  }

  /* re-render all pages */
  render_all(zathura);

error_free:

  /* cleanup */
  g_free(pages_per_row);

error_ret:

  return false;
}

bool
sc_change_mode(girara_session_t* session, girara_argument_t* argument,
    girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);

  girara_mode_set(session, argument->n);

  return false;
}

bool
sc_follow(girara_session_t* session, girara_argument_t* UNUSED(argument),
    girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  if (zathura->document == NULL) {
    return false;
  }

  char* font = girara_setting_get(session, "font");

  float transparency = 0.5;
  float* tmp = girara_setting_get(session, "highlight-transparency");
  if (tmp != NULL) {
    transparency = *tmp;
    g_free(tmp);
  }

  unsigned int link_id = 0;
  for (unsigned int page_id = 0; page_id < zathura->document->number_of_pages; page_id++) {
    zathura_page_t* page = zathura->document->pages[page_id];
    if (page == NULL) {
      continue;
    }

    // TODO: is page visible?
    girara_list_t* links = zathura_page_links_get(page);
    if (links == NULL || girara_list_size(links) == 0) {
      continue;
    }

    GIRARA_LIST_FOREACH(links, zathura_link_t*, iter, link)
      cairo_t* cairo = gdk_cairo_create(page->drawing_area->window);
      if (font != NULL) {
        cairo_select_font_face(cairo, font, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
      }

      zathura_rectangle_t position = recalc_rectangle(page, link->position);

      /* draw text */
      cairo_set_font_size(cairo, 10);
      cairo_move_to(cairo, position.x1 + 1, position.y1 - 1);
      char* link_number = g_strdup_printf("%i", ++link_id);
      cairo_show_text(cairo, link_number);

      /* draw rectangle */
      GdkColor color = zathura->ui.colors.highlight_color;
      cairo_set_source_rgba(cairo, color.red, color.green, color.blue, transparency);
      cairo_rectangle(cairo, position.x1, position.y1, (position.x2 - position.x1), (position.y2 - position.y1));
      cairo_fill(cairo);

      cairo_destroy(cairo);

    GIRARA_LIST_FOREACH_END(links, zathura_link_t*, iter, link);
  }

  if (font != NULL) {
    g_free(font);
  }

  return false;
}

bool
sc_goto(girara_session_t* session, girara_argument_t* argument, girara_event_t* UNUSED(event), unsigned int t)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(zathura->document != NULL, false);

  if (argument->n == TOP) {
    page_set(zathura, 0);
  } else {
    if (t == 0) {
      page_set(zathura, zathura->document->number_of_pages - 1);
      return true;
    } else {
      page_set(zathura, t - 1);
    }
  }

  return false;
}

bool
sc_navigate(girara_session_t* session, girara_argument_t* argument,
    girara_event_t* UNUSED(event), unsigned int t)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(zathura->document != NULL, false);

  unsigned int number_of_pages = zathura->document->number_of_pages;
  unsigned int new_page        = zathura->document->current_page_number;

  t = (t == 0) ? 1 : t;
  if (argument->n == NEXT) {
    new_page = (new_page + t) % number_of_pages;
  } else if (argument->n == PREVIOUS) {
    new_page = (new_page + number_of_pages - t) % number_of_pages;
  }

  page_set(zathura, new_page);

  return false;
}

bool
sc_recolor(girara_session_t* session, girara_argument_t* UNUSED(argument),
    girara_event_t* UNUSED(event), unsigned int UNUSED(t))
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
    girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  if (zathura->document == NULL || zathura->document->file_path == NULL) {
    return false;
  }

  /* save current document path and password */
  char* path     = g_strdup(zathura->document->file_path);
  char* password = zathura->document->password ? g_strdup(zathura->document->password) : NULL;

  /* close current document */
  document_close(zathura);

  /* reopen document */
  document_open(zathura, path, password);

  /* clean up */
  g_free(path);
  g_free(password);

  return false;
}

bool
sc_rotate(girara_session_t* session, girara_argument_t* UNUSED(argument),
    girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(zathura->document != NULL, false);

  /* update rotate value */
  zathura->document->rotate  = (zathura->document->rotate + 90) % 360;

  /* render all pages again */
  /* XXX: we don't need to rerender, only to resize the widgets and redraw */
  render_all(zathura);

  return false;
}

bool
sc_scroll(girara_session_t* session, girara_argument_t* argument,
    girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  if (zathura->document == NULL) {
    return false;
  }

  GtkAdjustment* adjustment = NULL;
  if ( (argument->n == LEFT) || (argument->n == RIGHT) ) {
    adjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(session->gtk.view));
  } else {
    adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(session->gtk.view));
  }

  gdouble view_size   = gtk_adjustment_get_page_size(adjustment);
  gdouble value       = gtk_adjustment_get_value(adjustment);
  gdouble max         = gtk_adjustment_get_upper(adjustment) - view_size;
  gdouble scroll_step = 40;
  float* tmp = girara_setting_get(session, "scroll-step");
  if (tmp != NULL) {
    scroll_step = *tmp;
    g_free(tmp);
  }
  gdouble new_value;

  switch(argument->n) {
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
      new_value = value;
  }

  gtk_adjustment_set_value(adjustment, new_value);

  return false;
}

bool
sc_search(girara_session_t* session, girara_argument_t* argument,
    girara_event_t* UNUSED(event), unsigned int UNUSED(t))
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
    girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(zathura->document != NULL, false);

  if(zathura->ui.index == NULL) {
    return false;
  }

  GtkTreeView *tree_view = gtk_container_get_children(GTK_CONTAINER(zathura->ui.index))->data;
  GtkTreePath *path;

  gtk_tree_view_get_cursor(tree_view, &path, NULL);
  if (path == NULL) {
    return false;
  }

  GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
  GtkTreeIter   iter;
  GtkTreeIter   child_iter;

  gboolean is_valid_path = TRUE;

  switch(argument->n) {
    case UP:
      if(gtk_tree_path_prev(path) == FALSE) {
        is_valid_path = gtk_tree_path_up(path);
      } else { /* row above */
        while(gtk_tree_view_row_expanded(tree_view, path)) {
          gtk_tree_model_get_iter(model, &iter, path);
          /* select last child */
          gtk_tree_model_iter_nth_child(model, &child_iter, &iter,
            gtk_tree_model_iter_n_children(model, &iter)-1);
          gtk_tree_path_free(path);
          path = gtk_tree_model_get_path(model, &child_iter);
        }
      }
      break;
    case COLLAPSE:
      if(!gtk_tree_view_collapse_row(tree_view, path)
        && gtk_tree_path_get_depth(path) > 1) {
        gtk_tree_path_up(path);
        gtk_tree_view_collapse_row(tree_view, path);
      }
      break;
    case DOWN:
      if(gtk_tree_view_row_expanded(tree_view, path)) {
        gtk_tree_path_down(path);
      } else {
        do {
          gtk_tree_model_get_iter(model, &iter, path);
          if (gtk_tree_model_iter_next(model, &iter)) {
            path = gtk_tree_model_get_path(model, &iter);
            break;
          }
        } while((is_valid_path = (gtk_tree_path_get_depth(path) > 1))
          && gtk_tree_path_up(path));
      }
      break;
    case EXPAND:
      if(gtk_tree_view_expand_row(tree_view, path, FALSE)) {
        gtk_tree_path_down(path);
      }
      break;
    case SELECT:
      cb_index_row_activated(tree_view, path, NULL, zathura);
      return false;
  }

  if (is_valid_path) {
    gtk_tree_view_set_cursor(tree_view, path, NULL, FALSE);
  }

  gtk_tree_path_free(path);

  return false;
}

bool
sc_toggle_index(girara_session_t* session, girara_argument_t* UNUSED(argument),
    girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  if (zathura->document == NULL) {
    return false;
  }

  girara_tree_node_t* document_index = NULL;
  GtkWidget* treeview                = NULL;
  GtkTreeModel* model                = NULL;
  GtkCellRenderer* renderer          = NULL;
  GtkCellRenderer* renderer2         = NULL;

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

    model = GTK_TREE_MODEL(gtk_tree_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER));
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

    renderer2 = gtk_cell_renderer_text_new();
    if (renderer2 == NULL) {
      goto error_free;
    }

    document_index_build(model, NULL, document_index);
    girara_node_free(document_index);

    /* setup widget */
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (treeview), 0, "Title", renderer, "markup", 0, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (treeview), 1, "Target", renderer2, "text", 1, NULL);

    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    g_object_set(G_OBJECT(gtk_tree_view_get_column(GTK_TREE_VIEW(treeview), 0)), "expand", TRUE, NULL);
    gtk_tree_view_column_set_alignment(gtk_tree_view_get_column(GTK_TREE_VIEW(treeview), 1), 1.0f);
    gtk_tree_view_set_cursor(GTK_TREE_VIEW(treeview), gtk_tree_path_new_first(), NULL, FALSE);
    g_signal_connect(G_OBJECT(treeview), "row-activated", G_CALLBACK(cb_index_row_activated), zathura);

    gtk_container_add(GTK_CONTAINER(zathura->ui.index), treeview);
    gtk_widget_show(treeview);
  }

  if (gtk_widget_get_visible(GTK_WIDGET(zathura->ui.index))) {
    girara_set_view(session, zathura->ui.page_view_alignment);
    gtk_widget_hide(GTK_WIDGET(zathura->ui.index));
    girara_mode_set(zathura->ui.session, zathura->modes.normal);
  } else {
    girara_set_view(session, zathura->ui.index);
    gtk_widget_show(GTK_WIDGET(zathura->ui.index));
    girara_mode_set(zathura->ui.session, zathura->modes.index);
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
    UNUSED(argument), girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);

  static bool fullscreen   = false;
  static int pages_per_row = 1;


  if (fullscreen == true) {
    /* reset pages per row */
    girara_setting_set(session, "pages-per-row", &pages_per_row);

    /* show status bar */
    gtk_widget_show(GTK_WIDGET(session->gtk.statusbar));

    /* set full screen */
    gtk_window_unfullscreen(GTK_WINDOW(session->gtk.window));
  } else {
    /* backup pages per row */
    int* tmp = girara_setting_get(session, "pages-per-row");
    if (tmp != NULL) {
      pages_per_row = *tmp;
      g_free(tmp);
    }

    /* set single view */
    int int_value = 1;
    girara_setting_set(session, "pages-per-row", &int_value);

    /* adjust window */
    girara_argument_t argument = { ADJUST_BESTFIT, NULL };
    sc_adjust_window(session, &argument, NULL, 0);

    /* hide status and inputbar */
    gtk_widget_hide(GTK_WIDGET(session->gtk.inputbar));
    gtk_widget_hide(GTK_WIDGET(session->gtk.statusbar));

    /* set full screen */
    gtk_window_fullscreen(GTK_WINDOW(session->gtk.window));
  }

  fullscreen = fullscreen ? false : true;

  return false;
}

bool
sc_quit(girara_session_t* session, girara_argument_t* UNUSED(argument),
    girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);

  girara_argument_t arg = { GIRARA_HIDE, NULL };
  girara_isc_completion(session, &arg, NULL, 0);

  cb_destroy(NULL, NULL);

  return false;
}

bool
sc_zoom(girara_session_t* session, girara_argument_t* argument, girara_event_t*
    UNUSED(event), unsigned int UNUSED(t))
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
  g_free(value);

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

