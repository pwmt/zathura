/* SPDX-License-Identifier: Zlib */

#include "jumplist.h"

#include "zathura.h"
#include "document.h"
#include "database.h"

#include <girara/utils.h>
#include <math.h>

static void zathura_jumplist_reset_current(zathura_t* zathura) {
  g_return_if_fail(zathura != NULL && zathura->jumplist.cur != NULL);

  while (girara_list_iterator_has_next(zathura->jumplist.cur) == true) {
    girara_list_iterator_next(zathura->jumplist.cur);
  }
}

static void zathura_jumplist_append_jump(zathura_t* zathura) {
  g_return_if_fail(zathura != NULL && zathura->jumplist.list != NULL);

  zathura_jump_t* jump = g_try_malloc0(sizeof(zathura_jump_t));
  if (jump == NULL) {
    return;
  }

  girara_list_append(zathura->jumplist.list, jump);

  if (zathura->jumplist.size == 0) {
    zathura->jumplist.cur = girara_list_iterator(zathura->jumplist.list);
  }

  ++zathura->jumplist.size;
  zathura_jumplist_trim(zathura);
}

static void zathura_jumplist_save(zathura_t* zathura) {
  g_return_if_fail(zathura_has_document(zathura) == true);

  zathura_jump_t* cur = zathura_jumplist_current(zathura);
  if (cur != NULL) {
    zathura_document_t* document = zathura_get_document(zathura);
    cur->x                       = zathura_document_get_position_x(document);
    cur->y                       = zathura_document_get_position_y(document);
    cur->page                    = zathura_document_get_current_page_number(document);
  }
}

bool zathura_jumplist_has_previous(zathura_t* zathura) {
  return girara_list_iterator_has_previous(zathura->jumplist.cur);
}

bool zathura_jumplist_has_next(zathura_t* zathura) {
  return girara_list_iterator_has_next(zathura->jumplist.cur);
}

zathura_jump_t* zathura_jumplist_current(zathura_t* zathura) {
  if (zathura->jumplist.cur != NULL) {
    return girara_list_iterator_data(zathura->jumplist.cur);
  } else {
    return NULL;
  }
}

void zathura_jumplist_forward(zathura_t* zathura) {
  if (girara_list_iterator_has_next(zathura->jumplist.cur)) {
    girara_list_iterator_next(zathura->jumplist.cur);
  }
}

void zathura_jumplist_backward(zathura_t* zathura) {
  if (girara_list_iterator_has_previous(zathura->jumplist.cur)) {
    girara_list_iterator_previous(zathura->jumplist.cur);
  }
}

void zathura_jumplist_trim(zathura_t* zathura) {
  g_return_if_fail(zathura != NULL && zathura->jumplist.list != NULL && zathura->jumplist.size != 0);

  girara_list_iterator_t* cur = girara_list_iterator(zathura->jumplist.list);

  while (zathura->jumplist.size > zathura->jumplist.max_size) {
    if (girara_list_iterator_data(cur) == girara_list_iterator_data(zathura->jumplist.cur)) {
      girara_list_iterator_free(zathura->jumplist.cur);
      zathura->jumplist.cur = NULL;
    }

    girara_list_iterator_remove(cur);
    --zathura->jumplist.size;
  }

  if (zathura->jumplist.size == 0 || zathura->jumplist.cur != NULL) {
    girara_list_iterator_free(cur);
  } else {
    zathura->jumplist.cur = cur;
  }
}

void zathura_jumplist_add(zathura_t* zathura) {
  g_return_if_fail(zathura_has_document(zathura) == true && zathura->jumplist.list != NULL);

  zathura_document_t* document = zathura_get_document(zathura);
  double x                     = zathura_document_get_position_x(document);
  double y                     = zathura_document_get_position_y(document);

  if (zathura->jumplist.size != 0) {
    zathura_jumplist_reset_current(zathura);

    zathura_jump_t* cur = zathura_jumplist_current(zathura);
    if (cur != NULL) {
      if (fabs(cur->x - x) <= DBL_EPSILON && fabs(cur->y - y) <= DBL_EPSILON) {
        return;
      }
    }
  }

  zathura_jumplist_append_jump(zathura);
  zathura_jumplist_reset_current(zathura);
  zathura_jumplist_save(zathura);
}

void zathura_jumplist_set_max_size(zathura_t* zathura, size_t max_size) {
  zathura->jumplist.max_size = max_size;
  if (zathura->jumplist.list != NULL && zathura->jumplist.size != 0) {
    zathura_jumplist_trim(zathura);
  }
}

bool zathura_jumplist_load(zathura_t* zathura, const char* file) {
  g_return_val_if_fail(zathura != NULL && file != NULL, false);

  if (zathura->database == NULL) {
    return false;
  }

  girara_list_t* list = zathura_db_load_jumplist(zathura->database, file);
  if (list == NULL) {
    girara_error("Failed to load the jumplist from the database");
    return false;
  }

  girara_list_free(zathura->jumplist.list);
  zathura->jumplist.list = list;
  zathura->jumplist.size = girara_list_size(zathura->jumplist.list);

  if (zathura->jumplist.size != 0) {
    zathura->jumplist.cur = girara_list_iterator(zathura->jumplist.list);
    zathura_jumplist_reset_current(zathura);
    zathura_jumplist_trim(zathura);
    girara_debug("Loaded the jumplist from the database");
  } else {
    girara_debug("No jumplist for this file in the database yet");
  }

  return true;
}

void zathura_jumplist_init(zathura_t* zathura, size_t max_size) {
  zathura->jumplist.max_size = max_size;
  zathura->jumplist.list     = girara_list_new_with_free(g_free);
  zathura->jumplist.size     = 0;
  zathura->jumplist.cur      = NULL;
}

bool zathura_jumplist_is_initalized(zathura_t* zathura) {
  return zathura->jumplist.list != NULL;
}

void zathura_jumplist_clear(zathura_t* zathura) {
  if (zathura == NULL) {
    return;
  }

  /* remove jump list */
  girara_list_iterator_free(zathura->jumplist.cur);
  zathura->jumplist.cur = NULL;
  girara_list_clear(zathura->jumplist.list);
  zathura->jumplist.size = 0;
}

void zathura_jumplist_free(zathura_t* zathura) {
  if (zathura == NULL) {
    return;
  }

  /* remove jump list */
  zathura_jumplist_clear(zathura);
  zathura->jumplist.list = NULL;
}
