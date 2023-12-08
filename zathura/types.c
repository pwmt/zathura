/* SPDX-License-Identifier: Zlib */

#include <stdlib.h>
#include <girara/datastructures.h>
#include <glib.h>

#include "types.h"
#include "links.h"
#include "internal.h"
#include "checked-integer-arithmetic.h"

zathura_index_element_t*
zathura_index_element_new(const char* title)
{
  if (title == NULL) {
    return NULL;
  }

  zathura_index_element_t* res = g_try_malloc0(sizeof(zathura_index_element_t));
  if (res == NULL) {
    return NULL;
  }

  res->title = g_strdup(title);

  return res;
}

void
zathura_index_element_free(zathura_index_element_t* index)
{
  if (index == NULL) {
    return;
  }

  g_free(index->title);
  zathura_link_free(index->link);
  g_free(index);
}

zathura_image_buffer_t*
zathura_image_buffer_create(unsigned int width, unsigned int height)
{
  g_return_val_if_fail(width != 0, NULL);
  g_return_val_if_fail(height != 0, NULL);

  unsigned int size = 0;
  if (checked_umul(width, height, &size) == true || checked_umul(size, 3, &size) == true) {
    return NULL;
  }

  zathura_image_buffer_t* image_buffer = malloc(sizeof(zathura_image_buffer_t));
  if (image_buffer == NULL) {
    return NULL;
  }

  image_buffer->data = calloc(size, sizeof(unsigned char));

  if (image_buffer->data == NULL) {
    free(image_buffer);
    return NULL;
  }

  image_buffer->width     = width;
  image_buffer->height    = height;
  image_buffer->rowstride = width * 3;

  return image_buffer;
}

void
zathura_image_buffer_free(zathura_image_buffer_t* image_buffer)
{
  if (image_buffer == NULL) {
    return;
  }

  free(image_buffer->data);
  free(image_buffer);
}

static void
document_information_entry_free(void* data)
{
  zathura_document_information_entry_t* entry = data;
  zathura_document_information_entry_free(entry);
}

girara_list_t*
zathura_document_information_entry_list_new(void)
{
  return girara_list_new2(document_information_entry_free);
}

zathura_document_information_entry_t*
zathura_document_information_entry_new(zathura_document_information_type_t type, const char* value)
{
  if (value == NULL) {
    return NULL;
  }

  zathura_document_information_entry_t* entry = g_try_malloc0(sizeof(zathura_document_information_entry_t));
  if (entry == NULL) {
    return NULL;
  }

  entry->type  = type;
  entry->value = g_strdup(value);

  return entry;
}

void
zathura_document_information_entry_free(zathura_document_information_entry_t* entry)
{
  if (entry == NULL) {
    return;
  }

  g_free(entry->value);
  g_free(entry);
}

zathura_signature_info_t* zathura_signature_info_new(void) {
  return g_try_malloc0(sizeof(zathura_signature_info_t));
}

void zathura_signature_info_free(zathura_signature_info_t* signature) {
  if (signature == NULL) {
    return;
  }

  g_free(signature->signer);
  if (signature->time) {
    g_date_time_unref(signature->time);
  }
  g_free(signature);
}
