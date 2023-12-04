/* SPDX-License-Identifier: Zlib */

#include <girara/statusbar.h>
#include <girara/session.h>
#include <girara/settings.h>
#include <girara/utils.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <glib/gi18n.h>
#include <math.h>

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
#include "synctex.h"
#include "dbus-interface.h"

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
update_visible_pages(zathura_t* zathura)
{
  const unsigned int number_of_pages = zathura_document_get_number_of_pages(zathura->document);

  for (unsigned int page_id = 0; page_id < number_of_pages; page_id++) {
    zathura_page_t* page = zathura_document_get_page(zathura->document, page_id);
    GtkWidget* page_widget = zathura_page_get_widget(zathura, page);
    ZathuraPage* zathura_page_widget = ZATHURA_PAGE(page_widget);

    if (page_is_visible(zathura->document, page_id) == true) {
      /* make page visible */
      if (zathura_page_get_visibility(page) == false) {
        zathura_page_set_visibility(page, true);
        zathura_page_widget_update_view_time(zathura_page_widget);
        zathura_renderer_page_cache_add(zathura->sync.render_thread, zathura_page_get_index(page));
      }

    } else {
      /* make page invisible */
      if (zathura_page_get_visibility(page) == true) {
        zathura_page_set_visibility(page, false);
        /* If a page becomes invisible, abort the render request. */
        zathura_page_widget_abort_render_request(zathura_page_widget);
      }

      /* reset current search result */
      girara_list_t* results = NULL;
      GObject* obj_page_widget = G_OBJECT(page_widget);
      g_object_get(obj_page_widget, "search-results", &results, NULL);
      if (results != NULL) {
        g_object_set(obj_page_widget, "search-current", 0, NULL);
      }
    }
  }
}

void
cb_view_hadjustment_value_changed(GtkAdjustment* adjustment, gpointer data)
{
  zathura_t* zathura = data;
  if (zathura == NULL || zathura->document == NULL) {
    return;
  }

  /* Do nothing in index mode */
  if (girara_mode_get(zathura->ui.session) == zathura->modes.index) {
    return;
  }

  update_visible_pages(zathura);

  const double position_x = zathura_adjustment_get_ratio(adjustment);
  const double position_y = zathura_document_get_position_y(zathura->document);
  unsigned int page_id = position_to_page_number(zathura->document, position_x, position_y);

  zathura_document_set_position_x(zathura->document, position_x);
  zathura_document_set_position_y(zathura->document, position_y);
  zathura_document_set_current_page_number(zathura->document, page_id);

  statusbar_page_number_update(zathura);
}

void
cb_view_vadjustment_value_changed(GtkAdjustment* adjustment, gpointer data)
{
  zathura_t* zathura = data;
  if (zathura == NULL || zathura->document == NULL) {
    return;
  }

  /* Do nothing in index mode */
  if (girara_mode_get(zathura->ui.session) == zathura->modes.index) {
    return;
  }

  update_visible_pages(zathura);

  const double position_x = zathura_document_get_position_x(zathura->document);
  const double position_y = zathura_adjustment_get_ratio(adjustment);
  const unsigned int page_id = position_to_page_number(zathura->document, position_x, position_y);

  zathura_document_set_position_x(zathura->document, position_x);
  zathura_document_set_position_y(zathura->document, position_y);
  zathura_document_set_current_page_number(zathura->document, page_id);

  statusbar_page_number_update(zathura);
}

static void
cb_view_adjustment_changed(GtkAdjustment* adjustment, zathura_t* zathura,
    bool width)
{
  /* Do nothing in index mode */
  if (girara_mode_get(zathura->ui.session) == zathura->modes.index) {
    return;
  }

  const zathura_adjust_mode_t adjust_mode =
    zathura_document_get_adjust_mode(zathura->document);

  /* Don't scroll, we're focusing the inputbar. */
  if (adjust_mode == ZATHURA_ADJUST_INPUTBAR) {
    return;
  }

  /* Save the viewport size */
  const unsigned int size = floor(gtk_adjustment_get_page_size(adjustment));
  if (width == true) {
    zathura_document_set_viewport_width(zathura->document, size);
  } else {
    zathura_document_set_viewport_height(zathura->document, size);
  }

  /* reset the adjustment, in case bounds have changed */
  const double ratio = width == true ?
    zathura_document_get_position_x(zathura->document) :
    zathura_document_get_position_y(zathura->document);
  zathura_adjustment_set_value_from_ratio(adjustment, ratio);
}

