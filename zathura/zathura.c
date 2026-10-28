/* SPDX-License-Identifier: Zlib */

#include "zathura.h"

#include <glib-object.h>
#include <glib.h>
#include <errno.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <girara/datastructures.h>
#include <girara/utils.h>
#include <girara-gtk/session.h>
#include <girara-gtk/statusbar.h>
#include <girara-gtk/settings.h>
#include <girara-gtk/shortcuts.h>
#include <girara/template.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#ifdef G_OS_UNIX
#include <glib-unix.h>
#include <gio/gunixinputstream.h>
#endif

#include "bookmarks.h"
#include "callbacks.h"
#include "config.h"
#include "commands.h"
#include "database-null.h"
#ifndef WITH_SANDBOX
#include "database-sqlite.h"
#endif
#include "document.h"
#include "document-widget.h"
#include "shortcuts.h"
#include "utils.h"
#include "visual.h"
#include "marks.h"
#include "page.h"
#include "plugin.h"
#include "adjustment.h"
#include "dbus-interface.h"
#include "resources.h"
#include "synctex.h"
#include "content-type.h"
#include "internal.h"

typedef struct zathura_document_info_s {
  zathura_t* zathura;
  char* path;
  char* password;
  char* mode;
  char* synctex;
  char* bookmark_name;
  char* search_string;
  int page_number;
} zathura_document_info_t;

// A prepared or retired view owns its resources independently of the active session.
typedef struct {
  zathura_document_t* document;
  ZathuraDocumentWidget* widget;
} document_view_t;

typedef struct document_replacement_s {
  document_view_t view;
  ZathuraFileMonitor* monitor;
  char* password;
} document_replacement_t;

static void document_view_clear(document_view_t* view) {
  /* release the page widgets before their document and page objects */
  if (view->widget) {
    zathura_document_widget_set_document(view->widget, NULL);
    g_clear_object(&view->widget);
  }

  /* remove document */
  g_clear_pointer(&view->document, zathura_document_free);
}

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC(document_view_t, document_view_clear)

static void save_fileinfo_to_db(zathura_t* zathura);
static void document_stop(zathura_t* zathura);
static void document_index_clear(zathura_t* zathura);

static gboolean document_info_open(gpointer data);
static void cb_document_widget_page_widgets_loaded(ZathuraDocumentWidget* document_widget, zathura_t* zathura);

#ifdef G_OS_UNIX
static gboolean zathura_signal_sigterm(gpointer data);
#endif

static void free_document_info(zathura_document_info_t* document_info) {
  if (document_info != NULL) {
    g_free(document_info->path);
    g_free(document_info->password);
    g_free(document_info->mode);
    g_free(document_info->synctex);
    g_free(document_info->bookmark_name);
    g_free(document_info->search_string);
    g_free(document_info);
  }
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(zathura_document_info_t, free_document_info)

/* function implementation */
zathura_t* zathura_create(void) {
  g_autoptr(zathura_t) zathura = g_try_malloc0(sizeof(zathura_t));
  if (zathura == NULL) {
    return NULL;
  }

  /* global settings */
  zathura->global.search_direction               = FORWARD;
  zathura->global.search_string                  = NULL;
  zathura->global.are_search_results_highlighted = false;
  zathura->global.synctex_edit_modmask           = GDK_CONTROL_MASK;
  zathura->global.highlighter_modmask            = GDK_SHIFT_MASK;
  zathura->global.double_click_follow            = true;

  /* initialize with default paths */
  {
    g_autofree gchar* config_path = girara_get_xdg_path(XDG_CONFIG);
    zathura->config.config_dir    = g_build_filename(config_path, "zathura", NULL);

    g_autofree gchar* data_path = girara_get_xdg_path(XDG_DATA);
    zathura->config.data_dir    = g_build_filename(data_path, "zathura", NULL);

    g_autofree gchar* cache_path = girara_get_xdg_path(XDG_CACHE);
    zathura->config.cache_dir    = g_build_filename(cache_path, "zathura", NULL);
  }

  /* plugins */
  zathura->plugins.manager = zathura_plugin_manager_new();
  if (zathura->plugins.manager == NULL) {
    return NULL;
  }

  /* UI */
  zathura->ui.session = girara_session_create();
  if (zathura->ui.session == NULL) {
    return NULL;
  }

#ifdef G_OS_UNIX
  /* signal handler */
  zathura->signals.sigterm = g_unix_signal_add(SIGTERM, zathura_signal_sigterm, zathura);
#endif

  /* MIME type detection */
  zathura->content_type_context = zathura_content_type_new();

  zathura->ui.session->global.data = zathura;

  return g_steal_pointer(&zathura);
}

#ifndef WITH_SANDBOX
static void create_directories(zathura_t* zathura) {
  static const unsigned int mode = 0700;

  if (g_mkdir_with_parents(zathura->config.data_dir, mode) == -1) {
    girara_error("Could not create '%s': %s", zathura->config.data_dir, strerror(errno));
  }
}
#endif

static bool get_view_ppi(zathura_t* zathura, double* view_ppi) {
  // get view widget GdkMonitor
  GtkNative* native = gtk_widget_get_native(zathura->ui.session->gtk.view);
  if (!native) {
    return false;
  }

  GdkSurface* surface = gtk_native_get_surface(native);
  GdkDisplay* display = gtk_widget_get_display(zathura->ui.session->gtk.view);
  if (!surface || !display) {
    return false;
  }

  GdkMonitor* monitor = gdk_display_get_monitor_at_surface(display, surface);
  if (!monitor) {
    return false;
  }

  // physical width of monitor
  const int width_mm = gdk_monitor_get_width_mm(monitor);

  // size of monitor in pixels
  GdkRectangle monitor_geom;
  gdk_monitor_get_geometry(monitor, &monitor_geom);

  double ppi = 0.0;
  // Due to a bug in Gtk, width is sometimes incorrectly reported to be 1mm. See
  // https://gitlab.gnome.org/GNOME/gtk/issues/3115 for details
  if (width_mm <= 1) {
    girara_debug("cannot calculate PPI: monitor has zero width");
  } else {
    // calculate ppi, knowing that 1 inch = 25.4 mm
    ppi = monitor_geom.width * 25.4 / width_mm;
  }

  /* gtk4 reports logical monitor pixels so ppi needs no device scale correction */

  *view_ppi = ppi;
  return true;
}

void zathura_update_view_ppi(zathura_t* zathura) {
  if (!zathura || !zathura_has_document(zathura)) {
    return;
  }

  double ppi = 0.0;
  if (!get_view_ppi(zathura, &ppi)) {
    return;
  }

  zathura_document_t* document = zathura_get_document(zathura);
  const double current_ppi     = zathura_document_get_viewport_ppi(document);
  if (fabs(ppi - current_ppi) > DBL_EPSILON) {
    girara_debug("view ppi: %0.2f", ppi);
    zathura_document_set_viewport_ppi(document, ppi);

    if (zathura->ui.document_widget) {
      zathura_document_widget_adjust_view(zathura->ui.document_widget);
      zathura_document_widget_render_all(zathura->ui.document_widget);
      refresh_view(zathura);
    }
  }
}

static bool init_ui(zathura_t* zathura) {
  if (girara_session_init(zathura->ui.session, "zathura") == false) {
    girara_error("Failed to initialize girara.");
    return false;
  }

  /* intial window icon and title */
  girara_set_window_title(zathura->ui.session, "zathura");
  girara_set_window_icon(zathura->ui.session, "org.pwmt.zathura");

  /* girara events */
  zathura->ui.session->events.buffer_changed  = cb_buffer_changed;
  zathura->ui.session->events.unknown_command = cb_unknown_command;

  /* gestures */
  GtkGesture* zoom = gtk_gesture_zoom_new();
  g_signal_connect(zoom, "scale-changed", G_CALLBACK(cb_gesture_zoom_scale_changed), zathura);
  g_signal_connect(zoom, "begin", G_CALLBACK(cb_gesture_zoom_begin), zathura);
  gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(zoom), GTK_PHASE_BUBBLE);
  gtk_widget_add_controller(GTK_WIDGET(zathura->ui.session->gtk.view), GTK_EVENT_CONTROLLER(zoom));

  /* allow dropping a file onto the window to open it */
  GtkDropTarget* drop_target = gtk_drop_target_new(GDK_TYPE_FILE_LIST, GDK_ACTION_COPY);
  g_signal_connect(drop_target, "drop", G_CALLBACK(cb_drop_file), zathura);
  gtk_widget_add_controller(GTK_WIDGET(zathura->ui.session->gtk.view), GTK_EVENT_CONTROLLER(drop_target));

  /* zathura signals */
  zathura->signals.refresh_view = g_signal_new("refresh-view", GTK_TYPE_WIDGET, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                               g_cclosure_marshal_generic, G_TYPE_NONE, 0);

  g_signal_connect(G_OBJECT(zathura->ui.session->gtk.view), "refresh-view", G_CALLBACK(cb_refresh_view), zathura);

  g_signal_connect(G_OBJECT(zathura->ui.session->gtk.view), "notify::scale-factor", G_CALLBACK(cb_scale_factor),
                   zathura);
  g_signal_connect(G_OBJECT(zathura->ui.session->gtk.view), "realize", G_CALLBACK(cb_view_realized), zathura);
  /* the window can already be shown at this point */
  if (gtk_widget_get_realized(zathura->ui.session->gtk.view) == TRUE) {
    cb_view_realized(zathura->ui.session->gtk.view, zathura);
  }

  /* update the view PPI whenever the monitor configuration changes */
  GdkDisplay* display = gtk_widget_get_display(zathura->ui.session->gtk.view);
  if (display != NULL) {
    GListModel* monitors = gdk_display_get_monitors(display);
    zathura->signals.monitors_handler =
        g_signal_connect(monitors, "items-changed", G_CALLBACK(cb_monitors_changed), zathura);
  }

  /* restore the adjustment mode even when the inputbar stays visible */
  g_signal_connect(zathura->ui.session->gtk.inputbar, "hide", G_CALLBACK(cb_hide_inputbar), zathura);
  GtkEventController* inputbar_focus = gtk_event_controller_focus_new();
  g_signal_connect(inputbar_focus, "leave", G_CALLBACK(cb_inputbar_focus_leave), zathura);
  gtk_widget_add_controller(zathura->ui.session->gtk.inputbar, inputbar_focus);

  /* page view */
  zathura->ui.view = gtk_scrolled_window_new();
  girara_set_view(zathura->ui.session, zathura->ui.view);

  /* load scrollbar settings */
  g_autofree char* view_options = NULL;
  girara_setting_get(zathura->ui.session, "guioptions", &view_options);

  const bool show_hscrollbar = view_options != NULL && strchr(view_options, 'h') != NULL;
  const bool show_vscrollbar = view_options != NULL && strchr(view_options, 'v') != NULL;

  GtkPolicyType hpolicy = show_hscrollbar ? GTK_POLICY_AUTOMATIC : GTK_POLICY_EXTERNAL;
  GtkPolicyType vpolicy = show_vscrollbar ? GTK_POLICY_AUTOMATIC : GTK_POLICY_EXTERNAL;
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(zathura->ui.view), hpolicy, vpolicy);

  GtkAdjustment* hadjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(zathura->ui.view));

  /* Connect hadjustment signals */
  g_signal_connect(G_OBJECT(hadjustment), "value-changed", G_CALLBACK(cb_view_hadjustment_value_changed), zathura);
  g_signal_connect(G_OBJECT(hadjustment), "changed", G_CALLBACK(cb_view_hadjustment_changed), zathura);

  GtkAdjustment* vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zathura->ui.view));

  /* Connect vadjustment signals */
  g_signal_connect(G_OBJECT(vadjustment), "value-changed", G_CALLBACK(cb_view_vadjustment_value_changed), zathura);
  g_signal_connect(G_OBJECT(vadjustment), "changed", G_CALLBACK(cb_view_vadjustment_changed), zathura);

  /* statusbar */
  GiraraStatusbar* statusbar = GIRARA_STATUSBAR(zathura->ui.session->gtk.statusbar);
  zathura->ui.statusbar.file = girara_statusbar_item_add(statusbar, true, true);
  if (zathura->ui.statusbar.file == NULL) {
    girara_error("Failed to create status bar file item.");
    return false;
  }

  zathura->ui.statusbar.buffer = girara_statusbar_item_add(statusbar, false, false);
  if (zathura->ui.statusbar.buffer == NULL) {
    girara_error("Failed to create status bar buffer item.");
    return false;
  }

  zathura->ui.statusbar.page_number = girara_statusbar_item_add(statusbar, false, false);
  if (zathura->ui.statusbar.page_number == NULL) {
    girara_error("Failed to create status bar page number item.");
    return false;
  }

  zathura->ui.statusbar.search_count = girara_statusbar_item_add(statusbar, false, false);
  if (zathura->ui.statusbar.search_count == NULL) {
    girara_error("Failed to create status bar search count item.");
    return false;
  }

  girara_statusbar_item_set_text(zathura->ui.statusbar.file, _("[No name]"));

  /* signals */
  zathura->signals.destroy_handler =
      g_signal_connect(G_OBJECT(zathura->ui.session->gtk.window), "destroy", G_CALLBACK(cb_destroy), zathura);

  return true;
}

