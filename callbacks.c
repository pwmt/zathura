/* See LICENSE file for license and copyright information */

#include <girara/statusbar.h>
#include <girara/session.h>
#include <girara/settings.h>
#include <girara/utils.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <glib/gi18n.h>

#include "callbacks.h"
#include "links.h"
#include "zathura.h"
#include "render.h"
#include "document.h"
#include "utils.h"
#include "shortcuts.h"
#include "page-widget.h"
#include "page.h"
#include "adjustment.h"

gboolean
cb_destroy(GtkWidget* UNUSED(widget), zathura_t* zathura)
{
  if (zathura != NULL && zathura->document != NULL) {
    document_close(zathura, false);
  }

  gtk_main_quit();
  return TRUE;
}

void
cb_buffer_changed(girara_session_t* session)
{
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);

  zathura_t* zathura = session->global.data;

  char* buffer = girara_buffer_get(session);
  if (buffer != NULL) {
    girara_statusbar_item_set_text(session, zathura->ui.statusbar.buffer, buffer);
    free(buffer);
  } else {
    girara_statusbar_item_set_text(session, zathura->ui.statusbar.buffer, "");
  }
}

void
cb_view_vadjustment_value_changed(GtkAdjustment* GIRARA_UNUSED(adjustment), gpointer data)
{
  zathura_t* zathura = data;
  if (zathura == NULL || zathura->document == NULL || zathura->ui.page_widget == NULL) {
    return;
  }

  GtkAdjustment* view_vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));
  GtkAdjustment* view_hadjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));

  /* current adjustment values */
  GdkRectangle view_rect = {
    .x      = 0,
    .y      = 0,
    .width  = gtk_adjustment_get_page_size(view_hadjustment),
    .height = gtk_adjustment_get_page_size(view_vadjustment)
  };

  int page_padding = 1;
  girara_setting_get(zathura->ui.session, "page-padding", &page_padding);

  GdkRectangle center = {
    .x      = (view_rect.width  + 1) / 2,
    .y      = (view_rect.height + 1) / 2,
    .width  = (2 * page_padding) + 1,
    .height = (2 * page_padding) + 1
  };

  unsigned int number_of_pages = zathura_document_get_number_of_pages(zathura->document);
  double scale = zathura_document_get_scale(zathura->document);

  bool updated = false;
  /* find page that fits */
  for (unsigned int page_id = 0; page_id < number_of_pages; page_id++) {
    zathura_page_t* page = zathura_document_get_page(zathura->document, page_id);

    GdkRectangle page_rect = {
      .width  = zathura_page_get_width(page)  * scale,
      .height = zathura_page_get_height(page) * scale
    };
    GtkWidget* page_widget = zathura_page_get_widget(zathura, page);
    gtk_widget_translate_coordinates(page_widget,
                                     zathura->ui.session->gtk.view, 0, 0, &page_rect.x, &page_rect.y);

    if (gdk_rectangle_intersect(&view_rect, &page_rect, NULL) == TRUE) {
      if (zathura_page_get_visibility(page) == false) {
        zathura_page_set_visibility(page, true);
        zathura_page_widget_update_view_time(ZATHURA_PAGE(page_widget));
        zathura_page_cache_add(zathura, zathura_page_get_index(page));
      }
      if (zathura->global.update_page_number == true && updated == false
          && gdk_rectangle_intersect(&center, &page_rect, NULL) == TRUE) {
        zathura_document_set_current_page_number(zathura->document, page_id);
        updated = true;
      }
    } else {
      zathura_page_set_visibility(page, false);
      /* if the page is not visible and not cached, but still has a surface, we
       * need to get rid of the surface */
      if (zathura_page_widget_have_surface(ZATHURA_PAGE(page_widget)) == true &&
          zathura_page_cache_is_cached(zathura, zathura_page_get_index(page)) == false) {
        zathura_page_widget_update_surface(ZATHURA_PAGE(page_widget), NULL);
      }

      girara_list_t* results = NULL;
      g_object_get(page_widget, "search-results", &results, NULL);

      if (results != NULL) {
        g_object_set(page_widget, "search-current", 0, NULL);
      }
    }
  }

  statusbar_page_number_update(zathura);
}

