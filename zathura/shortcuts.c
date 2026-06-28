/* SPDX-License-Identifier: Zlib */

#include "shortcuts.h"

#include <girara-gtk/internal.h>
#include <girara-gtk/session.h>
#include <girara-gtk/settings.h>
#include <girara-gtk/shortcuts.h>
#include <girara-gtk/statusbar.h>
#include <girara/datastructures.h>
#include <girara/log.h>
#include <girara/utils.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <math.h>

#include "adjustment.h"
#include "callbacks.h"
#include "commands.h"
#include "database.h"
#include "dbus-interface.h"
#include "document-widget.h"
#include "document.h"
#include "index-element-object.h"
#include "page-widget.h"
#include "page.h"
#include "plugin.h"
#include "print.h"
#include "render.h"
#include "utils.h"
#include "zathura.h"

/* Helper function for highlighting the links */
static bool draw_links(zathura_t* zathura) {
  /* set pages to draw links */
  bool show_links                    = false;
  unsigned int page_offset           = 0;
  zathura_document_t* document       = zathura_get_document(zathura);
  const unsigned int number_of_pages = zathura_document_get_number_of_pages(document);
  for (unsigned int page_id = 0; page_id < number_of_pages; page_id++) {
    zathura_page_t* page = zathura_document_get_page(document, page_id);
    if (page == NULL) {
      continue;
    }

    GtkWidget* page_widget   = zathura_page_get_widget(zathura, page);
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
static bool link_shortcuts(zathura_t* zathura, girara_callback_inputbar_activate_t callback, const char* text) {
  zathura_document_t* document = zathura_get_document(zathura);
  if (document == NULL || zathura->ui.session == NULL) {
    return false;
  }

  bool show_links = draw_links(zathura);

  /* ask for input */
  if (show_links == true) {
    GtkWidget* inputbar = zathura->ui.session->gtk.inputbar;
    gulong handler_id   = g_signal_connect(inputbar, "hide", G_CALLBACK(cb_hide_links), zathura);
    g_object_set_data(G_OBJECT(inputbar), "handler_id", GUINT_TO_POINTER(handler_id));

    zathura_document_set_adjust_mode(document, ZATHURA_ADJUST_INPUTBAR);
    girara_dialog(zathura->ui.session, text, FALSE, NULL, callback, zathura->ui.session);
  }

  return false;
}

bool sc_abort(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event),
              unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura           = session->global.data;
  zathura_document_t* document = zathura_get_document(zathura);

  bool clear_search = true;
  girara_setting_get(session, "abort-clear-search", &clear_search);

  if (document != NULL) {
    const unsigned int number_of_pages = zathura_document_get_number_of_pages(document);
    for (unsigned int page_id = 0; page_id < number_of_pages; ++page_id) {
      zathura_page_t* page = zathura_document_get_page(document, page_id);
      if (page == NULL) {
        continue;
      }

      GtkWidget* page_widget   = zathura_page_get_widget(zathura, page);
      GObject* obj_page_widget = G_OBJECT(page_widget);
      zathura_page_widget_clear_selection(ZATHURA_PAGE_WIDGET(page_widget));
      g_object_set(obj_page_widget, "draw-links", FALSE, NULL);
      if (clear_search == true) {
        g_object_set(obj_page_widget, "draw-search-results", FALSE, NULL);
      }
    }
    girara_statusbar_item_set_text(zathura->ui.session, zathura->ui.statusbar.search_count, "");
  }

  /* Setting the mode back here has not worked for ages. We need another way to
   * do this. Let's disable this for now.
   */
  /* girara_mode_set(session, session->modes.normal); */
  girara_sc_abort(session, NULL, NULL, 0);

  return false;
}

bool sc_adjust_window(girara_session_t* session, girara_argument_t* argument, girara_event_t* UNUSED(event),
                      unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);

  if (argument->n < ZATHURA_ADJUST_NONE || argument->n >= ZATHURA_ADJUST_MODE_NUMBER) {
    girara_error("Invalid adjust mode: %d", argument->n);
    girara_notify(session, GIRARA_ERROR, _("Invalid adjust mode: %d"), argument->n);
  } else {
    girara_debug("Setting adjust mode to: %d", argument->n);

    zathura_document_set_adjust_mode(zathura_get_document(zathura), argument->n);
    adjust_view(zathura);
  }

  return false;
}

bool sc_change_mode(girara_session_t* session, girara_argument_t* argument, girara_event_t* UNUSED(event),
                    unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);

  girara_mode_set(session, argument->n);

  return false;
}

bool sc_cycle_first_column(girara_session_t* session, girara_argument_t* UNUSED(argument),
                           girara_event_t* UNUSED(event), unsigned int t) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura           = session->global.data;
  zathura_document_t* document = zathura_get_document(zathura);

  if (document == NULL) {
    girara_notify(session, GIRARA_WARNING, _("No document opened."));
    return false;
  }

  unsigned int pages_per_row = 1;
  girara_setting_get(session, "pages-per-row", &pages_per_row);
  g_autofree char* first_page_column_list = NULL;
  girara_setting_get(session, "first-page-column", &first_page_column_list);

  if (t == 0) {
    t = 1;
  }

  g_autofree char* new_column_list = increment_first_page_column(first_page_column_list, pages_per_row, t);
  girara_setting_set(session, "first-page-column", new_column_list);

  return true;
}

bool sc_display_link(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event),
                     unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  return link_shortcuts(zathura, cb_sc_display_link, "Display Link: ");
}

bool sc_copy_link(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event),
                  unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  return link_shortcuts(zathura, cb_sc_copy_link, "Copy Link: ");
}

bool sc_copy_filepath(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event),
                      unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  zathura_document_t* document = zathura_get_document(zathura);
  if (document == NULL) {
    girara_notify(session, GIRARA_ERROR, _("No document opened."));
    return false;
  }

  GdkClipboard* selection = get_selection(zathura);
  if (selection == NULL) {
    return false;
  }

  const char* file_path = zathura_document_get_path(document);
  if (file_path == NULL) {
    girara_debug("Could not get file path for copying");
    return false;
  }

  girara_debug("Copying file path to clipboard");
  gdk_clipboard_set_text(selection, file_path);

  bool notification = true;
  girara_setting_get(session, "selection-notification", &notification);
  if (notification == true) {
    g_autofree char* target = NULL;
    girara_setting_get(session, "selection-clipboard", &target);
    g_autofree char* escaped = g_markup_printf_escaped(_("Copied file path to selection %s: %s"), target, file_path);
    girara_notify(session, GIRARA_INFO, "%s", escaped);
  }

  return true;
}

