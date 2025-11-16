/* SPDX-License-Identifier: Zlib */

#ifdef GTKOSXAPPLICATION
#include <gtkosxapplication.h>
#endif

#include <girara/settings.h>
#include <girara/log.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zathura.h"
#include "plugin.h"
#include "utils.h"
#ifdef WITH_SYNCTEX
#include "dbus-interface.h"
#include "synctex.h"
#endif

/* Handle synctex forward synchronization */
#ifdef WITH_SYNCTEX
static int run_synctex_forward(const char* synctex_fwd, const char* filename, int synctex_pid) {
  g_autoptr(GFile) file = g_file_new_for_commandline_arg(filename);
  if (file == NULL) {
    girara_error("Unable to handle argument '%s'.", filename);
    return -1;
  }

  g_autofree char* real_path = g_file_get_path(file);
  if (real_path == NULL) {
    girara_error("Failed to determine path for '%s'", filename);
    return -1;
  }

  int line         = 0;
  int column       = 0;
  g_autofree char* input_file = NULL;
  if (synctex_parse_input(synctex_fwd, &input_file, &line, &column) == false) {
    girara_error("Failed to parse argument to --synctex-forward.");
    return -1;
  }

  const int ret = zathura_dbus_synctex_position(real_path, input_file, line, column, synctex_pid);
  if (ret == -1) {
    /* D-Bus or SyncTeX failed */
    girara_error("Got no usable data from SyncTeX or D-Bus failed in some way.");
  }

  return ret;
}
#endif

static zathura_t* init_zathura(const char* config_dir, const char* data_dir, const char* cache_dir,
                               const char* plugin_path, char** argv, const char* synctex_editor, Window embed) {
  /* create zathura session */
  zathura_t* zathura = zathura_create();
  if (zathura == NULL) {
    return NULL;
  }

  zathura_set_xid(zathura, embed);
  zathura_set_config_dir(zathura, config_dir);
  zathura_set_data_dir(zathura, data_dir);
  zathura_set_cache_dir(zathura, cache_dir);
  zathura_set_plugin_dir(zathura, plugin_path);
  zathura_set_argv(zathura, argv);

  /* Init zathura */
  if (zathura_init(zathura) == false) {
    zathura_free(zathura);
    return NULL;
  }

  if (synctex_editor != NULL) {
    girara_setting_set(zathura->ui.session, "synctex-editor-command", synctex_editor);
  }

  return zathura;
}