static const char color_settings[][16] = {
    "index-fg", "index-bg", "index-active-fg", "index-active-bg", "scrollbar-fg", "scrollbar-bg",
};

static void init_css(zathura_t* zathura) {
  GiraraTemplate* csstemplate = girara_session_get_template(zathura->ui.session);
  for (size_t s = 0; s < LENGTH(color_settings); ++s) {
    girara_template_add_variable(csstemplate, color_settings[s]);
  }
}

static bool load_css(zathura_t* zathura) {
  GiraraTemplate* csstemplate = girara_session_get_template(zathura->ui.session);
  for (size_t s = 0; s < LENGTH(color_settings); ++s) {
    g_autofree char* tmp_value = NULL;
    GdkRGBA rgba               = {0, 0, 0, 0};
    girara_setting_get(zathura->ui.session, color_settings[s], &tmp_value);
    if (tmp_value != NULL) {
      parse_color(&rgba, tmp_value);
    }

    g_autofree char* color = gdk_rgba_to_string(&rgba);
    girara_template_set_variable_value(csstemplate, color_settings[s], color);
  }

  GResource* css_resource = zathura_resources_get_resource();
  g_autoptr(GBytes) css_data =
      g_resource_lookup_data(css_resource, "/org/pwmt/zathura/CSS/zathura.css_t", G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  if (!css_data) {
    return false;
  }

  g_autofree char* css =
      g_strdup_printf("%s\n%s", girara_template_get_base(csstemplate), (const char*)g_bytes_get_data(css_data, NULL));
  girara_template_set_base(csstemplate, css);

  return true;
}

static bool init_database(zathura_t* zathura) {
#ifndef WITH_SANDBOX
  g_autofree char* database = NULL;
  girara_setting_get(zathura->ui.session, "database", &database);

  const bool is_null_database = g_strcmp0(database, "null") == 0;

  /* create zathura data directory if database enabled */
  if (is_null_database == false) {
    create_directories(zathura);
  }

  if (g_strcmp0(database, "sqlite") == 0) {
    girara_debug("Using sqlite database backend.");
    g_autofree char* tmp = g_build_filename(zathura->config.data_dir, "bookmarks.sqlite", NULL);
    zathura->database    = zathura_sqldatabase_new(tmp);
  } else if (is_null_database == true) {
    zathura->database = zathura_nulldatabase_new();
  } else {
    zathura->database = zathura_nulldatabase_new();
    girara_error("Database backend '%s' is not supported. Using 'null' database.", database);
  }
#else
  zathura->database = zathura_nulldatabase_new();
#endif

  if (zathura->database == NULL) {
    return false;
  }

  g_object_set(G_OBJECT(zathura->ui.session->command_history), "io", zathura->database, NULL);
  return true;
}

static void init_shortcut_helpers(zathura_t* zathura) {
  zathura->shortcut.mouse.x = 0;
  zathura->shortcut.mouse.y = 0;

  zathura->shortcut.toggle_page_mode.pages = 2;

  zathura->shortcut.toggle_presentation_mode.pages                  = 1;
  zathura->shortcut.toggle_presentation_mode.first_page_column_list = NULL;
  zathura->shortcut.toggle_presentation_mode.zoom                   = 1.0;
}

bool zathura_init(zathura_t* zathura) {
  if (zathura == NULL) {
    return false;
  }

  /* Set application ID */
  g_set_prgname("org.pwmt.zathura");

  /* Add variables for CSS */
  init_css(zathura);

  /* load plugins */
  if (zathura_plugin_manager_load(zathura->plugins.manager) == false) {
    girara_warning("Found no plugins. Please install at least one plugin.");
  }

  /* configuration */
  config_load_default(zathura);
  config_load_files(zathura);

  /* UI */
  if (init_ui(zathura) == false) {
    girara_error("Failed to initialize UI.");
    goto error_free;
  }

  /* database */
  if (init_database(zathura) == false) {
    girara_error("Failed to initialize database.");
    goto error_free;
  }

  /* bookmarks */
  zathura_bookmarks_init(zathura);

  /* jumplist */
  unsigned int jumplist_size = 20;
  girara_setting_get(zathura->ui.session, "jumplist-size", &jumplist_size);
  zathura_jumplist_init(zathura, jumplist_size);

  /* CSS for index mode */
  if (load_css(zathura) == false) {
    girara_error("Failed to initialize CSS.");
    goto error_free;
  }

  /* Shortcut helpers */
  init_shortcut_helpers(zathura);

#ifndef WITH_SANDBOX
  bool dbus = true;
  girara_setting_get(zathura->ui.session, "dbus-service", &dbus);
  if (dbus == true) {
    /* Start D-Bus service */
    zathura->dbus = zathura_dbus_new(zathura);
  }
#endif

  return true;

error_free:
  return false;
}

void zathura_free(zathura_t* zathura) {
  if (zathura == NULL) {
    return;
  }

  document_close(zathura, false);

  /* MIME type detection */
  zathura_content_type_free(zathura->content_type_context);

#ifdef G_OS_UNIX
  if (zathura->signals.sigterm > 0) {
    g_source_remove(zathura->signals.sigterm);
    zathura->signals.sigterm = 0;
  }
#endif

  if (zathura->signals.monitors_handler > 0 && zathura->ui.session != NULL && zathura->ui.session->gtk.window != NULL) {
    GdkDisplay* display = gtk_widget_get_display(zathura->ui.session->gtk.view);
    if (display != NULL) {
      g_signal_handler_disconnect(gdk_display_get_monitors(display), zathura->signals.monitors_handler);
    }
    zathura->signals.monitors_handler = 0;
  }

  /* on a quit that did not close the window first, disconnect cb_destroy so the app's later window disposal can't run
   * it on freed zathura */
  if (zathura->signals.destroy_handler > 0 && zathura->ui.session != NULL && zathura->ui.session->gtk.window != NULL) {
    g_signal_handler_disconnect(zathura->ui.session->gtk.window, zathura->signals.destroy_handler);
    zathura->signals.destroy_handler = 0;
  }

  /* stop D-Bus */
  g_clear_object(&zathura->dbus);

  if (zathura->ui.session != NULL) {
    girara_session_destroy(zathura->ui.session);
  }

  /* shortcut */
  if (zathura->shortcut.toggle_presentation_mode.first_page_column_list != NULL) {
    g_free(zathura->shortcut.toggle_presentation_mode.first_page_column_list);
  }

  /* stdin support */
  if (zathura->stdin_support.file != NULL) {
    g_unlink(zathura->stdin_support.file);
    g_free(zathura->stdin_support.file);
  }

  /* bookmarks */
  zathura_bookmarks_free(zathura);

  /* database */
  g_clear_object(&zathura->database);

  /* free print settings */
  g_clear_object(&zathura->print.settings);
  g_clear_object(&zathura->print.page_setup);

  /* free registered plugins */
  zathura_plugin_manager_free(zathura->plugins.manager);

  /* free config variables */
  g_free(zathura->config.config_dir);
  g_free(zathura->config.data_dir);
  g_free(zathura->config.cache_dir);

  // free search string
  g_free(zathura->global.search_string);

  /* free jumplist */
  zathura_jumplist_free(zathura);

  g_free(zathura);
}

void zathura_set_config_dir(zathura_t* zathura, const char* dir) {
  g_return_if_fail(zathura != NULL);

  if (dir != NULL) {
    g_free(zathura->config.config_dir);
    zathura->config.config_dir = g_strdup(dir);
  }
}

void zathura_set_data_dir(zathura_t* zathura, const char* dir) {
  g_return_if_fail(zathura != NULL);

  if (dir != NULL) {
    g_free(zathura->config.data_dir);
    zathura->config.data_dir = g_strdup(dir);
  }
}

void zathura_set_cache_dir(zathura_t* zathura, const char* dir) {
  g_return_if_fail(zathura != NULL);

  if (dir != NULL) {
    g_free(zathura->config.cache_dir);
    zathura->config.cache_dir = g_strdup(dir);
  }
}

void zathura_set_plugin_dir(zathura_t* zathura, const char* dir) {
  g_return_if_fail(zathura != NULL);
  g_return_if_fail(zathura->plugins.manager != NULL);

  if (dir != NULL) {
    zathura_plugin_manager_set_dir(zathura->plugins.manager, dir);
  }
}

void zathura_set_argv(zathura_t* zathura, char** argv) {
  g_return_if_fail(zathura != NULL);

  zathura->global.arguments = argv;
}

#ifdef G_OS_UNIX
static gchar* prepare_document_open_from_stdin(const char* path) {
  int infileno = -1;
  if (g_strcmp0(path, "-") == 0) {
    infileno = fileno(stdin);
  } else if (g_str_has_prefix(path, "/proc/self/fd/") == true) {
    char* begin = g_strrstr(path, "/") + 1;
    gint64 temp = g_ascii_strtoll(begin, NULL, 10);
    if (temp > INT_MAX || temp < 0) {
      return NULL;
    }
    infileno = (int)temp;
  } else {
    return NULL;
  }

  if (infileno == -1) {
    girara_error("Can not read from file descriptor.");
    return NULL;
  }

  g_autoptr(GInputStream) input_stream = g_unix_input_stream_new(infileno, false);
  if (input_stream == NULL) {
    girara_error("Can not read from file descriptor.");
    return NULL;
  }

  g_autoptr(GFileIOStream) iostream = NULL;
  g_autoptr(GError) error           = NULL;
  g_autoptr(GFile) tmpfile          = g_file_new_tmp("zathura.stdin.XXXXXX", &iostream, &error);
  if (tmpfile == NULL) {
    if (error != NULL) {
      girara_error("Can not create temporary file: %s", error->message);
    }
    return NULL;
  }

  const ssize_t count = g_output_stream_splice(g_io_stream_get_output_stream(G_IO_STREAM(iostream)), input_stream,
                                               G_OUTPUT_STREAM_SPLICE_NONE, NULL, &error);
  if (count == -1) {
    if (error != NULL) {
      girara_error("Can not write to temporary file: %s", error->message);
    }
    g_file_delete(tmpfile, NULL, NULL);
    return NULL;
  }

  return g_file_get_path(tmpfile);
}
#endif

static gchar* prepare_document_open_from_gfile(GFile* source) {
  g_autofree gchar* basename = g_file_get_basename(source);
  g_autofree gchar* template = g_strdup_printf("zathura.gio.XXXXXX.%s", basename);

  g_autoptr(GFileIOStream) iostream = NULL;
  g_autoptr(GError) error           = NULL;
  g_autoptr(GFile) tmpfile          = g_file_new_tmp(template, &iostream, &error);
  if (!tmpfile) {
    if (error) {
      girara_error("Can not create temporary file: %s", error->message);
    }
    return NULL;
  }

  g_autofree gchar* tmpfile_path = g_file_get_path(tmpfile);
  girara_debug("Copying to temporary file: %s", tmpfile_path);

  gboolean rc = g_file_copy(source, tmpfile, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error);
  if (!rc) {
    if (error) {
      girara_error("Can not copy to temporary file: %s", error->message);
    }
    return NULL;
  }

  return g_file_get_path(tmpfile);
}

static gboolean document_info_open(gpointer data) {
  g_return_val_if_fail(data != NULL, FALSE);

  g_autoptr(zathura_document_info_t) document_info = data;
  g_autofree char* uri                             = NULL;
  zathura_t* zathura                               = document_info->zathura;

  if (zathura != NULL && document_info->path != NULL) {
    g_autofree char* file = NULL;
    if (g_strcmp0(document_info->path, "-") == 0 || g_str_has_prefix(document_info->path, "/proc/self/fd/") == true) {
#ifdef G_OS_UNIX
      file = prepare_document_open_from_stdin(document_info->path);
#endif
      if (file == NULL) {
        girara_notify(zathura->ui.session, GIRARA_ERROR,
                      _("Could not read file from stdin and write it to a temporary file."));
      } else {
        if (zathura->stdin_support.file != NULL) {
          g_unlink(zathura->stdin_support.file);
          g_free(zathura->stdin_support.file);
        }
        zathura->stdin_support.file = g_strdup(file);
      }
    } else {
      /* expand ~ and ~user in paths if present */
      g_autofree char* tmp_path = *document_info->path == '~' ? girara_fix_path(document_info->path) : NULL;
      g_autoptr(GFile) gf       = g_file_new_for_commandline_arg(tmp_path != NULL ? tmp_path : document_info->path);

      if (g_file_is_native(gf)) {
        /* file was given as a native path */
        file = g_file_get_path(gf);
      } else {
        /* copy file with GIO */
        uri  = g_file_get_uri(gf);
        file = prepare_document_open_from_gfile(gf);
        if (file == NULL) {
          girara_notify(zathura->ui.session, GIRARA_ERROR,
                        _("Could not read file from GIO and copy it to a temporary file."));
        } else {
          if (zathura->stdin_support.file != NULL) {
            g_unlink(zathura->stdin_support.file);
            g_free(zathura->stdin_support.file);
          }
          zathura->stdin_support.file = g_strdup(file);
        }
      }
    }

    if (file != NULL) {
      if (document_info->synctex != NULL) {
        document_open_synctex(zathura, file, uri, document_info->password, document_info->synctex);
      } else {
        document_open(zathura, file, uri, document_info->password, document_info->page_number, NULL);
      }

      if (document_info->mode != NULL) {
        if (g_strcmp0(document_info->mode, "presentation") == 0) {
          sc_toggle_presentation(zathura->ui.session, NULL, NULL, 0);
        } else if (g_strcmp0(document_info->mode, "fullscreen") == 0) {
          sc_toggle_fullscreen(zathura->ui.session, NULL, NULL, 0);
        } else {
          girara_error("Unknown mode: %s", document_info->mode);
        }
      }

      if (document_info->bookmark_name != NULL) {
        g_autoptr(girara_list_t) arg_list = girara_list_new();
        girara_list_append(arg_list, document_info->bookmark_name);
        cmd_bookmark_open(zathura->ui.session, arg_list);
      }

      if (document_info->search_string) {
        girara_argument_t search_arg;
        search_arg.n    = 1; // Forward search
        search_arg.data = NULL;
        cmd_search(zathura->ui.session, document_info->search_string, &search_arg);
      }
    }
  }

  return FALSE;
}

char* get_formatted_filename(zathura_t* zathura, bool statusbar) {
  bool basename_only           = false;
  const char* file_path        = NULL;
  zathura_document_t* document = zathura_get_document(zathura);
  if (document != NULL) {
    file_path = zathura_document_get_uri(document);
    if (file_path == NULL) {
      file_path = zathura_document_get_path(document);
    }
  }
  if (statusbar == true) {
    girara_setting_get(zathura->ui.session, "statusbar-basename", &basename_only);
  } else {
    girara_setting_get(zathura->ui.session, "window-title-basename", &basename_only);
  }

  if (basename_only == false) {
    bool home_tilde = false;
    if (statusbar) {
      girara_setting_get(zathura->ui.session, "statusbar-home-tilde", &home_tilde);
    } else {
      girara_setting_get(zathura->ui.session, "window-title-home-tilde", &home_tilde);
    }

    const size_t file_path_len = file_path ? strlen(file_path) : 0;

    if (home_tilde == true) {
      g_autofree char* home = girara_get_home_directory(NULL);
      const size_t home_len = home ? strlen(home) : 0;

      if (home_len > 1 && file_path_len >= home_len && g_str_has_prefix(file_path, home) &&
          (!file_path[home_len] || file_path[home_len] == '/')) {
        return g_strdup_printf("~%s", &file_path[home_len]);
      } else {
        return g_strdup(file_path);
      }
    } else {
      return g_strdup(file_path);
    }
  } else {
    const char* basename = zathura_document_get_basename(document);
    return g_strdup(basename);
  }
}

static void cb_document_widget_page_widgets_loaded(ZathuraDocumentWidget* document_widget, zathura_t* zathura) {
  if (document_widget != zathura->ui.document_widget || zathura->sync.pending_search_input == NULL) {
    return;
  }

  g_autofree char* input     = g_steal_pointer(&zathura->sync.pending_search_input);
  girara_argument_t argument = {.n = zathura->sync.pending_search_direction};
  cmd_search(zathura->ui.session, input, &argument);
}

// Create and configure a detached document widget for preparation.
static ZathuraDocumentWidget* document_widget_create(zathura_t* zathura, zathura_document_t* document,
                                                     unsigned int page_v_padding, unsigned int page_h_padding,
                                                     unsigned int pages_per_row, unsigned int first_page_column,
                                                     bool pages_right_to_left) {
  GtkWidget* widget = zathura_document_widget_new(zathura, document);
  if (!widget) {
    return NULL;
  }

  // Apply the startup preference to the first document widget
  bool single_page_mode = false;
  girara_setting_get(zathura->ui.session, "single-page-mode", &single_page_mode);

  // Keep old layout mode if there is a widget
  document_widget_mode_t mode = single_page_mode ? DOCUMENT_WIDGET_SINGLE : DOCUMENT_WIDGET_GRID;
  if (zathura->ui.document_widget) {
    g_object_get(zathura->ui.document_widget, "layout-mode", &mode, NULL);
  }

  ZathuraDocumentWidget* view = g_object_ref_sink(ZATHURA_DOCUMENT_WIDGET(widget));
  zathura_document_widget_set_page_layout(view, page_v_padding, page_h_padding, pages_per_row, first_page_column);
  g_object_set(view, "pages-right-to-left", pages_right_to_left, NULL);
  zathura_document_widget_refresh_layout(view);
  g_object_set(view, "layout-mode", mode, NULL);
  g_signal_connect(view, "page-widgets-loaded", G_CALLBACK(cb_document_widget_page_widgets_loaded), zathura);
  gtk_widget_set_visible(widget, TRUE);

  return view;
}

// Render the focused page once synchronously after the scale is settled
void render_focused_page_now(zathura_t* zathura) {
  if (!zathura->ui.document_widget) {
    return;
  }

  zathura->sync.initial_render_done = true;
  zathura_document_widget_render_current_page(zathura->ui.document_widget);
  // Release render requests for other visible pages that were suppressed by the initial-render hold
  zathura_document_widget_update_visible_pages(zathura->ui.document_widget);
}

static void cb_initial_render_after_paint(GdkFrameClock* clock, zathura_t* zathura) {
  if (zathura->sync.view_painted == false) {
    /* the first frame is painted, request one more so a pending scale change settles before the render */
    zathura->sync.view_painted = true;
    gtk_widget_queue_draw(zathura->ui.view);
    return;
  }

  /* the second frame is painted so the scale and viewport are settled, render the focused page once */
  g_signal_handler_disconnect(clock, zathura->sync.initial_render_handler);
  zathura->sync.initial_render_handler  = 0;
  zathura->sync.initial_render_instance = NULL;
  if (zathura->sync.initial_render_held == true) {
    zathura->sync.initial_render_held = false;
    render_focused_page_now(zathura);
  }
}

/* the frame clock exists only once the view is realised so hook the first paint here */
static void cb_initial_render_map(GtkWidget* view, zathura_t* zathura) {
  g_signal_handler_disconnect(view, zathura->sync.initial_render_handler);

  GdkFrameClock* clock = gtk_widget_get_frame_clock(view);
  if (clock != NULL) {
    zathura->sync.initial_render_handler =
        g_signal_connect(clock, "after-paint", G_CALLBACK(cb_initial_render_after_paint), zathura);
    zathura->sync.initial_render_instance = G_OBJECT(clock);
  } else {
    zathura->sync.initial_render_handler  = 0;
    zathura->sync.initial_render_instance = NULL;
    zathura->sync.initial_render_held     = false;
    zathura->sync.initial_render_done     = true;
  }
}

/* drop a pending first render hold and detach its release handler */
static void initial_render_cancel_hold(zathura_t* zathura) {
  if (zathura->sync.initial_render_instance != NULL) {
    g_clear_signal_handler(&zathura->sync.initial_render_handler, zathura->sync.initial_render_instance);
    zathura->sync.initial_render_instance = NULL;
  }
  zathura->sync.initial_render_held = false;
  zathura->sync.scale_settled       = false;
  zathura->sync.view_painted        = false;
}

static void document_view_prepare(ZathuraDocumentWidget* widget, unsigned int width, unsigned int height) {
  zathura_document_widget_compute_layout(widget);

  g_autoptr(GtkAdjustment) hadjustment = NULL;
  g_autoptr(GtkAdjustment) vadjustment = NULL;
  g_object_get(widget, "hadjustment", &hadjustment, "vadjustment", &vadjustment, NULL);

  gtk_adjustment_set_page_size(hadjustment, width);
  gtk_adjustment_set_page_size(vadjustment, height);
  gtk_adjustment_set_page_increment(hadjustment, width * 0.9);
  gtk_adjustment_set_page_increment(vadjustment, height * 0.9);

  zathura_adjustment_set_value_from_ratio(hadjustment, zathura_document_widget_get_position_x(widget));
  zathura_adjustment_set_value_from_ratio(vadjustment, zathura_document_widget_get_position_y(widget));

  zathura_document_widget_start_page_widget_preload(widget);
}

static void initial_render_hold(zathura_t* zathura) {
  /* hold the first render until the view is painted so it lands at the final zoom and scale */
  if (zathura->sync.initial_render_done == false && zathura->sync.initial_render_held == false) {
    zathura->sync.initial_render_held = true;
    zathura->sync.scale_settled       = false;
    zathura->sync.view_painted        = false;
    GdkFrameClock* clock              = gtk_widget_get_frame_clock(zathura->ui.view);
    if (clock != NULL) {
      zathura->sync.initial_render_handler =
          g_signal_connect(clock, "after-paint", G_CALLBACK(cb_initial_render_after_paint), zathura);
      zathura->sync.initial_render_instance = G_OBJECT(clock);
    } else {
      zathura->sync.initial_render_handler =
          g_signal_connect(zathura->ui.view, "map", G_CALLBACK(cb_initial_render_map), zathura);
      zathura->sync.initial_render_instance = G_OBJECT(zathura->ui.view);
    }
    gtk_widget_queue_draw(zathura->ui.view);
  }
}

static void copy_adjustment(GtkAdjustment* target, GtkAdjustment* prepared) {
  /* A page smaller than the viewport has page_size > upper, which configure() rejects. */
  gtk_adjustment_set_lower(target, gtk_adjustment_get_lower(prepared));
  gtk_adjustment_set_upper(target, gtk_adjustment_get_upper(prepared));
  gtk_adjustment_set_step_increment(target, gtk_adjustment_get_step_increment(prepared));
  gtk_adjustment_set_page_increment(target, gtk_adjustment_get_page_increment(prepared));
  gtk_adjustment_set_page_size(target, gtk_adjustment_get_page_size(prepared));
  gtk_adjustment_set_value(target, gtk_adjustment_get_value(prepared));
}

/* Helper function to redo the search and stay in place */
static gboolean reload_search(gpointer data) {
  g_return_val_if_fail(data, G_SOURCE_REMOVE);

  zathura_t* zathura = data;
  if (!zathura_has_document(zathura) || !zathura->global.search_string || !strlen(zathura->global.search_string)) {
    zathura->sync.reload_search_source = 0;
    girara_debug("no search to redo, aborting");
    return G_SOURCE_REMOVE;
  }

  girara_debug("redoing search");
  // retry later when we have the document widget
  if (!zathura_document_widget_page_widgets_loaded(zathura->ui.document_widget)) {
    return G_SOURCE_CONTINUE;
  }
  zathura->sync.reload_search_source = 0;

  zathura_error_t error = ZATHURA_ERROR_OK;

  unsigned int number_of_pages     = zathura_document_get_number_of_pages(zathura->document);
  unsigned int current_page_number = zathura_document_get_current_page_number(zathura->document);

  /* reset search results */
  zathura->global.total_search_results  = 0;
  zathura->global.current_search_result = 0;

  /* search pages */
  for (unsigned int page_id = 0; page_id < number_of_pages; ++page_id) {
    unsigned int index   = (page_id + current_page_number) % number_of_pages;
    zathura_page_t* page = zathura_document_get_page(zathura->document, index);
    if (!page) {
      continue;
    }

    GtkWidget* page_widget   = zathura_page_get_widget(zathura, page);
    GObject* obj_page_widget = G_OBJECT(page_widget);

    g_autoptr(girara_list_t) result = zathura_page_search_text(page, zathura->global.search_string, &error);
    const size_t result_size        = result ? girara_list_size(result) : 0;

    if (!result_size) {
      g_object_set(obj_page_widget, "search-results", NULL, NULL);
      if (error == ZATHURA_ERROR_NOT_IMPLEMENTED) {
        return G_SOURCE_REMOVE;
      } else {
        continue;
      }
    }

    g_object_set(obj_page_widget, "search-results", g_steal_pointer(&result), NULL);

    if (zathura->global.search_direction == BACKWARD) {
      /* start at bottom hit in page */
      g_object_set(obj_page_widget, "search-current", result_size - 1, NULL);
    } else {
      g_object_set(obj_page_widget, "search-current", 0, NULL);
    }

    zathura->global.total_search_results += result_size;
  }

  /* highlight search results */
  document_draw_search_results(zathura, true);
  return G_SOURCE_REMOVE;
}

static void document_view_install(zathura_t* zathura, document_view_t* pending, ZathuraFileMonitor* monitor,
                                  char* monitor_password, bool replacing) {
  zathura_document_t* document  = pending->document;
  ZathuraDocumentWidget* widget = pending->widget;

  const unsigned int number_of_pages = zathura_document_get_number_of_pages(document);
  const unsigned int page            = zathura_document_get_current_page_number(document);

  g_autoptr(GtkAdjustment) prepared_hadjustment = NULL;
  g_autoptr(GtkAdjustment) prepared_vadjustment = NULL;
  g_object_get(widget, "hadjustment", &prepared_hadjustment, "vadjustment", &prepared_vadjustment, NULL);

  GtkAdjustment* hadjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(zathura->ui.view));
  GtkAdjustment* vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zathura->ui.view));

  // Commit changes. Block adjustment callbacks while installing bounds.
  document_view_t previous = {zathura->document, zathura->ui.document_widget};
  if (replacing) {
    /* store file information */
    save_fileinfo_to_db(zathura);
    /* close current document */
    document_stop(zathura);
#ifndef WITH_SANDBOX
    /* emit DocumentClose signal */
    zathura_dbus_document_close(zathura, zathura_document_get_path(previous.document));
#endif
  }

  g_signal_handlers_block_matched(hadjustment, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, zathura);
  g_signal_handlers_block_matched(vadjustment, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, zathura);

  zathura->document           = g_steal_pointer(&pending->document);
  zathura->ui.document_widget = g_steal_pointer(&pending->widget);

  if (monitor) {
    zathura->file_monitor.monitor = monitor;
    zathura_filemonitor_start(monitor);
    monitor = NULL;
  }

  if (replacing) {
    zathura->sync.initial_render_done = true;
  }

  /* GTK replaces the detached widget's adjustments; transfer its prepared bounds and position. */
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(zathura->ui.view), GTK_WIDGET(widget));
  copy_adjustment(hadjustment, prepared_hadjustment);
  copy_adjustment(vadjustment, prepared_vadjustment);

  g_signal_handlers_unblock_matched(vadjustment, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, zathura);
  g_signal_handlers_unblock_matched(hadjustment, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, zathura);

  document_view_clear(&previous);
  if (monitor_password) {
    g_free(zathura->file_monitor.password);
    zathura->file_monitor.password = monitor_password;
  }
  document_index_clear(zathura);
  girara_set_view(zathura->ui.session, zathura->ui.view);

  // Initialize bisect state
  zathura->bisect.start     = 0;
  zathura->bisect.last_jump = page;
  zathura->bisect.end       = number_of_pages - 1;

  {
    // Update statusbar
    g_autofree char* filename = get_formatted_filename(zathura, true);
    girara_statusbar_item_set_text(zathura->ui.statusbar.file, filename);
    // Update title
    g_autofree char* title = get_formatted_filename(zathura, false);
    girara_set_window_title(zathura->ui.session, title);
  }
  statusbar_page_number_update(zathura);

  zathura_document_widget_update_visible_pages(widget);

