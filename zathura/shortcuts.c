/* SPDX-License-Identifier: Zlib */

#include <girara/session.h>
#include <girara/settings.h>
#include <girara/datastructures.h>
#include <girara/shortcuts.h>
#include <girara/utils.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "callbacks.h"
#include "shortcuts.h"
#include "dbus-interface.h"
#include "document.h"
#include "zathura.h"
#include "render.h"
#include "utils.h"
#include "page.h"
#include "print.h"
#include "page-widget.h"
#include "adjustment.h"
#include <math.h>

/* Helper function for highlighting the links */
static bool
draw_links(zathura_t* zathura)
{
  /* set pages to draw links */
  bool show_links = false;
  unsigned int page_offset = 0;
  const unsigned int number_of_pages = zathura_document_get_number_of_pages(zathura->document);
  for (unsigned int page_id = 0; page_id < number_of_pages; page_id++) {
    zathura_page_t* page = zathura_document_get_page(zathura->document, page_id);
    if (page == NULL) {
      continue;
    }

    GtkWidget* page_widget = zathura_page_get_widget(zathura, page);
    GObject* obj_page_widget = G_OBJECT(page_widget);
    g_object_set(obj_page_widget, "draw-search-results", FALSE, NULL);
    if (zathura_page_get_visibility(page) == true) {
      g_object_set(obj_page_widget, "draw-links", TRUE, NULL);

      int number_of_links = 0;
      g_object_get(obj_page_widget, "number-of-links", &number_of_links, NULL);
      if (number_of_links != 0) {
        show_links = true;
      }
      g_object_set(obj_page_widget, "offset-links", page_offset, NULL);
      page_offset += number_of_links;
    } else {
      g_object_set(obj_page_widget, "draw-links", FALSE, NULL);
    }
  }
  return show_links;
}

/* Common code for sc_follow, sc_display_link and sc_copy_link */
static bool
link_shortcuts(zathura_t* zathura, girara_callback_inputbar_activate_t callback,
		       const char* text)
{
  if (zathura->document == NULL || zathura->ui.session == NULL) {
    return false;
  }

  bool show_links = draw_links(zathura);

  /* ask for input */
  if (show_links == true) {
    GtkWidget *inputbar = zathura->ui.session->gtk.inputbar;
    gulong handler_id = g_signal_connect(inputbar, "hide", G_CALLBACK(cb_hide_links), zathura);
    g_object_set_data(G_OBJECT(inputbar), "handler_id", GUINT_TO_POINTER(handler_id));

    zathura_document_set_adjust_mode(zathura->document, ZATHURA_ADJUST_INPUTBAR);
    girara_dialog(zathura->ui.session, text, FALSE, NULL, callback, zathura->ui.session);
  }

  return false;
}

bool
sc_abort(girara_session_t* session, girara_argument_t* UNUSED(argument),
         girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  bool clear_search = true;
  girara_setting_get(session, "abort-clear-search", &clear_search);

  if (zathura->document != NULL) {
    const unsigned int number_of_pages = zathura_document_get_number_of_pages(zathura->document);
    for (unsigned int page_id = 0; page_id < number_of_pages; ++page_id) {
      zathura_page_t* page = zathura_document_get_page(zathura->document, page_id);
      if (page == NULL) {
        continue;
      }

      GtkWidget* page_widget = zathura_page_get_widget(zathura, page);
      GObject* obj_page_widget = G_OBJECT(page_widget);
      g_object_set(obj_page_widget, "draw-links", FALSE, NULL);
      if (clear_search == true) {
        g_object_set(obj_page_widget, "draw-search-results", FALSE, NULL);
      }
    }
  }

  /* Setting the mode back here has not worked for ages. We need another way to
   * do this. Let's disable this for now.
   */
  /* girara_mode_set(session, session->modes.normal); */
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

  if (argument->n < ZATHURA_ADJUST_NONE || argument->n >= ZATHURA_ADJUST_MODE_NUMBER) {
    girara_error("Invalid adjust mode: %d", argument->n);
    girara_notify(session, GIRARA_ERROR, _("Invalid adjust mode: %d"), argument->n);
  } else {
    girara_debug("Setting adjust mode to: %d", argument->n);

    zathura_document_set_adjust_mode(zathura->document, argument->n);
    adjust_view(zathura);
  }

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
sc_display_link(girara_session_t* session, girara_argument_t* UNUSED(argument),
                girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  return link_shortcuts(zathura, cb_sc_display_link, "Display Link: ");
}

bool
sc_copy_link(girara_session_t* session, girara_argument_t* UNUSED(argument),
             girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  return link_shortcuts(zathura, cb_sc_copy_link, "Copy Link: ");
}

bool
sc_copy_filepath(girara_session_t* session, girara_argument_t*  UNUSED(argument), girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
    g_return_val_if_fail(session != NULL, false);
    g_return_val_if_fail(session->global.data != NULL, false);
    zathura_t* zathura = session->global.data;

    GdkAtom* selection = get_selection(zathura);
    if (selection == NULL) {
        return false;
    }

    const char* file_path = zathura_document_get_path(zathura->document);
    if (file_path == NULL) {
        girara_debug("Could not get file path for copying");
        return false;
    }

    girara_debug("Copying file path to clipboard");
    gtk_clipboard_set_text(gtk_clipboard_get(*selection), file_path, -1);

    g_free(selection);
    return true;
}

bool
sc_focus_inputbar(girara_session_t* session, girara_argument_t* argument, girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->gtk.inputbar_entry != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);

  zathura_document_set_adjust_mode(zathura->document, ZATHURA_ADJUST_INPUTBAR);

  if (gtk_widget_get_visible(GTK_WIDGET(session->gtk.inputbar)) == false) {
    gtk_widget_show(GTK_WIDGET(session->gtk.inputbar));
  }

  if (gtk_widget_get_visible(GTK_WIDGET(session->gtk.notification_area)) == true) {
    gtk_widget_hide(GTK_WIDGET(session->gtk.notification_area));
  }

  gtk_widget_grab_focus(GTK_WIDGET(session->gtk.inputbar_entry));

  if (argument->data != NULL) {
    gtk_entry_set_text(session->gtk.inputbar_entry, (char*) argument->data);

    /* append filepath */
    if (argument->n == APPEND_FILEPATH && zathura->document != NULL) {
      const char* file_path = zathura_document_get_path(zathura->document);
      if (file_path == NULL) {
        return false;
      }

      char* path = g_path_get_dirname(file_path);
      char* escaped = girara_escape_string(path);
      char* tmp  = g_strdup_printf("%s%s/", (char*) argument->data, (g_strcmp0(path, "/") == 0) ? "" : escaped);
      g_free(path);
      g_free(escaped);

      gtk_entry_set_text(session->gtk.inputbar_entry, tmp);
      g_free(tmp);
    }

    GdkAtom* selection = get_selection(zathura);

    /* we save the X clipboard that will be clear by "grab_focus" */
    gchar* x_clipboard_text = NULL;

    if (selection != NULL) {
      x_clipboard_text = gtk_clipboard_wait_for_text(gtk_clipboard_get(*selection));
    }

    gtk_editable_set_position(GTK_EDITABLE(session->gtk.inputbar_entry), -1);

    if (x_clipboard_text != NULL && selection != NULL) {
      /* we reset the X clipboard with saved text */
      gtk_clipboard_set_text(gtk_clipboard_get(*selection), x_clipboard_text, -1);
      g_free(x_clipboard_text);
    }

    g_free(selection);
  }

  return true;
}

