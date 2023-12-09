/* SPDX-License-Identifier: Zlib */

#include <errno.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <girara/datastructures.h>
#include <girara/utils.h>
#include <girara/session.h>
#include <girara/statusbar.h>
#include <girara/settings.h>
#include <girara/shortcuts.h>
#include <girara/template.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#ifdef G_OS_UNIX
#include <glib-unix.h>
#include <gio/gunixinputstream.h>
#endif

#include "bookmarks.h"
#include "callbacks.h"
#include "config.h"
#include "commands.h"
#ifdef WITH_SQLITE
#include "database-sqlite.h"
#endif
#include "database-plain.h"
#include "document.h"
#include "shortcuts.h"
#include "zathura.h"
#include "utils.h"
#include "marks.h"
#include "render.h"
#include "page.h"
#include "page-widget.h"
#include "plugin.h"
#include "adjustment.h"
#include "dbus-interface.h"
#include "resources.h"
#include "synctex.h"
#include "content-type.h"
#ifdef WITH_SECCOMP
#include "seccomp-filters.h"
#endif

typedef struct zathura_document_info_s {
  zathura_t* zathura;
  char* path;
  char* password;
  int page_number;
  char* mode;
  char* synctex;
  char* bookmark_name;
  char* search_string;
} zathura_document_info_t;

static gboolean document_info_open(gpointer data);

#ifdef G_OS_UNIX
static gboolean zathura_signal_sigterm(gpointer data);
#endif

static void
free_document_info(zathura_document_info_t* document_info)
{
  if (document_info == NULL) {
    return;
  }

  g_free(document_info->path);
  g_free(document_info->password);
  g_free(document_info->mode);
  g_free(document_info->synctex);
  g_free(document_info->bookmark_name);
  g_free(document_info->search_string);
  g_free(document_info);
}

/* function implementation */
zathura_t*
zathura_create(void)
{
  zathura_t* zathura = g_try_malloc0(sizeof(zathura_t));
  if (zathura == NULL) {
    return NULL;
  }

  /* global settings */
  zathura->global.search_direction = FORWARD;
  zathura->global.synctex_edit_modmask = GDK_CONTROL_MASK;
  zathura->global.highlighter_modmask = GDK_SHIFT_MASK;
  zathura->global.sandbox = ZATHURA_SANDBOX_NORMAL;
  zathura->global.double_click_follow = true;

  /* plugins */
  zathura->plugins.manager = zathura_plugin_manager_new();
  if (zathura->plugins.manager == NULL) {
    goto error_out;
  }

  /* UI */
  zathura->ui.session = girara_session_create();
  if (zathura->ui.session == NULL) {
    goto error_out;
  }

  /* set default icon */
  girara_setting_set(zathura->ui.session, "window-icon", "org.pwmt.zathura");

#ifdef G_OS_UNIX
  /* signal handler */
  zathura->signals.sigterm = g_unix_signal_add(SIGTERM, zathura_signal_sigterm, zathura);
#endif

  /* MIME type detection */
  zathura->content_type_context = zathura_content_type_new();

  zathura->ui.session->global.data = zathura;

  return zathura;

error_out:

  zathura_free(zathura);

  return NULL;
}

static void
create_directories(zathura_t* zathura)
{
  static const unsigned int mode = 0700;

  if (g_mkdir_with_parents(zathura->config.data_dir, mode) == -1) {
    girara_error("Could not create '%s': %s", zathura->config.data_dir,
                 strerror(errno));
  }
}

void
zathura_update_view_ppi(zathura_t* zathura)
{
  if (zathura == NULL) {
    return;
  }

  /* get view widget GdkMonitor */
  GdkWindow* window = gtk_widget_get_window(zathura->ui.session->gtk.view); // NULL if not realized
  if (window == NULL) {
    return;
  }
  GdkDisplay* display = gtk_widget_get_display(zathura->ui.session->gtk.view);
  if (display == NULL) {
    return;
  }

  double ppi = 0.0;

  GdkMonitor* monitor = gdk_display_get_monitor_at_window(display, window);
  if (monitor == NULL) {
    return;
  }

  /* physical width of monitor */
  const int width_mm = gdk_monitor_get_width_mm(monitor);

  /* size of monitor in pixels */
  GdkRectangle monitor_geom;
  gdk_monitor_get_geometry(monitor, &monitor_geom);

  /* Due to a bug in Gtk, width is sometimes incorrectly reported to be 1mm
   * see https://gitlab.gnome.org/GNOME/gtk/issues/3115 for details */
  if (width_mm <= 1) {
    girara_debug("cannot calculate PPI: monitor has zero width");
  } else {
    /* calculate ppi, knowing that 1 inch = 25.4 mm */
    ppi = monitor_geom.width * 25.4 / width_mm;
  }

#ifdef GDK_WINDOWING_WAYLAND
  /* work around apparent bug in GDK: on Wayland, monitor geometry doesn't
   * return values in application pixels as documented, but in device pixels.
   * */
  if (GDK_IS_WAYLAND_DISPLAY(display)) {
    /* not using the cached value for the scale factor here to avoid issues
     * if this function is called before the cached value is updated */
    const int device_factor = gtk_widget_get_scale_factor(zathura->ui.session->gtk.view);
    girara_debug("on Wayland, correcting PPI for device scale factor = %d", device_factor);
    if (device_factor != 0) {
      ppi /= device_factor;
    }
  }
#endif

  const double current_ppi = zathura_document_get_viewport_ppi(zathura->document);
  if (fabs(ppi - current_ppi) > DBL_EPSILON) {
    girara_debug("monitor width: %d mm, pixels: %d, ppi: %0.2f", width_mm, monitor_geom.width, ppi);
    zathura_document_set_viewport_ppi(zathura->document, ppi);
    adjust_view(zathura);
    render_all(zathura);
    refresh_view(zathura);
  }
}

static void weak_ref_object_unref(void* data, GObject* UNUSED(object)) {
  g_object_unref(data);
}