void
cb_view_hadjustment_changed(GtkAdjustment* adjustment, gpointer data)
{
  zathura_t* zathura = data;
  g_return_if_fail(zathura != NULL);

  cb_view_adjustment_changed(adjustment, zathura, true);
}

void
cb_view_vadjustment_changed(GtkAdjustment* adjustment, gpointer data)
{
  zathura_t* zathura = data;
  g_return_if_fail(zathura != NULL);

  cb_view_adjustment_changed(adjustment, zathura, false);
}

void
cb_refresh_view(GtkWidget* GIRARA_UNUSED(view), gpointer data)
{
  zathura_t* zathura = data;
  if (zathura == NULL || zathura->document == NULL) {
    return;
  }

  unsigned int page_id = zathura_document_get_current_page_number(zathura->document);
  zathura_page_t* page = zathura_document_get_page(zathura->document, page_id);
  if (page == NULL) {
    return;
  }

  if (zathura->pages != NULL && zathura->pages[page_id] != NULL) {
	  ZathuraPage* page_widget = ZATHURA_PAGE(zathura->pages[page_id]);
	  if (page_widget != NULL) {
		  if (zathura_page_widget_have_surface(page_widget)) {
			  document_predecessor_free(zathura);
		  }
	  }
  }

  GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));
  GtkAdjustment* hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));

  const double position_x = zathura_document_get_position_x(zathura->document);
  const double position_y = zathura_document_get_position_y(zathura->document);

  zathura_adjustment_set_value_from_ratio(vadj, position_y);
  zathura_adjustment_set_value_from_ratio(hadj, position_x);

  statusbar_page_number_update(zathura);
}

void
cb_monitors_changed(GdkScreen* screen, gpointer data)
{
  girara_debug("signal received");

  zathura_t* zathura = data;
  if (screen == NULL || zathura == NULL) {
    return;
  }

  zathura_update_view_ppi(zathura);
}

void
cb_widget_screen_changed(GtkWidget* widget, GdkScreen* previous_screen, gpointer data)
{
  girara_debug("called");

  zathura_t* zathura = data;
  if (widget == NULL || zathura == NULL) {
    return;
  }

  /* disconnect previous screen handler if present */
  if (previous_screen != NULL && zathura->signals.monitors_changed_handler > 0) {
    g_signal_handler_disconnect(previous_screen, zathura->signals.monitors_changed_handler);
    zathura->signals.monitors_changed_handler = 0;
  }

  if (gtk_widget_has_screen(widget)) {
    GdkScreen* screen = gtk_widget_get_screen(widget);

    /* connect to new screen */
    zathura->signals.monitors_changed_handler = g_signal_connect(G_OBJECT(screen),
        "monitors-changed", G_CALLBACK(cb_monitors_changed), zathura);
  }

  zathura_update_view_ppi(zathura);
}

gboolean
cb_widget_configured(GtkWidget* UNUSED(widget), GdkEvent* UNUSED(event), gpointer data)
{
  zathura_t* zathura = data;
  if (zathura == NULL) {
    return false;
  }

  zathura_update_view_ppi(zathura);

  return false;
}

void
cb_scale_factor(GObject* object, GParamSpec* UNUSED(pspec), gpointer data)
{
  zathura_t* zathura = data;
  if (zathura == NULL || zathura->document == NULL) {
    return;
  }

  int new_factor = gtk_widget_get_scale_factor(GTK_WIDGET(object));
  if (new_factor == 0) {
    girara_debug("Ignoring new device scale factor = 0");
    return;
  }

  zathura_device_factors_t current = zathura_document_get_device_factors(zathura->document);
  if (fabs(new_factor - current.x) >= DBL_EPSILON ||
      fabs(new_factor - current.y) >= DBL_EPSILON) {
    zathura_document_set_device_factors(zathura->document, new_factor, new_factor);
    girara_debug("New device scale factor: %d", new_factor);
    zathura_update_view_ppi(zathura);
    render_all(zathura);
  }
}