bool sc_equal_page_mode(girara_session_t* session, girara_argument_t* argument, girara_event_t* UNUSED(event),
                        unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(zathura->document != NULL, false);

  if (argument->n >= ZATHURA_EQUAL_MODE_NUMBER) {
    girara_error("equal mode: unknown mode %d", argument->n);
    return false;
  }

  const unsigned int npag         = zathura_document_get_number_of_pages(zathura->document);
  const unsigned int current_page = zathura_document_get_current_page_number(zathura->document);

  zathura_page_t* c_page = zathura_document_get_page(zathura->document, current_page);
  zathura_page_set_zoom(c_page, 1.0);

  girara_debug("Setting page equal mode to: %d", argument->n);

  if (argument->n == ZATHURA_EQUAL_NONE) {
    /* reset page zooms */
    for (unsigned int i = 0; i < npag; i++) {
      zathura_page_t* p = zathura_document_get_page(zathura->document, i);
      zathura_page_set_zoom(p, 1.0);
    }

    zathura_document_widget_render_all(zathura->ui.document_widget);
    refresh_view(zathura);
    return true;
  }

  for (unsigned int i = 0; i != npag; i++) {
    zathura_page_t* page_i = zathura_document_get_page(zathura->document, i);

    zathura_page_set_zoom(page_i, 1.0);

    switch (argument->n) {
    case ZATHURA_EQUAL_WIDTH:
      zathura_page_set_zoom(page_i, zathura_page_get_width(c_page) / zathura_page_get_width(page_i));
      break;
    case ZATHURA_EQUAL_HEIGHT:
      zathura_page_set_zoom(page_i, zathura_page_get_height(c_page) / zathura_page_get_height(page_i));
      break;
    }
  }

  zathura_document_widget_render_all(zathura->ui.document_widget);
  refresh_view(zathura);

  return true;
}

bool sc_focus_inputbar(girara_session_t* session, girara_argument_t* argument, girara_event_t* UNUSED(event),
                       unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->gtk.inputbar_entry != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);

  zathura_document_set_adjust_mode(zathura->document, ZATHURA_ADJUST_INPUTBAR);

  if (gtk_widget_get_visible(GTK_WIDGET(session->gtk.inputbar)) == false) {
    gtk_widget_set_visible(GTK_WIDGET(session->gtk.inputbar), TRUE);
  }

  if (gtk_widget_get_visible(GTK_WIDGET(session->gtk.notification_area)) == true) {
    gtk_widget_set_visible(GTK_WIDGET(session->gtk.notification_area), FALSE);
  }

  gtk_widget_grab_focus(GTK_WIDGET(session->gtk.inputbar_entry));

  if (argument->data != NULL) {
    gtk_editable_set_text(GTK_EDITABLE(session->gtk.inputbar_entry), (char*)argument->data);

    /* append filepath */
    if (argument->n == APPEND_FILEPATH && zathura->document != NULL) {
      const char* file_path = zathura_document_get_path(zathura->document);
      if (file_path == NULL) {
        return false;
      }

      g_autofree char* path    = g_path_get_dirname(file_path);
      g_autofree char* escaped = girara_escape_string(path);
      g_autofree char* tmp =
          g_strdup_printf("%s%s/", (char*)argument->data, (g_strcmp0(path, "/") == 0) ? "" : escaped);

      gtk_editable_set_text(GTK_EDITABLE(session->gtk.inputbar_entry), tmp);
    }

    gtk_editable_set_position(GTK_EDITABLE(session->gtk.inputbar_entry), -1);
  }

  return true;
}

bool sc_follow(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event),
               unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  return link_shortcuts(zathura, cb_sc_follow, "Follow Link: ");
}

bool sc_goto(girara_session_t* session, girara_argument_t* argument, girara_event_t* UNUSED(event), unsigned int t) {
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

bool sc_mouse_scroll(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t) {
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
    x_adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(zathura->ui.view));
    y_adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zathura->ui.view));

    if (x_adj == NULL || y_adj == NULL) {
      return false;
    }

    zathura_adjustment_set_value(x_adj, gtk_adjustment_get_value(x_adj) - (event->x - zathura->shortcut.mouse.x));
    zathura_adjustment_set_value(y_adj, gtk_adjustment_get_value(y_adj) - (event->y - zathura->shortcut.mouse.y));
    /* save the current cursor position so the next motion event measures only the new movement */
    zathura->shortcut.mouse.x = event->x;
    zathura->shortcut.mouse.y = event->y;
    break;

    /* unhandled events */
  default:
    break;
  }

  return false;
}

bool sc_mouse_zoom(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t) {
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

bool sc_navigate(girara_session_t* session, girara_argument_t* argument, girara_event_t* UNUSED(event),
                 unsigned int t) {
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

  unsigned int offset = 1;
  if (columns_per_row_offset == true) {
    girara_setting_get(session, "pages-per-row", &offset);
  }

  t = (t == 0) ? offset : t;
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

bool sc_print(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event),
              unsigned int UNUSED(t)) {
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

bool sc_recolor(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event),
                unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);

  bool value = false;
  girara_setting_get(session, "recolor", &value);
  value = !value;
  girara_setting_set(session, "recolor", &value);

  return false;
}

bool sc_reload(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event),
               unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  if (zathura->file_monitor.monitor == NULL) {
    return false;
  }

  /* Get file info (zoom, current page, etc.) */
  zathura_fileinfo_t file_info;
  if (zathura->document == NULL && zathura->predecessor_document != NULL) {
    /* Try to get the info from the predecessor document if the current does not exist */
    file_info = zathura_get_prefileinfo(zathura);
  } else {
    file_info = zathura_get_fileinfo(zathura);
  }

  /* close current document */
  document_close(zathura, true);

  /* reopen document with old file info */
  document_open(zathura, zathura_filemonitor_get_filepath(zathura->file_monitor.monitor), NULL,
                zathura->file_monitor.password, file_info.current_page, &file_info);

  return false;
}

