/* SPDX-License-Identifier: Zlib */

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
#include "render.h"

struct zathura_link_s {
  zathura_rectangle_t position; /**< Position of the link */
  zathura_link_target_t target; /**< Link target */
  zathura_link_type_t type;     /**< Link type */
};

zathura_link_t* zathura_link_new(zathura_link_type_t type, zathura_rectangle_t position, zathura_link_target_t target) {
  zathura_link_t* link = g_try_malloc0(sizeof(zathura_link_t));
  if (link == NULL) {
    return NULL;
  }

  link->position = position;
  link->target   = target;
  link->type     = type;

  /* duplicate target.value if necessary */
  switch (type) {
  case ZATHURA_LINK_NONE:
  case ZATHURA_LINK_GOTO_DEST:
    if (target.value != NULL) {
      link->target.value = g_strdup(target.value);
    }
    break;
  case ZATHURA_LINK_GOTO_REMOTE:
  case ZATHURA_LINK_URI:
  case ZATHURA_LINK_LAUNCH:
  case ZATHURA_LINK_NAMED:
    /* target.value is required for these cases */
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

void zathura_link_free(zathura_link_t* link) {
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

zathura_link_type_t zathura_link_get_type(zathura_link_t* link) {
  if (link == NULL) {
    return ZATHURA_LINK_INVALID;
  }

  return link->type;
}

zathura_rectangle_t zathura_link_get_position(zathura_link_t* link) {
  if (link == NULL) {
    const zathura_rectangle_t position = {0, 0, 0, 0};
    return position;
  }

  return link->position;
}

zathura_link_target_t zathura_link_get_target(zathura_link_t* link) {
  if (link == NULL) {
    const zathura_link_target_t target = {0, NULL, 0, 0, 0, 0, 0, 0};
    return target;
  }

  return link->target;
}

static void link_goto_dest(zathura_t* zathura, const zathura_link_t* link) {
  if (link->target.destination_type == ZATHURA_LINK_DESTINATION_UNKNOWN) {
    girara_warning("link destination type unknown");
    return;
  }

  bool link_zoom = true;
  girara_setting_get(zathura->ui.session, "link-zoom", &link_zoom);

  if (link->target.zoom >= DBL_EPSILON && link_zoom == true) {
    zathura_document_set_zoom(zathura->document, zathura_correct_zoom_value(zathura->ui.session, link->target.zoom));
    render_all(zathura);
  }

  /* get page */
  zathura_page_t* page = zathura_document_get_page(zathura->document, link->target.page_number);
  if (page == NULL) {
    girara_warning("link to non-existing page %u", link->target.page_number);
    return;
  }

  /* compute the position with the page aligned to the top and left
     of the viewport */
  double pos_x = 0;
  double pos_y = 0;
  page_number_to_position(zathura->document, link->target.page_number, 0.0, 0.0, &pos_x, &pos_y);

  /* correct to place the target position at the top of the viewport     */
  /* NOTE: link->target is in page units, needs to be scaled and rotated */
  unsigned int cell_height = 0;
  unsigned int cell_width  = 0;
  zathura_document_get_cell_size(zathura->document, &cell_height, &cell_width);

  unsigned int doc_height = 0;
  unsigned int doc_width  = 0;
  zathura_document_get_document_size(zathura->document, &doc_height, &doc_width);

  bool link_hadjust = true;
  girara_setting_get(zathura->ui.session, "link-hadjust", &link_hadjust);

  /* scale and rotate */
  const double scale = zathura_document_get_scale(zathura->document);
  double shiftx      = link->target.left * scale / cell_width;
  double shifty      = link->target.top * scale / cell_height;
  page_calc_position(zathura->document, shiftx, shifty, &shiftx, &shifty);

  /* shift the position or set to auto */
  if (link->target.destination_type == ZATHURA_LINK_DESTINATION_XYZ && link->target.left != -1 &&
      link_hadjust == true) {
    pos_x += shiftx * cell_width / doc_width;
  } else {
    pos_x = -1; /* -1 means automatic */
  }

  if (link->target.destination_type == ZATHURA_LINK_DESTINATION_XYZ && link->target.top != -1) {
    pos_y += shifty * cell_height / doc_height;
  } else {
    pos_y = -1; /* -1 means automatic */
  }

  /* move to position */
  zathura_jumplist_add(zathura);
  zathura_document_set_current_page_number(zathura->document, link->target.page_number);
  position_set(zathura, pos_x, pos_y);
  zathura_jumplist_add(zathura);
}

#if WITH_SANDBOX
static void link_remote(zathura_t* zathura, const char* file) {
  if (zathura == NULL || file == NULL || zathura->document == NULL) {
    return;
  }

  const char* path = zathura_document_get_path(zathura->document);
  char* dir        = g_path_get_dirname(path);
  char* uri        = g_build_filename(file, NULL);

  char* argv[] = {*zathura->global.arguments, uri, NULL};

  GError* error = NULL;
  if (g_spawn_async(dir, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error) == FALSE) {
    girara_error("Failed to execute command: %s", error->message);
    g_error_free(error);
  }

  g_free(uri);
  g_free(dir);
}

static void link_launch(zathura_t* zathura, const zathura_link_t* link) {
  /* get file path */
  if (link->target.value == NULL) {
    return;
  }

  const char* document = zathura_document_get_path(zathura->document);
  char* dir            = g_path_get_dirname(document);

  if (girara_xdg_open_with_working_directory(link->target.value, dir) == false) {
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("Failed to run xdg-open."));
  }

  g_free(dir);
}
#endif

void zathura_link_evaluate(zathura_t* zathura, zathura_link_t* link) {
  if (zathura == NULL || zathura->document == NULL || link == NULL) {
    return;
  }

#if WITH_SANDBOX
  if (link->type != ZATHURA_LINK_GOTO_DEST) {
    girara_notify(zathura->ui.session, GIRARA_ERROR,
                  _("Opening external applications in strict sandbox mode is not permitted"));
    return;
  }
#endif

  switch (link->type) {

  case ZATHURA_LINK_GOTO_DEST:
    girara_debug("Going to link destination: page: %d", link->target.page_number);
    link_goto_dest(zathura, link);
    break;
#if WITH_SANDBOX
  case ZATHURA_LINK_GOTO_REMOTE:
    girara_debug("Going to remote destination: %s", link->target.value);
    link_remote(zathura, link->target.value);
    break;
  case ZATHURA_LINK_URI:
    girara_debug("Opening URI: %s", link->target.value);
    link_launch(zathura, link);
    break;
  case ZATHURA_LINK_LAUNCH:
    girara_debug("Launching link: %s", link->target.value);
    link_launch(zathura, link);
    break;
#endif
  default:
    girara_error("Unhandled link type: %d", link->type);
    break;
  }
}

void zathura_link_display(zathura_t* zathura, zathura_link_t* link) {
  zathura_link_type_t type     = zathura_link_get_type(link);
  zathura_link_target_t target = zathura_link_get_target(link);
  switch (type) {
  case ZATHURA_LINK_GOTO_DEST:
    girara_notify(zathura->ui.session, GIRARA_INFO, _("Link: page %d"), target.page_number);
    break;
  case ZATHURA_LINK_GOTO_REMOTE:
  case ZATHURA_LINK_URI:
  case ZATHURA_LINK_LAUNCH:
  case ZATHURA_LINK_NAMED:
    girara_notify(zathura->ui.session, GIRARA_INFO, _("Link: %s"), target.value);
    break;
  default:
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("Link: Invalid"));
  }
}

void zathura_link_copy(zathura_t* zathura, zathura_link_t* link, GdkAtom* selection) {
  zathura_link_type_t type     = zathura_link_get_type(link);
  zathura_link_target_t target = zathura_link_get_target(link);
  switch (type) {
  case ZATHURA_LINK_GOTO_DEST: {
    gchar* tmp = g_strdup_printf("%d", target.page_number);
    gtk_clipboard_set_text(gtk_clipboard_get(*selection), tmp, -1);
    g_free(tmp);
    girara_notify(zathura->ui.session, GIRARA_INFO, _("Copied page number: %d"), target.page_number);
    break;
  }
  case ZATHURA_LINK_GOTO_REMOTE:
  case ZATHURA_LINK_URI:
  case ZATHURA_LINK_LAUNCH:
  case ZATHURA_LINK_NAMED:
    gtk_clipboard_set_text(gtk_clipboard_get(*selection), target.value, -1);
    girara_notify(zathura->ui.session, GIRARA_INFO, _("Copied link: %s"), target.value);
    break;
  default:
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("Link: Invalid"));
  }
}