void
cb_view_hadjustment_changed(GtkAdjustment* adjustment, gpointer data)
{
  zathura_t* zathura = data;
  g_return_if_fail(zathura != NULL);

  zathura_adjust_mode_t adjust_mode =
    zathura_document_get_adjust_mode(zathura->document);

  gdouble lower, upper, page_size, value, ratio;
  bool zoom_center = false;

  switch (adjust_mode) {
    center:
    case ZATHURA_ADJUST_BESTFIT:
    case ZATHURA_ADJUST_WIDTH:
      lower = gtk_adjustment_get_lower(adjustment);
      upper = gtk_adjustment_get_upper(adjustment);
      page_size = gtk_adjustment_get_page_size(adjustment);
      value = ((upper - lower) - page_size) / 2.0;
      zathura_adjustment_set_value(adjustment, value);
      break;
    default:
      girara_setting_get(zathura->ui.session, "zoom-center", &zoom_center);
      if (zoom_center) {
        goto center;
      }

      ratio = zathura_adjustment_get_ratio(zathura->ui.hadjustment);
      zathura_adjustment_set_value_from_ratio(adjustment, ratio);
      break;
  }
}

void
cb_view_vadjustment_changed(GtkAdjustment* adjustment, gpointer data)
{
  zathura_t* zathura = data;
  g_return_if_fail(zathura != NULL);

  zathura_adjust_mode_t adjust_mode =
    zathura_document_get_adjust_mode(zathura->document);

  /* Don't scroll we're focusing the inputbar. */
  if (adjust_mode == ZATHURA_ADJUST_INPUTBAR) {
    return;
  }

  double ratio = zathura_adjustment_get_ratio(zathura->ui.vadjustment);
  zathura_adjustment_set_value_from_ratio(adjustment, ratio);
}

void
cb_adjustment_track_value(GtkAdjustment* adjustment, gpointer data)
{
  GtkAdjustment* tracker = data;

  gdouble lower = gtk_adjustment_get_lower(adjustment);
  gdouble upper = gtk_adjustment_get_upper(adjustment);
  if (lower != gtk_adjustment_get_lower(tracker) ||
      upper != gtk_adjustment_get_upper(tracker)) {
    return;
  }

  gdouble value = gtk_adjustment_get_value(adjustment);
  gtk_adjustment_set_value(tracker, value);
}

void
cb_adjustment_track_bounds(GtkAdjustment* adjustment, gpointer data)
{
  GtkAdjustment* tracker = data;
  gdouble value = gtk_adjustment_get_value(adjustment);
  gdouble lower = gtk_adjustment_get_lower(adjustment);
  gdouble upper = gtk_adjustment_get_upper(adjustment);
  gdouble page_size = gtk_adjustment_get_page_size(adjustment);
  gtk_adjustment_set_value(tracker, value);
  gtk_adjustment_set_lower(tracker, lower);
  gtk_adjustment_set_upper(tracker, upper);
  gtk_adjustment_set_page_size(tracker, page_size);
}

void
cb_pages_per_row_value_changed(girara_session_t* session, const char* UNUSED(name), girara_setting_type_t UNUSED(type), void* value, void* UNUSED(data))
{
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  zathura_t* zathura = session->global.data;

  int pages_per_row = *(int*) value;

  if (pages_per_row < 1) {
    pages_per_row = 1;
  }

  unsigned int first_page_column = 1;
  girara_setting_get(session, "first-page-column", &first_page_column);

  page_widget_set_mode(zathura, pages_per_row, first_page_column);

  if (zathura->document != NULL) {
    unsigned int current_page = zathura_document_get_current_page_number(zathura->document);
    page_set_delayed(zathura, current_page);
  }
}

void
cb_first_page_column_value_changed(girara_session_t* session, const char* UNUSED(name), girara_setting_type_t UNUSED(type), void* value, void* UNUSED(data))
{
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  zathura_t* zathura = session->global.data;

  int first_page_column = *(int*) value;

  if (first_page_column < 1) {
    first_page_column = 1;
  }

  unsigned int pages_per_row = 1;
  girara_setting_get(session, "pages-per-row", &pages_per_row);

  page_widget_set_mode(zathura, pages_per_row, first_page_column);

  if (zathura->document != NULL) {
    unsigned int current_page = zathura_document_get_current_page_number(zathura->document);
    page_set_delayed(zathura, current_page);
  }
}