bool sc_rotate(girara_session_t* session, girara_argument_t* argument, girara_event_t* UNUSED(event), unsigned int t) {
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
  t                     = (t == 0) ? 1 : t;
  unsigned int rotation = zathura_document_get_rotation(zathura->document);
  zathura_document_set_rotation(zathura->document, (rotation + angle * t) % 360);

  /* update scale */
  girara_argument_t new_argument = {.n = zathura_document_get_adjust_mode(zathura->document), .data = NULL};
  sc_adjust_window(zathura->ui.session, &new_argument, NULL, 0);

  /* render all pages again */
  zathura_document_widget_render_all(zathura->ui.document_widget);

  page_set(zathura, page_number);

  return false;
}

/* full page scrolling in single page mode steps the current page since the adjustment spans one page */
static bool scroll_single_page_full(zathura_t* zathura, bool down) {
  GtkAdjustment* vadj    = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zathura->ui.view));
  const double page_size = gtk_adjustment_get_page_size(vadj);
  const double lower     = gtk_adjustment_get_lower(vadj);
  const double upper     = gtk_adjustment_get_upper(vadj);
  const double value     = gtk_adjustment_get_value(vadj);
  const double maxvalue  = upper - page_size;

  float overlap = 0.0;
  girara_setting_get(zathura->ui.session, "scroll-full-overlap", &overlap);
  const double step = (1.0 - overlap) * page_size;

  zathura_document_t* document = zathura_get_document(zathura);
  const unsigned int page      = zathura_document_get_current_page_number(document);
  const unsigned int npag      = zathura_document_get_number_of_pages(document);

  if (down == true) {
    if (value < maxvalue - 1.0) {
      gtk_adjustment_set_value(vadj, MIN(value + step, maxvalue));
    } else if (page + 1 < npag) {
      page_set(zathura, page + 1);
    }
  } else {
    if (value > lower + 1.0) {
      gtk_adjustment_set_value(vadj, MAX(value - step, lower));
    } else if (page > 0) {
      page_set(zathura, page - 1);
    }
  }
  return false;
}

bool sc_scroll(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  if (zathura->document == NULL) {
    return false;
  }

  /* in single page mode the adjustment spans one page so route a full page scroll through the page */
  if (argument->n == FULL_DOWN || argument->n == FULL_UP) {
    int layout_mode = DOCUMENT_WIDGET_GRID;
    g_object_get(zathura->ui.document_widget, "layout-mode", &layout_mode, NULL);
    if (layout_mode == DOCUMENT_WIDGET_SINGLE) {
      return scroll_single_page_full(zathura, argument->n == FULL_DOWN);
    }
  }

  /* if TOP or BOTTOM, go there and we are done */
  if (argument->n == TOP) {
    zathura_jumplist_add(zathura);
    position_set(zathura, -1, 0);
    zathura_jumplist_add(zathura);
    return false;
  } else if (argument->n == BOTTOM) {
    zathura_jumplist_add(zathura);
    position_set(zathura, -1, 1.0);
    zathura_jumplist_add(zathura);
    return false;
  }

  /* Retrieve current page and position */
  const unsigned int page_id = zathura_document_get_current_page_number(zathura->document);
  double pos_x               = zathura_document_get_position_x(zathura->document);
  double pos_y               = zathura_document_get_position_y(zathura->document);

  /* If PAGE_TOP or PAGE_BOTTOM, go there and we are done */
  if (argument->n == PAGE_TOP) {
    double dontcare = 0.5;
    page_number_to_position(zathura, page_id, dontcare, 0.0, &dontcare, &pos_y);
    position_set(zathura, pos_x, pos_y);
    return false;
  } else if (argument->n == PAGE_BOTTOM) {
    double dontcare = 0.5;
    page_number_to_position(zathura, page_id, dontcare, 1.0, &dontcare, &pos_y);
    position_set(zathura, pos_x, pos_y);
    return false;
  }

  /* If SMOOTH_(UP|DOWN) , use GtkScrolledWindow signal */
  if (argument->n == SMOOTH_UP) {
    gboolean handled = FALSE;
    g_signal_emit_by_name(G_OBJECT(zathura->ui.view), "scroll-child", GTK_SCROLL_STEP_BACKWARD, FALSE, &handled);

    return false;
  } else if (argument->n == SMOOTH_DOWN) {
    gboolean handled = FALSE;
    g_signal_emit_by_name(G_OBJECT(zathura->ui.view), "scroll-child", GTK_SCROLL_STEP_FORWARD, FALSE, &handled);

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
  zathura_document_widget_get_document_size(ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget), &doc_height,
                                            &doc_width);

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
  if ((argument->n == LEFT) || (argument->n == FULL_LEFT) || (argument->n == HALF_LEFT) || (argument->n == UP) ||
      (argument->n == FULL_UP) || (argument->n == HALF_UP) || (argument->n == PARTIAL_UP)) {
    direction = -1.0;
  }

  const unsigned int v_padding =
      zathura_document_widget_get_page_v_padding(ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget));
  const unsigned int h_padding =
      zathura_document_widget_get_page_h_padding(ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget));

  const double vstep = (double)(view_height + v_padding) / (double)doc_height;
  const double hstep = (double)(view_width + h_padding) / (double)doc_width;

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

  case PARTIAL_UP:
  case PARTIAL_DOWN:
    pos_y += direction * t * (scroll_step * 0.2) / (double)doc_height;
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

  /* NOTE: the following `+ DBL_EPSILON` is added to avoid rounding errors, which can result in
   *       unwanted changes of page when positioned precisely in the middle of a dual-pane layout */
  if (pos_x < end_x + DBL_EPSILON) {
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
  const unsigned int new_page_id = position_to_page_number(zathura, pos_x, pos_y);
  if (scroll_page_aware == true && page_id != new_page_id) {
    double dummy = 0.0;
    switch (argument->n) {
    case FULL_LEFT:
    case HALF_LEFT:
      page_number_to_position(zathura, new_page_id, 1.0, 0.0, &pos_x, &dummy);
      break;

    case FULL_RIGHT:
    case HALF_RIGHT:
      page_number_to_position(zathura, new_page_id, 0.0, 0.0, &pos_x, &dummy);
      break;

    case FULL_UP:
    case HALF_UP:
    case PARTIAL_UP:
      page_number_to_position(zathura, new_page_id, 0.0, 1.0, &dummy, &pos_y);
      break;

    case FULL_DOWN:
    case HALF_DOWN:
    case PARTIAL_DOWN:
      page_number_to_position(zathura, new_page_id, 0.0, 0.0, &dummy, &pos_y);
      break;
    }
  }

  position_set(zathura, pos_x, pos_y);
  return false;
}