#ifndef WITH_SANDBOX
  // Emit DocumentOpen signal
  zathura_dbus_document_open(zathura, zathura_document_get_path(document));
#endif

  if (replacing && zathura->global.search_string && zathura->global.are_search_results_highlighted) {
    zathura->sync.reload_search_source = g_idle_add_full(G_PRIORITY_LOW + 10, reload_search, zathura, NULL);
  }
}

static void document_replacement_disconnect(ZathuraDocumentWidget* widget, zathura_t* zathura) {
  const char* signals[] = {"ready", "render-failed"};
  for (size_t i = 0; i < G_N_ELEMENTS(signals); ++i) {
    g_signal_handlers_disconnect_matched(widget, G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA,
                                         g_signal_lookup(signals[i], ZATHURA_TYPE_DOCUMENT_WIDGET), 0, NULL, NULL,
                                         zathura);
  }
}

static void document_replacement_free(document_replacement_t* replacement, zathura_t* zathura) {
  if (replacement) {
    if (replacement->view.widget) {
      document_replacement_disconnect(replacement->view.widget, zathura);
    }
    document_view_clear(&replacement->view);
    g_clear_object(&replacement->monitor);
    g_free(replacement->password);
    g_free(replacement);
  }
}

static void document_replacement_cancel(zathura_t* zathura) {
  document_replacement_t* replacement = g_steal_pointer(&zathura->sync.replacement);
  document_replacement_free(replacement, zathura);
}

