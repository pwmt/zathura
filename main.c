/* See LICENSE file for license and copyright information */

#include <stdio.h>
#include <glib/gstdio.h>

#include "zathura.h"

/* main function */
int main(int argc, char* argv[])
{
  g_thread_init(NULL);
  gdk_threads_init();
  gtk_init(&argc, &argv);

  zathura_t* zathura = zathura_init(argc, argv);
  if (zathura == NULL) {
    fprintf(stderr, "error: could not initialize zathura\n");
    return -1;
  }

  gdk_threads_enter();
  gtk_main();
  gdk_threads_leave();

  zathura_free(zathura);

  return 0;
}