static bool init_ui(zathura_t* zathura) {
  if (girara_session_init(zathura->ui.session, "zathura") == false) {
    girara_error("Failed to initialize girara.");
    return false;
  }

  /* girara events */
  zathura->ui.session->events.buffer_changed  = cb_buffer_changed;
  zathura->ui.session->events.unknown_command = cb_unknown_command;

  /* gestures */

  GtkGesture* zoom = gtk_gesture_zoom_new(GTK_WIDGET(zathura->ui.session->gtk.view));
  g_signal_connect(zoom, "scale-changed", G_CALLBACK(cb_gesture_zoom_scale_changed), zathura);
  g_signal_connect(zoom, "begin", G_CALLBACK(cb_gesture_zoom_begin), zathura);
  gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(zoom), GTK_PHASE_BUBBLE);
  g_object_weak_ref(G_OBJECT(zathura->ui.session->gtk.view), weak_ref_object_unref, zoom);

  /* zathura signals */
  zathura->signals.refresh_view = g_signal_new(
    "refresh-view", GTK_TYPE_WIDGET, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
    g_cclosure_marshal_generic, G_TYPE_NONE, 1, G_TYPE_POINTER);

  g_signal_connect(G_OBJECT(zathura->ui.session->gtk.view), "refresh-view",
                   G_CALLBACK(cb_refresh_view), zathura);

  g_signal_connect(G_OBJECT(zathura->ui.session->gtk.view),
      "notify::scale-factor", G_CALLBACK(cb_scale_factor), zathura);

  g_signal_connect(G_OBJECT(zathura->ui.session->gtk.view),
      "screen-changed", G_CALLBACK(cb_widget_screen_changed), zathura);

  g_signal_connect(G_OBJECT(zathura->ui.session->gtk.window),
      "configure-event", G_CALLBACK(cb_widget_configured), zathura);

  /* initialize the screen-changed handler to 0 (i.e. invalid) */
  zathura->signals.monitors_changed_handler = 0;

  /* page view */
  zathura->ui.page_widget = gtk_grid_new();
  gtk_grid_set_row_homogeneous(GTK_GRID(zathura->ui.page_widget), TRUE);
  gtk_grid_set_column_homogeneous(GTK_GRID(zathura->ui.page_widget), TRUE);
  if (zathura->ui.page_widget == NULL) {
    girara_error("Failed to create page widget.");
    return false;
  }

  g_signal_connect(G_OBJECT(zathura->ui.session->gtk.window), "size-allocate",
                   G_CALLBACK(cb_view_resized), zathura);

  GtkAdjustment* hadjustment = gtk_scrolled_window_get_hadjustment(
                 GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));

  /* Connect hadjustment signals */
  g_signal_connect(G_OBJECT(hadjustment), "value-changed",
                   G_CALLBACK(cb_view_hadjustment_value_changed), zathura);
  g_signal_connect(G_OBJECT(hadjustment), "changed",
                   G_CALLBACK(cb_view_hadjustment_changed), zathura);

  GtkAdjustment* vadjustment = gtk_scrolled_window_get_vadjustment(
                 GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));

  /* Connect vadjustment signals */
  g_signal_connect(G_OBJECT(vadjustment), "value-changed",
                   G_CALLBACK(cb_view_vadjustment_value_changed), zathura);
  g_signal_connect(G_OBJECT(vadjustment), "changed",
                   G_CALLBACK(cb_view_vadjustment_changed), zathura);

  /* page view alignment */
  gtk_widget_set_halign(zathura->ui.page_widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(zathura->ui.page_widget, GTK_ALIGN_CENTER);

  gtk_widget_set_hexpand_set(zathura->ui.page_widget, TRUE);
  gtk_widget_set_hexpand(zathura->ui.page_widget, FALSE);
  gtk_widget_set_vexpand_set(zathura->ui.page_widget, TRUE);
  gtk_widget_set_vexpand(zathura->ui.page_widget, FALSE);

  gtk_widget_show(zathura->ui.page_widget);

  /* statusbar */
  zathura->ui.statusbar.file =
    girara_statusbar_item_add(zathura->ui.session, TRUE, TRUE, TRUE, NULL);
  if (zathura->ui.statusbar.file == NULL) {
    girara_error("Failed to create status bar item.");
    return false;
  }

  zathura->ui.statusbar.buffer =
    girara_statusbar_item_add(zathura->ui.session, FALSE, FALSE, FALSE, NULL);
  if (zathura->ui.statusbar.buffer == NULL) {
    girara_error("Failed to create status bar item.");
    return false;
  }

  zathura->ui.statusbar.page_number =
    girara_statusbar_item_add(zathura->ui.session, FALSE, FALSE, FALSE, NULL);
  if (zathura->ui.statusbar.page_number == NULL) {
    girara_error("Failed to create status bar item.");
    return false;
  }

  girara_statusbar_item_set_text(zathura->ui.session,
                                 zathura->ui.statusbar.file, _("[No name]"));

  /* signals */
  g_signal_connect(G_OBJECT(zathura->ui.session->gtk.window), "destroy",
                   G_CALLBACK(cb_destroy), zathura);

  return true;
}