static void document_replacement_ready(ZathuraDocumentWidget* widget, zathura_t* zathura) {
  document_replacement_t* replacement = g_steal_pointer(&zathura->sync.replacement);
  document_replacement_disconnect(widget, zathura);

  // Capture live navigation immediately before replacing the active view.
  const unsigned int page                 = zathura_document_get_current_page_number(zathura->document);
  const double position_x                 = zathura_document_widget_get_position_x(zathura->ui.document_widget);
  const double position_y                 = zathura_document_widget_get_position_y(zathura->ui.document_widget);
  const double zoom                       = zathura_document_get_zoom(zathura->document);
  const unsigned int rotation             = zathura_document_get_rotation(zathura->document);
  const zathura_adjust_mode_t adjust_mode = zathura_document_widget_get_adjust_mode(zathura->ui.document_widget);

  document_widget_mode_t layout_mode;
  g_object_get(zathura->ui.document_widget, "layout-mode", &layout_mode, NULL);

  document_view_install(zathura, &replacement->view, g_steal_pointer(&replacement->monitor),
                        g_steal_pointer(&replacement->password), true);
  document_replacement_free(replacement, zathura);

  // Apply changes made while the replacement was rendering to the installed view.
  bool zoom_or_rotation_changed = false;
  if (zathura_document_get_zoom(zathura->document) != zoom) {
    zathura_document_set_zoom(zathura->document, zoom);
    zoom_or_rotation_changed = true;
  }
  if (zathura_document_get_rotation(zathura->document) != rotation) {
    zathura_document_set_rotation(zathura->document, rotation);
    zoom_or_rotation_changed = true;
  }
  if (zoom_or_rotation_changed) {
    zathura_document_widget_render_all(widget);
  }

  if (zathura_document_widget_get_adjust_mode(widget) != adjust_mode) {
    zathura_document_widget_set_adjust_mode(widget, adjust_mode);
  }
  g_object_set(widget, "layout-mode", layout_mode, NULL);

  unsigned int pages_per_row = 1;
  girara_setting_get(zathura->ui.session, "pages-per-row", &pages_per_row);
  g_autofree char* first_page_column_list = NULL;
  girara_setting_get(zathura->ui.session, "first-page-column", &first_page_column_list);
  const unsigned int first_page_column = find_first_page_column(first_page_column_list, pages_per_row);

  bool page_right_to_left = false;
  girara_setting_get(zathura->ui.session, "page-right-to-left", &page_right_to_left);
  gboolean widget_right_to_left = FALSE;
  g_object_get(widget, "pages-right-to-left", &widget_right_to_left, NULL);

  if (zathura_document_widget_get_pages_per_row(widget) != pages_per_row ||
      zathura_document_widget_get_first_page_column(widget) != first_page_column ||
      widget_right_to_left != page_right_to_left) {
    zathura_document_widget_set_page_layout(widget, zathura_document_widget_get_page_v_padding(widget),
                                            zathura_document_widget_get_page_h_padding(widget), pages_per_row,
                                            first_page_column);
    g_object_set(widget, "pages-right-to-left", page_right_to_left, NULL);
    zathura_document_widget_refresh_layout(widget);
  }

  // The following internally check if a change triggers any effects.
  bool recolor = false;
  girara_setting_get(zathura->ui.session, "recolor", &recolor);
  zathura_document_widget_set_recolor(widget, recolor);
  girara_setting_get(zathura->ui.session, "recolor-keephue", &recolor);
  zathura_document_widget_set_recolor_hue(widget, recolor);
  girara_setting_get(zathura->ui.session, "recolor-reverse-video", &recolor);
  zathura_document_widget_set_recolor_reverse_video(widget, recolor);
  girara_setting_get(zathura->ui.session, "recolor-adjust-lightness", &recolor);
  zathura_document_widget_set_recolor_adjust_lightness(widget, recolor);

  g_autofree char* recolor_light = NULL;
  g_autofree char* recolor_dark  = NULL;
  girara_setting_get(zathura->ui.session, "recolor-lightcolor", &recolor_light);
  girara_setting_get(zathura->ui.session, "recolor-darkcolor", &recolor_dark);
  zathura_document_widget_set_recolor_colors(widget, recolor_light, recolor_dark);

  // Only apply old position if open-first-page is not set.
  bool always_first_page = false;
  girara_setting_get(zathura->ui.session, "open-first-page", &always_first_page);
  if (!always_first_page) {
    const unsigned int count = zathura_document_get_number_of_pages(zathura->document);
    const double target_y    = page < count ? position_y : -1;
    page_set(zathura, MIN(page, count - 1), position_x, target_y, false);
    if (zathura_document_widget_mode_change_pending(widget)) {
      // Complete the mode transition before restoring its page-local scroll position.
      zathura_document_widget_set_position(widget, position_x, target_y);
      refresh_view(zathura);
    }
  }
  zathura_document_widget_update_visible_pages(widget);
}