bool
sc_follow(girara_session_t* session, girara_argument_t* UNUSED(argument),
          girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  return link_shortcuts(zathura, cb_sc_follow, "Follow Link: ");
}

bool
sc_goto(girara_session_t* session, girara_argument_t* argument, girara_event_t* UNUSED(event), unsigned int t)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(zathura->document != NULL, false);

  zathura_jumplist_add(zathura);
  if (t != 0) {
    /* add offset */
    t += zathura_document_get_page_offset(zathura->document);

    page_set(zathura, t - 1);
  } else if (argument->n == TOP) {
    page_set(zathura, 0);
  } else if (argument->n == BOTTOM) {
    page_set(zathura, zathura_document_get_number_of_pages(zathura->document) - 1);
  }

  zathura_jumplist_add(zathura);

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

  if (zathura->document == NULL) {
    return false;
  }

  GtkAdjustment* x_adj = NULL;
  GtkAdjustment* y_adj = NULL;

  switch (event->type) {
      /* scroll */
    case GIRARA_EVENT_SCROLL_UP:
    case GIRARA_EVENT_SCROLL_DOWN:
    case GIRARA_EVENT_SCROLL_LEFT:
    case GIRARA_EVENT_SCROLL_RIGHT:
    case GIRARA_EVENT_SCROLL_BIDIRECTIONAL:
      return sc_scroll(session, argument, event, t);

      /* drag */
    case GIRARA_EVENT_BUTTON_PRESS:
      zathura->shortcut.mouse.x = event->x;
      zathura->shortcut.mouse.y = event->y;
      break;
    case GIRARA_EVENT_BUTTON_RELEASE:
      zathura->shortcut.mouse.x = 0;
      zathura->shortcut.mouse.y = 0;
      break;
    case GIRARA_EVENT_MOTION_NOTIFY:
      x_adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(session->gtk.view));
      y_adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(session->gtk.view));

      if (x_adj == NULL || y_adj == NULL) {
        return false;
      }

      zathura_adjustment_set_value(x_adj,
          gtk_adjustment_get_value(x_adj) - (event->x - zathura->shortcut.mouse.x));
      zathura_adjustment_set_value(y_adj,
          gtk_adjustment_get_value(y_adj) - (event->y - zathura->shortcut.mouse.y));
      break;

      /* unhandled events */
    default:
      break;
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
  switch (event->type) {
    case GIRARA_EVENT_SCROLL_UP:
      argument->n = ZOOM_IN;
      break;
    case GIRARA_EVENT_SCROLL_DOWN:
      argument->n = ZOOM_OUT;
      break;
    case GIRARA_EVENT_SCROLL_BIDIRECTIONAL:
      argument->n = ZOOM_SMOOTH;
      break;
    default:
      return false;
  }

  return sc_zoom(session, argument, event, t);
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

  int number_of_pages = zathura_document_get_number_of_pages(zathura->document);
  int new_page        = zathura_document_get_current_page_number(zathura->document);

  bool scroll_wrap = false;
  girara_setting_get(session, "scroll-wrap", &scroll_wrap);

  bool columns_per_row_offset = false;
  girara_setting_get(session, "advance-pages-per-row", &columns_per_row_offset);

  int offset = 1;
  if (columns_per_row_offset == true) {
    girara_setting_get(session, "pages-per-row", &offset);
  }

  t = (t == 0) ? (unsigned int) offset : t;
  if (argument->n == NEXT) {
    if (scroll_wrap == false) {
      new_page = new_page + t;
    } else {
      new_page = (new_page + t) % number_of_pages;
    }
  } else if (argument->n == PREVIOUS) {
    if (scroll_wrap == false) {
      new_page = new_page - t;
    } else {
      new_page = (new_page + number_of_pages - t) % number_of_pages;
    }
  }

  if (!scroll_wrap) {
    if (new_page <= 0) {
      new_page = 0;
    } else if (new_page >= number_of_pages) {
      new_page = number_of_pages - 1;
    }
  }

  page_set(zathura, new_page);

  return false;
}