static bool
init_css(zathura_t* zathura)
{
  GiraraTemplate* csstemplate =
    girara_session_get_template(zathura->ui.session);

  static const char index_settings[][16] = {
    "index-fg",
    "index-bg",
    "index-active-fg",
    "index-active-bg"
  };

  for (size_t s = 0; s < LENGTH(index_settings); ++s) {
    girara_template_add_variable(csstemplate, index_settings[s]);

    char*   tmp_value = NULL;
    GdkRGBA rgba      = {0, 0, 0, 0};
    girara_setting_get(zathura->ui.session, index_settings[s], &tmp_value);
    if (tmp_value != NULL) {
      parse_color(&rgba, tmp_value);
      g_free(tmp_value);
    }

    char* color = gdk_rgba_to_string(&rgba);
    girara_template_set_variable_value(csstemplate, index_settings[s], color);
    g_free(color);
  }

  GResource* css_resource = zathura_resources_get_resource();
  GBytes* css_data = g_resource_lookup_data(css_resource,
                                            "/org/pwmt/zathura/CSS/zathura.css_t",
                                            G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  if (css_data != NULL) {
    char* css = g_strdup_printf("%s\n%s", girara_template_get_base(csstemplate),
                                (const char*) g_bytes_get_data(css_data, NULL));
    girara_template_set_base(csstemplate, css);
    g_free(css);
    g_bytes_unref(css_data);
  } else {
    return false;
  }

  return true;
}

static void
init_database(zathura_t* zathura)
{
  char* database = NULL;
  girara_setting_get(zathura->ui.session, "database", &database);

  /* create zathura data directory if database enabled */
  if (g_strcmp0(database, "null") != 0) {
    create_directories(zathura);
  }

  if (g_strcmp0(database, "plain") == 0) {
    girara_debug("Using plain database backend.");
    zathura->database = zathura_plaindatabase_new(zathura->config.data_dir);
#ifdef WITH_SQLITE
  } else if (g_strcmp0(database, "sqlite") == 0) {
    girara_debug("Using sqlite database backend.");
    char* tmp =
      g_build_filename(zathura->config.data_dir, "bookmarks.sqlite", NULL);
    zathura->database = zathura_sqldatabase_new(tmp);
    g_free(tmp);
#endif
  } else if (g_strcmp0(database, "null") != 0) {
    girara_error("Database backend '%s' is not supported.", database);
  }

  if (zathura->database == NULL && g_strcmp0(database, "null") != 0) {
    girara_error(
      "Unable to initialize database. Bookmarks won't be available.");
  }
  else {
    g_object_set(G_OBJECT(zathura->ui.session->command_history), "io",
                 zathura->database, NULL);
  }
  g_free(database);
}

static void
init_jumplist(zathura_t* zathura)
{
  int jumplist_size = 20;
  girara_setting_get(zathura->ui.session, "jumplist-size", &jumplist_size);

  zathura->jumplist.max_size = jumplist_size < 0 ? 0 : jumplist_size;
  zathura->jumplist.list     = NULL;
  zathura->jumplist.size     = 0;
  zathura->jumplist.cur      = NULL;
}

static void
init_shortcut_helpers(zathura_t* zathura)
{
  zathura->shortcut.mouse.x = 0;
  zathura->shortcut.mouse.y = 0;

  zathura->shortcut.toggle_page_mode.pages = 2;

  zathura->shortcut.toggle_presentation_mode.pages                  = 1;
  zathura->shortcut.toggle_presentation_mode.first_page_column_list = NULL;
  zathura->shortcut.toggle_presentation_mode.zoom                   = 1.0;
}

bool
zathura_init(zathura_t* zathura)
{
  if (zathura == NULL) {
    return false;
  }

  /* Set application ID */
  g_set_prgname("org.pwmt.zathura");

  /* load plugins */
  if (zathura_plugin_manager_load(zathura->plugins.manager) == false) {
    girara_error("Found no plugins. Please install at least one plugin.");
  }

  /* configuration */
  config_load_default(zathura);
  config_load_files(zathura);

#ifdef WITH_SECCOMP
  /* initialize seccomp filters */
  switch (zathura->global.sandbox) {
    case ZATHURA_SANDBOX_NONE:
      girara_debug("Sandbox deactivated.");
      break;
    case ZATHURA_SANDBOX_NORMAL:
      girara_debug("Basic sandbox allowing normal operation.");
      if (seccomp_enable_basic_filter() != 0) {
        girara_error("Failed to initialize basic seccomp filter.");
        goto error_free;
      }
      break;
    case ZATHURA_SANDBOX_STRICT:
      girara_debug("Strict sandbox preventing write and network access.");
      if (seccomp_enable_strict_filter(zathura) != 0) {
        girara_error("Failed to initialize strict seccomp filter.");
        goto error_free;
      }
      /* unset the input method to avoid communication with external services */
      unsetenv("GTK_IM_MODULE");
      break;
  }
#endif

  /* UI */
  if (init_ui(zathura) == false) {
    girara_error("Failed to initialize UI.");
    goto error_free;
  }

  /* disable unsupported features in strict sandbox mode */
  if (zathura->global.sandbox != ZATHURA_SANDBOX_STRICT) {
    /* database */
    init_database(zathura);
  }

  /* bookmarks */
  zathura->bookmarks.bookmarks = girara_sorted_list_new2(
    (girara_compare_function_t)zathura_bookmarks_compare,
    (girara_free_function_t)zathura_bookmark_free);

  /* jumplist */
  init_jumplist(zathura);

  /* CSS for index mode */
  if (init_css(zathura) == false) {
    girara_error("Failed to initialize CSS.");
    goto error_free;
  }

  /* Shortcut helpers */
  init_shortcut_helpers(zathura);

  /* Start D-Bus service */
  bool dbus = true;
  girara_setting_get(zathura->ui.session, "dbus-service", &dbus);
  if (dbus == true) {
    zathura->dbus = zathura_dbus_new(zathura);
  }

  return true;

error_free:

  if (zathura->ui.page_widget != NULL) {
    g_object_unref(zathura->ui.page_widget);
  }

  return false;
}

void
zathura_free(zathura_t* zathura)
{
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
  girara_list_free(zathura->bookmarks.bookmarks);

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

  /* free jumplist */
  if (zathura->jumplist.cur != NULL) {
    girara_list_iterator_free(zathura->jumplist.cur);
  }

  if (zathura->jumplist.list != NULL) {
    girara_list_free(zathura->jumplist.list);
  }

  g_free(zathura);
}

void
zathura_set_xid(zathura_t* zathura, Window xid)
{
  g_return_if_fail(zathura != NULL);

  zathura->ui.session->gtk.embed = xid;
}

void
zathura_set_config_dir(zathura_t* zathura, const char* dir)
{
  g_return_if_fail(zathura != NULL);

  if (dir != NULL) {
    zathura->config.config_dir = g_strdup(dir);
  } else {
    gchar* path = girara_get_xdg_path(XDG_CONFIG);
    zathura->config.config_dir = g_build_filename(path, "zathura", NULL);
    g_free(path);
  }
}

void
zathura_set_data_dir(zathura_t* zathura, const char* dir)
{
  g_return_if_fail(zathura != NULL);

  if (dir != NULL) {
    zathura->config.data_dir = g_strdup(dir);
  } else {
    gchar* path = girara_get_xdg_path(XDG_DATA);
    zathura->config.data_dir = g_build_filename(path, "zathura", NULL);
    g_free(path);
  }
}

void
zathura_set_cache_dir(zathura_t* zathura, const char* dir)
{
  g_return_if_fail(zathura != NULL);

  if (dir != NULL) {
    zathura->config.cache_dir = g_strdup(dir);
  } else {
    gchar* path = girara_get_xdg_path(XDG_CACHE);
    zathura->config.cache_dir = g_build_filename(path, "zathura", NULL);
    g_free(path);
  }
}

static void
add_dir(void* data, void* userdata)
{
  const char* path                         = data;
  zathura_plugin_manager_t* plugin_manager = userdata;

  zathura_plugin_manager_add_dir(plugin_manager, path);
}

static void
set_plugin_dir(zathura_t* zathura, const char* dir)
{
  girara_list_t* paths = girara_split_path_array(dir);
  girara_list_foreach(paths, add_dir, zathura->plugins.manager);
  girara_list_free(paths);
}

void
zathura_set_plugin_dir(zathura_t* zathura, const char* dir)
{
  g_return_if_fail(zathura != NULL);
  g_return_if_fail(zathura->plugins.manager != NULL);

  if (dir != NULL) {
    set_plugin_dir(zathura, dir);
  } else if (g_getenv("ZATHURA_PLUGINS_PATH") != NULL) {
    set_plugin_dir(zathura, g_getenv("ZATHURA_PLUGINS_PATH"));
#ifdef ZATHURA_PLUGINDIR
  } else {
    set_plugin_dir(zathura, ZATHURA_PLUGINDIR);
#endif
  }
}

void
zathura_set_argv(zathura_t* zathura, char** argv)
{
  g_return_if_fail(zathura != NULL);

  zathura->global.arguments = argv;
}

static bool
setup_renderer(zathura_t* zathura, zathura_document_t* document)
{
 /* page cache size */
  int cache_size = 0;
  girara_setting_get(zathura->ui.session, "page-cache-size", &cache_size);
  if (cache_size <= 0) {
    girara_warning("page-cache-size is not positive, using %d instead",
        ZATHURA_PAGE_CACHE_DEFAULT_SIZE);
    cache_size = ZATHURA_PAGE_CACHE_DEFAULT_SIZE;
  }

  girara_debug("starting renderer with cache size %d", cache_size);
  ZathuraRenderer* renderer = zathura_renderer_new(cache_size);
  if (renderer == NULL) {
    return false;
  }

  /* set up recolor info in ZathuraRenderer */
  char* recolor_dark  = NULL;
  char* recolor_light = NULL;
  girara_setting_get(zathura->ui.session, "recolor-darkcolor", &recolor_dark);
  girara_setting_get(zathura->ui.session, "recolor-lightcolor", &recolor_light);
  zathura_renderer_set_recolor_colors_str(renderer, recolor_light, recolor_dark);
  g_free(recolor_dark);
  g_free(recolor_light);

  bool recolor = false;
  girara_setting_get(zathura->ui.session, "recolor", &recolor);
  zathura_renderer_enable_recolor(renderer, recolor);
  girara_setting_get(zathura->ui.session, "recolor-keephue", &recolor);
  zathura_renderer_enable_recolor_hue(renderer, recolor);
  girara_setting_get(zathura->ui.session, "recolor-reverse-video", &recolor);
  zathura_renderer_enable_recolor_reverse_video(renderer, recolor);

  zathura->sync.render_thread = renderer;

  /* create render request to render window icon */
  bool window_icon = false;
  girara_setting_get(zathura->ui.session, "window-icon-document", &window_icon);
  if (window_icon == true) {
    girara_debug("starting render request for window icon");
    ZathuraRenderRequest* request = zathura_render_request_new(renderer, zathura_document_get_page(document, 0));
    g_signal_connect(request, "completed", G_CALLBACK(cb_window_update_icon), zathura);
    zathura_render_request_set_render_plain(request, true);
    zathura_render_request(request, 0);
    zathura->window_icon_render_request = request;
  }

  return true;
}

#ifdef G_OS_UNIX
static gchar*
prepare_document_open_from_stdin(const char* path)
{
  int infileno = -1;
  if (g_strcmp0(path, "-") == 0) {
    infileno = fileno(stdin);
  } else if (g_str_has_prefix(path, "/proc/self/fd/") == true) {
    char* begin = g_strrstr(path, "/") + 1;
    gint64 temp = g_ascii_strtoll(begin, NULL, 0);
    if (temp > INT_MAX || temp < 0) {
      return NULL;
    }
    infileno = (int) temp;
  } else {
    return NULL;
  }

  if (infileno == -1) {
    girara_error("Can not read from file descriptor.");
    return NULL;
  }

  GInputStream* input_stream = g_unix_input_stream_new(infileno, false);
  if (input_stream == NULL) {
    girara_error("Can not read from file descriptor.");
    return NULL;
  }

  GFileIOStream* iostream = NULL;
  GError*        error    = NULL;
  GFile* tmpfile = g_file_new_tmp("zathura.stdin.XXXXXX", &iostream, &error);
  if (tmpfile == NULL) {
    if (error != NULL) {
      girara_error("Can not create temporary file: %s", error->message);
      g_error_free(error);
    }
    g_object_unref(input_stream);
    return NULL;
  }

  const ssize_t count = g_output_stream_splice(
    g_io_stream_get_output_stream(G_IO_STREAM(iostream)), input_stream,
    G_OUTPUT_STREAM_SPLICE_NONE, NULL, &error);
  g_object_unref(input_stream);
  g_object_unref(iostream);
  if (count == -1) {
    if (error != NULL) {
      girara_error("Can not write to temporary file: %s", error->message);
      g_error_free(error);
    }
    g_file_delete(tmpfile, NULL, NULL);
    g_object_unref(tmpfile);
    return NULL;
  }

  char* file = g_file_get_path(tmpfile);
  g_object_unref(tmpfile);

  return file;
}
#endif

static gchar*
prepare_document_open_from_gfile(GFile* source)
{
  gchar*         file     = NULL;
  GFileIOStream* iostream = NULL;
  GError*        error    = NULL;

  GFile* tmpfile = g_file_new_tmp("zathura.gio.XXXXXX", &iostream, &error);
  if (tmpfile == NULL) {
    if (error != NULL) {
      girara_error("Can not create temporary file: %s", error->message);
      g_error_free(error);
    }
    return NULL;
  }

  gboolean rc = g_file_copy(source, tmpfile, G_FILE_COPY_OVERWRITE, NULL, NULL,
                            NULL, &error);
  if (rc == FALSE) {
    if (error != NULL) {
      girara_error("Can not copy to temporary file: %s", error->message);
      g_error_free(error);
    }
    g_object_unref(iostream);
    g_object_unref(tmpfile);
    return NULL;
  }

  file = g_file_get_path(tmpfile);
  g_object_unref(iostream);
  g_object_unref(tmpfile);

  return file;
}

static gboolean
document_info_open(gpointer data)
{
  zathura_document_info_t* document_info = data;
  g_return_val_if_fail(document_info != NULL, FALSE);
  char* uri = NULL;

  if (document_info->zathura != NULL && document_info->path != NULL) {
    char* file = NULL;
    if (g_strcmp0(document_info->path, "-") == 0 ||
        g_str_has_prefix(document_info->path, "/proc/self/fd/") == true) {
#ifdef G_OS_UNIX
      file = prepare_document_open_from_stdin(document_info->path);
#endif
      if (file == NULL) {
        girara_notify(document_info->zathura->ui.session, GIRARA_ERROR,
                      _("Could not read file from stdin and write it to a temporary file."));
      } else {
        document_info->zathura->stdin_support.file = g_strdup(file);
      }
    } else {
      /* expand ~ and ~user in paths if present */
      char* tmp_path = *document_info->path == '~' ? girara_fix_path(document_info->path) : NULL;
      GFile* gf = g_file_new_for_commandline_arg(tmp_path != NULL ? tmp_path : document_info->path);
      g_free(tmp_path);

      if (g_file_is_native(gf) == TRUE) {
        /* file was given as a native path */
        file = g_file_get_path(gf);
      } else {
        /* copy file with GIO */
        uri = g_file_get_uri(gf);
        file = prepare_document_open_from_gfile(gf);
        if (file == NULL) {
          girara_notify(document_info->zathura->ui.session, GIRARA_ERROR,
                        _("Could not read file from GIO and copy it to a temporary file."));
        } else {
          document_info->zathura->stdin_support.file = g_strdup(file);
        }
      }
      g_object_unref(gf);
    }

    if (file != NULL) {
      if (document_info->synctex != NULL) {
        document_open_synctex(document_info->zathura, file, uri,
                              document_info->password, document_info->synctex);
      } else {
        document_open(document_info->zathura, file, uri, document_info->password,
                      document_info->page_number);
      }
      g_free(file);
      g_free(uri);

      if (document_info->mode != NULL) {
        if (g_strcmp0(document_info->mode, "presentation") == 0) {
          sc_toggle_presentation(document_info->zathura->ui.session, NULL, NULL,
                                 0);
        } else if (g_strcmp0(document_info->mode, "fullscreen") == 0) {
          sc_toggle_fullscreen(document_info->zathura->ui.session, NULL, NULL,
                               0);
        } else {
          girara_error("Unknown mode: %s", document_info->mode);
        }
      }

      if (document_info->bookmark_name != NULL) {
        girara_list_t* arg_list = girara_list_new();
        girara_list_append(arg_list, document_info->bookmark_name);
        cmd_bookmark_open(document_info->zathura->ui.session, arg_list);
        girara_list_free(arg_list);
      }

      if (document_info->search_string != NULL) {
        girara_argument_t search_arg;
        search_arg.n = 1; // Forward search
        search_arg.data = NULL;
        cmd_search(document_info->zathura->ui.session, document_info->search_string,
                &search_arg);
      }
    }
  }

  free_document_info(document_info);
  return FALSE;
}

char*
get_formatted_filename(zathura_t* zathura, bool statusbar)
{
  bool basename_only = false;
  const char* file_path = zathura_document_get_uri(zathura->document);
  if (file_path == NULL) {
    file_path = zathura_document_get_path(zathura->document);
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
      char* home = girara_get_home_directory(NULL);
      const size_t home_len = home ? strlen(home) : 0;

      if (home_len > 1
          && file_path_len >= home_len
          && g_str_has_prefix(file_path, home)
          && (!file_path[home_len] || file_path[home_len] == '/')) {
        g_free(home);
        return g_strdup_printf("~%s", &file_path[home_len]);
      } else {
        g_free(home);
        return g_strdup(file_path);
      }
    } else {
      return g_strdup(file_path);
    }
  } else {
    const char* basename = zathura_document_get_basename(zathura->document);
    return g_strdup(basename);
  }
}

