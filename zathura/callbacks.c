/* SPDX-License-Identifier: Zlib */

#include "callbacks.h"

#include <girara-gtk/statusbar.h>
#include <girara-gtk/session.h>
#include <girara-gtk/settings.h>
#include <girara/log.h>
#include <girara/utils.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <glib/gi18n.h>
#include <math.h>

#include "links-internal.h"
#include "zathura.h"
#include "document.h"
#include "database.h"
#include "document-widget.h"
#include "index-element-object.h"
#include "utils.h"
#include "shortcuts.h"
#include "page-widget.h"
#include "page.h"
#include "render.h"
#include "adjustment.h"
#include "synctex.h"
#include "dbus-interface.h"

void cb_destroy(GtkWidget* widget, zathura_t* zathura) {
  /* hide the window on quit while the cleanup runs */
  if (widget == NULL && zathura != NULL && zathura->ui.session != NULL && zathura->ui.session->gtk.window != NULL) {
    gtk_widget_set_visible(zathura->ui.session->gtk.window, FALSE);
    GdkDisplay* display = gtk_widget_get_display(zathura->ui.session->gtk.window);
    if (display != NULL) {
      gdk_display_flush(display);
    }
  }

  /* genuine "destroy": the window and its child widgets are being finalized, so drop the borrowed pointers first */
  if (widget != NULL && zathura != NULL && zathura->ui.session != NULL) {
    zathura->ui.document_widget     = NULL;
    zathura->ui.session->gtk.window = NULL;
  }

  if (zathura_has_document(zathura) == true) {
    document_close(zathura, false);
  }

  GApplication* app = g_application_get_default();
  if (app != NULL) {
    g_application_quit(app);
  }
}

void cb_buffer_changed(girara_session_t* session) {
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);

  zathura_t* zathura = session->global.data;

  g_autofree char* buffer = girara_buffer_get(session);
  if (buffer != NULL) {
    girara_statusbar_item_set_text(zathura->ui.statusbar.buffer, buffer);
  } else {
    girara_statusbar_item_set_text(zathura->ui.statusbar.buffer, "");
  }
}

static bool in_single_page_mode(zathura_t* zathura) {
  if (zathura->ui.document_widget == NULL) {
    return false;
  }
  int layout_mode = DOCUMENT_WIDGET_GRID;
  g_object_get(zathura->ui.document_widget, "layout-mode", &layout_mode, NULL);
  return layout_mode == DOCUMENT_WIDGET_SINGLE;
}

/* the value is clamped at the ends of the document so there the ratio no longer marks the visible page */
static double page_position_y(GtkAdjustment* vadjustment) {
  const double value = gtk_adjustment_get_value(vadjustment);
  if (value <= gtk_adjustment_get_lower(vadjustment)) {
    return 0.0;
  }
  if (value >= gtk_adjustment_get_upper(vadjustment) - gtk_adjustment_get_page_size(vadjustment)) {
    return 1.0;
  }
  return zathura_adjustment_get_ratio(vadjustment);
}

void cb_document_widget_position_changed(ZathuraDocumentWidget* widget, zathura_t* zathura) {
  if (widget != zathura->ui.document_widget || !zathura_has_document(zathura) ||
      girara_mode_get(zathura->ui.session) == zathura->modes.index) {
    return;
  }

  /* In single-page mode the page is selected explicitly; scrolling must preserve it. */
  if (!in_single_page_mode(zathura)) {
    GtkAdjustment* vertical = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(widget));
    const unsigned int page =
        position_to_page_number(zathura, zathura_document_widget_get_position_x(widget), page_position_y(vertical));
    zathura_document_set_current_page_number(zathura_get_document(zathura), page);
  }
  statusbar_page_number_update(zathura);
}

void cb_refresh_view(GtkWidget* GIRARA_UNUSED(view), gpointer data) {
  zathura_t* zathura = data;
  if (zathura_has_document(zathura) == false || zathura->ui.document_widget == NULL) {
    return;
  }

  zathura_document_t* document = zathura_get_document(zathura);
  unsigned int page_id         = zathura_document_get_current_page_number(document);
  zathura_page_t* page         = zathura_document_get_page(document, page_id);
  if (page == NULL) {
    return;
  }

  zathura_document_widget_apply_position(zathura->ui.document_widget);

  statusbar_page_number_update(zathura);
}