bool sc_jumplist(girara_session_t* session, girara_argument_t* argument, girara_event_t* UNUSED(event),
                 unsigned int t) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(zathura->document != NULL, false);

  /* if no jumps in the jumplist */
  if (zathura->jumplist.size == 0) {
    return true;
  }

  zathura_jump_t* jump         = NULL;
  zathura_jump_t* current_jump = zathura_jumplist_current(zathura);

  switch (argument->n) {
  case FORWARD:
    for (int n = (t == 0 ? 1 : t); n > 0; n--) {
      zathura_jumplist_forward(zathura);
    }
    break;
  case BACKWARD:
    for (int n = (t == 0 ? 1 : t); n > 0; n--) {
      zathura_jumplist_backward(zathura);
    }
    break;
  }
  jump = zathura_jumplist_current(zathura);

  if (jump != current_jump) {
    page_set(zathura, jump->page);
    position_set(zathura, jump->x, jump->y);
  }

  return false;
}

bool sc_bisect(girara_session_t* session, girara_argument_t* argument, girara_event_t* UNUSED(event), unsigned int t) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(zathura->document != NULL, false);

  const unsigned int num_pages = zathura_document_get_number_of_pages(zathura->document);
  const unsigned int cur_page  = zathura_document_get_current_page_number(zathura->document);

  /* process arguments */
  int direction;
  if (t > 0 && t <= num_pages) {
    /* bisect between cur_page and t */
    t -= 1;
    if (t == cur_page) {
      /* nothing to do */
      return false;
    } else if (t > cur_page) {
      zathura->bisect.start = cur_page;
      zathura->bisect.end   = t;
      direction             = BACKWARD;
    } else {
      zathura->bisect.start = t;
      zathura->bisect.end   = cur_page;
      direction             = FORWARD;
    }
  } else {
    direction = argument->n;

    /* setup initial bisect range */
    zathura_jump_t* jump = zathura_jumplist_current(zathura);
    if (jump == NULL) {
      girara_debug("bisecting between first and last page because there are no jumps");
      zathura->bisect.start = 0;
      zathura->bisect.end   = num_pages - 1;
    } else if (jump->page != cur_page || jump->page != zathura->bisect.last_jump) {
      girara_debug("last jump doesn't match up, starting new bisecting");
      zathura->bisect.start = 0;
      zathura->bisect.end   = num_pages - 1;

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

      zathura->bisect.start     = MIN(prev_page, cur_page);
      zathura->bisect.end       = MAX(prev_page, cur_page);
      zathura->bisect.last_jump = cur_page;
    }
  }

  girara_debug("bisecting between %d and %d, at %d", zathura->bisect.start, zathura->bisect.end, cur_page);
  if (zathura->bisect.start == zathura->bisect.end) {
    /* nothing to do */
    return false;
  }

  unsigned int next_page  = cur_page;
  unsigned int next_start = zathura->bisect.start;
  unsigned int next_end   = zathura->bisect.end;

  /* here we have next_start <= next_page <= next_end */

  /* bisect step */
  switch (direction) {
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
  zathura->bisect.start     = next_start;
  zathura->bisect.end       = next_end;

  zathura_jumplist_add(zathura);
  page_set(zathura, next_page);
  zathura_jumplist_add(zathura);

  return false;
}

bool sc_search(girara_session_t* session, girara_argument_t* argument, girara_event_t* UNUSED(event),
               unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  return search_document(zathura, argument, false);
}

/* helper: get the current row index from the column view selection */
/* expand a tree-list row and recursively expand all its descendants by iterating the flat model */
static void index_expand_subtree(GListModel* flat_model, GtkTreeListRow* anchor) {
  if (anchor == NULL) {
    return;
  }
  gtk_tree_list_row_set_expanded(anchor, TRUE);
  /* fix-point: repeatedly scan and expand any descendant of anchor that is collapsed.
     each set_expanded inserts new rows into the flat model so we restart the scan. */
  guint anchor_pos = gtk_tree_list_row_get_position(anchor);
  gboolean changed;
  do {
    changed = FALSE;
    guint n = g_list_model_get_n_items(flat_model);
    for (guint i = anchor_pos + 1; i < n; i++) {
      g_autoptr(GtkTreeListRow) r = g_list_model_get_item(flat_model, i);
      if (r == NULL) {
        continue;
      }
      /* is r a descendant of anchor? walk up its parent chain */
      gboolean is_descendant        = FALSE;
      g_autoptr(GtkTreeListRow) cur = g_object_ref(r);
      while (TRUE) {
        g_autoptr(GtkTreeListRow) parent = gtk_tree_list_row_get_parent(cur);
        if (parent == NULL) {
          break;
        }
        if (parent == anchor) {
          is_descendant = TRUE;
          break;
        }
        g_set_object(&cur, parent);
      }
      if (!is_descendant) {
        continue;
      }
      if (gtk_tree_list_row_is_expandable(r) && !gtk_tree_list_row_get_expanded(r)) {
        gtk_tree_list_row_set_expanded(r, TRUE);
        changed = TRUE;
        break; /* restart scan since model size changed */
      }
    }
  } while (changed);
}

static guint index_get_position(GtkListView* view) {
  GtkSelectionModel* sel = gtk_list_view_get_model(view);
  GtkBitset* selected    = gtk_selection_model_get_selection(sel);
  guint pos              = 0;
  if (!gtk_bitset_is_empty(selected)) {
    pos = gtk_bitset_get_minimum(selected);
  }
  gtk_bitset_unref(selected);
  return pos;
}