static gboolean
document_open_password_dialog(gpointer data)
{
  zathura_password_dialog_info_t* password_dialog_info = data;

  girara_dialog(password_dialog_info->zathura->ui.session, _("Enter password:"), true, NULL,
                cb_password_dialog, password_dialog_info);
  return FALSE;
}

typedef struct {
  double h;
  double w;
  int freq;
} sample_t;

static int document_page_size_comp(const void* a, const void* b) {
  const sample_t* lhs = a;
  const sample_t* rhs = b;
  return rhs->freq - lhs->freq;
}

static void document_open_page_most_frequent_size(zathura_document_t* document, unsigned int* width,
                                                  unsigned int* height) {
  girara_list_t* samples             = girara_list_new2(g_free);
  const unsigned int number_of_pages = zathura_document_get_number_of_pages(document);

  for (unsigned int page_id = 0; page_id < number_of_pages; ++page_id) {
    zathura_page_t* page = zathura_document_get_page(document, page_id);
    const double w       = zathura_page_get_width(page);
    const double h       = zathura_page_get_height(page);

    bool found = false;
    for (size_t idx = 0; idx != girara_list_size(samples) && !found; ++idx) {
      sample_t* sample = girara_list_nth(samples, idx);
      if (fabs(sample->h - h) <= DBL_EPSILON && fabs(sample->w - w) <= DBL_EPSILON) {
        sample->freq++;
      }
    }

    if (found == false) {
      sample_t* sample = g_try_malloc0(sizeof(sample_t));
      sample->w        = w;
      sample->h        = h;
      sample->freq     = 1;
      girara_list_append(samples, sample);
    }
  }

  girara_list_sort(samples, document_page_size_comp);

  sample_t* max_sample = girara_list_nth(samples, 0);
  *width               = max_sample->w;
  *height              = max_sample->h;

  girara_list_free(samples);
}