void cb_monitors_changed(GListModel* UNUSED(model), guint UNUSED(position), guint UNUSED(removed), guint UNUSED(added),
                         gpointer data) {
  zathura_t* zathura = data;
  if (zathura == NULL) {
    return;
  }

  zathura_update_view_ppi(zathura);
}

void cb_scale_factor(GObject* UNUSED(object), GParamSpec* UNUSED(pspec), gpointer data) {
  zathura_t* zathura = data;
  if (zathura_has_document(zathura) == false || zathura->ui.document_widget == NULL) {
    return;
  }

  GtkWidget* view     = zathura->ui.session->gtk.view;
  GtkNative* native   = gtk_widget_get_native(view);
  GdkSurface* surface = native != NULL ? gtk_native_get_surface(native) : NULL;
  /* read the exact display scale instead of the rounded whole number */
  const double new_factor = surface != NULL ? gdk_surface_get_scale(surface) : gtk_widget_get_scale_factor(view);
  if (new_factor <= 0.0) {
    return;
  }

  zathura_document_t* document     = zathura_get_document(zathura);
  zathura_device_factors_t current = zathura_document_get_device_factors(document);
  if (fabs(new_factor - current.x) >= DBL_EPSILON || fabs(new_factor - current.y) >= DBL_EPSILON) {
    zathura_document_set_device_factors(document, new_factor, new_factor);
    girara_debug("New device scale factor: %0.2f", new_factor);
    zathura_update_view_ppi(zathura);
    /* the viewport is not yet resized for the new scale, the size allocation that follows renders it */
    if (zathura->sync.initial_render_held == true) {
      zathura->sync.scale_settled = true;
      return;
    }
    zathura_document_widget_render_all(zathura->ui.document_widget);
  }
}

void cb_view_realized(GtkWidget* widget, gpointer data) {
  zathura_t* zathura = data;

  GtkNative* native = gtk_widget_get_native(widget);
  if (native == NULL) {
    return;
  }
  GdkSurface* surface = gtk_native_get_surface(native);
  if (surface == NULL) {
    return;
  }

  /* run the scale handler when the display scale changes */
  g_signal_connect(G_OBJECT(surface), "notify::scale", G_CALLBACK(cb_scale_factor), zathura);
  cb_scale_factor(NULL, NULL, zathura);
}

void cb_page_layout_value_changed(girara_session_t* session, const char* name, girara_setting_type_t UNUSED(type),
                                  const void* value, void* UNUSED(data)) {
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  zathura_t* zathura = session->global.data;

  /* pages-per-row must not be 0 */
  if (g_strcmp0(name, "pages-per-row") == 0) {
    unsigned int pages_per_row = *((const unsigned int*)value);
    if (pages_per_row == 0) {
      pages_per_row = 1;
      girara_setting_set(session, name, &pages_per_row);
      girara_notify(session, GIRARA_WARNING, _("'%s' must not be 0. Set to 1."), name);
      return;
    }
  }

  if (zathura_has_document(zathura) == false || zathura->ui.document_widget == NULL) {
    /* no document has been openend yet */
    return;
  }

  unsigned int pages_per_row = 1;
  girara_setting_get(session, "pages-per-row", &pages_per_row);

  /* get list of first_page_column settings */
  g_autofree char* first_page_column_list = NULL;
  girara_setting_get(session, "first-page-column", &first_page_column_list);

  /* find value for first_page_column */
  unsigned int first_page_column = find_first_page_column(first_page_column_list, pages_per_row);

  unsigned int page_v_padding = 1;
  girara_setting_get(zathura->ui.session, "page-v-padding", &page_v_padding);

  unsigned int page_h_padding = 1;
  girara_setting_get(zathura->ui.session, "page-h-padding", &page_h_padding);

  bool page_right_to_left = false;
  girara_setting_get(zathura->ui.session, "page-right-to-left", &page_right_to_left);

  ZathuraDocumentWidget* document_widget = ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget);
  zathura_document_widget_set_page_layout(document_widget, page_v_padding, page_h_padding, pages_per_row,
                                          first_page_column);

  g_auto(GValue) page_right_to_left_value = G_VALUE_INIT;
  g_value_init(&page_right_to_left_value, G_TYPE_BOOLEAN);
  g_value_set_boolean(&page_right_to_left_value, page_right_to_left);
  g_object_set_property(G_OBJECT(zathura->ui.document_widget), "pages-right-to-left", &page_right_to_left_value);

  zathura_document_widget_refresh_layout(document_widget);

  /* padding/column changes shift each page's relative position, so re-anchor on the current page;
     otherwise its stale ratio leaves it off-screen and unrendered until the next manual scroll */
  zathura_document_t* document    = zathura_get_document(zathura);
  const unsigned int current_page = zathura_document_get_current_page_number(document);
  double anchor_x = 0.0, anchor_y = 0.0;
  page_number_to_position(zathura->ui.document_widget, current_page, 0.5, 0.5, &anchor_x, &anchor_y);
  zathura_document_widget_set_position(document_widget, anchor_x, anchor_y);
  refresh_view(zathura);
}

