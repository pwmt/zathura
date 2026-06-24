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
/* SyncTeX needs D-Bus, which the sandbox disables, so it is unavailable there. */
#if defined(WITH_SYNCTEX) && !defined(WITH_SANDBOX)
#include "dbus-interface.h"
#include "synctex.h"
#endif

#if defined(WITH_SYNCTEX) && !defined(WITH_SANDBOX)
/* Handle synctex forward synchronization */
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

  int line                    = 0;
  int column                  = 0;
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
                               const char* plugin_path, char** argv, const char* synctex_editor) {
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

  if (synctex_editor != NULL) {
    girara_setting_set(zathura->ui.session, "synctex-editor-command", synctex_editor);
  }

#ifdef WITH_SANDBOX
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
#endif

  return zathura;
}

/* state shared between the application lifecycle callbacks */
typedef struct {
  const char* config_dir;
  const char* data_dir;
  const char* cache_dir;
  const char* plugin_path;
  const char* synctex_editor;
  const char* password;
  const char* synctex_fwd;
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

  ctx->zathura =
      init_zathura(ctx->config_dir, ctx->data_dir, ctx->cache_dir, ctx->plugin_path, ctx->argv, ctx->synctex_editor);
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
  document_open_idle(ctx->zathura, path, ctx->password, page_number, ctx->mode, ctx->synctex_fwd, ctx->bookmark_name,
                     ctx->search_string);
}

static void cb_app_shutdown(GApplication* UNUSED(app), gpointer data) {
  zathura_app_ctx_t* ctx = data;
  if (ctx->zathura != NULL) {
    zathura_free(ctx->zathura);
    ctx->zathura = NULL;
  }
}

static GStrv build_argv_for_child(int idx, char** argv, int argc, char** orig_argv, int orig_argc, int file_idx_base) {
  GPtrArray* arr = g_ptr_array_new();
  g_ptr_array_add(arr, g_strdup(orig_argv[0]));
  for (int i = 1; i < orig_argc; i++) {
    if (g_strcmp0(orig_argv[i], "--fork") == 0) {
      continue;
    }
    bool is_file = false;
    for (int j = 0; j < argc - file_idx_base && !is_file; j++) {
      if (g_strcmp0(orig_argv[i], argv[file_idx_base + j]) == 0) {
        is_file = true;
      }
    }
    if (!is_file) {
      g_ptr_array_add(arr, g_strdup(orig_argv[i]));
    }
  }
  if (idx > 0) {
    g_ptr_array_add(arr, g_strdup(argv[idx]));
  }
  g_ptr_array_add(arr, NULL);
  return (GStrv)g_ptr_array_free(arr, FALSE);
}

static void start_process_group(void* GIRARA_UNUSED(data)) {
  if (setsid() == -1) {
    girara_error("Could not start new process group: %s", strerror(errno));
    exit(-1);
  }
}

/* main function */
GIRARA_VISIBLE int main(int argc, char* argv[]) {
  zathura_init_locale();

#ifdef WITH_SANDBOX
  girara_debug("Running zathura-sandbox, disable IPC services");
  /* Prevent default gtk dbus connection */
  g_setenv("DBUS_SESSION_BUS_ADDRESS", "disabled:", TRUE);

  /* disable dconf writing - uses /var/empty as alternative to /dev/null to avoid ioctl call */
  g_setenv("DCONF_PROFILE", "/var/empty", TRUE);

  /* use the software renderer to avoid the gpu stack attack surface (and high risk syscalls) */
  g_setenv("GSK_RENDERER", "cairo", TRUE);

  /* unset the input method to avoid communication with external services */
  unsetenv("GTK_IM_MODULE");
#endif

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

  const GOptionEntry entries[] = {
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
       _("SyncTeX editor (forwarded to the synctex command)"), "cmd"},
      {"synctex-forward", '\0', 0, G_OPTION_ARG_STRING, &synctex_fwd, _("Move to given SyncTeX position"), "position"},
      {"synctex-pid", '\0', 0, G_OPTION_ARG_INT, &synctex_pid, _("Highlight given position in the given process"),
       "pid"},
      {"mode", '\0', 0, G_OPTION_ARG_STRING, &mode, _("Start in a non-default mode"), "mode"},
      {"bookmark", 'b', 0, G_OPTION_ARG_STRING, &bookmark_name, _("Bookmark to go to"), "bookmark"},
      {"find", 'f', 0, G_OPTION_ARG_STRING, &search_string, _("Search for the given phrase and display results"),
       "string"},
      {NULL, '\0', 0, 0, NULL, NULL, NULL},
  };

  g_autoptr(GOptionContext) context = g_option_context_new(" [file1] [file2] [...]");
  g_option_context_add_main_entries(context, entries, NULL);

  const int orig_argc     = argc;
  g_auto(GStrv) orig_argv = g_strdupv(argv);

  g_autoptr(GError) error = NULL;
  if (g_option_context_parse(context, &argc, &argv, &error) == false) {
    girara_error("Error parsing command line arguments: %s\n", error->message);
    return -1;
  }

  zathura_set_log_level(loglevel);

#if defined(WITH_SYNCTEX) && !defined(WITH_SANDBOX)
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
  /* If more than one file, fork an instance for each. */
  if (print_version == false && argc > file_idx_base + 1) {
    g_autoptr(girara_list_t) child_pids = girara_list_new();

    for (int idx = file_idx_base; idx < argc; ++idx) {
      g_auto(GStrv) spawn_argv = build_argv_for_child(idx, argv, argc, orig_argv, orig_argc, file_idx_base);
      GPid pid;
      if (!g_spawn_async(NULL, spawn_argv, NULL,
                         G_SPAWN_SEARCH_PATH | (forkback ? G_SPAWN_DEFAULT : G_SPAWN_DO_NOT_REAP_CHILD),
                         forkback ? start_process_group : NULL, NULL, &pid, &error)) {
        girara_error("Could not spawn: %s", error->message);
        return -1;
      }

      girara_list_append(child_pids, (void*)(intptr_t)pid);
    }

    if (forkback == false) {
      for (size_t idx = 0; idx != girara_list_size(child_pids); ++idx) {
        const pid_t pid = (pid_t)(intptr_t)girara_list_nth(child_pids, idx);
        waitpid(pid, NULL, 0);
      }
    }
    return 0;
  }

  /* Fork into the background if the user really wants to ... */
  if (print_version == false && forkback == true && file_idx < file_idx_base + 1) {
    g_auto(GStrv) spawn_argv = build_argv_for_child(file_idx, argv, argc, orig_argv, orig_argc, file_idx_base);
    if (!g_spawn_async(NULL, spawn_argv, NULL, G_SPAWN_DEFAULT | G_SPAWN_SEARCH_PATH, start_process_group, NULL, NULL,
                       &error)) {
      girara_error("Could not spawn: %s", error->message);
      return -1;
    }

    return 0;
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
      .config_dir     = config_dir,
      .data_dir       = data_dir,
      .cache_dir      = cache_dir,
      .plugin_path    = plugin_path,
      .synctex_editor = synctex_editor,
      .password       = password,
      .synctex_fwd    = synctex_fwd,
      .mode           = mode,
      .bookmark_name  = bookmark_name,
      .search_string  = search_string,
      .raw_file       = NULL,
      .page_number    = page_number,
      .argv           = argv,
      .zathura        = NULL,
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