bool
document_open(zathura_t* zathura, const char* path, const char* uri, const char* password,
              int page_number)
{
  if (zathura == NULL || zathura->plugins.manager == NULL || path == NULL) {
    goto error_out;
  }

  /* FIXME: since there are many call chains leading here, check again if we need to expand ~ or
   * ~user. We should fix all call sites instead */
  char* tmp_path = *path == '~' ? girara_fix_path(path) : NULL;
  zathura_error_t error = ZATHURA_ERROR_OK;
  zathura_document_t* document = zathura_document_open(zathura, tmp_path != NULL ? tmp_path : path, uri, password, &error);
  g_free(tmp_path);

  if (document == NULL) {
    if (error == ZATHURA_ERROR_INVALID_PASSWORD) {
      girara_debug("Invalid or no password.");
      zathura_password_dialog_info_t* password_dialog_info = malloc(sizeof(zathura_password_dialog_info_t));
      if (password_dialog_info != NULL) {
        password_dialog_info->zathura = zathura;
        password_dialog_info->path = g_strdup(path);
        password_dialog_info->uri = g_strdup(uri);

        if (password_dialog_info->path != NULL) {
          gdk_threads_add_idle(document_open_password_dialog, password_dialog_info);
          goto error_out;
        } else {
          free(password_dialog_info);
        }
      }
      goto error_out;
    }
    if (error == ZATHURA_ERROR_OK ) {
      girara_notify(zathura->ui.session, GIRARA_ERROR, _("Unsupported file type. Please install the necessary plugin."));
    }
    goto error_out;
  }

  const char* file_path        = zathura_document_get_path(document);
  unsigned int number_of_pages = zathura_document_get_number_of_pages(document);

  if (number_of_pages == 0) {
    girara_notify(zathura->ui.session, GIRARA_WARNING,
        _("Document does not contain any pages"));
    goto error_free;
  }

  zathura->document = document;

  /* read history file */
  zathura_fileinfo_t file_info = {
    .current_page = 0,
    .page_offset = 0,
    .zoom = 1,
    .rotation = 0,
    .pages_per_row = 0,
    .first_page_column_list = NULL,
    .page_right_to_left = false,
    .position_x = 0,
    .position_y = 0
  };
  bool known_file = false;
  if (zathura->database != NULL) {
    const uint8_t* file_hash = zathura_document_get_hash(document);
    known_file = zathura_db_get_fileinfo(zathura->database, file_path, file_hash, &file_info);
  }

  /* set page offset */
  zathura_document_set_page_offset(document, file_info.page_offset);

  /* check for valid zoom value */
  if (file_info.zoom <= DBL_EPSILON) {
    file_info.zoom = 1;
  }
  zathura_document_set_zoom(document,
      zathura_correct_zoom_value(zathura->ui.session, file_info.zoom));

  /* check current page number */
  /* if it wasn't specified on the command-line, get it from file_info */
  if (page_number == ZATHURA_PAGE_NUMBER_UNSPECIFIED)
    page_number = file_info.current_page;
  if (page_number < 0)
    page_number += number_of_pages;
  if ((unsigned)page_number > number_of_pages) {
    girara_warning("document info: '%s' has an invalid page number", file_path);
    zathura_document_set_current_page_number(document, 0);
  } else {
    zathura_document_set_current_page_number(document, page_number);
  }

  /* check for valid rotation */
  if (file_info.rotation % 90 != 0) {
    girara_warning("document info: '%s' has an invalid rotation", file_path);
    zathura_document_set_rotation(document, 0);
  } else {
    zathura_document_set_rotation(document, file_info.rotation % 360);
  }

  /* jump to first page if setting enabled */
  bool always_first_page = false;
  girara_setting_get(zathura->ui.session, "open-first-page", &always_first_page);
  if (always_first_page == true) {
    zathura_document_set_current_page_number(document, 0);
  }

  /* apply open adjustment */
  char* adjust_open = "best-fit";
  if (known_file == false && girara_setting_get(zathura->ui.session, "adjust-open", &(adjust_open)) == true) {
    if (g_strcmp0(adjust_open, "best-fit") == 0) {
      zathura_document_set_adjust_mode(document, ZATHURA_ADJUST_BESTFIT);
    } else if (g_strcmp0(adjust_open, "width") == 0) {
      zathura_document_set_adjust_mode(document, ZATHURA_ADJUST_WIDTH);
    } else {
      zathura_document_set_adjust_mode(document, ZATHURA_ADJUST_NONE);
    }
    g_free(adjust_open);
  } else {
    zathura_document_set_adjust_mode(document, ZATHURA_ADJUST_NONE);
  }

  /* initialize bisect state */
  zathura->bisect.start = 0;
  zathura->bisect.last_jump = zathura_document_get_current_page_number(document);
  zathura->bisect.end = number_of_pages - 1;

  /* update statusbar */
  char* filename = get_formatted_filename(zathura, true);
  girara_statusbar_item_set_text(zathura->ui.session, zathura->ui.statusbar.file, filename);
  g_free(filename);

  /* install file monitor */
  if (zathura->file_monitor.monitor == NULL) {
    char* filemonitor_backend = NULL;
    girara_setting_get(zathura->ui.session, "filemonitor", &filemonitor_backend);
    zathura_filemonitor_type_t type = ZATHURA_FILEMONITOR_GLIB;
    if (g_strcmp0(filemonitor_backend, "noop") == 0) {
      type = ZATHURA_FILEMONITOR_NOOP;
    }
#ifdef G_OS_UNIX
    else if (g_strcmp0(filemonitor_backend, "signal") == 0) {
      type = ZATHURA_FILEMONITOR_SIGNAL;
    }
#endif
    g_free(filemonitor_backend);

    zathura->file_monitor.monitor = zathura_filemonitor_new(file_path, type);
    if (zathura->file_monitor.monitor == NULL) {
      goto error_free;
    }
    g_signal_connect(G_OBJECT(zathura->file_monitor.monitor), "reload-file",
                     G_CALLBACK(cb_file_monitor), zathura->ui.session);

    girara_debug("starting file monitor");
    zathura_filemonitor_start(zathura->file_monitor.monitor);
  }

  if (password != NULL) {
    g_free(zathura->file_monitor.password);
    zathura->file_monitor.password = g_strdup(password);
    if (zathura->file_monitor.password == NULL) {
      goto error_free;
    }
  }

  /* create marks list */
  zathura->global.marks = girara_list_new2((girara_free_function_t) mark_free);
  if (zathura->global.marks == NULL) {
    goto error_free;
  }

  /* threads */
  if (!setup_renderer(zathura, document)) {
    goto error_free;
  }

  /* get view port size */
  GtkAdjustment* hadjustment = gtk_scrolled_window_get_hadjustment(
                 GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));
  GtkAdjustment* vadjustment = gtk_scrolled_window_get_vadjustment(
                 GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));

  const unsigned int view_width = floor(gtk_adjustment_get_page_size(hadjustment));
  zathura_document_set_viewport_width(zathura->document, view_width);
  const unsigned int view_height = floor(gtk_adjustment_get_page_size(vadjustment));
  zathura_document_set_viewport_height(zathura->document, view_height);

  zathura_update_view_ppi(zathura);

  /* call screen-changed callback to connect monitors-changed signal on initial screen */
  cb_widget_screen_changed(zathura->ui.session->gtk.view, NULL, zathura);

  /* get initial device scale */
  const int device_factor = gtk_widget_get_scale_factor(zathura->ui.session->gtk.view);
  zathura_document_set_device_factors(zathura->document, device_factor, device_factor);

  /* create blank pages */
  zathura->pages = calloc(number_of_pages, sizeof(GtkWidget*));
  if (zathura->pages == NULL) {
    goto error_free;
  }

  unsigned int most_freq_width, most_freq_height;
  document_open_page_most_frequent_size(document, &most_freq_width,
                                        &most_freq_height);

  for (unsigned int page_id = 0; page_id < number_of_pages; page_id++) {
    zathura_page_t* page = zathura_document_get_page(document, page_id);
    if (page == NULL) {
      goto error_free;
    }

    unsigned int cell_height = 0, cell_width = 0;
    zathura_document_get_cell_size(document, &cell_height, &cell_width);
    zathura_document_set_cell_size(document, most_freq_height, most_freq_width);
    zathura_page_set_width(page, most_freq_width);
    zathura_page_set_height(page, most_freq_height);

    GtkWidget* page_widget = zathura_page_widget_new(zathura, page);
    if (page_widget == NULL) {
      goto error_free;
    }

    g_object_ref(page_widget);
    zathura->pages[page_id] = page_widget;

    gtk_widget_set_halign(page_widget, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(page_widget, GTK_ALIGN_CENTER);

    g_signal_connect(G_OBJECT(page_widget), "text-selected",
        G_CALLBACK(cb_page_widget_text_selected), zathura);
    g_signal_connect(G_OBJECT(page_widget), "image-selected",
        G_CALLBACK(cb_page_widget_image_selected), zathura);
    g_signal_connect(G_OBJECT(page_widget), "enter-link",
        G_CALLBACK(cb_page_widget_link), (gpointer) true);
    g_signal_connect(G_OBJECT(page_widget), "leave-link",
        G_CALLBACK(cb_page_widget_link), (gpointer) false);
    g_signal_connect(G_OBJECT(page_widget), "scaled-button-release",
        G_CALLBACK(cb_page_widget_scaled_button_release), zathura);
  }

  /* view mode */
  unsigned int pages_per_row = 1;
  char* first_page_column_list = NULL;
  unsigned int page_padding = 1;
  bool page_right_to_left = false;

  girara_setting_get(zathura->ui.session, "page-padding", &page_padding);

  if (file_info.pages_per_row > 0) {
    pages_per_row = file_info.pages_per_row;
  } else {
    girara_setting_get(zathura->ui.session, "pages-per-row", &pages_per_row);
  }

  /* read first_page_column list */
  if (file_info.first_page_column_list != NULL && *file_info.first_page_column_list != '\0') {
    first_page_column_list = file_info.first_page_column_list;
    file_info.first_page_column_list = NULL;
  } else {
    girara_setting_get(zathura->ui.session, "first-page-column", &first_page_column_list);
  }

  /* find value for first_page_column */
  unsigned int first_page_column = find_first_page_column(first_page_column_list, pages_per_row);

  girara_setting_set(zathura->ui.session, "pages-per-row", &pages_per_row);
  girara_setting_set(zathura->ui.session, "first-page-column", first_page_column_list);
  g_free(file_info.first_page_column_list);
  g_free(first_page_column_list);

  page_right_to_left = file_info.page_right_to_left;

  page_widget_set_mode(zathura, page_padding, pages_per_row, first_page_column, page_right_to_left);
  zathura_document_set_page_layout(zathura->document, page_padding, pages_per_row, first_page_column);

  girara_set_view(zathura->ui.session, zathura->ui.page_widget);

  /* bookmarks */
  if (zathura->database != NULL) {
    if (zathura_bookmarks_load(zathura, file_path) == false) {
      girara_debug("Failed to load bookmarks.");
    }

    /* jumplist */
    if (zathura_jumplist_load(zathura, file_path) == false) {
      zathura->jumplist.list = girara_list_new2(g_free);
    }
  }

  /* update title */
  char* formatted_filename = get_formatted_filename(zathura, false);
  girara_set_window_title(zathura->ui.session, formatted_filename);
  g_free(formatted_filename);

  /* adjust_view */
  adjust_view(zathura);
  for (unsigned int page_id = 0; page_id < number_of_pages; page_id++) {
    /* set widget size */
    zathura_page_t* page = zathura_document_get_page(document, page_id);
    unsigned int page_height = 0;
    unsigned int page_width  = 0;

    /* adjust_view calls render_all in some cases and render_all calls
     * gtk_widget_set_size_request. To be sure that it's really called, do it
     * here once again. */
    const double height = zathura_page_get_height(page);
    const double width = zathura_page_get_width(page);
    page_calc_height_width(zathura->document, height, width, &page_height, &page_width, true);
    gtk_widget_set_size_request(zathura->pages[page_id], page_width, page_height);

    /* show widget */
    gtk_widget_show(zathura->pages[page_id]);
  }

  /* Set page */
  page_set(zathura, zathura_document_get_current_page_number(document));

  /* Set position (only if restoring from history file) */
  if (file_info.current_page == zathura_document_get_current_page_number(document) &&
      (file_info.position_x != 0 || file_info.position_y != 0)) {
    position_set(zathura, file_info.position_x, file_info.position_y);
  }

  bool show_signature_information = false;
  girara_setting_get(zathura->ui.session, "show-signature-information", &show_signature_information);
  zathura_show_signature_information(zathura, show_signature_information);
  update_visible_pages(zathura);

  return true;

error_free:

  zathura_document_free(document);

error_out:

  return false;
}