void
cb_page_layout_value_changed(girara_session_t* session, const char* name, girara_setting_type_t UNUSED(type), const void* value, void* UNUSED(data))
{
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  zathura_t* zathura = session->global.data;

  /* pages-per-row must not be 0 */
  if (g_strcmp0(name, "pages-per-row") == 0) {
    unsigned int pages_per_row = *((const unsigned int*) value);
    if (pages_per_row == 0) {
      pages_per_row = 1;
      girara_setting_set(session, name, &pages_per_row);
      girara_notify(session, GIRARA_WARNING, _("'%s' must not be 0. Set to 1."), name);
      return;
    }
  }

  if (zathura->document == NULL) {
    /* no document has been openend yet */
    return;
  }

  unsigned int pages_per_row = 1;
  girara_setting_get(session, "pages-per-row", &pages_per_row);

  /* get list of first_page_column settings */
  char* first_page_column_list = NULL;
  girara_setting_get(session, "first-page-column", &first_page_column_list);

  /* find value for first_page_column */
  unsigned int first_page_column = find_first_page_column(first_page_column_list, pages_per_row);
  g_free(first_page_column_list);

  unsigned int page_padding = 1;
  girara_setting_get(zathura->ui.session, "page-padding", &page_padding);

  bool page_right_to_left = false;
  girara_setting_get(zathura->ui.session, "page-right-to-left", &page_right_to_left);

  page_widget_set_mode(zathura, page_padding, pages_per_row, first_page_column, page_right_to_left);
  zathura_document_set_page_layout(zathura->document, page_padding, pages_per_row, first_page_column);
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

  g_object_get(G_OBJECT(tree_view), "model", &model, NULL);

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
  ZATHURA_LINK_ACTION_COPY,
  ZATHURA_LINK_ACTION_DISPLAY
} zathura_link_action_t;

static gboolean
handle_link(GtkEntry* entry, girara_session_t* session,
            zathura_link_action_t action)
{
  g_return_val_if_fail(session != NULL, FALSE);
  g_return_val_if_fail(session->global.data != NULL, FALSE);

  zathura_t* zathura = session->global.data;
  gboolean eval = TRUE;

  char* input = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
  if (input == NULL || strlen(input) == 0) {
    eval = FALSE;
  }

  int index = 0;
  if (eval == TRUE) {
    index = atoi(input);
    if (index == 0 && g_strcmp0(input, "0") != 0) {
      girara_notify(session, GIRARA_WARNING, _("Invalid input '%s' given."), input);
      eval = FALSE;
    }
    index = index - 1;
  }

  /* find the link*/
  zathura_link_t* link = NULL;
  unsigned int number_of_pages = zathura_document_get_number_of_pages(zathura->document);
  for (unsigned int page_id = 0; page_id < number_of_pages; page_id++) {
    zathura_page_t* page = zathura_document_get_page(zathura->document, page_id);
    if (page == NULL || zathura_page_get_visibility(page) == false || eval == false) {
      continue;
    }
    GtkWidget* page_widget = zathura_page_get_widget(zathura, page);
    link = zathura_page_widget_link_get(ZATHURA_PAGE(page_widget), index);
    if (link != NULL) {
      break;
    }
  }

  if (eval == TRUE && link == NULL) {
    girara_notify(session, GIRARA_WARNING, _("Invalid index '%s' given."), input);
  } else {
    switch (action) {
      case ZATHURA_LINK_ACTION_FOLLOW:
        zathura_link_evaluate(zathura, link);
        break;
      case ZATHURA_LINK_ACTION_DISPLAY:
        zathura_link_display(zathura, link);
        break;
      case ZATHURA_LINK_ACTION_COPY: {
        GdkAtom* selection = get_selection(zathura);
        if (selection == NULL) {
          break;
        }

        zathura_link_copy(zathura, link, selection);
        g_free(selection);
        break;
      }
    }
  }

  g_free(input);

  return eval;
}

gboolean
cb_sc_follow(GtkEntry* entry, void* data)
{
  girara_session_t* session = data;
  return handle_link(entry, session, ZATHURA_LINK_ACTION_FOLLOW);
}

gboolean
cb_sc_display_link(GtkEntry* entry, void* data)
{
  girara_session_t* session = data;
  return handle_link(entry, session, ZATHURA_LINK_ACTION_DISPLAY);
}

gboolean
cb_sc_copy_link(GtkEntry* entry, void* data)
{
  girara_session_t* session = data;
  return handle_link(entry, session, ZATHURA_LINK_ACTION_COPY);
}

static gboolean
file_monitor_reload(void* data)
{
  sc_reload((girara_session_t*) data, NULL, NULL, 0);
  return FALSE;
}

void
cb_file_monitor(ZathuraFileMonitor* monitor, girara_session_t* session)
{
  g_return_if_fail(monitor  != NULL);
  g_return_if_fail(session  != NULL);

  g_main_context_invoke(NULL, file_monitor_reload, session);
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
      cb_password_dialog,
      dialog
    );
  }

  return FALSE;
}

