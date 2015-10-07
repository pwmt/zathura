/* See LICENSE file for license and copyright information */

#include <girara/utils.h>
#include <girara/settings.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zathura.h"
#include "utils.h"
#include "dbus-interface.h"
#include "synctex.h"

/* main function */
int
main(int argc, char* argv[])
{
  /* init locale */
  setlocale(LC_ALL, "");
  bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
  textdomain(GETTEXT_PACKAGE);

  /* init gtk */
#if !GLIB_CHECK_VERSION(2, 31, 0)
  g_thread_init(NULL);
#endif
#if !GTK_CHECK_VERSION(3, 6, 0)
  gdk_threads_init();
#endif
  gtk_init(&argc, &argv);

  /* parse command line arguments */
  gchar* config_dir     = NULL;
  gchar* data_dir       = NULL;
  gchar* cache_dir      = NULL;
  gchar* plugin_path    = NULL;
  gchar* loglevel       = NULL;
  gchar* password       = NULL;
  gchar* synctex_editor = NULL;
  gchar* synctex_fwd    = NULL;
  gchar* mode           = NULL;
  bool forkback         = false;
  bool print_version    = false;
  int page_number       = ZATHURA_PAGE_NUMBER_UNSPECIFIED;
  int synctex_pid       = -1;
#ifdef GDK_WINDOWING_X11
  Window embed          = 0;
#endif

  GOptionEntry entries[] = {
#ifdef GDK_WINDOWING_X11
    { "reparent",               'e',  0, G_OPTION_ARG_INT,      &embed,          _("Reparents to window specified by xid"),              "xid"  },
#endif
    { "config-dir",             'c',  0, G_OPTION_ARG_FILENAME, &config_dir,     _("Path to the config directory"),                      "path" },
    { "data-dir",               'd',  0, G_OPTION_ARG_FILENAME, &data_dir,       _("Path to the data directory"),                        "path" },
    { "cache-dir",              '\0', 0, G_OPTION_ARG_FILENAME, &cache_dir,      _("Path to the cache directory"),                       "path"},
    { "plugins-dir",            'p',  0, G_OPTION_ARG_STRING,   &plugin_path,    _("Path to the directories containing plugins"),        "path" },
    { "fork",                   '\0', 0, G_OPTION_ARG_NONE,     &forkback,       _("Fork into the background"),                          NULL },
    { "password",               'w',  0, G_OPTION_ARG_STRING,   &password,       _("Document password"),                                 "password" },
    { "page",                   'P',  0, G_OPTION_ARG_INT,      &page_number,    _("Page number to go to"),                              "number" },
    { "debug",                  'l',  0, G_OPTION_ARG_STRING,   &loglevel,       _("Log level (debug, info, warning, error)"),           "level" },
    { "version",                'v',  0, G_OPTION_ARG_NONE,     &print_version,  _("Print version information"),                         NULL },
    { "synctex-editor-command", 'x',  0, G_OPTION_ARG_STRING,   &synctex_editor, _("Synctex editor (forwarded to the synctex command)"), "cmd" },
    { "synctex-forward",        '\0', 0, G_OPTION_ARG_STRING,   &synctex_fwd,    _("Move to given synctex position"),                    "position" },
    { "synctex-pid",            '\0', 0, G_OPTION_ARG_INT,      &synctex_pid,    _("Highlight given position in the given process"),     "pid" },
    { "mode",                   '\0', 0, G_OPTION_ARG_STRING,   &mode,           _("Start in a non-default mode"),                       "mode" },
    { NULL, '\0', 0, 0, NULL, NULL, NULL }
  };

  GOptionContext* context = g_option_context_new(" [file1] [file2] [...]");
  g_option_context_add_main_entries(context, entries, NULL);

  GError* error = NULL;
  if (g_option_context_parse(context, &argc, &argv, &error) == false) {
    girara_error("Error parsing command line arguments: %s\n", error->message);
    g_option_context_free(context);
    g_error_free(error);

    return -1;
  }
  g_option_context_free(context);

  /* Set log level. */
  if (loglevel == NULL || g_strcmp0(loglevel, "info") == 0) {
    girara_set_debug_level(GIRARA_INFO);
  } else if (g_strcmp0(loglevel, "warning") == 0) {
    girara_set_debug_level(GIRARA_WARNING);
  } else if (g_strcmp0(loglevel, "error") == 0) {
    girara_set_debug_level(GIRARA_ERROR);
  }

  /* handle synctex forward synchronization */
  if (synctex_fwd != NULL) {
    if (argc != 2) {
      girara_error("Too many arguments or missing filename while running with --synctex-forward");
      return -1;
    }

    GFile* file = g_file_new_for_commandline_arg(argv[1]);
    if (file == NULL) {
      girara_error("Unable to handle argument '%s'.", argv[1]);
      return -1;
    }

    char* real_path = g_file_get_path(file);
    g_object_unref(file);
    if (real_path == NULL) {
      girara_error("Failed to determine path for '%s'", argv[1]);
      return -1;
    }

    int line = 0;
    int column = 0;
    char* input_file = NULL;
    if (synctex_parse_input(synctex_fwd, &input_file, &line, &column) == false) {
      girara_error("Failed to parse argument to --synctex-forward.");
      g_free(real_path);
      return -1;
    }

    const int ret = zathura_dbus_synctex_position(real_path, input_file, line, column, synctex_pid);
    g_free(input_file);
    g_free(real_path);

    if (ret == -1) {
      /* D-Bus or SyncTeX failed */
      girara_error("Got no usable data from SyncTeX or D-Bus failed in some way.");
      return -1;
    } else if (ret == 1) {
      /* Found a instance */
      return 0;
    }

    girara_debug("No instance found. Starting new one.");
  }

  /* check mode */
  if (mode != NULL && g_strcmp0(mode, "presentation") != 0 && g_strcmp0(mode, "fullscreen") != 0) {
    girara_error("Invalid argument for --mode: %s", mode);
    return -1;
  }

  /* Fork into the background if the user really wants to ... */
  if (forkback == true) {
    const pid_t pid = fork();
    if (pid > 0) { /* parent */
      return 0;
    } else if (pid < 0) { /* error */
      girara_error("Couldn't fork.");
    }

    setsid();
  }

  /* create zathura session */
  zathura_t* zathura = zathura_create();
  if (zathura == NULL) {
    return -1;
  }

#ifdef GDK_WINDOWING_X11
  zathura_set_xid(zathura, embed);
#endif
  zathura_set_config_dir(zathura, config_dir);
  zathura_set_data_dir(zathura, data_dir);
  zathura_set_cache_dir(zathura, cache_dir);
  zathura_set_plugin_dir(zathura, plugin_path);
  zathura_set_argv(zathura, argv);

  /* Init zathura */
  if (zathura_init(zathura) == false) {
    girara_error("Could not initialize zathura.");
    zathura_free(zathura);
    return -1;
  }

  if (synctex_editor != NULL) {
    girara_setting_set(zathura->ui.session, "synctex-editor-command", synctex_editor);
  }

  /* Print version */
  if (print_version == true) {
    char* string = zathura_get_version_string(zathura, false);
    if (string != NULL) {
      fprintf(stdout, "%s\n", string);
      g_free(string);
    }
    zathura_free(zathura);

    return 0;
  }

  /* open document if passed */
  if (argc > 1) {
    if (page_number > 0)
      --page_number;
    document_open_idle(zathura, argv[1], password, page_number, mode, synctex_fwd);

    /* open additional files */
    for (int i = 2; i < argc; i++) {
      char* new_argv[] = {
        *(zathura->global.arguments),
        argv[i],
        NULL
      };

      g_spawn_async(NULL, new_argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
    }
  }

  /* run zathura */
#if !GTK_CHECK_VERSION(3, 6, 0)
  gdk_threads_enter();
#endif
  gtk_main();
#if !GTK_CHECK_VERSION(3, 6, 0)
  gdk_threads_leave();
#endif

  /* free zathura */
  zathura_free(zathura);

  return 0;
}