bool
document_open_synctex(zathura_t* zathura, const char* path, const char* uri,
                      const char* password, const char* synctex)
{
  bool ret = document_open(zathura, path, password, uri,
                           ZATHURA_PAGE_NUMBER_UNSPECIFIED);
  if (ret == false) {
    return false;
  }
  if (synctex == NULL) {
    return true;
  }

  int line = 0;
  int column = 0;
  char* input_file = NULL;
  if (synctex_parse_input(synctex, &input_file, &line, &column) == false) {
    return false;
  }

  ret = synctex_view(zathura, input_file, line, column);
  g_free(input_file);
  return ret;
}

void
document_open_idle(zathura_t* zathura, const char* path, const char* password,
                   int page_number, const char* mode, const char* synctex,
                   const char* bookmark_name, const char *search_string)
{
  g_return_if_fail(zathura != NULL);
  g_return_if_fail(path != NULL);

  zathura_document_info_t* document_info = g_try_malloc0(sizeof(zathura_document_info_t));
  if (document_info == NULL) {
    return;
  }

  document_info->zathura     = zathura;
  document_info->path        = g_strdup(path);
  if (password != NULL) {
    document_info->password  = g_strdup(password);
  }
  document_info->page_number = page_number;
  if (mode != NULL) {
    document_info->mode      = g_strdup(mode);
  }
  if (synctex != NULL) {
    document_info->synctex   = g_strdup(synctex);
  }
  if (bookmark_name != NULL) {
      document_info->bookmark_name = g_strdup(bookmark_name);
  }
  if (search_string != NULL) {
    document_info->search_string = g_strdup(search_string);
  }

  gdk_threads_add_idle(document_info_open, document_info);
}

bool
document_save(zathura_t* zathura, const char* path, bool overwrite)
{
  g_return_val_if_fail(zathura, false);
  g_return_val_if_fail(zathura->document, false);
  g_return_val_if_fail(path, false);

  gchar* file_path = girara_fix_path(path);
  /* use current basename if path points to a directory  */
  if (g_file_test(file_path, G_FILE_TEST_IS_DIR) == TRUE) {
    char* basename = g_path_get_basename(zathura_document_get_path(zathura->document));
    char* tmp = file_path;
    file_path = g_build_filename(file_path, basename, NULL);
    g_free(tmp);
    g_free(basename);
  }

  if ((overwrite == false) && g_file_test(file_path, G_FILE_TEST_EXISTS)) {
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("File already exists: %s. Use :write! to overwrite it."), file_path);
    g_free(file_path);
    return false;
  }

  const zathura_error_t error = zathura_document_save_as(zathura->document, file_path);
  g_free(file_path);

  if (error != ZATHURA_ERROR_OK) {
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("Failed to save document."));
    return false;
  }

  girara_notify(zathura->ui.session, GIRARA_INFO, _("Document saved."));
  return true;
}