void cb_index_row_activated(GtkListView* view, guint position, void* data) {
  zathura_t* zathura = data;
  if (view == NULL || zathura == NULL || zathura->ui.session == NULL) {
    return;
  }

  GListModel* model             = G_LIST_MODEL(gtk_list_view_get_model(view));
  g_autoptr(GtkTreeListRow) row = g_list_model_get_item(model, position);
  if (row == NULL) {
    return;
  }
  g_autoptr(ZathuraIndexElementObject) item = gtk_tree_list_row_get_item(row);
  if (item == NULL || item->element == NULL) {
    return;
  }

  sc_toggle_index(zathura->ui.session, NULL, NULL, 0);
  zathura_link_evaluate(zathura, item->element->link);
}

typedef enum zathura_link_action_e {
  ZATHURA_LINK_ACTION_FOLLOW,
  ZATHURA_LINK_ACTION_COPY,
  ZATHURA_LINK_ACTION_DISPLAY
} zathura_link_action_t;

static bool handle_link(const char* input, girara_session_t* session, zathura_link_action_t action) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);

  zathura_t* zathura = session->global.data;

  if (input == NULL || strlen(input) == 0) {
    return false;
  }

  unsigned int index = 0;
  guint64 num;
  if (g_ascii_string_to_unsigned(input, 10, 1, UINT_MAX, &num, NULL)) {
    index = num - 1;
  } else {
    girara_notify(session, GIRARA_WARNING, _("Invalid input '%s' given."), input);
    return false;
  }

  zathura_link_t* link = zathura_document_widget_get_visible_link(zathura->ui.document_widget, index);

  if (link == NULL) {
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
      GdkClipboard* selection = get_selection(zathura);
      if (selection == NULL) {
        break;
      }

      zathura_link_copy(zathura, link, selection);
      break;
    }
    }
  }

  return true;
}

gboolean cb_sc_follow(GiraraDialog* UNUSED(inputbar), const char* input, void* data) {
  girara_session_t* session = data;
  return handle_link(input, session, ZATHURA_LINK_ACTION_FOLLOW);
}

gboolean cb_sc_display_link(GiraraDialog* UNUSED(inputbar), const char* input, void* data) {
  girara_session_t* session = data;
  return handle_link(input, session, ZATHURA_LINK_ACTION_DISPLAY);
}

gboolean cb_sc_copy_link(GiraraDialog* UNUSED(inputbar), const char* input, void* data) {
  girara_session_t* session = data;
  return handle_link(input, session, ZATHURA_LINK_ACTION_COPY);
}

static gboolean file_monitor_reload(void* data) {
  sc_reload((girara_session_t*)data, NULL, NULL, 0);
  return FALSE;
}

void cb_file_monitor(ZathuraFileMonitor* monitor, girara_session_t* session) {
  g_return_if_fail(monitor != NULL);
  g_return_if_fail(session != NULL);

  g_main_context_invoke(NULL, file_monitor_reload, session);
}

void zathura_password_dialog_info_free(zathura_password_dialog_info_t* dialog) {
  if (dialog) {
    g_free(dialog->path);
    g_free(dialog->uri);
    g_free(dialog);
  }
}

static void cb_password_dialog_hide(GtkWidget* UNUSED(w), void* data) {
  zathura_password_dialog_info_free(data);
}

static void password_dialog_arm_hide(zathura_password_dialog_info_t* dialog) {
  if (dialog == NULL || dialog->zathura == NULL || dialog->zathura->ui.session == NULL ||
      dialog->zathura->ui.session->gtk.dialog == NULL) {
    return;
  }
  g_signal_connect(dialog->zathura->ui.session->gtk.dialog, "hide", G_CALLBACK(cb_password_dialog_hide), dialog);
}

