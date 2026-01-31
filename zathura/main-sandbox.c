/* SPDX-License-Identifier: Zlib */

#include <girara/settings.h>
#include <girara/log.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "zathura.h"
#include "plugin.h"
#include "utils.h"
#ifdef WITH_SECCOMP
#include "seccomp-filters.h"
#endif
#ifdef WITH_LANDLOCK
#include "landlock.h"
#endif

static zathura_t* init_zathura(const char* config_dir, const char* data_dir, const char* cache_dir,
                               const char* plugin_path, char** argv, Window embed) {
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

  girara_debug("Strict sandbox preventing write and network access.");
#ifdef WITH_LANDLOCK
  landlock_drop_write();
#endif
#ifdef WITH_SECCOMP
  if (seccomp_enable_strict_filter(zathura) != 0) {
    girara_error("Failed to initialize strict seccomp filter.");
    zathura_free(zathura);
    return NULL;
  }
#endif
#ifdef __OpenBSD__
  if (pledge("stdio rpath", "") != 0) {
    girara_error("Failed to pledge: %s", strerror(errno));
    zathura_free(zathura);
    return NULL;
  }
#endif
  /* unset the input method to avoid communication with external services */
  unsetenv("GTK_IM_MODULE");

  return zathura;
}

/* main function */
GIRARA_VISIBLE int main(int argc, char* argv[]) {
  zathura_init_locale();

  /* parse command line arguments */
  g_autofree gchar* config_dir    = NULL;
  g_autofree gchar* data_dir      = NULL;
  g_autofree gchar* cache_dir     = NULL;
  g_autofree gchar* plugin_path   = NULL;
  g_autofree gchar* loglevel      = NULL;
  g_autofree gchar* password      = NULL;
  g_autofree gchar* mode          = NULL;
  g_autofree gchar* bookmark_name = NULL;
  g_autofree gchar* search_string = NULL;
  bool forkback                   = false;
  bool print_version              = false;
  int page_number                 = ZATHURA_PAGE_NUMBER_UNSPECIFIED;
  Window embed                    = 0;

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
      {"mode", '\0', 0, G_OPTION_ARG_STRING, &mode, _("Start in a non-default mode"), "mode"},
      {"bookmark", 'b', 0, G_OPTION_ARG_STRING, &bookmark_name, _("Bookmark to go to"), "bookmark"},
      {"find", 'f', 0, G_OPTION_ARG_STRING, &search_string, _("Search for the given phrase and display results"),
       "string"},
      {NULL, '\0', 0, 0, NULL, NULL, NULL},
  };

  g_autoptr(GOptionContext) context = g_option_context_new(" [file1] [file2] [...]");
  g_option_context_add_main_entries(context, entries, NULL);

  g_autoptr(GError) error = NULL;
  if (g_option_context_parse(context, &argc, &argv, &error) == false) {
    girara_error("Error parsing command line arguments: %s\n", error->message);
    return -1;
  }

  zathura_set_log_level(loglevel);

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
  /* If more than one file, fork an instance for each. */
  if (print_version == false && argc > file_idx_base + 1) {
    const pid_t parent_pid              = getpid();
    g_autoptr(girara_list_t) child_pids = girara_list_new();

    for (int idx = file_idx_base; idx < argc; ++idx) {
      const pid_t pid = fork();
      if (pid == 0) {
        // child process
        file_idx = idx;
        if (forkback == true && setsid() == -1) {
          // start new process group if forkback was requested
          girara_error("Could not start new process group: %s", strerror(errno));
          return -1;
        }
        break;
      } else if (pid < 0) {
        // error
        girara_error("Could not fork: %s", strerror(errno));
        return -1;
      } else {
        // parent process
        girara_list_append(child_pids, (void*)(intptr_t)pid);
      }
    }

    if (parent_pid == getpid()) {
      // parent
      if (forkback == true) {
        // forkback was requested, so no need to wait
        return 0;
      }
      // wait for all children
      for (size_t idx = 0; idx != girara_list_size(child_pids); ++idx) {
        const pid_t pid = (pid_t)(intptr_t)girara_list_nth(child_pids, idx);
        waitpid(pid, NULL, 0);
      }
      return 0;
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
    zathura_plugin_manager_t* plugin_manager = zathura_plugin_manager_new();
    zathura_plugin_manager_set_dir(plugin_manager, plugin_path);
    zathura_plugin_manager_load(plugin_manager);

    char* string = zathura_get_version_string(plugin_manager, false);
    if (string != NULL) {
      fprintf(stdout, "%s\n", string);
      g_free(string);
    }
    zathura_plugin_manager_free(plugin_manager);

    return 0;
  }

  girara_debug("Running zathura-sandbox, disable IPC services");
  /* Prevent default gtk dbus connection */
  g_setenv("DBUS_SESSION_BUS_ADDRESS", "disabled:", TRUE);

  /* disable dconf writing - uses /var/empty as alternative to /dev/null to avoid ioctl call */
  g_setenv("DCONF_PROFILE", "/var/empty", TRUE);

  /* Initialize GTK+ */
  gtk_init(&argc, &argv);

  /* Create zathura session */
  g_autoptr(zathura_t) zathura = init_zathura(config_dir, data_dir, cache_dir, plugin_path, argv, embed);
  if (zathura == NULL) {
    girara_error("Could not initialize zathura.");
    return -1;
  }

  /* open document if passed */
  if (file_idx != 0) {
    if (page_number > 0) {
      --page_number;
    }
    document_open_idle(zathura, argv[file_idx], password, page_number, mode, NULL, bookmark_name, search_string);
  } else if (bookmark_name != NULL) {
    girara_error("Can not use bookmark argument when no file is given");
    return -1;
  } else if (search_string != NULL) {
    girara_error("Can not use find argument when no file is given");
    return -1;
  }

  /* run zathura */
  gtk_main();
  return 0;
}
