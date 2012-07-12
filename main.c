/* See LICENSE file for license and copyright information */

#include <stdio.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <locale.h>

#include "zathura.h"

/* main function */
int main(int argc, char* argv[])
{
  setlocale(LC_ALL, "");
  bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
  textdomain(GETTEXT_PACKAGE);

  g_thread_init(NULL);
  gdk_threads_init();
  gtk_init(&argc, &argv);

  zathura_t* zathura = zathura_init(argc, argv);
  if (zathura == NULL) {
    return -1;
  }

  gdk_threads_enter();
  gtk_main();
  gdk_threads_leave();

  zathura_free(zathura);

  return 0;
}