static void
remove_page_from_table(GtkWidget* page, gpointer UNUSED(permanent))
{
  gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(page)), page);
}

static void
save_fileinfo_to_db(zathura_t* zathura)
{
  const char* path = zathura_document_get_path(zathura->document);
  const uint8_t* file_hash = zathura_document_get_hash(zathura->document);

  zathura_fileinfo_t file_info = {
    .current_page = zathura_document_get_current_page_number(zathura->document),
    .page_offset = zathura_document_get_page_offset(zathura->document),
    .zoom = zathura_document_get_zoom(zathura->document),
    .rotation = zathura_document_get_rotation(zathura->document),
    .pages_per_row = 1,
    .first_page_column_list = "1:2",
    .page_right_to_left = false,
    .position_x = zathura_document_get_position_x(zathura->document),
    .position_y = zathura_document_get_position_y(zathura->document)
  };

  girara_setting_get(zathura->ui.session, "pages-per-row",
                     &(file_info.pages_per_row));
  girara_setting_get(zathura->ui.session, "first-page-column",
                     &(file_info.first_page_column_list));
  girara_setting_get(zathura->ui.session, "page-right-to-left",
                     &(file_info.page_right_to_left));

  /* save file info */
  zathura_db_set_fileinfo(zathura->database, path, file_hash, &file_info);
  /* save jumplist */
  zathura_db_save_jumplist(zathura->database, path, zathura->jumplist.list);

  g_free(file_info.first_page_column_list);
}

bool
document_predecessor_free(zathura_t* zathura) {
  if (zathura == NULL
	  || (zathura->predecessor_document == NULL
		  && zathura->predecessor_pages == NULL)) {
    return false;
  }

  if (zathura->predecessor_pages != NULL) {
	  for (unsigned int i = 0; i < zathura_document_get_number_of_pages(zathura->predecessor_document); i++) {
		g_object_unref(zathura->predecessor_pages[i]);
	  }
	  free(zathura->predecessor_pages);
	  zathura->predecessor_pages = NULL;
	  girara_debug("freed predecessor pages");
  }
  if (zathura->predecessor_document != NULL) {
	  /* remove document */
	  zathura_document_free(zathura->predecessor_document);
	  zathura->predecessor_document = NULL;
	  girara_debug("freed predecessor document");
  }

  return true;
}

bool
document_close(zathura_t* zathura, bool keep_monitor)
{
  if (zathura == NULL || zathura->document == NULL) {
    return false;
  }

  /* reset window icon */
  if (zathura->ui.session != NULL && zathura->window_icon_render_request != NULL) {
    char* window_icon = NULL;
    girara_setting_get(zathura->ui.session, "window-icon", &window_icon);
    girara_setting_set(zathura->ui.session, "window-icon", window_icon);
    g_free(window_icon);
  }

  bool smooth_reload = true;
  girara_setting_get(zathura->ui.session, "smooth-reload", &smooth_reload);

  /* stop rendering */
  zathura_renderer_stop(zathura->sync.render_thread);
  g_clear_object(&zathura->window_icon_render_request);

  /* remove monitor */
  if (keep_monitor == false) {
    g_clear_object(&zathura->file_monitor.monitor);

    if (zathura->file_monitor.password != NULL) {
      g_free(zathura->file_monitor.password);
      zathura->file_monitor.password = NULL;
    }
  }

  /* remove marks */
  if (zathura->global.marks != NULL) {
    girara_list_free(zathura->global.marks);
    zathura->global.marks = NULL;
  }

  /* store file information */
  if (zathura->database != NULL) {
    save_fileinfo_to_db(zathura);
  }

  /* remove jump list */
  girara_list_iterator_free(zathura->jumplist.cur);
  zathura->jumplist.cur = NULL;
  girara_list_free(zathura->jumplist.list);
  zathura->jumplist.list = NULL;
  zathura->jumplist.size = 0;

  /* release render thread */
  g_clear_object(&zathura->sync.render_thread);

  /* keep the current state to prevent flicker? */
  bool override_predecessor = keep_monitor && smooth_reload;

  if (override_predecessor) {
	  /* do not override predecessor buffer with empty pages */
	  unsigned int cur_page_num = zathura_document_get_current_page_number(zathura->document);
	  ZathuraPage* cur_page = ZATHURA_PAGE(zathura->pages[cur_page_num]);
	  if (!zathura_page_widget_have_surface(cur_page)) {
		  override_predecessor = false;
	  }
  }

  /* free predecessor buffer if we want to overwrite it or if we destroy the document for good */
  if (override_predecessor || !keep_monitor || !smooth_reload) {
    document_predecessor_free(zathura);
  }

  /* remove widgets */
  gtk_container_foreach(GTK_CONTAINER(zathura->ui.page_widget), remove_page_from_table, NULL);

  if (!override_predecessor) {
	  for (unsigned int i = 0; i < zathura_document_get_number_of_pages(zathura->document); i++) {
		g_object_unref(zathura->pages[i]);
	  }
	  free(zathura->pages);
	  zathura->pages = NULL;

	  /* remove document */
	  zathura_document_free(zathura->document);
	  zathura->document = NULL;
  } else {
	  girara_debug("preserving pages and document as predecessor");
	  zathura->predecessor_pages = zathura->pages;
	  zathura->pages = NULL;
	  zathura->predecessor_document = zathura->document;
	  zathura->document = NULL;
  }

  /* remove index */
  if (zathura->ui.index != NULL) {
    g_object_ref_sink(zathura->ui.index);
    zathura->ui.index = NULL;
  }

  gtk_widget_hide(zathura->ui.page_widget);

  statusbar_page_number_update(zathura);

  if (zathura->ui.session != NULL && zathura->ui.statusbar.file != NULL) {
    girara_statusbar_item_set_text(zathura->ui.session, zathura->ui.statusbar.file, _("[No name]"));
  }

  /* update title */
  girara_set_window_title(zathura->ui.session, "zathura");

  return true;
}

bool
page_set(zathura_t* zathura, unsigned int page_id)
{
  if (zathura == NULL || zathura->document == NULL) {
    goto error_out;
  }

  zathura_page_t* page = zathura_document_get_page(zathura->document, page_id);
  if (page == NULL) {
    goto error_out;
  }

  zathura_document_set_current_page_number(zathura->document, page_id);

  bool continuous_hist_save = false;
  girara_setting_get(zathura->ui.session, "continuous-hist-save", &continuous_hist_save);
  if (continuous_hist_save) {
    save_fileinfo_to_db(zathura);
  }

  /* negative position means auto */
  return position_set(zathura, -1, -1);

error_out:
  return false;
}

void
statusbar_page_number_update(zathura_t* zathura)
{
  if (zathura == NULL || zathura->ui.statusbar.page_number == NULL) {
    return;
  }

  unsigned int number_of_pages     = zathura_document_get_number_of_pages(zathura->document);
  unsigned int current_page_number = zathura_document_get_current_page_number(zathura->document);
  unsigned int page_number_percent = number_of_pages ? 100 * (current_page_number + 1) / number_of_pages : 0;

  if (zathura->document != NULL) {
    zathura_page_t* page = zathura_document_get_page(zathura->document, current_page_number);
    char* page_label = zathura_page_get_label(page, NULL);

    bool show_percent = false;
    girara_setting_get(zathura->ui.session, "statusbar-page-percent", &show_percent);

    char* page_number_text = NULL;
    if (page_label != NULL) {
      if (show_percent) {
        page_number_text = g_strdup_printf("[%s (%d/%d) (%d%%)]", page_label, current_page_number + 1, number_of_pages, page_number_percent);
      } else {
        page_number_text = g_strdup_printf("[%s (%d/%d)]", page_label, current_page_number + 1, number_of_pages);
      }
      g_free(page_label);
    } else {
      if (show_percent) {
        page_number_text = g_strdup_printf("[%d/%d (%d%%)]", current_page_number + 1, number_of_pages, page_number_percent);
      } else {
        page_number_text = g_strdup_printf("[%d/%d]", current_page_number + 1, number_of_pages);
      }
    }
    girara_statusbar_item_set_text(zathura->ui.session, zathura->ui.statusbar.page_number, page_number_text);

    bool page_number_in_window_title = false;
    girara_setting_get(zathura->ui.session, "window-title-page", &page_number_in_window_title);

    if (page_number_in_window_title == true) {
      char* filename = get_formatted_filename(zathura, false);
      char* title = g_strdup_printf("%s %s", filename, page_number_text);
      girara_set_window_title(zathura->ui.session, title);
      g_free(title);
      g_free(filename);
    }

    g_free(page_number_text);
  } else {
    girara_statusbar_item_set_text(zathura->ui.session, zathura->ui.statusbar.page_number, "");
  }
}

