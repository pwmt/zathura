/* SPDX-License-Identifier: Zlib */

#include <girara-gtk/settings.h>
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
                               const char* plugin_path, char** argv) {
  /* create zathura session */
  zathura_t* zathura = zathura_create();
  if (zathura == NULL) {
    return NULL;
  }

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
  if (landlock_drop_write() < 0) {
    girara_error("Failed to apply landlock write restriction.");
    zathura_free(zathura);
    return NULL;
  }
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

  return zathura;
}

/* state shared between the application lifecycle callbacks */
typedef struct {
  const char* config_dir;
  const char* data_dir;
  const char* cache_dir;
  const char* plugin_path;
  const char* password;
  const char* mode;
  const char* bookmark_name;
  const char* search_string;
  /* file argument exactly as given on the command line */
  const char* raw_file;
  int page_number;
  char** argv;
  zathura_t* zathura;
} zathura_app_ctx_t;

static void cb_app_startup(GApplication* app, gpointer data) {
  zathura_app_ctx_t* ctx = data;

  ctx->zathura = init_zathura(ctx->config_dir, ctx->data_dir, ctx->cache_dir, ctx->plugin_path, ctx->argv);
  if (ctx->zathura == NULL) {
    girara_error("Could not initialize zathura.");
    g_application_quit(app);
    return;
  }

  gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(ctx->zathura->ui.session->gtk.window));
}

static void cb_app_activate(GApplication* UNUSED(app), gpointer data) {
  /* present the window when the app starts without a file */
  zathura_app_ctx_t* ctx = data;
  if (ctx->zathura != NULL && ctx->zathura->ui.session != NULL && ctx->zathura->ui.session->gtk.window != NULL) {
    gtk_window_present(GTK_WINDOW(ctx->zathura->ui.session->gtk.window));
  }
}

static void cb_app_open(GApplication* UNUSED(app), GFile** files, gint n_files, const gchar* UNUSED(hint),
                        gpointer data) {
  zathura_app_ctx_t* ctx = data;
  if (ctx->zathura == NULL || n_files < 1) {
    return;
  }

  /* gfile turns a plain dash into an absolute path which breaks stdin so keep the raw argument */
  g_autofree char* gfile_path = NULL;
  const char* path            = NULL;
  if (g_strcmp0(ctx->raw_file, "-") == 0) {
    path = "-";
  } else {
    gfile_path = g_file_get_path(files[0]);
    /* g_file_get_path returns NULL for non-local URIs; fall back to the raw argument */
    path = gfile_path != NULL ? gfile_path : ctx->raw_file;
  }
  if (path == NULL) {
    girara_error("Failed to determine path for the given file.");
    return;
  }

  int page_number = ctx->page_number;
  if (page_number > 0) {
    --page_number;
  }
  document_open_idle(ctx->zathura, path, ctx->password, page_number, ctx->mode, NULL, ctx->bookmark_name,
                     ctx->search_string);
}

static void cb_app_shutdown(GApplication* UNUSED(app), gpointer data) {
  zathura_app_ctx_t* ctx = data;
  if (ctx->zathura != NULL) {
    zathura_free(ctx->zathura);
    ctx->zathura = NULL;
  }
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

  GOptionEntry entries[] = {
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

  /* use the software renderer to avoid the gpu stack attack surface (and high risk syscalls) */
  g_setenv("GSK_RENDERER", "cairo", TRUE);

  /* unset the input method to avoid communication with external services */
  unsetenv("GTK_IM_MODULE");

  /* fail early so these errors still exit with an error status */
  if (file_idx == 0) {
    if (bookmark_name != NULL) {
      girara_error("Can not use bookmark argument when no file is given");
      return -1;
    }
    if (search_string != NULL) {
      girara_error("Can not use find argument when no file is given");
      return -1;
    }
  }

  /* run zathura as a GtkApplication */
  zathura_app_ctx_t ctx = {
      .config_dir    = config_dir,
      .data_dir      = data_dir,
      .cache_dir     = cache_dir,
      .plugin_path   = plugin_path,
      .password      = password,
      .mode          = mode,
      .bookmark_name = bookmark_name,
      .search_string = search_string,
      .raw_file      = NULL,
      .page_number   = page_number,
      .argv          = argv,
      .zathura       = NULL,
  };

  /* a NULL application id keeps the process non-unique and skips D-Bus registration */
  g_autoptr(GtkApplication) app = gtk_application_new(NULL, G_APPLICATION_NON_UNIQUE | G_APPLICATION_HANDLES_OPEN);
  g_signal_connect(app, "startup", G_CALLBACK(cb_app_startup), &ctx);
  g_signal_connect(app, "activate", G_CALLBACK(cb_app_activate), &ctx);
  g_signal_connect(app, "open", G_CALLBACK(cb_app_open), &ctx);
  g_signal_connect(app, "shutdown", G_CALLBACK(cb_app_shutdown), &ctx);

  /* feed g_application_run a minimal argv so it routes via open or activate */
  char* run_argv[3] = {argv[0], NULL, NULL};
  int run_argc      = 1;
  if (file_idx != 0) {
    ctx.raw_file = argv[file_idx];
    run_argv[1]  = argv[file_idx];
    run_argc     = 2;
  }

  return g_application_run(G_APPLICATION(app), run_argc, run_argv);
}