void
cb_index_row_activated(GtkTreeView* tree_view, GtkTreePath* path,
                       GtkTreeViewColumn* UNUSED(column), void* data)
{
  zathura_t* zathura = data;
  if (tree_view == NULL || zathura == NULL || zathura->ui.session == NULL) {
    return;
  }

  GtkTreeModel  *model;
  GtkTreeIter   iter;

  g_object_get(tree_view, "model", &model, NULL);

  if(gtk_tree_model_get_iter(model, &iter, path)) {
    zathura_index_element_t* index_element;
    gtk_tree_model_get(model, &iter, 2, &index_element, -1);

    if (index_element == NULL) {
      return;
    }

    sc_toggle_index(zathura->ui.session, NULL, NULL, 0);
    zathura_link_evaluate(zathura, index_element->link);
  }

  g_object_unref(model);
}

typedef enum zathura_link_action_e
{
  ZATHURA_LINK_ACTION_FOLLOW,
  ZATHURA_LINK_ACTION_DISPLAY
} zathura_link_action_t;

static bool
handle_link(GtkEntry* entry, girara_session_t* session,
            zathura_link_action_t action)
{
  g_return_val_if_fail(session != NULL, FALSE);
  g_return_val_if_fail(session->global.data != NULL, FALSE);

  zathura_t* zathura = session->global.data;
  bool eval = true;

  char* input = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
  if (input == NULL || strlen(input) == 0) {
    eval = false;
  }

  int index = 0;
  if (eval == true) {
    index = atoi(input);
    if (index == 0 && g_strcmp0(input, "0") != 0) {
      girara_notify(session, GIRARA_WARNING, _("Invalid input '%s' given."), input);
      eval = false;
    }
    index = index - 1;
  }

  /* set pages to draw links */
  bool invalid_index = true;
  unsigned int number_of_pages = zathura_document_get_number_of_pages(zathura->document);
  for (unsigned int page_id = 0; page_id < number_of_pages; page_id++) {
    zathura_page_t* page = zathura_document_get_page(zathura->document, page_id);
    if (page == NULL || zathura_page_get_visibility(page) == false) {
      continue;
    }

    GtkWidget* page_widget = zathura_page_get_widget(zathura, page);
    g_object_set(page_widget, "draw-links", FALSE, NULL);

    if (eval == true) {
      zathura_link_t* link = zathura_page_widget_link_get(ZATHURA_PAGE(page_widget), index);

      if (link != NULL) {
        invalid_index = false;
        switch (action) {
          case ZATHURA_LINK_ACTION_FOLLOW:
            zathura_link_evaluate(zathura, link);
            break;
          case ZATHURA_LINK_ACTION_DISPLAY:
            zathura_link_display(zathura, link);
            break;
        }
      }
    }
  }

  if (eval == true && invalid_index == true) {
    girara_notify(session, GIRARA_WARNING, _("Invalid index '%s' given."), input);
  }

  g_free(input);

  return (eval == TRUE) ? TRUE : FALSE;
}

bool
cb_sc_follow(GtkEntry* entry, girara_session_t* session)
{
  return handle_link(entry, session, ZATHURA_LINK_ACTION_FOLLOW);
}

bool
cb_sc_display_link(GtkEntry* entry, girara_session_t* session)
{
  return handle_link(entry, session, ZATHURA_LINK_ACTION_DISPLAY);
}

void
cb_file_monitor(GFileMonitor* monitor, GFile* file, GFile* UNUSED(other_file), GFileMonitorEvent event, girara_session_t* session)
{
  g_return_if_fail(monitor  != NULL);
  g_return_if_fail(file     != NULL);
  g_return_if_fail(session  != NULL);

  switch (event) {
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    case G_FILE_MONITOR_EVENT_CREATED:
      gdk_threads_enter();
      sc_reload(session, NULL, NULL, 0);
      gdk_threads_leave();
      break;
    default:
      return;
  }
}

static gboolean
password_dialog(gpointer data)
{
  zathura_password_dialog_info_t* dialog = data;

  if (dialog != NULL) {
    girara_dialog(
      dialog->zathura->ui.session,
      "Incorrect password. Enter password:",
      true,
      NULL,
      (girara_callback_inputbar_activate_t) cb_password_dialog,
      dialog
    );
  }

  return FALSE;
}