gboolean document_open_password_dialog(gpointer data) {
  zathura_password_dialog_info_t* dialog = data;

  GiraraDialog* widget = girara_dialog(dialog->zathura->ui.session, _("Enter password:"), true);
  password_dialog_arm_hide(dialog);
  g_signal_connect(widget, "activate", G_CALLBACK(cb_password_dialog), dialog);
  return FALSE;
}

static gboolean password_dialog(gpointer data) {
  zathura_password_dialog_info_t* dialog = data;

  if (dialog != NULL) {
    GiraraDialog* widget = girara_dialog(dialog->zathura->ui.session, "Incorrect password. Enter password:", true);
    password_dialog_arm_hide(dialog);
    g_signal_connect(widget, "activate", G_CALLBACK(cb_password_dialog), dialog);
  }

  return FALSE;
}

gboolean cb_password_dialog(GiraraDialog* inputbar, const char* input, void* data) {
  g_signal_handlers_disconnect_matched(inputbar, G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA,
                                       g_signal_lookup("hide", GTK_TYPE_WIDGET), 0, NULL, NULL, data);
  g_autoptr(zathura_password_dialog_info_t) dialog = data;
  if (input == NULL || dialog == NULL || dialog->path == NULL || dialog->zathura == NULL) {
    return false;
  }

  // No or empty password: ask again
  if (input == NULL || strlen(input) == 0) {
    g_idle_add(password_dialog, g_steal_pointer(&dialog));
    return false;
  }

  // Passwort prompt from a reload leaves the password dialg open until the retry succeeds.
  const bool reloading         = zathura_has_document(dialog->zathura);
  zathura_fileinfo_t file_info = {0};
  if (reloading) {
    if (g_strcmp0(zathura_document_get_path(zathura_get_document(dialog->zathura)), dialog->path) != 0) {
      return false;
    }
    file_info = zathura_get_fileinfo(dialog->zathura);
  }

  /* try to open document again */
  const bool opened = document_open(dialog->zathura, dialog->path, dialog->uri, input, ZATHURA_PAGE_NUMBER_UNSPECIFIED,
                                    reloading ? &file_info : NULL);
  g_free(file_info.first_page_column_list);
  if (!opened) {
    g_idle_add(password_dialog, g_steal_pointer(&dialog));
  }

  return true;
}

void cb_setting_recolor_change(girara_session_t* session, const char* name, girara_setting_type_t UNUSED(type),
                               const void* value, void* UNUSED(data)) {
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  g_return_if_fail(name != NULL);
  zathura_t* zathura = session->global.data;

  if (zathura->ui.document_widget) {
    zathura_document_widget_set_recolor(zathura->ui.document_widget, *(const bool*)value);
  }
}

void cb_setting_recolor_keep_hue_change(girara_session_t* session, const char* name, girara_setting_type_t UNUSED(type),
                                        const void* value, void* UNUSED(data)) {
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  g_return_if_fail(name != NULL);
  zathura_t* zathura = session->global.data;

  if (zathura->ui.document_widget) {
    zathura_document_widget_set_recolor_hue(zathura->ui.document_widget, *(const bool*)value);
  }
}

void cb_setting_recolor_keep_reverse_video_change(girara_session_t* session, const char* name,
                                                  girara_setting_type_t UNUSED(type), const void* value,
                                                  void* UNUSED(data)) {
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  g_return_if_fail(name != NULL);
  zathura_t* zathura = session->global.data;

  if (zathura->ui.document_widget) {
    zathura_document_widget_set_recolor_reverse_video(zathura->ui.document_widget, *(const bool*)value);
  }
}

bool cb_unknown_command(girara_session_t* session, const char* input) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  g_return_val_if_fail(input != NULL, false);

  zathura_t* zathura = session->global.data;
  if (zathura_has_document(zathura) == false) {
    return false;
  }

  unsigned int index = 0;
  guint64 num;
  if (g_ascii_string_to_unsigned(input, 10, 1, UINT_MAX, &num, NULL)) {
    index = num - 1;
  } else {
    return false;
  }

  zathura_jumplist_add(zathura);
  page_set(zathura, index, -1, -1, false);
  zathura_jumplist_add(zathura);

  return true;
}

