/* See LICENSE file for license and copyright information */

#include <girara/session.h>
#include <girara/settings.h>
#include <girara/datastructures.h>
#include <girara/shortcuts.h>
#include <girara/utils.h>
#include <gtk/gtk.h>

#include "callbacks.h"
#include "shortcuts.h"
#include "document.h"
#include "zathura.h"
#include "render.h"
#include "utils.h"
#include "page_widget.h"

bool
sc_abort(girara_session_t* session, girara_argument_t* UNUSED(argument),
    girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  if (zathura->document != NULL) {
    for (unsigned int page_id = 0; page_id < zathura->document->number_of_pages; ++page_id) {
      zathura_page_t* page = zathura->document->pages[page_id];
      if (page == NULL) {
        continue;
      }

      g_object_set(page->drawing_area, "draw-links", FALSE, NULL);
    }
  }

  girara_mode_set(session, session->modes.normal);
  girara_sc_abort(session, NULL, NULL, 0);

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

  unsigned int pages_per_row = 1;
  girara_setting_get(session, "pages-per-row", &pages_per_row);

  if (zathura->ui.page_widget == NULL || zathura->document == NULL) {
    goto error_ret;
  }

  zathura->document->adjust_mode = argument->n;

  /* get window size */
  GtkAllocation allocation;
  gtk_widget_get_allocation(session->gtk.view, &allocation);
  gint width = allocation.width;
  gint height = allocation.height;

  /* calculate total width and max-height */
  double total_width = 0;
  double max_height = 0;

  for (unsigned int page_id = 0; page_id < pages_per_row; page_id++) {
    if (page_id == zathura->document->number_of_pages) {
      break;
    }

    zathura_page_t* page = zathura->document->pages[page_id];
    if (page == NULL) {
      goto error_ret;
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
    goto error_ret;
  }

  /* re-render all pages */
  render_all(zathura);

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

  /* set pages to draw links */
  bool show_links = false;
  unsigned int page_offset = 0;
  for (unsigned int page_id = 0; page_id < zathura->document->number_of_pages; page_id++) {
    zathura_page_t* page = zathura->document->pages[page_id];
    if (page == NULL) {
      continue;
    }

    g_object_set(page->drawing_area, "search-results", NULL, NULL);
    if (page->visible == true) {
      g_object_set(page->drawing_area, "draw-links", TRUE, NULL);

      int number_of_links = 0;
      g_object_get(page->drawing_area, "number-of-links", &number_of_links, NULL);
      if (number_of_links != 0) {
        show_links = true;
      }
      g_object_set(page->drawing_area, "offset-links", page_offset, NULL);
      page_offset += number_of_links;
    } else {
      g_object_set(page->drawing_area, "draw-links", FALSE, NULL);
    }
  }

  /* ask for input */
  if (show_links == true) {
    girara_dialog(zathura->ui.session, "Follow link:", FALSE, NULL, (girara_callback_inputbar_activate_t) cb_sc_follow, NULL);
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
    } else {
      page_set(zathura, t - 1);
    }
  }

  return false;
}

bool
sc_mouse_scroll(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(event != NULL, false);

  static int x = 0;
  static int y = 0;

  if (zathura->document == NULL) {
    return false;
  }

  /* scroll event */
  if (event->type == GIRARA_EVENT_SCROLL) {
    switch (event->direction) {
      case GIRARA_SCROLL_UP:
        argument->n = UP;
        break;
      case GIRARA_SCROLL_DOWN:
        argument->n = DOWN;
        break;
      case GIRARA_SCROLL_LEFT:
        argument->n = LEFT;
        break;
      case GIRARA_SCROLL_RIGHT:
        argument->n = RIGHT;
        break;
    }

    return sc_scroll(session, argument, NULL, t);
  } else if (event->type == GIRARA_EVENT_BUTTON_PRESS) {
    x = event->x;
    y = event->y;
  } else if (event->type == GIRARA_EVENT_BUTTON_RELEASE) {
    x = 0;
    y = 0;
  } else if (event->type == GIRARA_EVENT_MOTION_NOTIFY) {
    GtkAdjustment* x_adj =
      gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(session->gtk.view));
    GtkAdjustment* y_adj =
      gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(session->gtk.view));

    if (x_adj == NULL || y_adj == NULL) {
      return false;
    }

    set_adjustment(x_adj, gtk_adjustment_get_value(x_adj) - (event->x - x));
    set_adjustment(y_adj, gtk_adjustment_get_value(y_adj) - (event->y - y));

    x = event->x;
    y = event->y;
  }

  return false;
}