static void document_replacement_failed(ZathuraDocumentWidget* UNUSED(widget), zathura_t* zathura) {
  document_replacement_cancel(zathura);
}

bool document_open(zathura_t* zathura, const char* path, const char* uri, const char* password, int page_number,
                   zathura_fileinfo_t* file_info_p) {
  if (zathura == NULL || zathura->plugins.manager == NULL || path == NULL) {
    return false;
  }

  const bool replacing = zathura_has_document(zathura);
  g_return_val_if_fail(!replacing || file_info_p != NULL, false);
  document_replacement_cancel(zathura);

  g_autofree char* first_page_column_list = NULL;
  g_autofree char* monitor_password       = g_strdup(password);

  /* FIXME: since there are many call chains leading here, check again if we need to expand ~ or
   * ~user. We should fix all call sites instead */
  g_autofree char* tmp_path = *path == '~' ? girara_fix_path(path) : NULL;
  girara_debug("opening document: %s", tmp_path != NULL ? tmp_path : path);

  zathura_error_t error = ZATHURA_ERROR_OK;
  zathura_document_t* document =
      zathura_document_open(zathura, tmp_path != NULL ? tmp_path : path, uri, monitor_password, &error);

  if (!document) {
    if (error == ZATHURA_ERROR_INVALID_PASSWORD) {
      girara_debug("Invalid or no password.");
      g_autoptr(zathura_password_dialog_info_t) password_dialog_info =
          g_try_malloc0(sizeof(zathura_password_dialog_info_t));
      if (!password_dialog_info) {
        return false;
      }

      password_dialog_info->zathura = zathura;
      password_dialog_info->path    = g_strdup(path);
      password_dialog_info->uri     = g_strdup(uri);
      if (password_dialog_info->path != NULL) {
        g_idle_add(document_open_password_dialog, g_steal_pointer(&password_dialog_info));
        return false;
      }
    }
    if (error == ZATHURA_ERROR_OK) {
      girara_notify(zathura->ui.session, GIRARA_ERROR,
                    _("Unsupported file type. Please install the necessary plugin."));
    }
    return false;
  }

  g_auto(document_view_t) pending = {.document = document};

  const char* file_path        = zathura_document_get_path(document);
  unsigned int number_of_pages = zathura_document_get_number_of_pages(document);

  if (!number_of_pages) {
    girara_notify(zathura->ui.session, GIRARA_WARNING, _("Document does not contain any pages"));
    return false;
  }

  // Read file info from database
  zathura_fileinfo_t file_info = {
      .current_page           = 0,
      .page_offset            = 0,
      .zoom                   = 1,
      .rotation               = 0,
      .pages_per_row          = 0,
      .first_page_column_list = NULL,
      .page_right_to_left     = false,
      .position_x             = 0,
      .position_y             = 0,
  };

  bool known_file = file_info_p != NULL;
  if (file_info_p) {
    file_info              = *file_info_p;
    first_page_column_list = g_strdup(file_info.first_page_column_list);
  } else {
    const uint8_t* file_hash =
        zathura_db_supports_hash_queries(zathura->database) ? zathura_document_get_hash(document) : NULL;
    known_file             = zathura_db_get_fileinfo(zathura->database, file_path, file_hash, &file_info);
    first_page_column_list = g_steal_pointer(&file_info.first_page_column_list);
  }
  girara_debug("checking file info: found = %s", known_file ? "true" : "false");

  // Set page offset
  zathura_document_set_page_offset(document, file_info.page_offset);

  // check for valid zoom value
  if (file_info.zoom <= DBL_EPSILON) {
    file_info.zoom = 1;
  }
  zathura_document_set_zoom(document, zathura_correct_zoom_value(zathura->ui.session, file_info.zoom));

  // Check current page number; if it wasn't specified on the command-line, get it from file_info
  if (page_number == ZATHURA_PAGE_NUMBER_UNSPECIFIED) {
    girara_debug("using page from file info: %d", file_info.current_page);
    page_number = file_info.current_page;
  }
  if (page_number < 0) {
    page_number += number_of_pages;
  }

  if ((unsigned)page_number >= number_of_pages) {
    girara_warning("document info: '%s' has an invalid page number", file_path);
    zathura_document_set_current_page_number(document, 0);
  } else {
    girara_debug("setting current page: %d", page_number);
    zathura_document_set_current_page_number(document, page_number);
  }

  // check for valid rotation
  if (file_info.rotation % 90 != 0) {
    girara_warning("document info: '%s' has an invalid rotation", file_path);
    zathura_document_set_rotation(document, 0);
  } else {
    zathura_document_set_rotation(document, file_info.rotation % 360);
  }

  // jump to first page if setting enabled
  bool always_first_page = false;
  girara_setting_get(zathura->ui.session, "open-first-page", &always_first_page);
  if (always_first_page == true) {
    girara_debug("setting current page: 0 (always open first page)");
    zathura_document_set_current_page_number(document, 0);
  }

  // Parse the displayed page and size the others from it until they are parsed
  const unsigned int current_page_number = zathura_document_get_current_page_number(document);
  zathura_page_t* current_page           = zathura_document_get_page(document, current_page_number);
  if (!current_page || !zathura_page_load(current_page, NULL)) {
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("Failed to parse the displayed page of the document"));
    return false;
  }

  const double width  = zathura_page_get_width(current_page);
  const double height = zathura_page_get_height(current_page);
  for (unsigned int page_id = 0; page_id < number_of_pages; page_id++) {
    zathura_page_t* page = zathura_document_get_page(document, page_id);
    if (page && page != current_page) {
      zathura_page_set_width(page, width);
      zathura_page_set_height(page, height);
    }
  }

  // Initialize scale before creating page layouts or submitting render requests.
  GtkNative* native   = gtk_widget_get_native(zathura->ui.session->gtk.view);
  GdkSurface* surface = native != NULL ? gtk_native_get_surface(native) : NULL;
  const double device_factor =
      surface != NULL ? gdk_surface_get_scale(surface) : gtk_widget_get_scale_factor(zathura->ui.session->gtk.view);
  zathura_document_set_device_factors(document, device_factor, device_factor);
  double ppi = 0.0;
  if (get_view_ppi(zathura, &ppi)) {
    zathura_document_set_viewport_ppi(document, ppi);
  }

  // Set view mode
  unsigned int pages_per_row  = file_info.pages_per_row;
  unsigned int page_v_padding = 1, page_h_padding = 1;
  girara_setting_get(zathura->ui.session, "page-v-padding", &page_v_padding);
  girara_setting_get(zathura->ui.session, "page-h-padding", &page_h_padding);
  if (!pages_per_row) {
    girara_setting_get(zathura->ui.session, "pages-per-row", &pages_per_row);
  }

  // Read first_page_column list
  if (!first_page_column_list || *first_page_column_list == '\0') {
    g_clear_pointer(&first_page_column_list, g_free);
    girara_setting_get(zathura->ui.session, "first-page-column", &first_page_column_list);
  }

  // Find value for first_page_column
  const unsigned int first_page_column = find_first_page_column(first_page_column_list, pages_per_row);
  // Create and fully configure the view only once there is a document to display
  pending.widget = document_widget_create(zathura, document, page_v_padding, page_h_padding, pages_per_row,
                                          first_page_column, file_info.page_right_to_left);
  if (!pending.widget) {
    return false;
  }

  ZathuraDocumentWidget* widget = pending.widget;
  // The blocks below can change the current page number due to callbacks that are being executed. So we store the
  // page number here and reset it below.
  const unsigned int page = zathura_document_get_current_page_number(document);
  // Apply open adjustment
  if (!known_file) {
    g_autofree char* adjust_open = NULL;
    girara_setting_get(zathura->ui.session, "adjust-open", &adjust_open);
    if (g_strcmp0(adjust_open, "best-fit") == 0) {
      zathura_document_widget_set_adjust_mode(widget, ZATHURA_ADJUST_BESTFIT);
    } else if (g_strcmp0(adjust_open, "width") == 0) {
      zathura_document_widget_set_adjust_mode(widget, ZATHURA_ADJUST_WIDTH);
    }
  }

  // Page widgets are created on demand, not all at once
  if (!zathura_document_widget_ensure_page(widget, page)) {
    return false;
  }

  // Get viewport size
  GtkAdjustment* hadjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(zathura->ui.view));
  GtkAdjustment* vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zathura->ui.view));
  unsigned int view_width    = floor(gtk_adjustment_get_page_size(hadjustment));
  unsigned int view_height   = floor(gtk_adjustment_get_page_size(vadjustment));
  if (replacing) {
    zathura_document_widget_get_viewport_size(zathura->ui.document_widget, &view_height, &view_width);
  }
  zathura_document_widget_set_viewport_width(widget, view_width);
  zathura_document_widget_set_viewport_height(widget, view_height);
  zathura_document_widget_adjust_view(widget);

  // Restore the (document-relative) position when opening the page saved in history.
  double position_x = -1, position_y = -1;
  if (file_info.current_page == page && (file_info.position_x != 0 || file_info.position_y != 0)) {
    position_x = file_info.position_x;
    position_y = file_info.position_y;
  }
  zathura_document_widget_set_position(widget, position_x, position_y);
  bool show_signature_information = false;
  girara_setting_get(zathura->ui.session, "show-signature-information", &show_signature_information);
  zathura_document_widget_set_draw_signatures(widget, show_signature_information);

  // Apply default page mode
  {
    g_autofree char* page_mode = NULL;
    girara_setting_get(zathura->ui.session, "page-mode", &page_mode);
    if (g_strcmp0(page_mode, "equal_width") == 0) {
      zathura_document_widget_set_equal_mode(widget, ZATHURA_EQUAL_WIDTH);
    } else if (g_strcmp0(page_mode, "equal_height") == 0) {
      zathura_document_widget_set_equal_mode(widget, ZATHURA_EQUAL_HEIGHT);
    }
  }

  // Install file monitor
  g_autoptr(ZathuraFileMonitor) monitor = NULL;
  if (!zathura->file_monitor.monitor) {
    g_autofree char* filemonitor_backend = NULL;
    girara_setting_get(zathura->ui.session, "filemonitor", &filemonitor_backend);
    girara_debug("creating file monitor with backend: %s", filemonitor_backend);

    zathura_filemonitor_type_t type = ZATHURA_FILEMONITOR_GLIB;
    if (g_strcmp0(filemonitor_backend, "noop") == 0) {
      type = ZATHURA_FILEMONITOR_NOOP;
    }
#ifdef G_OS_UNIX
    else if (g_strcmp0(filemonitor_backend, "signal") == 0) {
      type = ZATHURA_FILEMONITOR_SIGNAL;
    }
#endif

    monitor = zathura_filemonitor_new(file_path, type);
    if (!monitor) {
      return false;
    }

    g_signal_connect(monitor, "reload-file", G_CALLBACK(cb_file_monitor), zathura->ui.session);
  }

  // Reloads retain the live bookmarks, jumplist and marks. An initial open loads history.
  if (!replacing) {
    // Bookmarks
    if (!zathura_bookmarks_load(zathura, file_path)) {
      girara_debug("Failed to load bookmarks.");
    }

    // Jumplist
    if (!zathura_jumplist_load(zathura, file_path)) {
      girara_debug("Failed to load jumplist.");
    }

    // Quickmarks
    if (!zathura_quickmarks_load(zathura, file_path)) {
      girara_debug("Failed to load quickmarks.");
      zathura->global.marks = girara_list_new_with_free(g_free);
    }

    if (!zathura_jumplist_is_initialized(zathura) || !zathura->global.marks) {
      girara_error("No jumplist or no marks");
      return false;
    }
  }

  // Process layout preferences before rendering without applying them to the active view.
  // FIXME: this is a hack
  zathura_document_t* active_document = g_steal_pointer(&zathura->document);
  girara_setting_set(zathura->ui.session, "pages-per-row", &pages_per_row);
  girara_setting_set(zathura->ui.session, "first-page-column", first_page_column_list);
  zathura->document = g_steal_pointer(&active_document);

  document_view_prepare(widget, view_width, view_height);

  if (replacing) {
    document_replacement_t* replacement = g_new0(document_replacement_t, 1);

    replacement->view.document = g_steal_pointer(&pending.document);
    replacement->view.widget   = g_steal_pointer(&pending.widget);
    replacement->monitor       = g_steal_pointer(&monitor);
    replacement->password      = g_steal_pointer(&monitor_password);

    zathura->sync.replacement = replacement;

    g_signal_connect(widget, "ready", G_CALLBACK(document_replacement_ready), zathura);
    g_signal_connect(widget, "render-failed", G_CALLBACK(document_replacement_failed), zathura);

    if (!zathura_document_widget_prepare_visible_pages(widget, position_x, position_y)) {
      document_replacement_cancel(zathura);
      return false;
    }
    return true;
  }

  initial_render_hold(zathura);
  document_view_install(zathura, &pending, g_steal_pointer(&monitor), g_steal_pointer(&monitor_password), false);

  return true;
}

