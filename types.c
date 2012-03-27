/* See LICENSE file for license and copyright information */

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
