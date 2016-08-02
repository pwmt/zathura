#include "jumplist.h"

#include "zathura.h"
#include "document.h"
#include "database.h"

#include <girara/utils.h>
#include <math.h>

static void zathura_jumplist_reset_current(zathura_t* zathura);
static void zathura_jumplist_append_jump(zathura_t* zathura);
static void zathura_jumplist_save(zathura_t* zathura);

bool
zathura_jumplist_has_previous(zathura_t* zathura)
{
  return girara_list_iterator_has_previous(zathura->jumplist.cur);
}

bool
zathura_jumplist_has_next(zathura_t* zathura)
{
  return girara_list_iterator_has_next(zathura->jumplist.cur);
}

zathura_jump_t*
zathura_jumplist_current(zathura_t* zathura)
{
  if (zathura->jumplist.cur != NULL) {
    return girara_list_iterator_data(zathura->jumplist.cur);
  } else {
    return NULL;
  }
}

void
zathura_jumplist_forward(zathura_t* zathura)
{
  if (girara_list_iterator_has_next(zathura->jumplist.cur)) {
    girara_list_iterator_next(zathura->jumplist.cur);
  }
}

void
zathura_jumplist_backward(zathura_t* zathura)
{
  if (girara_list_iterator_has_previous(zathura->jumplist.cur)) {
    girara_list_iterator_previous(zathura->jumplist.cur);
  }
}

static void
zathura_jumplist_reset_current(zathura_t* zathura)
{
  g_return_if_fail(zathura != NULL && zathura->jumplist.cur != NULL);

  while (girara_list_iterator_has_next(zathura->jumplist.cur) == true) {
    girara_list_iterator_next(zathura->jumplist.cur);
  }
}

static void
zathura_jumplist_append_jump(zathura_t* zathura)
{
  g_return_if_fail(zathura != NULL && zathura->jumplist.list != NULL);

  zathura_jump_t* jump = g_try_malloc0(sizeof(zathura_jump_t));
  if (jump == NULL) {
    return;
  }

  jump->page = 0;
  jump->x    = 0.0;
  jump->y    = 0.0;
  girara_list_append(zathura->jumplist.list, jump);

  if (zathura->jumplist.size == 0) {
    zathura->jumplist.cur = girara_list_iterator(zathura->jumplist.list);
  }

  ++zathura->jumplist.size;
  zathura_jumplist_trim(zathura);
}

void
zathura_jumplist_trim(zathura_t* zathura)
{
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

void
zathura_jumplist_add(zathura_t* zathura)
{
  g_return_if_fail(zathura != NULL && zathura->document != NULL && zathura->jumplist.list != NULL);

  unsigned int pagenum = zathura_document_get_current_page_number(zathura->document);
  double x = zathura_document_get_position_x(zathura->document);
  double y = zathura_document_get_position_y(zathura->document);

  if (zathura->jumplist.size != 0) {
    zathura_jumplist_reset_current(zathura);

    zathura_jump_t* cur = zathura_jumplist_current(zathura);

    if (cur != NULL) {
      if (cur->page == pagenum && fabs(cur->x - x) <= DBL_EPSILON && fabs(cur->y - y) <= DBL_EPSILON) {
        return;
      }
    }
  }

  zathura_jumplist_append_jump(zathura);
  zathura_jumplist_reset_current(zathura);
  zathura_jumplist_save(zathura);
}

bool
zathura_jumplist_load(zathura_t* zathura, const char* file)
{
  g_return_val_if_fail(zathura != NULL && file != NULL, false);

  if (zathura->database == NULL) {
    return false;
  }

  zathura->jumplist.list = zathura_db_load_jumplist(zathura->database, file);

  if (zathura->jumplist.list == NULL) {
    girara_error("Failed to load the jumplist from the database");

    return false;
  }

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

static void
zathura_jumplist_save(zathura_t* zathura)
{
  g_return_if_fail(zathura != NULL && zathura->document != NULL);

  zathura_jump_t* cur = zathura_jumplist_current(zathura);

  unsigned int pagenum = zathura_document_get_current_page_number(zathura->document);

  if (cur != NULL) {
    cur->page = pagenum;
    cur->x = zathura_document_get_position_x(zathura->document);
    cur->y = zathura_document_get_position_y(zathura->document);
  }
}