bool
sc_mouse_zoom(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(event != NULL, false);

  if (zathura->document == NULL) {
    return false;
  }

  /* scroll event */
  if (event->type == GIRARA_EVENT_SCROLL) {
    switch (event->direction) {
      case GIRARA_SCROLL_UP:
        argument->n = ZOOM_IN;
        break;
      case GIRARA_SCROLL_DOWN:
        argument->n = ZOOM_OUT;
        break;
      default:
        return false;
    }

    return sc_zoom(session, argument, NULL, t);
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

  unsigned int page_number = zathura->document->current_page_number;

  /* update rotate value */
  zathura->document->rotate  = (zathura->document->rotate + 90) % 360;

  /* render all pages again */
  render_all(zathura);

  page_set_delayed(zathura, page_number);

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
  float scroll_step = 40;
  girara_setting_get(session, "scroll-step", &scroll_step);
  gdouble new_value;

  switch(argument->n) {
    case FULL_UP:
      new_value = value - view_size;
      break;
    case FULL_DOWN:
      new_value = value + view_size;
      break;
    case HALF_UP:
      new_value = value - (view_size / 2);
      break;
    case HALF_DOWN:
      new_value = value + (view_size / 2);
      break;
    case LEFT:
    case UP:
      new_value = value - scroll_step;
      break;
    case RIGHT:
    case DOWN:
      new_value = value + scroll_step;
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

  set_adjustment(adjustment, new_value);

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

  const int num_pages = zathura->document->number_of_pages;
  const int cur_page = zathura->document->current_page_number;
  int diff = argument->n == FORWARD ? 1 : -1;

  zathura_page_t* target_page = NULL;
  int target_idx = 0;

  for (int page_id = 0; page_id < num_pages; ++page_id) {
    int tmp = cur_page + diff * page_id;
    zathura_page_t* page = zathura->document->pages[(tmp + num_pages) % num_pages];
    if (page == NULL) {
      continue;
    }

    int num_search_results = 0, current = -1;
    g_object_get(page->drawing_area, "search-current", &current,
        "search-length", &num_search_results, NULL);
    if (num_search_results == 0 || current == -1) {
      continue;
    }

    if (diff == 1 && current < num_search_results - 1) {
      /* the next result is on the same page */
      target_page = page;
      target_idx = current + 1;
      // g_object_set(page->drawing_area, "search-current", current + 1, NULL);
    } else if (diff == -1 && current >= 1) {
      // g_object_set(page->drawing_area, "search-current", current - 1, NULL);
      target_page = page;
      target_idx = current - 1;
    } else {
      /* the next result is on a different page */
      g_object_set(page->drawing_area, "search-current", -1, NULL);

      for (int npage_id = 1; page_id < num_pages; ++npage_id) {
        int ntmp = cur_page + diff * (page_id + npage_id);
        zathura_page_t* npage = zathura->document->pages[(ntmp + 2*num_pages) % num_pages];
        g_object_get(npage->drawing_area, "search-length", &num_search_results, NULL);
        if (num_search_results != 0) {
          target_page = npage;
          target_idx = diff == 1 ? 0 : num_search_results - 1;
          break;
        }
      }
    }
    break;
  }

  if (target_page != NULL) {
    girara_list_t* results = NULL;
    g_object_set(target_page->drawing_area, "search-current", target_idx, NULL);
    g_object_get(target_page->drawing_area, "search-results", &results, NULL);

    zathura_rectangle_t* rect = girara_list_nth(results, target_idx);
    zathura_rectangle_t rectangle = recalc_rectangle(target_page, *rect);
    page_offset_t offset;
    page_calculate_offset(target_page, &offset);

    GtkAdjustment* view_vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));
    GtkAdjustment* view_hadjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));

    int x = offset.x - gtk_adjustment_get_page_size(view_hadjustment) / 2 + rectangle.x1;
    int y = offset.y - gtk_adjustment_get_page_size(view_vadjustment) / 2 + rectangle.y1;
    set_adjustment(view_hadjustment, x);
    set_adjustment(view_vadjustment, y);
  }

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
    document_index = zathura_document_index_generate(zathura->document, NULL);
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
    girara_set_view(session, zathura->ui.page_widget_alignment);
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
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  static bool fullscreen   = false;
  static int pages_per_row = 1;
  static double zoom = 1.0;

  if (fullscreen == true) {
    /* reset pages per row */
    girara_setting_set(session, "pages-per-row", &pages_per_row);

    /* show status bar */
    gtk_widget_show(GTK_WIDGET(session->gtk.statusbar));

    /* set full screen */
    gtk_window_unfullscreen(GTK_WINDOW(session->gtk.window));

    /* reset scale */
    zathura->document->scale = zoom;
    render_all(zathura);
  } else {
    /* backup pages per row */
    girara_setting_get(session, "pages-per-row", &pages_per_row);

    /* set single view */
    int int_value = 1;
    girara_setting_set(session, "pages-per-row", &int_value);

    /* back up zoom */
    zoom = zathura->document->scale;

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
    UNUSED(event), unsigned int t)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(zathura->document != NULL, false);

  zathura->document->adjust_mode = ADJUST_NONE;

  /* retreive zoom step value */
  int value = 1;
  girara_setting_get(zathura->ui.session, "zoom-step", &value);

  float zoom_step = value / 100.0f;
  float oldzoom = zathura->document->scale;

  /* specify new zoom value */
  if (argument->n == ZOOM_IN) {
    zathura->document->scale += zoom_step;
  } else if (argument->n == ZOOM_OUT) {
    zathura->document->scale -= zoom_step;
  } else if (argument->n == ZOOM_SPECIFIC) {
    zathura->document->scale = t / 100.0f;
  } else {
    zathura->document->scale = 1.0;
  }

  /* zoom limitations */
  if (zathura->document->scale < 0.1f) {
    zathura->document->scale = 0.1f;
  } else if (zathura->document->scale > 10.0f) {
    zathura->document->scale = 10.0f;
  }

  /* keep position */
  GtkAdjustment* view_vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));
  GtkAdjustment* view_hadjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));
  gdouble valx = gtk_adjustment_get_value(view_hadjustment) / oldzoom * zathura->document->scale;
  gdouble valy = gtk_adjustment_get_value(view_vadjustment) / oldzoom * zathura->document->scale;
  set_adjustment(view_hadjustment, valx);
  set_adjustment(view_vadjustment, valy);

  render_all(zathura);

  return false;
}