bool
cb_password_dialog(GtkEntry* entry, zathura_password_dialog_info_t* dialog)
{
  if (entry == NULL || dialog == NULL) {
    goto error_ret;
  }

  if (dialog->path == NULL) {
    free(dialog);
    goto error_ret;
  }

  if (dialog->zathura == NULL) {
    goto error_free;
  }

  char* input = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);

  /* no or empty password: ask again */
  if (input == NULL || strlen(input) == 0) {
    if (input != NULL) {
      g_free(input);
    }

    gdk_threads_add_idle(password_dialog, dialog);
    return false;
  }

  /* try to open document again */
  if (document_open(dialog->zathura, dialog->path, input,
                    ZATHURA_PAGE_NUMBER_UNSPECIFIED) == false) {
    gdk_threads_add_idle(password_dialog, dialog);
  } else {
    g_free(dialog->path);
    free(dialog);
  }

  g_free(input);

  return true;

error_free:

  g_free(dialog->path);
  free(dialog);

error_ret:

  return false;
}

bool
cb_view_resized(GtkWidget* UNUSED(widget), GtkAllocation* allocation, zathura_t* zathura)
{
  if (zathura == NULL || zathura->document == NULL) {
    return false;
  }

  static int height = -1;
  static int width = -1;

  /* adjust only if the allocation changed */
  if (width != allocation->width || height != allocation->height) {
    girara_argument_t argument = { zathura_document_get_adjust_mode(zathura->document), NULL };
    sc_adjust_window(zathura->ui.session, &argument, NULL, 0);

    width  = allocation->width;
    height = allocation->height;
  }

  return false;
}

void
cb_setting_recolor_change(girara_session_t* session, const char* name,
                          girara_setting_type_t UNUSED(type), void* value, void* UNUSED(data))
{
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  g_return_if_fail(name != NULL);
  zathura_t* zathura = session->global.data;

  bool bool_value = *((bool*) value);

  if (zathura->global.recolor != bool_value) {
    zathura->global.recolor = bool_value;
    render_all(zathura);
  }
}

void
cb_setting_recolor_keep_hue_change(girara_session_t* session, const char* name,
                                   girara_setting_type_t UNUSED(type), void* value, void* UNUSED(data))
{
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  g_return_if_fail(name != NULL);
  zathura_t* zathura = session->global.data;

  bool bool_value = *((bool*) value);

  if (zathura->global.recolor_keep_hue != bool_value) {
    zathura->global.recolor_keep_hue = bool_value;
    render_all(zathura);
  }
}


bool
cb_unknown_command(girara_session_t* session, const char* input)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  g_return_val_if_fail(input != NULL, false);

  zathura_t* zathura = session->global.data;

  if (zathura->document == NULL) {
    return false;
  }

  /* check for number */
  for (unsigned int i = 0; i < strlen(input); i++) {
    if (g_ascii_isdigit(input[i]) == FALSE) {
      return false;
    }
  }

  zathura_jumplist_add(zathura);
  page_set(zathura, atoi(input) - 1);
  zathura_jumplist_add(zathura);

  return true;
}

void
cb_page_widget_text_selected(ZathuraPage* page, const char* text, void* data)
{
  g_return_if_fail(page != NULL);
  g_return_if_fail(text != NULL);
  g_return_if_fail(data != NULL);

  zathura_t* zathura = data;
  GdkAtom* selection = get_selection(zathura);

  /* copy to clipboard */
  if (selection != NULL) {
    gtk_clipboard_set_text(gtk_clipboard_get(*selection), text, -1);

    char* stripped_text = g_strdelimit(g_strdup(text), "\n\t\r\n", ' ');
    girara_notify(zathura->ui.session, GIRARA_INFO, _("Copied selected text to clipboard: %s"), stripped_text);
    g_free(stripped_text);
  }

  g_free(selection);
}

void
cb_page_widget_image_selected(ZathuraPage* page, GdkPixbuf* pixbuf, void* data)
{
  g_return_if_fail(page != NULL);
  g_return_if_fail(pixbuf != NULL);
  g_return_if_fail(data != NULL);

  zathura_t* zathura = data;
  GdkAtom* selection = get_selection(zathura);

  if (selection != NULL) {
    gtk_clipboard_set_image(gtk_clipboard_get(*selection), pixbuf);
  }

  g_free(selection);
}
