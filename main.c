/* See LICENSE file for license and copyright information */

#include <stdio.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <girara/utils.h>
#include <locale.h>
#include <unistd.h>

#include "zathura.h"
#include "utils.h"

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

  /* create zathura session */
  zathura_t* zathura = zathura_create();
  if (zathura == NULL) {
    return -1;
  }

  /* parse command line arguments */
  gchar* config_dir     = NULL;
  gchar* data_dir       = NULL;
  gchar* plugin_path    = NULL;
  gchar* loglevel       = NULL;
  gchar* password       = NULL;
  gchar* synctex_editor = NULL;
  bool forkback         = false;
  bool print_version    = false;
  bool synctex          = false;
  int page_number       = ZATHURA_PAGE_NUMBER_UNSPECIFIED;
  Window embed          = 0;

  GOptionEntry entries[] = {
    { "reparent",               'e', 0, G_OPTION_ARG_INT,      &embed,          _("Reparents to window specified by xid"),              "xid"  },
    { "config-dir",             'c', 0, G_OPTION_ARG_FILENAME, &config_dir,     _("Path to the config directory"),                      "path" },
    { "data-dir",               'd', 0, G_OPTION_ARG_FILENAME, &data_dir,       _("Path to the data directory"),                        "path" },
    { "plugins-dir",            'p', 0, G_OPTION_ARG_STRING,   &plugin_path,    _("Path to the directories containing plugins"),        "path" },
    { "fork",                   '\0',0, G_OPTION_ARG_NONE,     &forkback,       _("Fork into the background"),                          NULL },
    { "password",               'w', 0, G_OPTION_ARG_STRING,   &password,       _("Document password"),                                 "password" },
    { "page",                   'P', 0, G_OPTION_ARG_INT,      &page_number,    _("Page number to go to"),                              "number" },
    { "debug",                  'l', 0, G_OPTION_ARG_STRING,   &loglevel,       _("Log level (debug, info, warning, error)"),           "level" },
    { "version",                'v', 0, G_OPTION_ARG_NONE,     &print_version,  _("Print version information"),                         NULL },
    { "synctex",                's', 0, G_OPTION_ARG_NONE,     &synctex,        _("Enable synctex support"),                            NULL },
    { "synctex-editor-command", 'x', 0, G_OPTION_ARG_STRING,   &synctex_editor, _("Synctex editor (forwarded to the synctex command)"), "cmd" },
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

  /* Fork into the background if the user really wants to ... */
  if (forkback == true) {
    int pid = fork();
    if (pid > 0) { /* parent */
      exit(0);
    } else if (pid < 0) { /* error */
      girara_error("Couldn't fork.");
    }

    setsid();
  }

  /* Set log level. */
  if (loglevel == NULL || g_strcmp0(loglevel, "info") == 0) {
    girara_set_debug_level(GIRARA_INFO);
  } else if (g_strcmp0(loglevel, "warning") == 0) {
    girara_set_debug_level(GIRARA_WARNING);
  } else if (g_strcmp0(loglevel, "error") == 0) {
    girara_set_debug_level(GIRARA_ERROR);
  }

  zathura_set_xid(zathura, embed);
  zathura_set_config_dir(zathura, config_dir);
  zathura_set_data_dir(zathura, data_dir);
  zathura_set_plugin_dir(zathura, plugin_path);
  zathura_set_synctex_editor_command(zathura, synctex_editor);
  zathura_set_argv(zathura, argv);

  /* Init zathura */
  if (zathura_init(zathura) == false) {
    girara_error("Could not initialize zathura.");
    zathura_free(zathura);
    return -1;
  }

  /* Enable/Disable synctex support */
  zathura_set_synctex(zathura, synctex);

  /* Print version */
  if (print_version == true) {
    char* string = zathura_get_version_string(zathura, false);
    if (string != NULL) {
      fprintf(stdout, "%s\n", string);
    }
    zathura_free(zathura);

    return 0;
  }

  /* open document if passed */
  if (argc > 1) {
    if (page_number > 0)
      --page_number;
    document_open_idle(zathura, argv[1], password, page_number);

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
