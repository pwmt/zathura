/* SPDX-License-Identifier: Zlib */

#include "bookmarks.h"

#include <girara-gtk/session.h>
#include <girara/datastructures.h>
#include <girara/log.h>
#include <girara/utils.h>
#include <string.h>

#include "database.h"
#include "document.h"
#include "adjustment.h"

static int bookmark_compare_find(const void* item, const void* data) {
  const zathura_bookmark_t* bookmark = item;
  const char* id                     = data;

  return g_strcmp0(bookmark->id, id);
}

zathura_bookmark_t* zathura_bookmark_add(zathura_t* zathura, const gchar* id, unsigned int page) {
  g_return_val_if_fail(zathura_has_document(zathura) == true && zathura->bookmarks.bookmarks, NULL);
  g_return_val_if_fail(id, NULL);

  zathura_document_t* document = zathura_get_document(zathura);
  double position_x            = zathura_document_get_position_x(document);
  double position_y            = zathura_document_get_position_y(document);
  zathura_bookmark_t* old      = zathura_bookmark_get(zathura, id);

  if (old != NULL) {
    old->page = page;
    old->x    = position_x;
    old->y    = position_y;

    const char* path = zathura_document_get_path(document);
    if (zathura_db_remove_bookmark(zathura->database, path, old->id) == false) {
      girara_warning("Failed to remove old bookmark from database.");
    }

    if (zathura_db_add_bookmark(zathura->database, path, old) == false) {
      girara_warning("Failed to add new bookmark to database.");
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

  const char* path = zathura_document_get_path(document);
  if (zathura_db_add_bookmark(zathura->database, path, bookmark) == false) {
    girara_warning("Failed to add bookmark to database.");
  }

  return bookmark;
}

bool zathura_bookmark_remove(zathura_t* zathura, const gchar* id) {
  g_return_val_if_fail(zathura_has_document(zathura) == true && zathura->bookmarks.bookmarks, false);
  g_return_val_if_fail(id, false);

  zathura_bookmark_t* bookmark = zathura_bookmark_get(zathura, id);
  if (bookmark == NULL) {
    return false;
  }

  const char* path = zathura_document_get_path(zathura_get_document(zathura));
  if (zathura_db_remove_bookmark(zathura->database, path, bookmark->id) == false) {
    girara_warning("Failed to remove bookmark from database.");
  }

  girara_list_remove(zathura->bookmarks.bookmarks, bookmark);

  return true;
}

zathura_bookmark_t* zathura_bookmark_get(zathura_t* zathura, const gchar* id) {
  g_return_val_if_fail(zathura && zathura->bookmarks.bookmarks, NULL);
  g_return_val_if_fail(id, NULL);

  return girara_list_find(zathura->bookmarks.bookmarks, bookmark_compare_find, id);
}

static int zathura_bookmarks_compare(const void* lhs, const void* rhs) {
  if (lhs && rhs) {
    const zathura_bookmark_t* l = lhs;
    const zathura_bookmark_t* r = rhs;

    return g_strcmp0(l->id, r->id);
  }

  return memcmp(&lhs, &rhs, sizeof(lhs));
}

static void zathura_bookmark_free(void* data) {
  if (data != NULL) {
    zathura_bookmark_t* bookmark = data;
    g_free(bookmark->id);
    g_free(bookmark);
  }
}

bool zathura_bookmarks_init(zathura_t* zathura) {
  if (!zathura) {
    return false;
  }

  zathura->bookmarks.bookmarks = girara_sorted_list_new_with_free(zathura_bookmarks_compare, zathura_bookmark_free);

  return zathura->bookmarks.bookmarks != NULL;
}

bool zathura_bookmarks_load(zathura_t* zathura, const gchar* file) {
  g_return_val_if_fail(zathura && zathura->database, false);
  g_return_val_if_fail(file, false);

  girara_list_clear(zathura->bookmarks.bookmarks);
  return zathura_db_load_bookmarks(zathura->database, file, zathura->bookmarks.bookmarks);
}

void zathura_bookmarks_free(zathura_t* zathura) {
  if (zathura) {
    girara_list_free(zathura->bookmarks.bookmarks);
  }
}