bool document_open_synctex(zathura_t* zathura, const char* path, const char* uri, const char* password,
                           const char* synctex) {
  bool ret = document_open(zathura, path, uri, password, ZATHURA_PAGE_NUMBER_UNSPECIFIED, NULL);
  if (ret == false) {
    return false;
  }
  if (synctex == NULL) {
    return true;
  }

  int line                    = 0;
  int column                  = 0;
  g_autofree char* input_file = NULL;
  if (synctex_parse_input(synctex, &input_file, &line, &column) == false) {
    return false;
  }

  return synctex_view(zathura, input_file, line, column);
}

void document_open_idle(zathura_t* zathura, const char* path, const char* password, int page_number, const char* mode,
                        const char* synctex, const char* bookmark_name, const char* search_string) {
  g_return_if_fail(zathura != NULL);
  g_return_if_fail(path != NULL);

  zathura_document_info_t* document_info = g_try_malloc0(sizeof(zathura_document_info_t));
  if (document_info == NULL) {
    return;
  }

  document_info->zathura = zathura;
  // there is no need to check if arguments are NULL before calling g_strdup since g_strdup(NULL) == NULL
  document_info->path          = g_strdup(path);
  document_info->password      = g_strdup(password);
  document_info->mode          = g_strdup(mode);
  document_info->synctex       = g_strdup(synctex);
  document_info->bookmark_name = g_strdup(bookmark_name);
  document_info->search_string = g_strdup(search_string);
  document_info->page_number   = page_number;

  g_idle_add(document_info_open, document_info);
}