static void index_select(GtkListView* view, guint pos) {
  GtkSelectionModel* sel = gtk_list_view_get_model(view);
  gtk_selection_model_select_item(sel, pos, TRUE);
  gtk_list_view_scroll_to(view, pos, GTK_LIST_SCROLL_FOCUS | GTK_LIST_SCROLL_SELECT, NULL);
}

bool sc_navigate_index(girara_session_t* session, girara_argument_t* argument, girara_event_t* UNUSED(event),
                       unsigned int t) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(zathura->document != NULL, false);

  if (zathura->ui.index == NULL) {
    return false;
  }

  GtkListView* view            = GTK_LIST_VIEW(gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(zathura->ui.index)));
  GtkSelectionModel* selection = gtk_list_view_get_model(view);
  GListModel* model            = G_LIST_MODEL(selection);
  const guint n_items          = g_list_model_get_n_items(model);
  if (n_items == 0) {
    return false;
  }

  guint pos                     = index_get_position(view);
  g_autoptr(GtkTreeListRow) row = g_list_model_get_item(model, pos);

  switch (argument->n) {
  case TOP:
    pos = 0;
    break;
  case BOTTOM:
    pos = n_items - 1;
    break;
  case UP:
    for (int n = (t == 0 ? 1 : t); n > 0 && pos > 0; n--) {
      pos--;
    }
    break;
  case DOWN:
    for (int n = (t == 0 ? 1 : t); n > 0 && pos + 1 < n_items; n--) {
      pos++;
    }
    break;
  case HALF_UP:
  case PARTIAL_UP:
  case HALF_DOWN:
  case PARTIAL_DOWN: {
    /* compute number of rows currently in viewport from the vertical adjustment */
    GtkAdjustment* vadj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(view));
    double page_size    = vadj != NULL ? gtk_adjustment_get_page_size(vadj) : 0.0;
    double upper        = vadj != NULL ? gtk_adjustment_get_upper(vadj) : 0.0;
    guint step          = (upper > 0.0) ? (guint)((page_size / upper) * (double)n_items) : 1;
    if (step == 0) {
      step = 1;
    }
    if (argument->n == HALF_UP || argument->n == PARTIAL_UP) {
      pos = pos > step ? pos - step : 0;
    } else {
      pos = pos + step < n_items ? pos + step : n_items - 1;
    }
    break;
  }
  case EXPAND:
    if (row != NULL && gtk_tree_list_row_is_expandable(row)) {
      if (!gtk_tree_list_row_get_expanded(row)) {
        gtk_tree_list_row_set_expanded(row, TRUE);
      }
      /* move cursor to the first child */
      if (pos + 1 < g_list_model_get_n_items(model)) {
        pos++;
      }
    }
    break;
  case EXPAND_RECURSIVE:
    if (row != NULL && gtk_tree_list_row_is_expandable(row)) {
      index_expand_subtree(model, row);
      if (pos + 1 < g_list_model_get_n_items(model)) {
        pos++;
      }
    }
    break;
  case EXPAND_ALL: {
    /* fix-point expansion of every expandable row in the model */
    gboolean changed;
    do {
      changed = FALSE;
      guint n = g_list_model_get_n_items(model);
      for (guint i = 0; i < n; i++) {
        g_autoptr(GtkTreeListRow) r = g_list_model_get_item(model, i);
        if (r != NULL && gtk_tree_list_row_is_expandable(r) && !gtk_tree_list_row_get_expanded(r)) {
          gtk_tree_list_row_set_expanded(r, TRUE);
          changed = TRUE;
          break;
        }
      }
    } while (changed);
    pos = 0;
    break;
  }
  case COLLAPSE:
    if (row != NULL) {
      if (gtk_tree_list_row_get_expanded(row)) {
        gtk_tree_list_row_set_expanded(row, FALSE);
      } else {
        g_autoptr(GtkTreeListRow) parent = gtk_tree_list_row_get_parent(row);
        if (parent != NULL) {
          gtk_tree_list_row_set_expanded(parent, FALSE);
          pos = gtk_tree_list_row_get_position(parent);
        }
      }
    }
    break;
  case COLLAPSE_RECURSIVE:
    if (row != NULL) {
      g_autoptr(GtkTreeListRow) walk = g_object_ref(row);
      while (TRUE) {
        g_autoptr(GtkTreeListRow) parent = gtk_tree_list_row_get_parent(walk);
        if (parent == NULL) {
          break;
        }
        g_set_object(&walk, parent);
      }
      gtk_tree_list_row_set_expanded(walk, FALSE);
      pos = gtk_tree_list_row_get_position(walk);
    }
    break;
  case COLLAPSE_ALL: {
    /* walk current row up to its top-level ancestor */
    if (row != NULL) {
      g_autoptr(GtkTreeListRow) walk = g_object_ref(row);
      while (TRUE) {
        g_autoptr(GtkTreeListRow) parent = gtk_tree_list_row_get_parent(walk);
        if (parent == NULL) {
          break;
        }
        g_set_object(&walk, parent);
      }
      pos = gtk_tree_list_row_get_position(walk);
    }
    /* collapsing a row hides all its descendants in the flat model, so collapsing
       every depth-0 row is sufficient to hide all descendants */
    guint n = g_list_model_get_n_items(model);
    for (guint i = 0; i < n; i++) {
      g_autoptr(GtkTreeListRow) r = g_list_model_get_item(model, i);
      if (r != NULL && gtk_tree_list_row_get_depth(r) == 0 && gtk_tree_list_row_get_expanded(r)) {
        gtk_tree_list_row_set_expanded(r, FALSE);
      }
    }
    break;
  }
  case TOGGLE:
    if (row != NULL && gtk_tree_list_row_is_expandable(row)) {
      gtk_tree_list_row_set_expanded(row, !gtk_tree_list_row_get_expanded(row));
    }
    break;
  case SELECT:
    cb_index_row_activated(view, pos, zathura);
    return false;
  }

  index_select(view, pos);
  return false;
}

