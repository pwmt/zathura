/* SPDX-License-Identifier: Zlib */

#include <string.h>
#include "bookmarks.h"
#include "database.h"
#include "document.h"
#include "adjustment.h"

#include <girara/datastructures.h>
#include <girara/utils.h>
#include <girara/session.h>

static int bookmark_compare_find(const void* item, const void* data) {
  const zathura_bookmark_t* bookmark = item;
  const char* id                     = data;

  return g_strcmp0(bookmark->id, id);
}

zathura_bookmark_t* zathura_bookmark_add(zathura_t* zathura, const gchar* id, unsigned int page) {
  g_return_val_if_fail(zathura && zathura->document && zathura->bookmarks.bookmarks, NULL);
  g_return_val_if_fail(id, NULL);

  double position_x       = zathura_document_get_position_x(zathura->document);
  double position_y       = zathura_document_get_position_y(zathura->document);
  zathura_bookmark_t* old = zathura_bookmark_get(zathura, id);

  if (old != NULL) {
    old->page = page;
    old->x    = position_x;
    old->y    = position_y;

    if (zathura->database != NULL) {
      const char* path = zathura_document_get_path(zathura->document);
      if (zathura_db_remove_bookmark(zathura->database, path, old->id) == false) {
        girara_warning("Failed to remove old bookmark from database.");
      }

      if (zathura_db_add_bookmark(zathura->database, path, old) == false) {
        girara_warning("Failed to add new bookmark to database.");
      }
    }

    return old;
  }

  zathura_bookmark_t* bookmark = g_try_malloc0(sizeof(zathura_bookmark_t));
  if (bookmark == NULL) {
    return NULL;
  }

  bookmark->id   = g_strdup(id);
  bookmark->page = page;
  bookmark->x    = position_x;
  bookmark->y    = position_y;
  girara_list_append(zathura->bookmarks.bookmarks, bookmark);

  if (zathura->database != NULL) {
    const char* path = zathura_document_get_path(zathura->document);
    if (zathura_db_add_bookmark(zathura->database, path, bookmark) == false) {
      girara_warning("Failed to add bookmark to database.");
    }
  }

  return bookmark;
}

bool zathura_bookmark_remove(zathura_t* zathura, const gchar* id) {
  g_return_val_if_fail(zathura && zathura->document && zathura->bookmarks.bookmarks, false);
  g_return_val_if_fail(id, false);

  zathura_bookmark_t* bookmark = zathura_bookmark_get(zathura, id);
  if (bookmark == NULL) {
    return false;
  }

  if (zathura->database != NULL) {
    const char* path = zathura_document_get_path(zathura->document);
    if (zathura_db_remove_bookmark(zathura->database, path, bookmark->id) == false) {
      girara_warning("Failed to remove bookmark from database.");
    }
  }

  girara_list_remove(zathura->bookmarks.bookmarks, bookmark);

  return true;
}

zathura_bookmark_t* zathura_bookmark_get(zathura_t* zathura, const gchar* id) {
  g_return_val_if_fail(zathura && zathura->bookmarks.bookmarks, NULL);
  g_return_val_if_fail(id, NULL);

  return girara_list_find(zathura->bookmarks.bookmarks, bookmark_compare_find, id);
}

void zathura_bookmark_free(zathura_bookmark_t* bookmark) {
  if (bookmark != NULL) {
    g_free(bookmark->id);
    g_free(bookmark);
  }
}

bool zathura_bookmarks_load(zathura_t* zathura, const gchar* file) {
  g_return_val_if_fail(zathura, false);
  g_return_val_if_fail(file, false);

  if (zathura->database == NULL) {
    return false;
  }

  girara_list_t* bookmarks = zathura_db_load_bookmarks(zathura->database, file);
  if (bookmarks == NULL) {
    return false;
  }

  girara_list_free(zathura->bookmarks.bookmarks);
  zathura->bookmarks.bookmarks = bookmarks;

  return true;
}

int zathura_bookmarks_compare(const zathura_bookmark_t* lhs, const zathura_bookmark_t* rhs) {
  if (lhs == NULL && rhs == NULL) {
    return 0;
  }

  if (lhs == NULL) {
    return -1;
  }

  if (rhs == NULL) {
    return 1;
  }

  return g_strcmp0(lhs->id, rhs->id);
}