bool document_save(zathura_t* zathura, const char* path, bool overwrite) {
  zathura_document_t* document = zathura_get_document(zathura);
  g_return_val_if_fail(document, false);
  g_return_val_if_fail(path, false);

  g_autofree gchar* file_path = girara_fix_path(path);
  /* use current basename if path points to a directory  */
  if (g_file_test(file_path, G_FILE_TEST_IS_DIR)) {
    g_autofree char* basename = g_path_get_basename(zathura_document_get_path(document));
    g_autofree char* tmp      = file_path;
    file_path                 = g_build_filename(file_path, basename, NULL);
  }

  if ((overwrite == false) && g_file_test(file_path, G_FILE_TEST_EXISTS)) {
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("File already exists: %s. Use :write! to overwrite it."),
                  file_path);
    return false;
  }

  const zathura_error_t error = zathura_document_save_as(document, file_path);
  if (error != ZATHURA_ERROR_OK) {
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("Failed to save document."));
    return false;
  }

  girara_notify(zathura->ui.session, GIRARA_INFO, _("Document saved."));
  return true;
}

static zathura_fileinfo_t zathura_get_document_fileinfo(zathura_t* zathura, zathura_document_t* document,
                                                        ZathuraDocumentWidget* document_widget) {
  /* Caller needs to g_free(file_info.first_page_column_list) */

  zathura_fileinfo_t file_info = {
      .current_page           = zathura_document_get_current_page_number(document),
      .page_offset            = zathura_document_get_page_offset(document),
      .zoom                   = zathura_document_get_zoom(document),
      .rotation               = zathura_document_get_rotation(document),
      .pages_per_row          = 1,
      .first_page_column_list = "1:2",
      .page_right_to_left     = false,
      .position_x             = zathura_document_widget_get_position_x(document_widget),
      .position_y             = zathura_document_widget_get_position_y(document_widget),
  };

  girara_setting_get(zathura->ui.session, "pages-per-row", &file_info.pages_per_row);
  girara_setting_get(zathura->ui.session, "first-page-column", &file_info.first_page_column_list);
  girara_setting_get(zathura->ui.session, "page-right-to-left", &file_info.page_right_to_left);

  return file_info;
}

zathura_fileinfo_t zathura_get_fileinfo(zathura_t* zathura) {
  return zathura_get_document_fileinfo(zathura, zathura_get_document(zathura), zathura->ui.document_widget);
}

static void save_fileinfo_to_db(zathura_t* zathura) {
  zathura_document_t* document = zathura_get_document(zathura);
  const char* path             = zathura_document_get_path(document);
  const uint8_t* file_hash =
      zathura_db_supports_hash_queries(zathura->database) ? zathura_document_get_hash(document) : NULL;

  zathura_fileinfo_t file_info = zathura_get_fileinfo(zathura);

  /* save file info */
  zathura_db_set_fileinfo(zathura->database, path, file_hash, &file_info);
  /* save jumplist */
  zathura_db_save_jumplist(zathura->database, path, zathura->jumplist.list);
  /* save quickmarks */
  zathura_db_save_quickmarks(zathura->database, path, zathura->global.marks);

  g_free(file_info.first_page_column_list);
}

// Stop active rendering and preloading and cancel pending initial-render and search work.
static void document_stop(zathura_t* zathura) {
  if (zathura->sync.reload_search_source != 0) {
    g_source_remove(zathura->sync.reload_search_source);
    zathura->sync.reload_search_source = 0;
  }
  zathura->ui.inputbar_adjust_mode = ZATHURA_ADJUST_NONE;

  zathura_visual_reset(zathura);

  if (zathura->ui.document_widget != NULL) {
    zathura_document_widget_stop(zathura->ui.document_widget);
  }

  g_clear_pointer(&zathura->sync.pending_search_input, g_free);

  // drop a first render hold that never got released
  initial_render_cancel_hold(zathura);

#ifdef WITH_SYNCTEX
  // invalidate synctex scanner */
  g_clear_pointer(&zathura->synctex.scanner, synctex_scanner_free);
#endif
}

/**
 * Release the document index widget and reset its navigation position.
 *
 * @param zathura A non-NULL session
 */
static void document_index_clear(zathura_t* zathura) {
  /* remove index */
  if (zathura->ui.index != NULL) {
    GtkWidget* parent = gtk_widget_get_parent(zathura->ui.index);
    if (GTK_IS_STACK(parent)) {
      gtk_stack_remove(GTK_STACK(parent), zathura->ui.index);
    } else {
      g_object_ref_sink(zathura->ui.index);
      g_object_unref(zathura->ui.index);
    }
    zathura->ui.index = NULL;
  }
  zathura->global.current_index_position = 0;
}

