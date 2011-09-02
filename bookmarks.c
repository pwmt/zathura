/* See LICENSE file for license and copyright information */

#include <string.h>
#include "bookmarks.h"
#include "database.h"

zathura_bookmark_t*
zathura_bookmark_add(zathura_t* zathura, const gchar* id, unsigned int page)
{
  g_return_val_if_fail(zathura && zathura->bookmarks.bookmarks, NULL);
  g_return_val_if_fail(id, NULL);

  GIRARA_LIST_FOREACH(zathura->bookmarks.bookmarks, zathura_bookmark_t*, iter, bookmark)
    if (strcmp(bookmark->id, id) == 0) {
      girara_list_iterator_free(iter);
      return NULL;
    }
  GIRARA_LIST_FOREACH_END(zathura->bookmarks.bookmarks, zathura_bookmark_t*, iter, bookmark)

  zathura_bookmark_t* bookmark = g_malloc0(sizeof(zathura_bookmark_t));
  bookmark->id = g_strdup(id);
  bookmark->page = page;
  girara_list_append(zathura->bookmarks.bookmarks, bookmark);
  
  return bookmark;
}

void zathura_bookmark_remove(zathura_t* zathura, const gchar* id)
{
  g_return_if_fail(zathura && zathura->bookmarks.bookmarks);
  g_return_if_fail(id);

  zathura_bookmark_t* bookmark = zathura_bookmark_get(zathura, id);
  if (bookmark) {
      girara_list_remove(zathura->bookmarks.bookmarks, bookmark);
  }
}

zathura_bookmark_t* zathura_bookmark_get(zathura_t* zathura, const gchar* id)
{
  g_return_val_if_fail(zathura && zathura->bookmarks.bookmarks, NULL);
  g_return_val_if_fail(id, NULL);

  GIRARA_LIST_FOREACH(zathura->bookmarks.bookmarks, zathura_bookmark_t*, iter, bookmark)
    if (strcmp(bookmark->id, id) == 0) {
      girara_list_iterator_free(iter);
      return bookmark;
    }
  GIRARA_LIST_FOREACH_END(zathura->bookmarks.bookmarks, zathura_bookmark_t*, iter, bookmark)

  return NULL;
}

void zathura_bookmark_free(zathura_bookmark_t* bookmark)
{
  if (!bookmark) {
    return;
  }

  g_free(bookmark->id);
  g_free(bookmark);
}

bool
zathura_bookmarks_load(zathura_t* zathura, const gchar* file) {
  g_return_val_if_fail(zathura && zathura->database, false);
  g_return_val_if_fail(file, false);

  girara_list_t* bookmarks = zathura_db_load_bookmarks(zathura->database, file);
  if (!bookmarks) {
    return false;
  }

  girara_list_free(zathura->bookmarks.bookmarks);
  zathura->bookmarks.bookmarks = bookmarks;
  return true;
}