void
page_widget_set_mode(zathura_t* zathura, unsigned int page_padding,
                     unsigned int pages_per_row, unsigned int first_page_column,
                     bool page_right_to_left)
{
  /* show at least one page */
  if (pages_per_row == 0) {
    pages_per_row = 1;
  }

  /* ensure: 0 < first_page_column <= pages_per_row */
  if (first_page_column < 1) {
    first_page_column = 1;
  }
  if (first_page_column > pages_per_row) {
    first_page_column = ((first_page_column - 1) % pages_per_row) + 1;
  }

  if (zathura->document == NULL) {
    return;
  }

  gtk_container_foreach(GTK_CONTAINER(zathura->ui.page_widget), remove_page_from_table, NULL);

  unsigned int number_of_pages     = zathura_document_get_number_of_pages(zathura->document);

  gtk_grid_set_row_spacing(GTK_GRID(zathura->ui.page_widget), page_padding);
  gtk_grid_set_column_spacing(GTK_GRID(zathura->ui.page_widget), page_padding);

  for (unsigned int i = 0; i < number_of_pages; i++) {
    int x = (i + first_page_column - 1) % pages_per_row;
    int y = (i + first_page_column - 1) / pages_per_row;

    GtkWidget* page_widget = zathura->pages[i];
    if (page_right_to_left) {
      x = pages_per_row - 1 - x;
    }

    gtk_grid_attach(GTK_GRID(zathura->ui.page_widget), page_widget, x, y, 1, 1);
  }

  gtk_widget_show_all(zathura->ui.page_widget);
}

bool
position_set(zathura_t* zathura, double position_x, double position_y)
{
  if (zathura == NULL || zathura->document == NULL) {
    return false;
  }

  double comppos_x, comppos_y;
  const unsigned int page_id = zathura_document_get_current_page_number(zathura->document);

  bool vertical_center = false;
  girara_setting_get(zathura->ui.session, "vertical-center", &vertical_center);

  /* xalign = 0.5: center horizontally (with the page, not the document) */
  if (vertical_center == true) {
    /* yalign = 0.5: center vertically */
    page_number_to_position(zathura->document, page_id, 0.5, 0.5, &comppos_x, &comppos_y);
  } else {
    /* yalign = 0.0: align page an viewport edges at the top               */
    page_number_to_position(zathura->document, page_id, 0.5, 0.0, &comppos_x, &comppos_y);
  }

  /* automatic horizontal adjustment */
  zathura_adjust_mode_t adjust_mode = zathura_document_get_adjust_mode(zathura->document);

  /* negative position_x mean: use the computed value */
  if (position_x < 0) {
    position_x = comppos_x;
    bool zoom_center = false;
    girara_setting_get(zathura->ui.session, "zoom-center", &zoom_center);

    /* center horizontally */
    if (adjust_mode == ZATHURA_ADJUST_BESTFIT ||
      adjust_mode == ZATHURA_ADJUST_WIDTH ||
      zoom_center == true) {
        position_x = 0.5;
    }
  }

  if (position_y < 0) {
    position_y = comppos_y;
  }

  /* set the position */
  zathura_document_set_position_x(zathura->document, position_x);
  zathura_document_set_position_y(zathura->document, position_y);

  /* trigger a 'change' event for both adjustments */
  refresh_view(zathura);

  return true;
}

void
refresh_view(zathura_t* zathura)
{
  g_return_if_fail(zathura != NULL);

  /* emit a custom refresh-view signal */
  g_signal_emit(zathura->ui.session->gtk.view, zathura->signals.refresh_view,
                0, zathura);
}

bool
adjust_view(zathura_t* zathura)
{
  g_return_val_if_fail(zathura != NULL, false);

  if (zathura->ui.page_widget == NULL || zathura->document == NULL) {
    goto error_ret;
  }

  zathura_adjust_mode_t adjust_mode = zathura_document_get_adjust_mode(zathura->document);
  if (adjust_mode == ZATHURA_ADJUST_NONE) {
    /* there is nothing todo */
    goto error_ret;
  }

  unsigned int cell_height = 0, cell_width = 0;
  unsigned int document_height = 0, document_width = 0;
  unsigned int view_height = 0, view_width = 0;

  zathura_document_get_cell_size(zathura->document, &cell_height, &cell_width);
  zathura_document_get_document_size(zathura->document, &document_height, &document_width);
  zathura_document_get_viewport_size(zathura->document, &view_height, &view_width);

  if (view_height == 0 || view_width == 0 || cell_height == 0 || cell_width == 0 || document_width == 0) {
    goto error_ret;
  }

  double page_ratio = (double)cell_height / (double)document_width;
  double view_ratio = (double)view_height / (double)view_width;
  double zoom = zathura_document_get_zoom(zathura->document);
  double newzoom = zoom;

  if (adjust_mode == ZATHURA_ADJUST_WIDTH ||
      (adjust_mode == ZATHURA_ADJUST_BESTFIT && page_ratio < view_ratio)) {
    newzoom *= (double)view_width / (double)document_width;
  } else if (adjust_mode == ZATHURA_ADJUST_BESTFIT) {
    newzoom *= (double)view_height / (double)cell_height;
  } else {
    goto error_ret;
  }

  /* save new zoom and recompute cell size */
  zathura_document_set_zoom(zathura->document, newzoom);
  unsigned int new_cell_height = 0, new_cell_width = 0;
  zathura_document_get_cell_size(zathura->document, &new_cell_height, &new_cell_width);

  /*
   * XXX requiring a larger difference apparently circumvents #94 for some users; this is not a
   * proper fix
   */
  static const int min_change = 2;
  /* if the change in zoom changes page cell dimensions, render */
  if (abs((int)new_cell_width - (int)cell_width) > min_change ||
      abs((int)new_cell_height - (int)cell_height) > min_change) {
    render_all(zathura);
    refresh_view(zathura);
  } else {
    /* otherwise set the old zoom and leave */
    zathura_document_set_zoom(zathura->document, zoom);
  }

error_ret:
  return false;
}

#ifdef G_OS_UNIX
static gboolean
zathura_signal_sigterm(gpointer data)
{
  if (data != NULL) {
    zathura_t* zathura = data;
    cb_destroy(NULL, zathura);
  }

  return TRUE;
}
#endif

void zathura_show_signature_information(zathura_t* zathura, bool show) {
  if (zathura->document == NULL) {
    return;
  }

  GValue show_sig_info_value = {0};
  g_value_init(&show_sig_info_value, G_TYPE_BOOLEAN);
  g_value_set_boolean(&show_sig_info_value, show);

  const unsigned int number_of_pages = zathura_document_get_number_of_pages(zathura->document);
  for (unsigned int page = 0; page != number_of_pages; ++page) {
    // draw signature info
    g_object_set_property(G_OBJECT(zathura->pages[page]), "draw-signatures", &show_sig_info_value);
  }

  g_value_unset(&show_sig_info_value);
}