/* factory: build a complete index row (expander with title, page target, alt page) */
static void index_row_setup(GtkSignalListItemFactory* UNUSED(factory), GObject* listitem, gpointer UNUSED(data)) {
  GtkWidget* box      = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  GtkWidget* expander = gtk_tree_expander_new();
  GtkWidget* title    = gtk_label_new(NULL);
  gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
  gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
  gtk_label_set_use_markup(GTK_LABEL(title), TRUE);
  gtk_tree_expander_set_child(GTK_TREE_EXPANDER(expander), title);
  gtk_widget_set_hexpand(expander, TRUE);

  GtkWidget* page = gtk_label_new(NULL);
  gtk_label_set_xalign(GTK_LABEL(page), 1.0f);

  GtkWidget* alt = gtk_label_new(NULL);
  gtk_label_set_xalign(GTK_LABEL(alt), 0.0f);

  gtk_box_append(GTK_BOX(box), expander);
  gtk_box_append(GTK_BOX(box), page);
  gtk_box_append(GTK_BOX(box), alt);
  gtk_list_item_set_child(GTK_LIST_ITEM(listitem), box);
}

static void index_row_bind(GtkSignalListItemFactory* UNUSED(factory), GObject* listitem, gpointer UNUSED(data)) {
  GtkWidget* box                = gtk_list_item_get_child(GTK_LIST_ITEM(listitem));
  GtkWidget* expander           = gtk_widget_get_first_child(box);
  GtkWidget* page               = gtk_widget_get_next_sibling(expander);
  GtkWidget* alt                = gtk_widget_get_next_sibling(page);
  GtkTreeListRow* row           = gtk_list_item_get_item(GTK_LIST_ITEM(listitem));
  ZathuraIndexElementObject* it = gtk_tree_list_row_get_item(row);

  gtk_tree_expander_set_list_row(GTK_TREE_EXPANDER(expander), row);
  gtk_label_set_markup(GTK_LABEL(gtk_tree_expander_get_child(GTK_TREE_EXPANDER(expander))), it->title);
  gtk_label_set_text(GTK_LABEL(page), it->page_label ? it->page_label : "");
  gtk_label_set_text(GTK_LABEL(alt), it->page_alt ? it->page_alt : "");
  g_object_unref(it);
}

/* GtkTreeListModelCreateModelFunc */
static GListModel* index_create_child_model(gpointer item, gpointer UNUSED(user_data)) {
  ZathuraIndexElementObject* obj = item;
  if (obj->children == NULL) {
    return NULL;
  }
  return G_LIST_MODEL(g_object_ref(obj->children));
}

static void cb_index_activate(GtkListView* view, guint position, gpointer user_data) {
  cb_index_row_activated(view, position, user_data);
}

bool sc_toggle_index(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event),
                     unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  if (zathura->document == NULL) {
    return false;
  }

  if (zathura->ui.index == NULL) {
    /* create new index widget */
    zathura->ui.index = gtk_scrolled_window_new();
    if (zathura->ui.index == NULL) {
      goto error_ret;
    }
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(zathura->ui.index), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    g_autoptr(girara_tree_node_t) document_index = zathura_document_index_generate(zathura->document, NULL);
    if (document_index == NULL) {
      girara_notify(session, GIRARA_WARNING, _("This document does not contain any index"));
      goto error_free;
    }

    GListModel* root            = document_index_build_model(session, document_index);
    GtkTreeListModel* tree      = gtk_tree_list_model_new(root, FALSE, FALSE, index_create_child_model, NULL, NULL);
    GtkSingleSelection* sel     = gtk_single_selection_new(G_LIST_MODEL(tree));
    GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(index_row_setup), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(index_row_bind), NULL);
    GtkListView* view = GTK_LIST_VIEW(gtk_list_view_new(GTK_SELECTION_MODEL(sel), factory));

    gtk_widget_add_css_class(GTK_WIDGET(view), "indexmode");

    g_signal_connect(view, "activate", G_CALLBACK(cb_index_activate), zathura);

    /* the tree list model, selection, and list view consume their model/factory references */

    gtk_widget_set_visible(GTK_WIDGET(view), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(zathura->ui.index), GTK_WIDGET(view));
  }

  if (girara_mode_get(session) == zathura->modes.index) {
    girara_set_view(zathura->ui.session, zathura->ui.view);
    girara_mode_set(zathura->ui.session, zathura->modes.normal);
    refresh_view(zathura);
  } else {
    zathura_jumplist_add(zathura);

    const zathura_adjust_mode_t adjust_mode = zathura_document_get_adjust_mode(zathura->document);
    if (adjust_mode == ZATHURA_ADJUST_INPUTBAR) {
      zathura_document_set_adjust_mode(zathura->document, ZATHURA_ADJUST_NONE);
    }

    girara_set_view(session, zathura->ui.index);
    index_scroll_to_current_page(zathura);
    girara_mode_set(zathura->ui.session, zathura->modes.index);
  }

  return false;

error_free:
  if (zathura->ui.index != NULL) {
    GtkWidget* index_parent = gtk_widget_get_parent(zathura->ui.index);
    if (GTK_IS_STACK(index_parent)) {
      gtk_stack_remove(GTK_STACK(index_parent), zathura->ui.index);
    } else {
      g_object_ref_sink(zathura->ui.index);
      g_object_unref(zathura->ui.index);
    }
    zathura->ui.index = NULL;
  }
error_ret:
  return false;
}

bool sc_toggle_page_mode(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event),
                         unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  if (zathura->document == NULL) {
    girara_notify(session, GIRARA_WARNING, _("No document opened."));
    return false;
  }

  unsigned int page_id = zathura_document_get_current_page_number(zathura->document);

  unsigned int pages_per_row = 1;
  girara_setting_get(zathura->ui.session, "pages-per-row", &pages_per_row);

  unsigned int value = 1;
  if (pages_per_row == 1) {
    value = zathura->shortcut.toggle_page_mode.pages;
  } else {
    zathura->shortcut.toggle_page_mode.pages = pages_per_row;
  }

  girara_setting_set(zathura->ui.session, "pages-per-row", &value);
  adjust_view(zathura);

  page_set(zathura, page_id);
  zathura_document_widget_render_all(zathura->ui.document_widget);
  refresh_view(zathura);

  return true;
}