bool document_close(zathura_t* zathura, bool keep_monitor) {
  g_return_val_if_fail(zathura != NULL, false);
  document_replacement_cancel(zathura);
  if (!zathura_has_document(zathura)) {
    return false;
  }

  /* store file information */
  save_fileinfo_to_db(zathura);
  document_stop(zathura);
  if (!keep_monitor) {
    /* remove monitor */
    g_clear_object(&zathura->file_monitor.monitor);
    g_clear_pointer(&zathura->file_monitor.password, g_free);
    // also free old search string in this case
    g_clear_pointer(&zathura->global.search_string, g_free);
    zathura->global.are_search_results_highlighted = false;
  }
  /* remove marks */
  g_clear_pointer(&zathura->global.marks, girara_list_free);
  zathura_jumplist_clear(zathura);

  document_view_t previous = {zathura->document, zathura->ui.document_widget};
#ifndef WITH_SANDBOX
  /* emit DocumentClose signal */
  zathura_dbus_document_close(zathura, zathura_document_get_path(previous.document));
#endif

  // TODO: replace with g_steal_pointer
  zathura->document           = NULL;
  zathura->ui.document_widget = NULL;

  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(zathura->ui.view), NULL);
  document_view_clear(&previous);

  // The remaining part only refreshes the UI; skip it when the window is gone
  if (!zathura->ui.session || !zathura->ui.session->gtk.window) {
    return true;
  }

  document_index_clear(zathura);
  statusbar_page_number_update(zathura);
  if (zathura->ui.statusbar.file) {
    girara_statusbar_item_set_text(zathura->ui.statusbar.file, _("[No name]"));
  }

  // Update title
  girara_set_window_title(zathura->ui.session, "zathura");

  return true;
}

bool page_set(zathura_t* zathura, unsigned int page_id, double position_x, double position_y, bool page_relative) {
  if (zathura_has_document(zathura) == false) {
    return false;
  }

  zathura_document_t* document = zathura_get_document(zathura);
  zathura_page_t* page         = zathura_document_get_page(document, page_id);
  if (page == NULL) {
    return false;
  }

  zathura_document_set_current_page_number(document, page_id);
  zathura_document_widget_update_mode(ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget));

  bool continuous_hist_save = false;
  girara_setting_get(zathura->ui.session, "continuous-hist-save", &continuous_hist_save);
  if (continuous_hist_save) {
    save_fileinfo_to_db(zathura);
  }

  // convert page-relative positions to document-relative positions
  ZathuraDocumentWidget* widget = zathura->ui.document_widget;
  if (page_relative && (position_x >= 0 || position_y >= 0)) {
    unsigned int page_width = 0, page_height = 0, doc_width = 0, doc_height = 0;
    page_calc_height_width(document, page, &page_height, &page_width, true);

    document_widget_mode_t layout_mode;
    g_object_get(widget, "layout-mode", &layout_mode, NULL);

    double origin_x = 0, origin_y = 0;
    if (layout_mode == DOCUMENT_WIDGET_SINGLE) {
      doc_width  = page_width;
      doc_height = page_height;
    } else {
      zathura_document_widget_get_document_size(widget, &doc_height, &doc_width);
      zathura_document_widget_get_page_origin(widget, page_id, &origin_x, &origin_y);
    }

    if (position_x >= 0) {
      position_x = doc_width > 0 ? CLAMP((origin_x + position_x * page_width) / doc_width, 0.0, 1.0) : -1;
    }
    if (position_y >= 0) {
      position_y = doc_height > 0 ? CLAMP((origin_y + position_y * page_height) / doc_height, 0.0, 1.0) : -1;
    }
  }

  return position_set(zathura, position_x, position_y);
}

void statusbar_page_number_update(zathura_t* zathura) {
  if (zathura == NULL || zathura->ui.statusbar.page_number == NULL) {
    return;
  }

  zathura_document_t* document = zathura_get_document(zathura);
  if (document != NULL) {
    unsigned int number_of_pages     = zathura_document_get_number_of_pages(document);
    unsigned int current_page_number = zathura_document_get_current_page_number(document);
    unsigned int page_number_percent = number_of_pages ? 100 * (current_page_number + 1) / number_of_pages : 0;

    zathura_page_t* page   = zathura_document_get_page(document, current_page_number);
    const char* page_label = zathura_page_get_label(page, NULL);

    bool show_percent = false;
    girara_setting_get(zathura->ui.session, "statusbar-page-percent", &show_percent);

    g_autofree char* page_number_text = NULL;
    if (page_label != NULL && zathura_page_label_is_number(page) == false) {
      if (show_percent) {
        page_number_text = g_strdup_printf("[%s (%d/%d) (%d%%)]", page_label, current_page_number + 1, number_of_pages,
                                           page_number_percent);
      } else {
        page_number_text = g_strdup_printf("[%s (%d/%d)]", page_label, current_page_number + 1, number_of_pages);
      }
    } else {
      if (show_percent) {
        page_number_text =
            g_strdup_printf("[%d/%d (%d%%)]", current_page_number + 1, number_of_pages, page_number_percent);
      } else {
        page_number_text = g_strdup_printf("[%d/%d]", current_page_number + 1, number_of_pages);
      }
    }
    girara_statusbar_item_set_text(zathura->ui.statusbar.page_number, page_number_text);

    bool page_number_in_window_title = false;
    girara_setting_get(zathura->ui.session, "window-title-page", &page_number_in_window_title);

    if (page_number_in_window_title == true) {
      g_autofree char* filename = get_formatted_filename(zathura, false);
      g_autofree char* title    = g_strdup_printf("%s %s", filename, page_number_text);
      girara_set_window_title(zathura->ui.session, title);
    }
  } else {
    girara_statusbar_item_set_text(zathura->ui.statusbar.page_number, "");
  }
}

bool position_set(zathura_t* zathura, double position_x, double position_y) {
  zathura_document_t* document = zathura_get_document(zathura);
  if (document == NULL) {
    return false;
  }

  // FIXME: why do we end up here during mode changes?
  // allocation will position the most recently selected page using the new layout.
  if (zathura_document_widget_mode_change_pending(zathura->ui.document_widget)) {
    girara_debug("Handling position change while processing page mode change.");
    return true;
  }

  // Cancel current kinetic scrolling.
  // This makes sure that GtkScrolledWindow's internal scroll state is synced to
  // adjustment values later set by zathura_document_set_position_ functions.
  // When later GtkScrolledWindow happen to handle GDK_SCROLL events, it makes
  // sure that the adjustment values are modified correctly.
  gtk_scrolled_window_set_kinetic_scrolling(GTK_SCROLLED_WINDOW(zathura->ui.view), FALSE);
  gtk_scrolled_window_set_kinetic_scrolling(GTK_SCROLLED_WINDOW(zathura->ui.view), TRUE);

  zathura_document_widget_set_position(zathura->ui.document_widget, position_x, position_y);

  /* trigger a 'change' event for both adjustments */
  refresh_view(zathura);

  return true;
}

void zathura_inputbar_save_adjust_mode(zathura_t* zathura) {
  g_return_if_fail(zathura != NULL);
  if (zathura->ui.document_widget == NULL) {
    return;
  }

  const zathura_adjust_mode_t mode = zathura_document_widget_get_adjust_mode(zathura->ui.document_widget);
  if (mode != ZATHURA_ADJUST_INPUTBAR) {
    zathura->ui.inputbar_adjust_mode = mode;
    zathura_document_widget_set_adjust_mode(zathura->ui.document_widget, ZATHURA_ADJUST_INPUTBAR);
  }
}

void zathura_inputbar_restore_adjust_mode(zathura_t* zathura) {
  g_return_if_fail(zathura != NULL);
  if (zathura_document_widget_get_adjust_mode(zathura->ui.document_widget) == ZATHURA_ADJUST_INPUTBAR) {
    zathura_document_widget_set_adjust_mode(zathura->ui.document_widget, zathura->ui.inputbar_adjust_mode);
  }
  zathura->ui.inputbar_adjust_mode = ZATHURA_ADJUST_NONE;
}

void refresh_view(zathura_t* zathura) {
  g_return_if_fail(zathura != NULL);

  /* emit a custom refresh-view signal */
  g_signal_emit(zathura->ui.session->gtk.view, zathura->signals.refresh_view, 0);
}

#ifdef G_OS_UNIX
static gboolean zathura_signal_sigterm(gpointer UNUSED(data)) {
  GApplication* app = g_application_get_default();
  if (app != NULL) {
    g_application_quit(app);
  }

  return G_SOURCE_CONTINUE;
}
#endif

bool zathura_has_document(zathura_t* zathura) {
  return zathura != NULL && zathura->document != NULL;
}

zathura_document_t* zathura_get_document(zathura_t* zathura) {
  g_return_val_if_fail(zathura != NULL, NULL);

  return zathura->document;
}

void zathura_modify_current_search_result(zathura_t* zathura, int diff) {
  if (zathura->global.total_search_results == 0 || diff == 0) {
    return;
  }

  int current = zathura->global.current_search_result;
  int total   = zathura->global.total_search_results;
  current += diff;
  if (current >= total) {
    current = current % total;
  } else if (current < 0) {
    current = total + (current % total);
  }

  if (current == 0) {
    current = total;
  }

  zathura->global.current_search_result = current;
}

void zathura_set_current_search_result_previous_pages(zathura_t* zathura, unsigned int current_page_number) {
  zathura->global.current_search_result =
      zathura_document_widget_get_search_result_count(zathura->ui.document_widget, current_page_number);
}
