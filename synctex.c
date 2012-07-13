/* See LICENSE file for license and copyright information */

#include "synctex.h"

#include "zathura.h"
#include "page.h"
#include "document.h"

#include <glib.h>

void
synctex_edit(zathura_t* zathura, zathura_page_t* page, int x, int y)
{
  zathura_document_t* doc = zathura_page_get_document(page);
  const char *filename = zathura_document_get_path(doc);
  int pageIdx = zathura_page_get_index(page);

  char *buffer = g_strdup_printf("%d:%d:%d:%s", pageIdx+1, x, y, filename);
  if (buffer == NULL)
    return;

  if (zathura->synctex.editor) {
    char* argv[] = {"synctex", "edit", "-o", buffer, "-x", zathura->synctex.editor, NULL};
    g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
  } else {
    char* argv[] = {"synctex", "edit", "-o", buffer, NULL};
    g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
  }

  g_free(buffer);
}