void cb_page_widget_text_selected(ZathuraPageWidget* page, const char* text, void* data) {
  g_return_if_fail(page != NULL);
  g_return_if_fail(text != NULL);
  g_return_if_fail(data != NULL);

  zathura_t* zathura = data;
  girara_mode_t mode = girara_mode_get(zathura->ui.session);
  if (mode != zathura->modes.normal && mode != zathura->modes.fullscreen) {
    return;
  }

  copy_text_to_selection(zathura, text);
}

void cb_page_widget_image_selected(ZathuraPageWidget* page, GdkTexture* texture, void* data) {
  g_return_if_fail(page != NULL);
  g_return_if_fail(texture != NULL);
  g_return_if_fail(data != NULL);

  zathura_t* zathura = data;
  copy_image_to_selection(zathura, texture);
}

/* time in ms the pointer has to stop on a link before its target is rendered */
#define LINK_PREVIEW_DELAY 150

static void cb_link_preview_draw(GtkDrawingArea* UNUSED(area), cairo_t* cairo, int UNUSED(width), int UNUSED(height),
                                 gpointer data) {
  cairo_set_source_surface(cairo, data, 0, 0);
  cairo_paint(cairo);
}

/* copy a strip of the link's target page at the current zoom, starting just above the target position */
static GtkWidget* link_preview_new(zathura_t* zathura, zathura_link_t* link) {
  const zathura_link_target_t target = zathura_link_get_target(link);
  zathura_document_t* document       = zathura_get_document(zathura);
  ZathuraRenderer* renderer          = zathura_document_widget_get_renderer(zathura->ui.document_widget);
  if (document == NULL || renderer == NULL || target.destination_type == ZATHURA_LINK_DESTINATION_UNKNOWN ||
      target.page_number >= zathura_document_get_number_of_pages(document)) {
    return NULL;
  }

  zathura_page_t* page     = zathura_document_get_page(document, target.page_number);
  unsigned int page_width  = 0;
  unsigned int page_height = 0;
  const double scale       = page_calc_height_width(document, page, &page_height, &page_width, false);
  if (page_width == 0) {
    return NULL;
  }

  /* the preview is at most 80% of the window wide and half of it high */
  GtkWidget* window    = zathura->ui.session->gtk.window;
  const int max_width  = gtk_widget_get_width(window) * 0.8;
  const int max_height = gtk_widget_get_height(window) / 2;

  /* show the whole page width, rendered smaller than the current zoom if it does not fit the window */
  const double fit          = MIN(1.0, (double)max_width / page_width);
  cairo_surface_t* rendered = zathura_renderer_render_page_scaled(renderer, page, fit);
  if (rendered == NULL) {
    return NULL;
  }

  /* crop only vertically, in device pixels of the rendered page so that the copy is 1:1 */
  double scale_x = 1;
  double scale_y = 1;
  cairo_surface_get_device_scale(rendered, &scale_x, &scale_y);
  const int surface_width  = cairo_image_surface_get_width(rendered);
  const int surface_height = cairo_image_surface_get_height(rendered);
  const int strip_height   = MIN(surface_height, round(max_height * scale_y));
  const double target_top  = target.top * scale * fit * scale_y;
  const int top = target.top >= 0 ? round(CLAMP(target_top - strip_height / 10, 0, surface_height - strip_height)) : 0;

  cairo_surface_t* strip = NULL;
  if (surface_width > 0 && strip_height > 0) {
    strip = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, surface_width, strip_height);
    cairo_surface_set_device_scale(strip, scale_x, scale_y);
    cairo_t* cairo = cairo_create(strip);
    cairo_set_source_surface(cairo, rendered, 0, -top / scale_y);
    cairo_paint(cairo);
    cairo_destroy(cairo);
  }
  cairo_surface_destroy(rendered);

  if (strip == NULL || cairo_surface_status(strip) != CAIRO_STATUS_SUCCESS) {
    cairo_surface_destroy(strip);
    return NULL;
  }

  GtkWidget* area = gtk_drawing_area_new();
  gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(area), ceil(surface_width / scale_x));
  gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(area), ceil(strip_height / scale_y));
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area), cb_link_preview_draw, strip,
                                 (GDestroyNotify)cairo_surface_destroy);
  return g_object_ref_sink(area);
}

static bool link_preview_enabled(zathura_t* zathura) {
  bool link_preview = true;
  girara_setting_get(zathura->ui.session, "link-preview", &link_preview);
  return link_preview;
}