bool
sc_print(girara_session_t* session, girara_argument_t* UNUSED(argument),
         girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  if (zathura->document == NULL) {
    girara_notify(session, GIRARA_ERROR, _("No document opened."));
    return false;
  }

  print(zathura);

  return true;
}

bool
sc_recolor(girara_session_t* session, girara_argument_t* UNUSED(argument),
           girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);

  bool value = false;
  girara_setting_get(session, "recolor", &value);
  value = !value;
  girara_setting_set(session, "recolor", &value);

  return false;
}

bool
sc_reload(girara_session_t* session, girara_argument_t* UNUSED(argument),
          girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  if (zathura->file_monitor.monitor == NULL) {
    return false;
  }

  /* close current document */
  document_close(zathura, true);

  /* reopen document */
  document_open(
    zathura, zathura_filemonitor_get_filepath(zathura->file_monitor.monitor),
    NULL, zathura->file_monitor.password, ZATHURA_PAGE_NUMBER_UNSPECIFIED);

  return false;
}

bool
sc_rotate(girara_session_t* session, girara_argument_t* argument,
          girara_event_t* UNUSED(event), unsigned int t)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(zathura->document != NULL, false);

  const unsigned int page_number = zathura_document_get_current_page_number(zathura->document);

  int angle = 90;
  if (argument != NULL && argument->n == ROTATE_CCW) {
    angle = 270;
  }

  /* update rotate value */
  t = (t == 0) ? 1 : t;
  unsigned int rotation = zathura_document_get_rotation(zathura->document);
  zathura_document_set_rotation(zathura->document, (rotation + angle * t) % 360);

  /* update scale */
  girara_argument_t new_argument = { zathura_document_get_adjust_mode(zathura->document), NULL };
  sc_adjust_window(zathura->ui.session, &new_argument, NULL, 0);

  /* render all pages again */
  render_all(zathura);

  page_set(zathura, page_number);

  return false;
}

