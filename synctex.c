/* See LICENSE file for license and copyright information */

#include "synctex.h"

#include "zathura.h"
#include "page.h"
#include "document.h"

#include <glib.h>

void
synctex_edit(zathura_t* zathura, zathura_page_t* page, int x, int y)
{
  if (zathura == NULL || page == NULL) {
    return;
  }

  zathura_document_t* document = zathura_page_get_document(page);
  if (document == NULL) {
    return;
  }

  const char *filename = zathura_document_get_path(document);
  if (filename == NULL) {
    return;
  }

  int page_idx = zathura_page_get_index(page);
  char *buffer = g_strdup_printf("%d:%d:%d:%s", page_idx + 1, x, y, filename);

  if (zathura->synctex.editor != NULL) {
    char* argv[] = {"synctex", "edit", "-o", buffer, "-x", zathura->synctex.editor, NULL};
    g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
  } else {
    char* argv[] = {"synctex", "edit", "-o", buffer, NULL};
    g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
  }

  g_free(buffer);
}
