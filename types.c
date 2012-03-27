/* See LICENSE file for license and copyright information */

#include <stdlib.h>

#include "types.h"

zathura_link_t*
zathura_link_new(zathura_link_type_t type, zathura_rectangle_t position, 
    zathura_link_target_t target)
{
  zathura_link_t* link = g_malloc0(sizeof(zathura_link_t));
  
  link->type     = type;
  link->position = position;

  switch (type) {
    case ZATHURA_LINK_TO_PAGE:
      link->target.page_number = target.page_number;
      break;
    case ZATHURA_LINK_EXTERNAL:
      if (target.uri == NULL) {
        g_free(link);
        return NULL;
      }

      link->target.uri = g_strdup(target.uri);
      break;
    default:
      g_free(link);
      return NULL;
  }

  return link;
}

void
zathura_link_free(zathura_link_t* link)
{
  if (link == NULL) {
    return;
  }

  if (link->type == ZATHURA_LINK_EXTERNAL) {
    if (link->target.uri != NULL) {
      g_free(link->target.uri);
    }
  }

  g_free(link);
}

zathura_link_type_t
zathura_link_get_type(zathura_link_t* link)
{
  if (link == NULL) {
    return ZATHURA_LINK_INVALID;
  }

  return link->type;
}

zathura_rectangle_t
zathura_link_get_position(zathura_link_t* link)
{
  if (link == NULL) {
    zathura_rectangle_t position = { 0, 0, 0, 0 };
    return position;
  }

  return link->position;
}

zathura_link_target_t
zathura_link_get_target(zathura_link_t* link)
{
  if (link == NULL) {
    zathura_link_target_t target = { 0 };
    return target;
  }

  return link->target;
}

zathura_index_element_t*
zathura_index_element_new(const char* title)
{
  if (title == NULL) {
    return NULL;
  }

  zathura_index_element_t* res = g_malloc0(sizeof(zathura_index_element_t));

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

  if (index->type == ZATHURA_LINK_EXTERNAL) {
    g_free(index->target.uri);
  }

  g_free(index);
}

zathura_image_buffer_t*
zathura_image_buffer_create(unsigned int width, unsigned int height)
{
  zathura_image_buffer_t* image_buffer = malloc(sizeof(zathura_image_buffer_t));

  if (image_buffer == NULL) {
    return NULL;
  }

  image_buffer->data = calloc(width * height * 3, sizeof(unsigned char));

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

