/* See LICENSE file for license and copyright information */

#include <glib.h>
#include <glib/gi18n.h>
#include <girara/utils.h>
#include <girara/session.h>
#include <girara/settings.h>

#include "adjustment.h"
#include "links.h"
#include "zathura.h"
#include "document.h"
#include "utils.h"
#include "page.h"

struct zathura_link_s {
  zathura_rectangle_t position; /**< Position of the link */
  zathura_link_type_t type; /**< Link type */
  zathura_link_target_t target; /**< Link target */
};

/* forward declarations */
static void link_remote(zathura_t* zathura, const char* file);
static void link_launch(zathura_t* zathura, zathura_link_t* link);

zathura_link_t*
zathura_link_new(zathura_link_type_t type, zathura_rectangle_t position,
                 zathura_link_target_t target)
{
  zathura_link_t* link = g_malloc0(sizeof(zathura_link_t));

  link->type     = type;
  link->position = position;

  switch (type) {
    case ZATHURA_LINK_NONE:
    case ZATHURA_LINK_GOTO_DEST:
      link->target = target;

      if (target.value != NULL) {
        link->target.value = g_strdup(target.value);
      }
      break;
    case ZATHURA_LINK_GOTO_REMOTE:
    case ZATHURA_LINK_URI:
    case ZATHURA_LINK_LAUNCH:
    case ZATHURA_LINK_NAMED:
      if (target.value == NULL) {
        g_free(link);
        return NULL;
      }

      link->target.value = g_strdup(target.value);
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

  switch (link->type) {
    case ZATHURA_LINK_NONE:
    case ZATHURA_LINK_GOTO_DEST:
    case ZATHURA_LINK_GOTO_REMOTE:
    case ZATHURA_LINK_URI:
    case ZATHURA_LINK_LAUNCH:
    case ZATHURA_LINK_NAMED:
      if (link->target.value != NULL) {
        g_free(link->target.value);
      }
      break;
    default:
      break;
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
    zathura_link_target_t target = { 0, NULL, 0, 0, 0, 0, 0, 0 };
    return target;
  }

  return link->target;
}

void
zathura_link_evaluate(zathura_t* zathura, zathura_link_t* link)
{
  if (zathura == NULL || zathura->document == NULL || link == NULL) {
    return;
  }

  bool link_zoom = true;
  girara_setting_get(zathura->ui.session, "link-zoom", &link_zoom);

  switch (link->type) {
    case ZATHURA_LINK_GOTO_DEST:
      if (link->target.destination_type != ZATHURA_LINK_DESTINATION_UNKNOWN) {
        if (link->target.scale != 0 && link_zoom) {
          zathura_document_set_scale(zathura->document, link->target.scale);
          render_all(zathura);
        }

        /* get page */
        zathura_page_t* page = zathura_document_get_page(zathura->document,
            link->target.page_number);
        if (page == NULL) {
          return;
        }

        /* compute the position with the page aligned to the top and left
           of the viewport */
        double pos_x = 0;
        double pos_y = 0;
        page_number_to_position(zathura->document, link->target.page_number,
                                0.0, 0.0, &pos_x, &pos_y);

        /* correct to place the target position at the top of the viewport     */
        /* NOTE: link->target is in page units, needs to be scaled and rotated */
        unsigned int cell_height = 0;
        unsigned int cell_width = 0;
        zathura_document_get_cell_size(zathura->document, &cell_height, &cell_width);

        unsigned int doc_height = 0;
        unsigned int doc_width = 0;
        zathura_document_get_document_size(zathura->document, &doc_height, &doc_width);

        bool link_hadjust = true;
        girara_setting_get(zathura->ui.session, "link-hadjust", &link_hadjust);

        /* scale and rotate */
        double scale = zathura_document_get_scale(zathura->document);
        double shiftx = link->target.left * scale / (double)cell_width;
        double shifty = link->target.top * scale / (double)cell_height;
        page_calc_position(zathura->document, shiftx, shifty, &shiftx, &shifty);

        /* shift the position or set to auto */
        if (link->target.destination_type == ZATHURA_LINK_DESTINATION_XYZ &&
            link->target.left != -1 && link_hadjust == true) {
          pos_x += shiftx / (double)doc_width;
        } else {
          pos_x = -1;     /* -1 means automatic */
        }

        if (link->target.destination_type == ZATHURA_LINK_DESTINATION_XYZ &&
            link->target.top != -1) {
          pos_y += shifty / (double)doc_height;
        } else {
          pos_y = -1;     /* -1 means automatic */
        }

        /* move to position */
        zathura_jumplist_add(zathura);
        zathura_document_set_current_page_number(zathura->document, link->target.page_number);
        position_set(zathura, pos_x, pos_y);
        zathura_jumplist_add(zathura);
      }
      break;
    case ZATHURA_LINK_GOTO_REMOTE:
      link_remote(zathura, link->target.value);
      break;
    case ZATHURA_LINK_URI:
      if (girara_xdg_open(link->target.value) == false) {
        girara_notify(zathura->ui.session, GIRARA_ERROR, _("Failed to run xdg-open."));
      }
      break;
    case ZATHURA_LINK_LAUNCH:
      link_launch(zathura, link);
      break;
    default:
      break;
  }
}

void
zathura_link_display(zathura_t* zathura, zathura_link_t* link)
{
  zathura_link_type_t type = zathura_link_get_type(link);
  zathura_link_target_t target = zathura_link_get_target(link);
  switch (type) {
    case ZATHURA_LINK_GOTO_DEST:
      girara_notify(zathura->ui.session, GIRARA_INFO, _("Link: page %d"),
          target.page_number);
      break;
    case ZATHURA_LINK_GOTO_REMOTE:
    case ZATHURA_LINK_URI:
    case ZATHURA_LINK_LAUNCH:
    case ZATHURA_LINK_NAMED:
      girara_notify(zathura->ui.session, GIRARA_INFO, _("Link: %s"),
          target.value);
      break;
    default:
      girara_notify(zathura->ui.session, GIRARA_ERROR, _("Link: Invalid"));
  }
}

static void
link_remote(zathura_t* zathura, const char* file)
{
  if (zathura == NULL || file == NULL || zathura->document == NULL) {
    return;
  }

  const char* path = zathura_document_get_path(zathura->document);
  char* dir        = g_path_get_dirname(path);
  char* uri        = g_build_filename(dir, file, NULL);

  char* argv[] = {
    *(zathura->global.arguments),
    uri,
    NULL
  };

  g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);

  g_free(uri);
  g_free(dir);
}

static void
link_launch(zathura_t* zathura, zathura_link_t* link)
{
  if (zathura == NULL || link == NULL || zathura->document == NULL) {
    return;
  }

  /* get file path */
  if (link->target.value == NULL) {
    return;
  };

  char* path = NULL;
  if (g_path_is_absolute(link->target.value) == TRUE) {
    path = g_strdup(link->target.value);
  } else {
    const char* document = zathura_document_get_path(zathura->document);
    char* dir  = g_path_get_dirname(document);
    path = g_build_filename(dir, link->target.value, NULL);
    g_free(dir);
  }

  if (girara_xdg_open(path) == false) {
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("Failed to run xdg-open."));
  }

  g_free(path);
}