/* main function */
GIRARA_VISIBLE int main(int argc, char* argv[]) {
  zathura_init_locale();

  /* parse command line arguments */
  g_autofree gchar* config_dir     = NULL;
  g_autofree gchar* data_dir       = NULL;
  g_autofree gchar* cache_dir      = NULL;
  g_autofree gchar* plugin_path    = NULL;
  g_autofree gchar* loglevel       = NULL;
  g_autofree gchar* password       = NULL;
  g_autofree gchar* synctex_editor = NULL;
  g_autofree gchar* synctex_fwd    = NULL;
  g_autofree gchar* mode           = NULL;
  g_autofree gchar* bookmark_name  = NULL;
  g_autofree gchar* search_string  = NULL;
  gboolean forkback                = false;
  gboolean print_version           = false;
  gint page_number                 = ZATHURA_PAGE_NUMBER_UNSPECIFIED;
  gint synctex_pid                 = -1;
  Window embed                     = 0;

  GOptionEntry entries[] = {
      {"reparent", 'e', 0, G_OPTION_ARG_INT, &embed, _("Reparents to window specified by xid (X11)"), "xid"},
      {"config-dir", 'c', 0, G_OPTION_ARG_FILENAME, &config_dir, _("Path to the config directory"), "path"},
      {"data-dir", 'd', 0, G_OPTION_ARG_FILENAME, &data_dir, _("Path to the data directory"), "path"},
      {"cache-dir", '\0', 0, G_OPTION_ARG_FILENAME, &cache_dir, _("Path to the cache directory"), "path"},
      {"plugins-dir", 'p', 0, G_OPTION_ARG_STRING, &plugin_path, _("Path to the directories containing plugins"),
       "path"},
      {"fork", '\0', 0, G_OPTION_ARG_NONE, &forkback, _("Fork into the background"), NULL},
      {"password", 'w', 0, G_OPTION_ARG_STRING, &password, _("Document password"), "password"},
      {"page", 'P', 0, G_OPTION_ARG_INT, &page_number, _("Page number to go to"), "number"},
      {"log-level", 'l', 0, G_OPTION_ARG_STRING, &loglevel, _("Log level (debug, info, warning, error)"), "level"},
      {"version", 'v', 0, G_OPTION_ARG_NONE, &print_version, _("Print version information"), NULL},
      {"synctex-editor-command", 'x', 0, G_OPTION_ARG_STRING, &synctex_editor,
       _("Synctex editor (forwarded to the synctex command)"), "cmd"},
      {"synctex-forward", '\0', 0, G_OPTION_ARG_STRING, &synctex_fwd, _("Move to given synctex position"), "position"},
      {"synctex-pid", '\0', 0, G_OPTION_ARG_INT, &synctex_pid, _("Highlight given position in the given process"),
       "pid"},
      {"mode", '\0', 0, G_OPTION_ARG_STRING, &mode, _("Start in a non-default mode"), "mode"},
      {"bookmark", 'b', 0, G_OPTION_ARG_STRING, &bookmark_name, _("Bookmark to go to"), "bookmark"},
      {"find", 'f', 0, G_OPTION_ARG_STRING, &search_string, _("Search for the given phrase and display results"),
       "string"},
      {NULL, '\0', 0, 0, NULL, NULL, NULL},
  };

  GOptionContext* context = g_option_context_new(" [file1] [file2] [...]");
  g_option_context_add_main_entries(context, entries, NULL);

  g_autoptr(GError) error = NULL;
  if (g_option_context_parse(context, &argc, &argv, &error) == false) {
    girara_error("Error parsing command line arguments: %s\n", error->message);
    g_option_context_free(context);

    return -1;
  }
  g_option_context_free(context);

  zathura_set_log_level(loglevel);

#ifdef WITH_SYNCTEX
  /* handle synctex forward synchronization */
  if (synctex_fwd != NULL) {
    if (argc != 2) {
      girara_error("Too many arguments or missing filename while running with "
                   "--synctex-forward");
      return -1;
    }

    int ret = run_synctex_forward(synctex_fwd, argv[1], synctex_pid);
    if (ret > 0) {
      /* Instance found. */
      return 0;
    } else if (ret < 0) {
      /* Error occurred. */
      return -1;
    }

    girara_debug("No instance found. Starting new one.");
  }
#else
  if (synctex_fwd != NULL || synctex_editor != NULL || synctex_pid != -1) {
    girara_error("Built without synctex support, but synctex specific option was specified.");
    return -1;
  }
#endif

  /* check mode */
  if (mode != NULL && g_strcmp0(mode, "presentation") != 0 && g_strcmp0(mode, "fullscreen") != 0) {
    girara_error("Invalid argument for --mode: %s", mode);
    return -1;
  }

  /* g_option_context_parse has some funny (documented) behavior:
   * * for "-- a b c" you get no -- in argv
   * * for "-- --" you get -- in argv twice
   * * for "-- -a" you get -- in argv
   *
   * So if there is one -- in argv, we need to ignore it. */
  const bool has_double_dash = argc > 1 && g_strcmp0(argv[1], "--") == 0;
  const int file_idx_base    = has_double_dash ? 2 : 1;

  int file_idx = argc > file_idx_base ? file_idx_base : 0;
  /* Fork instances for other files. */
  if (print_version == false && argc > file_idx_base + 1) {
    for (int idx = file_idx_base + 1; idx < argc; ++idx) {
      const pid_t pid = fork();
      if (pid == 0) { /* child */
        file_idx = idx;
        if (setsid() == -1) {
          girara_error("Could not start new process group: %s", strerror(errno));
          return -1;
        }
        break;
      } else if (pid < 0) { /* error */
        girara_error("Could not fork: %s", strerror(errno));
        return -1;
      }
    }
  }

  /* Fork into the background if the user really wants to ... */
  if (print_version == false && forkback == true && file_idx < file_idx_base + 1) {
    const pid_t pid = fork();
    if (pid > 0) { /* parent */
      return 0;
    } else if (pid < 0) { /* error */
      girara_error("Could not fork: %s", strerror(errno));
      return -1;
    }

    if (setsid() == -1) {
      girara_error("Could not start new process group: %s", strerror(errno));
      return -1;
    }
  }

  /* Print version */
  if (print_version == true) {
    g_autoptr(zathura_plugin_manager_t) plugin_manager = zathura_plugin_manager_new();
    zathura_plugin_manager_set_dir(plugin_manager, plugin_path);
    zathura_plugin_manager_load(plugin_manager);

    g_autofree char* string = zathura_get_version_string(plugin_manager, false);
    if (string != NULL) {
      fprintf(stdout, "%s\n", string);
    }
    return 0;
  }

  /* Initialize GTK+ */
  gtk_init(&argc, &argv);

  /* Create zathura session */
  g_autoptr(zathura_t) zathura = init_zathura(config_dir, data_dir, cache_dir, plugin_path, argv, synctex_editor, embed);
  if (zathura == NULL) {
    girara_error("Could not initialize zathura.");
    return -1;
  }

  /* open document if passed */
  if (file_idx != 0) {
    if (page_number > 0) {
      --page_number;
    }
    document_open_idle(zathura, argv[file_idx], password, page_number, mode, synctex_fwd, bookmark_name, search_string);
  } else if (bookmark_name != NULL) {
    girara_error("Can not use bookmark argument when no file is given");
    return -1;
  } else if (search_string != NULL) {
    girara_error("Can not use find argument when no file is given");
    return -1;
  }

#ifdef GTKOSXAPPLICATION
  GtkosxApplication* zathuraApp = g_object_new(GTKOSX_TYPE_APPLICATION, NULL);
  gtkosx_application_set_use_quartz_accelerators(zathuraApp, FALSE);
  gtkosx_application_ready(zathuraApp);
  {
    const gchar* id = gtkosx_application_get_bundle_id();
    if (id != NULL) {
      girara_warning("TestIntegration Error! Bundle has ID %s", id);
    }
  }
#endif // GTKOSXAPPLICATION

  /* run zathura */
  gtk_main();
  return 0;
}