bool sc_toggle_fullscreen(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event),
                          unsigned int UNUSED(t)) {
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

bool sc_toggle_presentation(girara_session_t* session, girara_argument_t* UNUSED(argument),
                            girara_event_t* UNUSED(event), unsigned int UNUSED(t)) {
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

    // reset layout mode
    if (zathura->shortcut.toggle_presentation_mode.layout_mode != DOCUMENT_WIDGET_SINGLE) {
      g_object_set(zathura->ui.document_widget, "layout-mode", zathura->shortcut.toggle_presentation_mode.layout_mode,
                   NULL);
    }

    /* reset first page column */
    if (zathura->shortcut.toggle_presentation_mode.first_page_column_list != NULL) {
      girara_setting_set(session, "first-page-column",
                         zathura->shortcut.toggle_presentation_mode.first_page_column_list);
    }

    /* show status bar if it was enabled */
    if (zathura->shortcut.toggle_presentation_mode.is_status_bar_visible) {
      gtk_widget_set_visible(GTK_WIDGET(session->gtk.statusbar), TRUE);
    }
    /* show input bar if if was enabled */
    if (zathura->shortcut.toggle_presentation_mode.is_input_bar_visible) {
      gtk_widget_set_visible(GTK_WIDGET(session->gtk.inputbar), TRUE);
    }

    /* set full screen */
    gtk_window_unfullscreen(GTK_WINDOW(session->gtk.window));

    /* reset zoom */
    zathura_document_set_zoom(zathura->document, zathura->shortcut.toggle_presentation_mode.zoom);
    zathura_document_widget_render_all(zathura->ui.document_widget);
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
    girara_setting_get(session, "first-page-column",
                       &zathura->shortcut.toggle_presentation_mode.first_page_column_list);

    /* back up zoom */
    zathura->shortcut.toggle_presentation_mode.zoom = zathura_document_get_zoom(zathura->document);

    // backup layout mode
    g_object_get(zathura->ui.document_widget, "layout-mode", &zathura->shortcut.toggle_presentation_mode.layout_mode,
                 NULL);

    /* set single view */
    if (zathura->shortcut.toggle_presentation_mode.layout_mode != DOCUMENT_WIDGET_SINGLE) {
      g_object_set(zathura->ui.document_widget, "layout-mode", DOCUMENT_WIDGET_SINGLE, NULL);
    }

    /* the gtk4 grid does not honor single-page layout yet, so force one column for a usable presentation */
    const unsigned int presentation_pages_per_row = 1;
    girara_setting_set(session, "pages-per-row", &presentation_pages_per_row);

    /* adjust window */
    girara_argument_t argument = {.n = ZATHURA_ADJUST_BESTFIT, .data = NULL};
    sc_adjust_window(session, &argument, NULL, 0);

    zathura->shortcut.toggle_presentation_mode.is_status_bar_visible =
        gtk_widget_get_visible(GTK_WIDGET(session->gtk.statusbar));
    zathura->shortcut.toggle_presentation_mode.is_input_bar_visible =
        gtk_widget_get_visible(GTK_WIDGET(session->gtk.inputbar));

    /* hide status and inputbar */
    gtk_widget_set_visible(GTK_WIDGET(session->gtk.inputbar), FALSE);
    gtk_widget_set_visible(GTK_WIDGET(session->gtk.statusbar), FALSE);

    /* set full screen */
    gtk_window_fullscreen(GTK_WINDOW(session->gtk.window));
    refresh_view(zathura);

    /* set mode */
    girara_mode_set(session, zathura->modes.presentation);
  }

  return false;
}

bool sc_toggle_single_page_mode(girara_session_t* session, girara_argument_t* UNUSED(argument),
                                girara_event_t* UNUSED(event), unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  if (zathura->document == NULL) {
    girara_notify(session, GIRARA_WARNING, _("No document opened."));
    return false;
  }

  document_widget_mode_t old_mode;
  g_object_get(zathura->ui.document_widget, "layout-mode", &old_mode, NULL);
  if (old_mode == DOCUMENT_WIDGET_SINGLE) {
    g_object_set(zathura->ui.document_widget, "layout-mode", DOCUMENT_WIDGET_GRID, NULL);
  } else {
    const unsigned int pages_per_row = 1;
    girara_setting_set(zathura->ui.session, "pages-per-row", &pages_per_row);
    g_object_set(zathura->ui.document_widget, "layout-mode", DOCUMENT_WIDGET_SINGLE, NULL);
  }

  return true;
}

bool sc_quit(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event),
             unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  girara_argument_t arg = {.n = GIRARA_HIDE, .data = NULL};
  girara_isc_completion(session, &arg, NULL, 0);

  cb_destroy(NULL, zathura);

  return false;
}

bool sc_zoom(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(zathura->document != NULL, false);

  zathura_document_set_adjust_mode(zathura->document, ZATHURA_ADJUST_NONE);

  /* retrieve zoom step value */
  unsigned int value = 1;
  girara_setting_get(zathura->ui.session, "zoom-step", &value);

  const int nt           = (t == 0) ? 1 : t;
  const double zoom_step = MAX(DBL_EPSILON, 1.0 + value / 100.0 * nt);
  const double old_zoom  = zathura_document_get_zoom(zathura->document);

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
    const double z  = pow(zoom_step, -dy);
    girara_debug("Increasing zoom by %0.2f.", z - 1.0);
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
  zathura_document_widget_render_all(zathura->ui.document_widget);
  refresh_view(zathura);

  return false;
}

static bool sc_exec_internal(girara_session_t* session, girara_argument_t* argument) {
  if (argument == NULL || argument->data == NULL) {
    return false;
  }

  /* create argument list */
  g_autoptr(girara_list_t) argument_list = argument_to_argument_list(argument);
  if (argument_list == NULL) {
    return false;
  }

  /* call exec */
  cmd_exec(session, argument_list);

  return false;
}