bool
sc_scroll(girara_session_t* session, girara_argument_t* argument,
          girara_event_t* event, unsigned int t)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  if (zathura->document == NULL) {
    return false;
  }

  /* if TOP or BOTTOM, go there and we are done */
  if (argument->n == TOP) {
    position_set(zathura, -1, 0);
    return false;
  } else if (argument->n == BOTTOM) {
    position_set(zathura, -1, 1.0);
    return false;
  }

  /* Retrieve current page and position */
  const unsigned int page_id = zathura_document_get_current_page_number(zathura->document);
  double pos_x = zathura_document_get_position_x(zathura->document);
  double pos_y = zathura_document_get_position_y(zathura->document);

  /* If PAGE_TOP or PAGE_BOTTOM, go there and we are done */
  if (argument->n == PAGE_TOP) {
    double dontcare = 0.5;
    page_number_to_position(zathura->document, page_id, dontcare, 0.0, &dontcare, &pos_y);
    position_set(zathura, pos_x, pos_y);
    return false;
  } else if (argument->n == PAGE_BOTTOM) {
    double dontcare = 0.5;
    page_number_to_position(zathura->document, page_id, dontcare, 1.0, &dontcare, &pos_y);
    position_set(zathura, pos_x, pos_y);
    return false;
  }

  if (t == 0) {
    t = 1;
  }

  unsigned int view_width  = 0;
  unsigned int view_height = 0;
  zathura_document_get_viewport_size(zathura->document, &view_height, &view_width);

  unsigned int doc_width  = 0;
  unsigned int doc_height = 0;
  zathura_document_get_document_size(zathura->document, &doc_height, &doc_width);

  float scroll_step = 40;
  girara_setting_get(session, "scroll-step", &scroll_step);
  float scroll_hstep = -1;
  girara_setting_get(session, "scroll-hstep", &scroll_hstep);
  if (scroll_hstep < 0) {
    scroll_hstep = scroll_step;
  }
  float scroll_full_overlap = 0.0;
  girara_setting_get(session, "scroll-full-overlap", &scroll_full_overlap);
  bool scroll_page_aware = false;
  girara_setting_get(session, "scroll-page-aware", &scroll_page_aware);

  bool scroll_wrap = false;
  girara_setting_get(session, "scroll-wrap", &scroll_wrap);

  /* compute the direction of scrolling */
  double direction = 1.0;
  if ((argument->n == LEFT) || (argument->n == FULL_LEFT) || (argument->n == HALF_LEFT) ||
      (argument->n == UP) || (argument->n == FULL_UP) || (argument->n == HALF_UP)) {
    direction = -1.0;
  }

  unsigned int pad  = zathura_document_get_page_padding(zathura->document);
  const double vstep = (double)(view_height + pad) / (double)doc_height;
  const double hstep = (double)(view_width + pad) / (double)doc_width;

  /* compute new position */
  switch (argument->n) {
    case FULL_UP:
    case FULL_DOWN:
      pos_y += direction * (1.0 - scroll_full_overlap) * vstep;
      break;

    case FULL_LEFT:
    case FULL_RIGHT:
      pos_x += direction * (1.0 - scroll_full_overlap) * hstep;
      break;

    case HALF_UP:
    case HALF_DOWN:
      pos_y += direction * 0.5 * vstep;
      break;

    case HALF_LEFT:
    case HALF_RIGHT:
      pos_x += direction * 0.5 * hstep;
      break;

    case UP:
    case DOWN:
      pos_y += direction * t * scroll_step / (double)doc_height;
      break;

    case LEFT:
    case RIGHT:
      pos_x += direction * t * scroll_hstep / (double)doc_width;
      break;

    case BIDIRECTIONAL: {
      pos_x += event->x * t * scroll_hstep / (double)doc_width;
      pos_y += event->y * t * scroll_step / (double)doc_height;
      break;
    }
  }

  /* handle boundaries */
  const double end_x = 0.5 * (double)view_width / (double)doc_width;
  const double end_y = 0.5 * (double)view_height / (double)doc_height;

  const double new_x = scroll_wrap ? 1.0 - end_x : end_x;
  const double new_y = scroll_wrap ? 1.0 - end_y : end_y;

  if (pos_x < end_x) {
    pos_x = new_x;
  } else if (pos_x > 1.0 - end_x) {
    pos_x = 1 - new_x;
  }

  if (pos_y < end_y) {
    pos_y = new_y;
  } else if (pos_y > 1.0 - end_y) {
    pos_y = 1 - new_y;
  }

  /* snap to the border if we change page */
  const unsigned int new_page_id = position_to_page_number(zathura->document, pos_x, pos_y);
  if (scroll_page_aware == true && page_id != new_page_id) {
    double dummy = 0.0;
    switch(argument->n) {
      case FULL_LEFT:
      case HALF_LEFT:
        page_number_to_position(zathura->document, new_page_id, 1.0, 0.0, &pos_x, &dummy);
        break;

      case FULL_RIGHT:
      case HALF_RIGHT:
        page_number_to_position(zathura->document, new_page_id, 0.0, 0.0, &pos_x, &dummy);
        break;

      case FULL_UP:
      case HALF_UP:
        page_number_to_position(zathura->document, new_page_id, 0.0, 1.0, &dummy, &pos_y);
        break;

      case FULL_DOWN:
      case HALF_DOWN:
        page_number_to_position(zathura->document, new_page_id, 0.0, 0.0, &dummy, &pos_y);
        break;
    }
  }

  position_set(zathura, pos_x, pos_y);
  return false;
}


bool
sc_jumplist(girara_session_t* session, girara_argument_t* argument,
            girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(zathura->document != NULL, false);

  /* if no jumps in the jumplist */
  if (zathura->jumplist.size == 0) {
    return true;
  }

  double x = zathura_document_get_position_x(zathura->document);
  double y = zathura_document_get_position_y(zathura->document);

  zathura_jump_t* jump = NULL;
  zathura_jump_t* prev_jump = zathura_jumplist_current(zathura);
  bool go_to_current = false;

  if (zathura_jumplist_has_next(zathura) == false || zathura_jumplist_has_previous(zathura) == false) {
    if (x == prev_jump->x && y == prev_jump->y) {
      go_to_current = false;
    } else {
      go_to_current = true;
    }
  }

  switch(argument->n) {
    case FORWARD:
      if (go_to_current == true && zathura_jumplist_has_previous(zathura) == false) {
        jump = zathura_jumplist_current(zathura);
      } else {
        zathura_jumplist_forward(zathura);
        jump = zathura_jumplist_current(zathura);
      }
      break;
    case BACKWARD:
      if (go_to_current == true && zathura_jumplist_has_next(zathura) == false) {
        jump = zathura_jumplist_current(zathura);
      } else {
        zathura_jumplist_backward(zathura);
        jump = zathura_jumplist_current(zathura);
      }
      break;
  }

  if (jump == prev_jump) {
    if ((zathura_jumplist_has_previous(zathura) == false && argument->n == BACKWARD) ||
        (zathura_jumplist_has_next(zathura) == false && argument->n == FORWARD)) {
      jump = NULL;
    }
  }

  if (jump != NULL) {
    page_set(zathura, jump->page);
    position_set(zathura, jump->x, jump->y);
  }

  return false;
}