static gboolean cb_link_preview_timeout(gpointer data) {
  zathura_t* zathura               = data;
  zathura->link_preview.timeout_id = 0;
  if (link_preview_enabled(zathura) == false) {
    return G_SOURCE_REMOVE;
  }

  zathura->link_preview.widget = link_preview_new(zathura, zathura->link_preview.link);
  if (zathura->link_preview.widget != NULL) {
    gtk_widget_trigger_tooltip_query(GTK_WIDGET(zathura->link_preview.page));
  }

  return G_SOURCE_REMOVE;
}

/* (re)start rendering the preview once the pointer has rested on the link */
static void link_preview_schedule(zathura_t* zathura) {
  g_clear_object(&zathura->link_preview.widget);
  g_clear_handle_id(&zathura->link_preview.timeout_id, g_source_remove);
  if (zathura_link_get_type(zathura->link_preview.link) == ZATHURA_LINK_GOTO_DEST) {
    zathura->link_preview.timeout_id = g_timeout_add(LINK_PREVIEW_DELAY, cb_link_preview_timeout, zathura);
  }
}

void cb_page_widget_enter_link(ZathuraPageWidget* page, zathura_link_t* link, void* data) {
  g_return_if_fail(page != NULL);
  g_return_if_fail(link != NULL);
  g_return_if_fail(data != NULL);

  zathura_t* zathura = data;
  gtk_widget_set_cursor_from_name(GTK_WIDGET(page), "pointer");

  if (link_preview_enabled(zathura) == false) {
    return;
  }

  zathura->link_preview.page = page;
  zathura->link_preview.link = link;
  link_preview_schedule(zathura);
}

void cb_page_widget_rendered(ZathuraPageWidget* page, void* data) {
  zathura_t* zathura = data;

  zathura_link_t* link = zathura->link_preview.link;
  if (link == NULL) {
    return;
  }

  /* zoom, recolor or rotation changes re-render the page, so the preview is outdated as well. GTK hides the
   * tooltip on the key press that caused it, so show the new preview without waiting for the pointer to move. */
  const bool hovered_page = zathura->link_preview.page == page;
  /* the target cannot be rendered while it is queued in the background renderer, so retry once it is done. The
   * page still counts as queued while "rendered" is emitted, so the retry has to go through the delay. */
  const bool target_page =
      zathura->link_preview.widget == NULL && zathura->link_preview.timeout_id == 0 &&
      zathura_link_get_type(link) == ZATHURA_LINK_GOTO_DEST &&
      zathura_page_get_index(zathura_page_widget_get_page(page)) == zathura_link_get_target(link).page_number;
  if (hovered_page == true || target_page == true) {
    link_preview_schedule(zathura);
  }
}

void cb_page_widget_leave_link(ZathuraPageWidget* page, void* data) {
  g_return_if_fail(page != NULL);
  g_return_if_fail(data != NULL);

  zathura_t* zathura = data;
  gtk_widget_set_cursor_from_name(GTK_WIDGET(page), "default");

  if (zathura->link_preview.page == page) {
    zathura->link_preview.page = NULL;
    zathura->link_preview.link = NULL;
    g_clear_object(&zathura->link_preview.widget);
    g_clear_handle_id(&zathura->link_preview.timeout_id, g_source_remove);
  }
}

gboolean cb_page_widget_query_tooltip(GtkWidget* widget, int x, int y, gboolean UNUSED(keyboard_mode),
                                      GtkTooltip* tooltip, void* data) {
  zathura_t* zathura   = data;
  zathura_link_t* link = zathura->link_preview.link;
  if (link == NULL || GTK_WIDGET(zathura->link_preview.page) != widget || link_preview_enabled(zathura) == false) {
    return FALSE;
  }

  zathura_page_t* page           = zathura_page_widget_get_page(zathura->link_preview.page);
  const zathura_rectangle_t rect = recalc_rectangle(page, zathura_link_get_position(link));
  if (x < rect.x1 || x > rect.x2 || y < rect.y1 || y > rect.y2) {
    /* a zoom moved the link away from the pointer */
    return FALSE;
  }
  const GdkRectangle area = {rect.x1, rect.y1, rect.x2 - rect.x1, rect.y2 - rect.y1};
  gtk_tooltip_set_tip_area(tooltip, &area);

  switch (zathura_link_get_type(link)) {
  case ZATHURA_LINK_GOTO_DEST:
    if (zathura->link_preview.widget == NULL) {
      return FALSE;
    }
    gtk_tooltip_set_custom(tooltip, zathura->link_preview.widget);
    return TRUE;
  case ZATHURA_LINK_GOTO_REMOTE:
  case ZATHURA_LINK_URI:
    gtk_tooltip_set_text(tooltip, zathura_link_get_target(link).value);
    return TRUE;
  default:
    return FALSE;
  }
}