bool sc_exec(girara_session_t* session, girara_argument_t* argument, girara_event_t* UNUSED(event),
             unsigned int UNUSED(t)) {
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
    const char* path  = zathura_document_get_path(zathura->document);
    unsigned int page = zathura_document_get_current_page_number(zathura->document);
    char page_buf[G_ASCII_DTOSTR_BUF_SIZE];
    g_ascii_dtostr(page_buf, G_ASCII_DTOSTR_BUF_SIZE, page + 1);

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

  const bool ret = sc_exec_internal(session, &new_argument);
  g_free(new_argument.data);
  return ret;
}

bool sc_zoom_page(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(zathura->document != NULL, false);

  zathura_document_set_adjust_mode(zathura->document, ZATHURA_ADJUST_NONE);

  /* retrieve zoom step value */
  unsigned int value = 1;
  girara_setting_get(zathura->ui.session, "zoom-step", &value);

  unsigned int current_page = zathura_document_get_current_page_number(zathura->document);
  zathura_page_t* page      = zathura_document_get_page(zathura->document, current_page);

  const int nt           = (t == 0) ? 1 : t;
  const double zoom_step = MAX(DBL_EPSILON, 1.0 + value / 100.0 * nt);
  const double old_zoom  = zathura_page_get_zoom(page);

  /* specify new zoom value */
  if (argument->n == ZOOM_IN) {
    girara_debug("Increasing page %d zoom by %0.2f.", current_page, zoom_step - 1.0);
    zathura_page_set_zoom(page, old_zoom * zoom_step);
  } else if (argument->n == ZOOM_OUT) {
    girara_debug("Decreasing page %d zoom by %0.2f.", current_page, zoom_step - 1.0);
    zathura_page_set_zoom(page, old_zoom / zoom_step);
  } else if (argument->n == ZOOM_SPECIFIC) {
    if (t == 0) {
      girara_debug("Setting page %d zoom to 1.", current_page);
      zathura_page_set_zoom(page, 1.0);
    } else {
      girara_debug("Setting page %d zoom to %0.2f.", current_page, t / 100.0);
      zathura_page_set_zoom(page, t / 100.0);
    }
  } else if (argument->n == ZOOM_SMOOTH) {
    const double dy = (event != NULL) ? event->y : 1.0;
    const double z  = pow(zoom_step, -dy);
    girara_debug("Increasing page %d zoom by %0.2f.", current_page, z - 1.0);
    zathura_page_set_zoom(page, old_zoom * z);
  } else {
    girara_debug("Setting page %d zoom to 1.", current_page);
    zathura_page_set_zoom(page, 1.0);
  }

  /* zoom limitations */
  const double zoom = zathura_page_get_zoom(page);
  zathura_page_set_zoom(page, zathura_correct_zoom_value(session, zoom));

  const double new_zoom = zathura_page_get_zoom(page);
  if (fabs(new_zoom - old_zoom) <= DBL_EPSILON) {
    girara_debug("New and old page %d zoom level are too close: %0.2f vs. %0.2f", current_page, new_zoom, old_zoom);
    return false;
  }

  girara_debug("Re-rendering with page %d new zoom level %0.2f.", current_page, new_zoom);
  zathura_document_widget_render_all(zathura->ui.document_widget);
  refresh_view(zathura);

  return false;
}

bool sc_nohlsearch(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event),
                   unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  document_draw_search_results(zathura, false);
  zathura_document_widget_render_all(zathura->ui.document_widget);

  return false;
}

bool sc_snap_to_page(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event),
                     unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(zathura->document != NULL, false);
  zathura_document_t* document = zathura->document;

  int page = zathura_document_get_current_page_number(document);
  return page_set(zathura, page);
}

/* async callback invoked after the user picks a file or cancels the dialog */
static void cb_file_chooser_open(GObject* source, GAsyncResult* result, gpointer user_data) {
  GtkFileDialog* dialog   = GTK_FILE_DIALOG(source);
  zathura_t* zathura      = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file   = gtk_file_dialog_open_finish(dialog, result, &error);
  if (file == NULL) {
    /* user cancelled, do not warn */
    if (error != NULL && !g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED)) {
      girara_notify(zathura->ui.session, GIRARA_ERROR, "%s", error->message);
    }
    return;
  }

  g_autofree char* path = g_file_get_path(file);
  if (path == NULL) {
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("Could not get path from file."));
    return;
  }

  if (zathura_has_document(zathura) == true) {
    document_close(zathura, false);
  }
  document_open_idle(zathura, path, NULL, ZATHURA_PAGE_NUMBER_UNSPECIFIED, NULL, NULL, NULL, NULL);
}

bool sc_file_chooser(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event),
                     unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  g_autoptr(GtkFileDialog) dialog = gtk_file_dialog_new();
  gtk_file_dialog_set_title(dialog, _("Open file"));

  /* build a filter for all mime types supported by loaded plugins */
  zathura_plugin_manager_t* manager = zathura->plugins.manager;
  girara_list_t* types              = zathura_plugin_manager_get_content_types(manager);
  if (types != NULL && girara_list_size(types) > 0) {
    g_autoptr(GListStore) filters      = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_autoptr(GtkFileFilter) supported = gtk_file_filter_new();
    gtk_file_filter_set_name(supported, _("Supported documents"));
    for (size_t idx = 0; idx != girara_list_size(types); ++idx) {
      gtk_file_filter_add_mime_type(supported, girara_list_nth(types, idx));
    }
    g_list_store_append(filters, supported);

    g_autoptr(GtkFileFilter) all = gtk_file_filter_new();
    gtk_file_filter_set_name(all, _("All files"));
    gtk_file_filter_add_pattern(all, "*");
    g_list_store_append(filters, all);

    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    gtk_file_dialog_set_default_filter(dialog, supported);
  }

  /* seed the dialog with the folder of the currently open document */
  zathura_document_t* document = zathura_get_document(zathura);
  if (document != NULL) {
    const char* current_path = zathura_document_get_path(document);
    if (current_path != NULL) {
      g_autoptr(GFile) current = g_file_new_for_path(current_path);
      gtk_file_dialog_set_initial_file(dialog, current);
    }
  }

  GtkWindow* parent = GTK_WINDOW(session->gtk.window);
  gtk_file_dialog_open(dialog, parent, NULL, cb_file_chooser_open, zathura);
  return true;
}
