/* See LICENSE file for license and copyright information */

#ifndef BOOKMARKS_H
#define BOOKMARKS_H

#include <stdbool.h>
#include "zathura.h"

struct zathura_bookmark_s
{
  gchar* id;
  unsigned int page;
};

typedef struct zathura_bookmark_s zathura_bookmark_t;

zathura_bookmark_t* zathura_bookmark_add(zathura_t* zathura, const gchar* id, unsigned int page);

void zathura_bookmark_remove(zathura_t* zathura, const gchar* id);

zathura_bookmark_t* zathura_bookmark_get(zathura_t* zathura, const gchar* id);

void zathura_bookmark_free(zathura_bookmark_t* bookmark);

bool zathura_bookmarks_load(zathura_t* zathura, const gchar* file);

bool zathura_bookmarks_save(zathura_t* zathura, const gchar* file);

#endif // BOOKMARKS_H