void cb_page_widget_scaled_button_release(ZathuraPageWidget* page_widget, scaled_button_release_event_t* event,
                                          void* data) {
  zathura_t* zathura   = data;
  zathura_page_t* page = zathura_page_widget_get_page(page_widget);

  if (event->button != GDK_BUTTON_PRIMARY) {
    return;
  }

  /* set page number (but don't scroll there. it was clicked on, so it's visible) */
  zathura_document_set_current_page_number(zathura_get_document(zathura), zathura_page_get_index(page));
  refresh_view(zathura);

  if (event->state & zathura->global.synctex_edit_modmask) {
    bool synctex = false;
    girara_setting_get(zathura->ui.session, "synctex", &synctex);
    if (synctex == false) {
      return;
    }

    if (zathura->dbus != NULL) {
      zathura_dbus_edit(zathura, zathura_page_get_index(page), event->x, event->y);
    }

    g_autofree char* editor = NULL;
    girara_setting_get(zathura->ui.session, "synctex-editor-command", &editor);
    if (editor == NULL || *editor == '\0') {
      girara_debug("No SyncTeX editor specified.");
      return;
    }

    synctex_edit(zathura, editor, page, event->x, event->y);
  }
}

void cb_gesture_zoom_begin(GtkGesture* UNUSED(self), GdkEventSequence* UNUSED(sequence), void* data) {
  zathura_t* zathura = data;
  if (zathura_has_document(zathura) == false) {
    return;
  }
  zathura->gesture.initial_zoom = zathura_document_get_zoom(zathura_get_document(zathura));
}

void cb_gesture_zoom_scale_changed(GtkGestureZoom* UNUSED(self), gdouble scale, void* data) {
  zathura_t* zathura = data;
  if (zathura_has_document(zathura) == false) {
    return;
  }

  const double next_zoom     = zathura->gesture.initial_zoom * scale;
  girara_argument_t argument = {.n = ZOOM_SPECIFIC};

  sc_zoom(zathura->ui.session, &argument, NULL, next_zoom * 100);
}

gboolean cb_drop_file(GtkDropTarget* UNUSED(self), const GValue* value, double UNUSED(x), double UNUSED(y),
                      void* data) {
  zathura_t* zathura = data;
  if (zathura == NULL || value == NULL) {
    return FALSE;
  }

  /* the dropped value holds the list of file GFiles (matched GDK_TYPE_FILE_LIST) */
  GSList* files = NULL;
  if (G_VALUE_HOLDS(value, GDK_TYPE_FILE_LIST)) {
    files = g_value_get_boxed(value);
  }

  const GSList* file = files;
  while (file != NULL && g_file_is_native(G_FILE(file->data)) == FALSE) {
    file = file->next;
  }
  if (file == NULL) {
    return FALSE;
  }

  g_autofree char* path = g_file_get_path(G_FILE(file->data));
  if (path == NULL) {
    return FALSE;
  }

  if (zathura_has_document(zathura) == true) {
    document_close(zathura, false);
  }

  document_open_idle(zathura, path, NULL, ZATHURA_PAGE_NUMBER_UNSPECIFIED, NULL, NULL, NULL, NULL);
  return TRUE;
}

void cb_hide_inputbar(GtkWidget* widget, gpointer data) {
  g_return_if_fail(widget != NULL);
  g_return_if_fail(data != NULL);

  zathura_t* zathura = data;
  zathura_inputbar_restore_adjust_mode(zathura);
}

void cb_inputbar_focus_leave(GtkEventControllerFocus* controller, gpointer data) {
  cb_hide_inputbar(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller)), data);
}

void cb_hide_links(GtkWidget* widget, gpointer data) {
  g_return_if_fail(widget != NULL);
  g_return_if_fail(data != NULL);

  zathura_t* zathura = data;
  zathura_document_widget_hide_links(zathura->ui.document_widget);
}