gboolean
cb_password_dialog(GtkEntry* entry, void* data)
{
  if (entry == NULL || data == NULL) {
    goto error_ret;
  }

  zathura_password_dialog_info_t* dialog = data;

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
  if (document_open(dialog->zathura, dialog->path, dialog->uri, input,
                    ZATHURA_PAGE_NUMBER_UNSPECIFIED) == false) {
    gdk_threads_add_idle(password_dialog, dialog);
  } else {
    g_free(dialog->path);
    g_free(dialog->uri);
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

gboolean
cb_view_resized(GtkWidget* UNUSED(widget), GtkAllocation* UNUSED(allocation), zathura_t* zathura)
{
  if (zathura == NULL || zathura->document == NULL) {
    return false;
  }

  /* adjust the scale according to settings. If nothing needs to be resized,
     it does not trigger the resize event.

     The right viewport size is already in the document object, due to a
     previous call to adjustment_changed. We don't want to use the allocation in
     here, because we would have to subtract scrollbars, etc. */
  adjust_view(zathura);

  return false;
}

void
cb_setting_recolor_change(girara_session_t* session, const char* name,
                          girara_setting_type_t UNUSED(type), const void* value, void* UNUSED(data))
{
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  g_return_if_fail(name != NULL);
  zathura_t* zathura = session->global.data;

  const bool bool_value = *((const bool*) value);

  if (zathura->sync.render_thread != NULL && zathura_renderer_recolor_enabled(zathura->sync.render_thread) != bool_value) {
     zathura_renderer_enable_recolor(zathura->sync.render_thread, bool_value);
     render_all(zathura);
  }
}

void
cb_setting_recolor_keep_hue_change(girara_session_t* session, const char* name,
                                   girara_setting_type_t UNUSED(type), const void* value, void* UNUSED(data))
{
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  g_return_if_fail(name != NULL);
  zathura_t* zathura = session->global.data;

  const bool bool_value = *((const bool*) value);

  if (zathura->sync.render_thread != NULL && zathura_renderer_recolor_hue_enabled(zathura->sync.render_thread) != bool_value) {
     zathura_renderer_enable_recolor_hue(zathura->sync.render_thread, bool_value);
     render_all(zathura);
  }
}

void
cb_setting_recolor_keep_reverse_video_change(girara_session_t* session, const char* name,
                                   girara_setting_type_t UNUSED(type), const void* value, void* UNUSED(data))
{
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  g_return_if_fail(name != NULL);
  zathura_t* zathura = session->global.data;

  const bool bool_value = *((const bool*) value);

  if (zathura->sync.render_thread != NULL && zathura_renderer_recolor_reverse_video_enabled(zathura->sync.render_thread) != bool_value) {
     zathura_renderer_enable_recolor_reverse_video(zathura->sync.render_thread, bool_value);
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
  const size_t size = strlen(input);
  for (size_t i = 0; i < size; i++) {
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
  girara_mode_t mode = girara_mode_get(zathura->ui.session);
  if (mode != zathura->modes.normal && mode != zathura->modes.fullscreen) {
    return;
  }

  GdkAtom* selection = get_selection(zathura);
  if (selection == NULL) {
    return;
  }

  /* copy to clipboard */
  gtk_clipboard_set_text(gtk_clipboard_get(*selection), text, -1);

  bool notification = true;
  girara_setting_get(zathura->ui.session, "selection-notification", &notification);

  if (notification == true) {
    char* target = NULL;
    girara_setting_get(zathura->ui.session, "selection-clipboard", &target);

    char* stripped_text = g_strdelimit(g_strdup(text), "\n\t\r\n", ' ');
    char* escaped_text = g_markup_printf_escaped(
        _("Copied selected text to selection %s: %s"), target, stripped_text);
    g_free(target);
    g_free(stripped_text);

    girara_notify(zathura->ui.session, GIRARA_INFO, "%s", escaped_text);
    g_free(escaped_text);
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

    bool notification = true;
    girara_setting_get(zathura->ui.session, "selection-notification", &notification);

    if (notification == true) {
      char* target = NULL;
      girara_setting_get(zathura->ui.session, "selection-clipboard", &target);

      char* escaped_text = g_markup_printf_escaped(
          _("Copied selected image to selection %s"), target);
      g_free(target);

      girara_notify(zathura->ui.session, GIRARA_INFO, "%s", escaped_text);
    }

    g_free(selection);
  }
}

void
cb_page_widget_link(ZathuraPage* page, void* data)
{
  g_return_if_fail(page != NULL);

  bool enter = (bool) data;

  GdkWindow* window = gtk_widget_get_parent_window(GTK_WIDGET(page));
  GdkDisplay* display = gtk_widget_get_display(GTK_WIDGET(page));
  GdkCursor* cursor = gdk_cursor_new_for_display(display, enter == true ? GDK_HAND1 : GDK_LEFT_PTR);
  gdk_window_set_cursor(window, cursor);
  g_object_unref(cursor);
}

void
cb_page_widget_scaled_button_release(ZathuraPage* page_widget, GdkEventButton* event,
                                     void* data)
{
  zathura_t* zathura = data;
  zathura_page_t* page = zathura_page_widget_get_page(page_widget);

  if (event->button != GDK_BUTTON_PRIMARY) {
    return;
  }

  /* set page number (but don't scroll there. it was clicked on, so it's visible) */
  zathura_document_set_current_page_number(zathura->document, zathura_page_get_index(page));
  refresh_view(zathura);

  if (event->state & zathura->global.synctex_edit_modmask) {
    bool synctex = false;
    girara_setting_get(zathura->ui.session, "synctex", &synctex);
    if (synctex == false) {
      return;
    }

    if (zathura->dbus != NULL) {
      zathura_dbus_edit(zathura->dbus, zathura_page_get_index(page), event->x, event->y);
    }

    char* editor = NULL;
    girara_setting_get(zathura->ui.session, "synctex-editor-command", &editor);
    if (editor == NULL || *editor == '\0') {
      girara_debug("No SyncTeX editor specified.");
      g_free(editor);
      return;
    }

    synctex_edit(editor, page, event->x, event->y);
    g_free(editor);
  }
}

void
cb_window_update_icon(ZathuraRenderRequest* GIRARA_UNUSED(request), cairo_surface_t* surface, void* data)
{
  zathura_t* zathura = data;

  girara_debug("updating window icon");
  GdkPixbuf* pixbuf = gdk_pixbuf_get_from_surface(surface, 0, 0, cairo_image_surface_get_width(surface), cairo_image_surface_get_height(surface));
  if (pixbuf == NULL) {
    girara_error("Unable to convert cairo surface to Gdk Pixbuf.");
    return;
  }

  gtk_window_set_icon(GTK_WINDOW(zathura->ui.session->gtk.window), pixbuf);
  g_object_unref(pixbuf);
}

void
cb_gesture_zoom_begin(GtkGesture* UNUSED(self), GdkEventSequence* UNUSED(sequence), void* data)
{
  zathura_t* zathura = data;
  if (zathura == NULL || zathura->document == NULL) {
    return;
  }
  const double current_zoom     = zathura_document_get_zoom(zathura->document);
  zathura->gesture.initial_zoom = current_zoom;
}

void
cb_gesture_zoom_scale_changed(GtkGestureZoom* UNUSED(self), gdouble scale, void* data)
{
  zathura_t* zathura = data;
  if (zathura == NULL || zathura->document == NULL) {
    return;
  }

  const double next_zoom = zathura->gesture.initial_zoom * scale;
  const double corrected_zoom = zathura_correct_zoom_value(zathura->ui.session, next_zoom);
  zathura_document_set_zoom(zathura->document, corrected_zoom);
  render_all(zathura);
  refresh_view(zathura);
}

void cb_hide_links(GtkWidget* widget, gpointer data) {
  g_return_if_fail(widget != NULL);
  g_return_if_fail(data != NULL);

  /* disconnect from signal */
  gulong handler_id = GPOINTER_TO_UINT(g_object_steal_data(G_OBJECT(widget), "handler_id"));
  g_signal_handler_disconnect(G_OBJECT(widget), handler_id);

  zathura_t* zathura           = data;
  unsigned int number_of_pages = zathura_document_get_number_of_pages(zathura->document);
  for (unsigned int page_id = 0; page_id < number_of_pages; page_id++) {
    zathura_page_t* page = zathura_document_get_page(zathura->document, page_id);
    if (page == NULL) {
      continue;
    }

    GtkWidget* page_widget = zathura_page_get_widget(zathura, page);
    g_object_set(G_OBJECT(page_widget), "draw-links", FALSE, NULL);
  }
}