bool
sc_bisect(girara_session_t* session, girara_argument_t* argument,
          girara_event_t* UNUSED(event), unsigned int t)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(zathura->document != NULL, false);

  const unsigned int num_pages = zathura_document_get_number_of_pages(zathura->document);
  const unsigned int cur_page = zathura_document_get_current_page_number(zathura->document);

  /* process arguments */
  int direction;
  if (t > 0 && t <= num_pages) {
    /* bisect between cur_page and t */
    t -= 1;
    if (t == cur_page) {
      /* nothing to do */
      return false;
    }
    else if (t > cur_page) {
      zathura->bisect.start = cur_page;
      zathura->bisect.end = t;
      direction = BACKWARD;
    } else {
      zathura->bisect.start = t;
      zathura->bisect.end = cur_page;
      direction = FORWARD;
    }
  } else {
    direction = argument->n;

    /* setup initial bisect range */
    zathura_jump_t* jump = zathura_jumplist_current(zathura);
    if (jump == NULL) {
      girara_debug("bisecting between first and last page because there are no jumps");
      zathura->bisect.start = 0;
      zathura->bisect.end = num_pages - 1;
    } else if (jump->page != cur_page || jump->page != zathura->bisect.last_jump) {
      girara_debug("last jump doesn't match up, starting new bisecting");
      zathura->bisect.start = 0;
      zathura->bisect.end = num_pages - 1;

      unsigned int prev_page;
      if (direction == FORWARD) {
        prev_page = num_pages - 1;
      } else {
        prev_page = 0;
      }

      /* check if we have previous jumps */
      if (zathura_jumplist_has_previous(zathura) == true) {
        zathura_jumplist_backward(zathura);
        jump = zathura_jumplist_current(zathura);
        if (jump != NULL) {
          prev_page = jump->page;
        }
        zathura_jumplist_forward(zathura);
      }

      zathura->bisect.start = MIN(prev_page, cur_page);
      zathura->bisect.end = MAX(prev_page, cur_page);
      zathura->bisect.last_jump = cur_page;
    }
  }

  girara_debug("bisecting between %d and %d, at %d", zathura->bisect.start, zathura->bisect.end, cur_page);
  if (zathura->bisect.start == zathura->bisect.end) {
    /* nothing to do */
    return false;
  }

  unsigned int next_page = cur_page;
  unsigned int next_start = zathura->bisect.start;
  unsigned int next_end = zathura->bisect.end;

  /* here we have next_start <= next_page <= next_end */

  /* bisect step */
  switch(direction) {
    case FORWARD:
      if (cur_page != zathura->bisect.end) {
        next_page = (cur_page + zathura->bisect.end) / 2;
        if (next_page == cur_page) {
          ++next_page;
        }
        next_start = cur_page;
      }
      break;

    case BACKWARD:
      if (cur_page != zathura->bisect.start) {
        next_page = (cur_page + zathura->bisect.start) / 2;
        if (next_page == cur_page) {
          --next_page;
        }
        next_end = cur_page;
      }
      break;
  }

  if (next_page == cur_page) {
    /* nothing to do */
    return false;
  }

  girara_debug("bisecting between %d and %d, jumping to %d", zathura->bisect.start, zathura->bisect.end, next_page);
  zathura->bisect.last_jump = next_page;
  zathura->bisect.start = next_start;
  zathura->bisect.end = next_end;

  zathura_jumplist_add(zathura);
  page_set(zathura, next_page);
  zathura_jumplist_add(zathura);

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

  const unsigned int num_pages = zathura_document_get_number_of_pages(zathura->document);
  const unsigned int cur_page  = zathura_document_get_current_page_number(zathura->document);
  GtkWidget* cur_page_widget   = zathura_page_get_widget(zathura, zathura_document_get_page(zathura->document, cur_page));
  bool nohlsearch              = false;
  bool first_time_after_abort  = false;

  girara_setting_get(session, "nohlsearch", &nohlsearch);
  if (nohlsearch == false) {
    gboolean draw = FALSE;
    g_object_get(G_OBJECT(cur_page_widget), "draw-search-results", &draw, NULL);

    if (draw == false) {
      first_time_after_abort = true;
    }

    document_draw_search_results(zathura, true);
  }

  int diff = argument->n == FORWARD ? 1 : -1;
  if (zathura->global.search_direction == BACKWARD) {
    diff = -diff;
  }

  zathura_page_t* target_page = NULL;
  int target_idx = 0;

  for (unsigned int page_id = 0; page_id < num_pages; ++page_id) {
    int tmp = cur_page + diff * page_id;
    zathura_page_t* page = zathura_document_get_page(zathura->document, (tmp + num_pages) % num_pages);
    if (page == NULL) {
      continue;
    }

    GtkWidget* page_widget = zathura_page_get_widget(zathura, page);

    int num_search_results = 0, current = -1;
    g_object_get(G_OBJECT(page_widget), "search-current", &current, "search-length", &num_search_results, NULL);
    if (num_search_results == 0 || current == -1) {
      continue;
    }

    if (first_time_after_abort == true || (tmp + num_pages) % num_pages != cur_page) {
      target_page = page;
      target_idx = diff == 1 ? 0 : num_search_results - 1;
      break;
    }

    if (diff == 1 && current < num_search_results - 1) {
      /* the next result is on the same page */
      target_page = page;
      target_idx = current + 1;
    } else if (diff == -1 && current > 0) {
      target_page = page;
      target_idx = current - 1;
    } else {
      /* the next result is on a different page */
      g_object_set(G_OBJECT(page_widget), "search-current", -1, NULL);

      for (unsigned int npage_id = 1; npage_id < num_pages; ++npage_id) {
        int ntmp = cur_page + diff * (page_id + npage_id);
        zathura_page_t* npage = zathura_document_get_page(zathura->document, (ntmp + 2*num_pages) % num_pages);
        GtkWidget* npage_page_widget = zathura_page_get_widget(zathura, npage);
        g_object_get(G_OBJECT(npage_page_widget), "search-length", &num_search_results, NULL);
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
    GtkWidget* page_widget = zathura_page_get_widget(zathura, target_page);
    GObject* obj_page_widget = G_OBJECT(page_widget);
    g_object_set(obj_page_widget, "search-current", target_idx, NULL);
    g_object_get(obj_page_widget, "search-results", &results, NULL);

    /* Need to adjust rectangle to page scale and orientation */
    zathura_rectangle_t* rect = girara_list_nth(results, target_idx);
    zathura_rectangle_t rectangle = recalc_rectangle(target_page, *rect);

    bool search_hadjust = true;
    girara_setting_get(session, "search-hadjust", &search_hadjust);

    /* compute the position of the center of the page */
    double pos_x = 0;
    double pos_y = 0;
    page_number_to_position(zathura->document, zathura_page_get_index(target_page),
                            0.5, 0.5, &pos_x, &pos_y);

    /* correction to center the current result                          */
    /* NOTE: rectangle is in viewport units, already scaled and rotated */
    unsigned int cell_height = 0;
    unsigned int cell_width = 0;
    zathura_document_get_cell_size(zathura->document, &cell_height, &cell_width);

    unsigned int doc_height = 0;
    unsigned int doc_width = 0;
    zathura_document_get_document_size(zathura->document, &doc_height, &doc_width);

    /* compute the center of the rectangle, which will be aligned to the center
       of the viewport */
    const double center_y = (rectangle.y1 + rectangle.y2) / 2;
    pos_y += (center_y - (double)cell_height/2) / (double)doc_height;

    if (search_hadjust == true) {
      const double center_x = (rectangle.x1 + rectangle.x2) / 2;
      pos_x += (center_x - (double)cell_width/2) / (double)doc_width;
    }

    /* move to position */
    zathura_jumplist_add(zathura);
    position_set(zathura, pos_x, pos_y);
    zathura_jumplist_add(zathura);
  } else if (argument->data != NULL) {
    const char* input = argument->data;
    char* escaped_text = g_markup_printf_escaped(_("Pattern not found: %s"), input);
    girara_notify(session, GIRARA_ERROR, "%s", escaped_text);
    g_free(escaped_text);
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
    case TOP:
      /* go to the first node */
      gtk_tree_path_free(path);
      path = gtk_tree_path_new_first();
      break;
    case BOTTOM:
      /* go to the last visiible node */
      gtk_tree_path_free(path);
      path = gtk_tree_path_new_from_indices(gtk_tree_model_iter_n_children(model, NULL) - 1, -1);
      gtk_tree_model_get_iter(model, &iter, path);
      while (gtk_tree_model_iter_has_child(model, &iter) == TRUE &&
          gtk_tree_view_row_expanded(tree_view, path) == TRUE) {
          gtk_tree_path_append_index(path, gtk_tree_model_iter_n_children(model, &iter) - 1);
      }
      break;
    case UP:
      if (gtk_tree_path_prev(path) == FALSE) {
        /* For some reason gtk_tree_path_up returns TRUE although we're not
         * moving anywhere. */
        is_valid_path = gtk_tree_path_up(path) && (gtk_tree_path_get_depth(path) > 0);
      } else { /* row above */
        while (gtk_tree_view_row_expanded(tree_view, path)) {
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
      if (gtk_tree_view_collapse_row(tree_view, path) == FALSE
          && gtk_tree_path_get_depth(path) > 1) {
        gtk_tree_path_up(path);
        gtk_tree_view_collapse_row(tree_view, path);
      }
      break;
    case DOWN:
      if (gtk_tree_view_row_expanded(tree_view, path) == TRUE) {
        gtk_tree_path_down(path);
      } else {
        do {
          gtk_tree_model_get_iter(model, &iter, path);
          if (gtk_tree_model_iter_next(model, &iter)) {
            gtk_tree_path_free(path);
            path = gtk_tree_model_get_path(model, &iter);
            break;
          }
        } while((is_valid_path = (gtk_tree_path_get_depth(path) > 1))
                && gtk_tree_path_up(path));
      }
      break;
    case EXPAND:
      if (gtk_tree_view_expand_row(tree_view, path, FALSE)) {
        gtk_tree_path_down(path);
      }
      break;
    case EXPAND_ALL:
      gtk_tree_view_expand_all(tree_view);
      break;
    case COLLAPSE_ALL:
      gtk_tree_view_collapse_all(tree_view);
      gtk_tree_path_free(path);
      path = gtk_tree_path_new_first();
      gtk_tree_view_set_cursor(tree_view, path, NULL, FALSE);
      break;
    case TOGGLE:
      gtk_tree_model_get_iter(model, &iter, path);
      if (gtk_tree_model_iter_has_child(model, &iter) == TRUE) {
        if (gtk_tree_view_row_expanded(tree_view, path) == TRUE) {
          gtk_tree_view_collapse_row(tree_view, path);
        } else {
          gtk_tree_view_expand_row(tree_view, path, FALSE);
        }
      }
      break;
    case SELECT:
      cb_index_row_activated(tree_view, path, NULL, zathura);
      gtk_tree_path_free(path);
      return false;
  }

  if (is_valid_path == TRUE) {
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
      girara_notify(session, GIRARA_WARNING, _("This document does not contain any index"));
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

    gtk_style_context_add_class(gtk_widget_get_style_context(treeview),
        "indexmode");

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
    girara_set_view(session, zathura->ui.page_widget);
    gtk_widget_hide(GTK_WIDGET(zathura->ui.index));
    girara_mode_set(zathura->ui.session, zathura->modes.normal);

    /* refresh view */
    refresh_view(zathura);
  } else {
    /* save current position to the jumplist */
    zathura_jumplist_add(zathura);

    const zathura_adjust_mode_t adjust_mode =
      zathura_document_get_adjust_mode(zathura->document);

    /* zathura goes to the first page when toggling index mode if this isn't done */
    if (adjust_mode == ZATHURA_ADJUST_INPUTBAR) {
      zathura_document_set_adjust_mode(zathura->document, ZATHURA_ADJUST_NONE);
    }

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
sc_toggle_page_mode(girara_session_t* session, girara_argument_t*
                    UNUSED(argument), girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  if (zathura->document == NULL) {
    girara_notify(session, GIRARA_WARNING, _("No document opened."));
    return false;
  }

  unsigned int page_id = zathura_document_get_current_page_number(zathura->document);

  int pages_per_row = 1;
  girara_setting_get(zathura->ui.session, "pages-per-row", &pages_per_row);

  int value = 1;
  if (pages_per_row == 1) {
    value = zathura->shortcut.toggle_page_mode.pages;
  } else {
    zathura->shortcut.toggle_page_mode.pages = pages_per_row;
  }

  girara_setting_set(zathura->ui.session, "pages-per-row", &value);
  adjust_view(zathura);

  page_set(zathura, page_id);
  render_all(zathura);
  refresh_view(zathura);

  return true;
}

bool
sc_toggle_fullscreen(girara_session_t* session, girara_argument_t*
                     UNUSED(argument), girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  if (zathura->document == NULL) {
    girara_notify(session, GIRARA_WARNING, _("No document opened."));
    return false;
  }

  const girara_mode_t old_mode = girara_mode_get(session);
  if (old_mode == zathura->modes.fullscreen) {
    gtk_window_unfullscreen(GTK_WINDOW(session->gtk.window));
    refresh_view(zathura);
    girara_mode_set(session, zathura->modes.normal);
  } else if (old_mode == zathura->modes.normal) {
    gtk_window_fullscreen(GTK_WINDOW(session->gtk.window));
    refresh_view(zathura);
    girara_mode_set(session, zathura->modes.fullscreen);
  }

  return false;
}

bool
sc_toggle_presentation(girara_session_t* session, girara_argument_t*
                     UNUSED(argument), girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  if (zathura->document == NULL) {
    girara_notify(session, GIRARA_WARNING, _("No document opened."));
    return false;
  }

  const girara_mode_t old_mode = girara_mode_get(session);
  if (old_mode == zathura->modes.presentation) {
    /* reset pages per row */
    girara_setting_set(session, "pages-per-row", &zathura->shortcut.toggle_presentation_mode.pages);

    /* reset first page column */
    if (zathura->shortcut.toggle_presentation_mode.first_page_column_list != NULL) {
      girara_setting_set(session, "first-page-column", zathura->shortcut.toggle_presentation_mode.first_page_column_list);
    }

    /* show status bar */
    gtk_widget_show(GTK_WIDGET(session->gtk.statusbar));

    /* set full screen */
    gtk_window_unfullscreen(GTK_WINDOW(session->gtk.window));

    /* reset zoom */
    zathura_document_set_zoom(zathura->document, zathura->shortcut.toggle_presentation_mode.zoom);
    render_all(zathura);
    refresh_view(zathura);

    /* set mode */
    girara_mode_set(session, zathura->modes.normal);
  } else if (old_mode == zathura->modes.normal) {
    /* backup pages per row */
    girara_setting_get(session, "pages-per-row", &zathura->shortcut.toggle_presentation_mode.pages);

    /* backup first page column */
    g_free(zathura->shortcut.toggle_presentation_mode.first_page_column_list);
    zathura->shortcut.toggle_presentation_mode.first_page_column_list = NULL;
    /* this will leak. we need to move the values somewhere else */
    girara_setting_get(session, "first-page-column", &zathura->shortcut.toggle_presentation_mode.first_page_column_list);

    /* set single view */
    int int_value = 1;
    girara_setting_set(session, "pages-per-row", &int_value);

    /* back up zoom */
    zathura->shortcut.toggle_presentation_mode.zoom = zathura_document_get_zoom(zathura->document);

    /* adjust window */
    girara_argument_t argument = { ZATHURA_ADJUST_BESTFIT, NULL };
    sc_adjust_window(session, &argument, NULL, 0);

    /* hide status and inputbar */
    gtk_widget_hide(GTK_WIDGET(session->gtk.inputbar));
    gtk_widget_hide(GTK_WIDGET(session->gtk.statusbar));

    /* set full screen */
    gtk_window_fullscreen(GTK_WINDOW(session->gtk.window));
    refresh_view(zathura);

    /* set mode */
    girara_mode_set(session, zathura->modes.presentation);
  }

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
        event, unsigned int t)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(zathura->document != NULL, false);

  zathura_document_set_adjust_mode(zathura->document, ZATHURA_ADJUST_NONE);

  /* retrieve zoom step value */
  int value = 1;
  girara_setting_get(zathura->ui.session, "zoom-step", &value);

  const int nt = (t == 0) ? 1 : t;
  const double zoom_step = 1.0 + value / 100.0 * nt;
  const double old_zoom = zathura_document_get_zoom(zathura->document);

  /* specify new zoom value */
  if (argument->n == ZOOM_IN) {
    girara_debug("Increasing zoom by %0.2f.", zoom_step - 1.0);
    zathura_document_set_zoom(zathura->document, old_zoom * zoom_step);
  } else if (argument->n == ZOOM_OUT) {
    girara_debug("Decreasing zoom by %0.2f.", zoom_step - 1.0);
    zathura_document_set_zoom(zathura->document, old_zoom / zoom_step);
  } else if (argument->n == ZOOM_SPECIFIC) {
    if (t == 0) {
      girara_debug("Setting zoom to 1.");
      zathura_document_set_zoom(zathura->document, 1.0);
    } else {
      girara_debug("Setting zoom to %0.2f.", t / 100.0);
      zathura_document_set_zoom(zathura->document, t / 100.0);
    }
  } else if (argument->n == ZOOM_SMOOTH) {
    const double dy = (event != NULL) ? event->y : 1.0;
    const double z = pow(zoom_step, -dy);
    girara_debug("Increasing zoom by %0.2f.",  z - 1.0);
    zathura_document_set_zoom(zathura->document, old_zoom * z);
  } else {
    girara_debug("Setting zoom to 1.");
    zathura_document_set_zoom(zathura->document, 1.0);
  }

  /* zoom limitations */
  const double zoom = zathura_document_get_zoom(zathura->document);
  zathura_document_set_zoom(zathura->document, zathura_correct_zoom_value(session, zoom));

  const double new_zoom = zathura_document_get_zoom(zathura->document);
  if (fabs(new_zoom - old_zoom) <= DBL_EPSILON) {
    girara_debug("New and old zoom level are too close: %0.2f vs. %0.2f", new_zoom, old_zoom);
    return false;
  }

  girara_debug("Re-rendering with new zoom level %0.2f.", new_zoom);
  render_all(zathura);
  refresh_view(zathura);

  return false;
}

bool
sc_exec(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  if (argument == NULL || argument->data == NULL) {
    return false;
  }

  girara_argument_t new_argument = *argument;
  const char* bus_name           = zathura_dbus_get_name(zathura);
  char* s                        = girara_replace_substring(new_argument.data, "$DBUS", bus_name);
  if (s == NULL) {
    return false;
  }
  new_argument.data = s;

  if (zathura->document != NULL) {
    const char* path = zathura_document_get_path(zathura->document);
    unsigned int page = zathura_document_get_current_page_number(zathura->document);
    char page_buf[G_ASCII_DTOSTR_BUF_SIZE];
    g_ascii_dtostr(page_buf, G_ASCII_DTOSTR_BUF_SIZE, page+1);

    s = girara_replace_substring(new_argument.data, "$FILE", path);
    g_free(new_argument.data);

    if (s == NULL) {
      return false;
    }
    new_argument.data = s;

    s = girara_replace_substring(new_argument.data, "$PAGE", page_buf);
    g_free(new_argument.data);

    if (s == NULL) {
      return false;
    }
    new_argument.data = s;
  }

  const bool ret = girara_sc_exec(session, &new_argument, event, t);
  g_free(new_argument.data);
  return ret;
}

bool
sc_nohlsearch(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  document_draw_search_results(zathura, false);
  render_all(zathura);

  return false;
}

bool sc_snap_to_page(girara_session_t *session,
                     girara_argument_t *UNUSED(argument),
                     girara_event_t *UNUSED(event), unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t *zathura = session->global.data;
  g_return_val_if_fail(zathura->document != NULL, false);
  zathura_document_t *document = zathura->document;

  int page = zathura_document_get_current_page_number(document);
  return page_set(zathura, page);
}
