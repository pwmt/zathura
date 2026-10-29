/* SPDX-License-Identifier: Zlib */

#include "links-internal.h"

#include <math.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <girara/utils.h>
#include <girara-gtk/session.h>
#include <girara-gtk/settings.h>

#include "adjustment.h"
#include "zathura.h"
#include "document.h"
#include "document-widget.h"
#include "utils.h"
#include "page.h"
#include "render.h"

struct zathura_link_s {
  zathura_rectangle_t position; /**< Position of the link */
  zathura_link_target_t target; /**< Link target */
  zathura_link_type_t type;     /**< Link type */
};

zathura_link_t* zathura_link_new(zathura_link_type_t type, zathura_rectangle_t position, zathura_link_target_t target) {
  g_autoptr(zathura_link_t) link = g_try_malloc0(sizeof(zathura_link_t));
  if (!link) {
    return NULL;
  }

  link->position = position;
  link->target   = target;
  link->type     = type;

  // duplicate target.value if necessary and check target constraints
  switch (type) {
  case ZATHURA_LINK_NONE:
  case ZATHURA_LINK_GOTO_DEST:
    link->target.value = g_strdup(target.value);
    if (!isfinite(target.bottom) || !isfinite(target.left) || !isfinite(target.right) || isfinite(target.top) ||
        !isfinite(target.zoom)) {
      return NULL;
    }
    break;
  case ZATHURA_LINK_GOTO_REMOTE:
  case ZATHURA_LINK_URI:
  case ZATHURA_LINK_LAUNCH:
  case ZATHURA_LINK_NAMED:
    // target.value is required for these cases
    if (!target.value) {
      return NULL;
    }

    link->target.value = g_strdup(target.value);
    break;
  default:
    return NULL;
  }

  return g_steal_pointer(&link);
}

void zathura_link_free(zathura_link_t* link) {
  if (!link) {
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
  if (!link) {
    return ZATHURA_LINK_INVALID;
  }

  return link->type;
}

zathura_rectangle_t zathura_link_get_position(zathura_link_t* link) {
  if (!link) {
    const zathura_rectangle_t position = {0, 0, 0, 0};
    return position;
  }

  return link->position;
}

zathura_link_target_t zathura_link_get_target(zathura_link_t* link) {
  if (!link) {
    const zathura_link_target_t target = {ZATHURA_LINK_DESTINATION_UNKNOWN, NULL, 0, -1, -1, -1, -1, -1};
    return target;
  }

  return link->target;
}

// NAN means retain the current page-local offset.
typedef struct {
  double scale;
  double x;
  double y;
} zathura_link_destination_view_t;

// Calculate fitting scale and viewport centre for a destination
static bool calculate_link_destination_view(const zathura_link_target_t* target, double width, double height,
                                            unsigned int rotation, double view_width, double view_height, double scale,
                                            bool fit, zathura_link_destination_view_t* view) {
  if (width <= 0 || height <= 0 || scale <= 0 || view_width <= 0 || view_height <= 0) {
    return false;
  }

  zathura_rectangle_t rectangle = {.x1 = 0, .y1 = 0, .x2 = width, .y2 = height};
  switch (target->destination_type) {
  case ZATHURA_LINK_DESTINATION_FITB:
  case ZATHURA_LINK_DESTINATION_FITBH:
  case ZATHURA_LINK_DESTINATION_FITBV:
  case ZATHURA_LINK_DESTINATION_FITR:
    rectangle.x1 = target->left;
    rectangle.y1 = target->top;
    rectangle.x2 = target->right;
    rectangle.y2 = target->bottom;
    if (rectangle.x1 == -1 || rectangle.x2 == -1 || rectangle.y1 == -1 || rectangle.y2 == -1 ||
        rectangle.x2 <= rectangle.x1 || rectangle.y2 <= rectangle.y1) {
      return false;
    }
    break;
  default:
    break;
  }

  bool fit_x = true;
  bool fit_y = true;
  bool has_x = true;
  bool has_y = true;
  switch (target->destination_type) {
  case ZATHURA_LINK_DESTINATION_XYZ:
    fit_x        = false;
    fit_y        = false;
    has_x        = target->left != -1;
    has_y        = target->top != -1;
    rectangle.x1 = rectangle.x2 = has_x ? target->left : 0;
    rectangle.y1 = rectangle.y2 = has_y ? target->top : 0;
    break;
  case ZATHURA_LINK_DESTINATION_FITH:
  case ZATHURA_LINK_DESTINATION_FITBH:
    fit_y        = false;
    has_y        = target->top != -1;
    rectangle.y1 = rectangle.y2 = has_y ? target->top : 0;
    break;
  case ZATHURA_LINK_DESTINATION_FITV:
  case ZATHURA_LINK_DESTINATION_FITBV:
    fit_x        = false;
    has_x        = target->left != -1;
    rectangle.x1 = rectangle.x2 = has_x ? target->left : 0;
    break;
  case ZATHURA_LINK_DESTINATION_FITR:
  case ZATHURA_LINK_DESTINATION_FIT:
  case ZATHURA_LINK_DESTINATION_FITB:
    break;
  default:
    return false;
  }

  // Rotate the target point, line or rectangle in page units.
  rectangle = rotate_rectangle(rectangle, rotation % 360, height, width);
  if (rotation % 180 != 0) {
    bool tmp = fit_x;
    fit_x    = fit_y;
    fit_y    = tmp;
    tmp      = has_x;
    has_x    = has_y;
    has_y    = tmp;
  }

  if (fit && (fit_x || fit_y)) {
    scale = MIN(fit_x ? view_width / (rectangle.x2 - rectangle.x1) : INFINITY,
                fit_y ? view_height / (rectangle.y2 - rectangle.y1) : INFINITY);
    if (!isfinite(scale) || scale < FLT_EPSILON) {
      return false;
    }
  }

  view->scale = scale;
  view->x     = has_x ? (fit_x ? (rectangle.x1 + rectangle.x2) / 2 : rectangle.x1 + view_width / (2 * scale)) : NAN;
  view->y     = has_y ? (fit_y ? (rectangle.y1 + rectangle.y2) / 2 : rectangle.y1 + view_height / (2 * scale)) : NAN;
  return true;
}

static void link_goto_dest(zathura_t* zathura, const zathura_link_t* link) {
  zathura_document_t* document = zathura_get_document(zathura);
  if (link->target.page_number >= zathura_document_get_number_of_pages(document)) {
    girara_warning("link to non-existing page %u", link->target.page_number);
    return;
  }

  zathura_page_t* page = zathura_document_get_page(document, link->target.page_number);
  if (!page) {
    return;
  }

  ZathuraDocumentWidget* widget = zathura->ui.document_widget;
  unsigned int view_height      = 0;
  unsigned int view_width       = 0;
  unsigned int doc_height       = 0;
  unsigned int doc_width        = 0;
  zathura_document_widget_get_viewport_size(widget, &view_height, &view_width);
  zathura_document_widget_get_document_size(widget, &doc_height, &doc_width);
  if (!view_height || !view_width || !doc_height || !doc_width) {
    return;
  }

  const double old_zoom       = zathura_document_get_zoom(document);
  double scale                = zathura_document_get_scale(document) * zathura_page_get_zoom(page);
  const double width          = zathura_page_get_width(page);
  const double height         = zathura_page_get_height(page);
  const unsigned int rotation = (zathura_document_get_rotation(document) + zathura_page_get_rotation(page)) % 360;

  bool link_zoom = true;
  girara_setting_get(zathura->ui.session, "link-zoom", &link_zoom);

  zathura_link_destination_view_t view;
  if (!calculate_link_destination_view(&link->target, width, height, rotation, view_width, view_height, scale,
                                       link_zoom, &view)) {
    girara_warning("invalid link destination");
    return;
  }

  // with a valid target, store current position before jumping to the link target
  zathura_jumplist_add(zathura);

  int mode = DOCUMENT_WIDGET_GRID;
  g_object_get(widget, "layout-mode", &mode, NULL);

  /* Preserve page-local viewport offsets for unspecified coordinates. */
  zathura_page_t* current_page =
      zathura_document_get_page(document, zathura_document_get_current_page_number(document));
  if (mode == DOCUMENT_WIDGET_SINGLE) {
    page_calc_height_width(document, current_page, &doc_height, &doc_width, true);
  }

  double old_x = 0;
  double old_y = 0;
  zathura_document_widget_get_page_origin(widget, zathura_page_get_index(current_page), &old_x, &old_y);

  const double old_scale = zathura_document_get_scale(document) * zathura_page_get_zoom(current_page);
  old_x = (zathura_document_widget_get_position_x(widget) * doc_width - view_width / 2.0 - old_x) / old_scale;
  old_y = (zathura_document_widget_get_position_y(widget) * doc_height - view_height / 2.0 - old_y) / old_scale;

  double zoom = old_zoom * view.scale / scale;
  if (link_zoom) {
    // use provided zoom for XYZ destinations
    if (link->target.destination_type == ZATHURA_LINK_DESTINATION_XYZ && link->target.zoom > 0) {
      zoom = link->target.zoom;
    }

    // apply new zoom if we have one
    if (link->target.destination_type != ZATHURA_LINK_DESTINATION_XYZ || link->target.zoom > 0) {
      zoom = zathura_correct_zoom_value(zathura->ui.session, zoom);
      zathura_document_widget_set_adjust_mode(widget, ZATHURA_ADJUST_NONE);
      zathura_document_set_zoom(document, zoom);
      zathura_document_widget_render_all(widget);
    }
  }

  // recompute positions at the actual zoom
  scale = zathura_document_get_scale(document) * zathura_page_get_zoom(page);
  calculate_link_destination_view(&link->target, width, height, rotation, view_width, view_height, scale, false, &view);
  if (mode == DOCUMENT_WIDGET_SINGLE) {
    page_calc_height_width(document, page, &doc_height, &doc_width, true);
  } else {
    zathura_document_widget_get_document_size(widget, &doc_height, &doc_width);
  }

  double origin_x = 0;
  double origin_y = 0;
  zathura_document_widget_get_page_origin(widget, link->target.page_number, &origin_x, &origin_y);

  // find the viewport centre in page pixels
  double center_x;
  double center_y;
  if (isnan(view.x)) {
    center_x = old_x * scale + view_width / 2.0;
  } else {
    center_x = view.x * scale;
  }
  if (isnan(view.y)) {
    center_y = old_y * scale + view_height / 2.0;
  } else {
    center_y = view.y * scale;
  }

  // convert to document ratios
  bool link_hadjust = true;
  girara_setting_get(zathura->ui.session, "link-hadjust", &link_hadjust);

  double pos_x = -1;
  if (link_hadjust) {
    pos_x = (origin_x + center_x) / doc_width;
  } else if (mode == DOCUMENT_WIDGET_SINGLE) {
    pos_x = 0.5;
  }
  const double pos_y = (origin_y + center_y) / doc_height;

  zathura_document_set_current_page_number(document, link->target.page_number);
  position_set(zathura, pos_x, pos_y);
  zathura_jumplist_add(zathura);
}

#ifndef WITH_SANDBOX
static void link_remote(zathura_t* zathura, const char* file) {
  if (!zathura_has_document(zathura) || !file) {
    return;
  }

  const char* path     = zathura_document_get_path(zathura_get_document(zathura));
  g_autofree char* dir = g_path_get_dirname(path);
  g_autofree char* uri = g_build_filename(file, NULL);

  char* argv[] = {*zathura->global.arguments, uri, NULL};

  g_autoptr(GError) error = NULL;
  if (!g_spawn_async(dir, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error)) {
    girara_error("Failed to execute command: %s", error->message);
  }
}

static void link_launch(zathura_t* zathura, const char* link) {
  if (link == NULL) {
    return;
  }

  const char* document = zathura_document_get_path(zathura_get_document(zathura));
  g_autofree char* dir = g_path_get_dirname(document);

  if (girara_xdg_open_with_working_directory(link, dir) == false) {
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("Failed to run xdg-open."));
  }
}

/**
 * Context passed to the external link confirmation dialog callback
 */
typedef struct {
  zathura_t* zathura;
  zathura_link_type_t type;
  char* value; /**< Owned copy of the link target */
} link_confirm_data_t;

static void link_confirm_data_free(link_confirm_data_t* data) {
  if (!data) {
    return;
  }
  g_free(data->value);
  g_free(data);
}

static void cb_link_confirm_hide(GtkWidget* UNUSED(w), void* data) {
  link_confirm_data_free(data);
}

/**
 * Callback for the external link confirmation dialog
 * Opens the link only if the user pressed Enter with empty input or 'y'
 */
static gboolean cb_link_confirm(GiraraDialog* inputbar, const char* input, void* data) {
  g_signal_handlers_disconnect_matched(inputbar, G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA,
                                       g_signal_lookup("hide", GTK_TYPE_WIDGET), 0, NULL, NULL, data);
  link_confirm_data_t* ctx = data;
  if (!input || !ctx) {
    link_confirm_data_free(ctx);
    return true;
  }

  /* Accept: empty string (bare Enter), or confirmation string */
  const bool confirmed = (!input || input[0] == '\0' || g_strcmp0(input, _("y")) == 0 || g_strcmp0(input, _("Y")) == 0);

  if (confirmed) {
    switch (ctx->type) {
    case ZATHURA_LINK_GOTO_REMOTE:
      link_remote(ctx->zathura, ctx->value);
      break;
    case ZATHURA_LINK_URI:
    case ZATHURA_LINK_LAUNCH: {
      link_launch(ctx->zathura, ctx->value);
      break;
    }
    default:
      break;
    }
  } else {
    girara_notify(ctx->zathura->ui.session, GIRARA_INFO, _("Cancelled."));
  }

  link_confirm_data_free(ctx);
  return true;
}

/**
 * Prompt the user for confirmation before opening an external link
 */
static gboolean link_confirm_spawn(void* data) {
  link_confirm_data_t* ctx = data;

  g_autofree gchar* escaped = g_markup_escape_text(ctx->value, -1);
  g_autofree gchar* prompt  = g_strdup_printf(_("Open external link <b>%s</b>? [Y/n]"), escaped);

  GiraraDialog* dialog = girara_dialog(ctx->zathura->ui.session, prompt, false);
  g_signal_connect(dialog, "hide", G_CALLBACK(cb_link_confirm_hide), ctx);
  g_signal_connect(dialog, "activate", G_CALLBACK(cb_link_confirm), ctx);

  return G_SOURCE_REMOVE;
}

static void link_confirm(zathura_t* zathura, zathura_link_type_t type, const char* value) {
  if (!value) {
    return;
  }

  link_confirm_data_t* ctx = g_try_malloc0(sizeof(link_confirm_data_t));
  if (!ctx) {
    return;
  }

  ctx->zathura = zathura;
  ctx->type    = type;
  ctx->value   = g_strdup(value);

  g_idle_add(link_confirm_spawn, ctx);
}
#endif

void zathura_link_evaluate(zathura_t* zathura, zathura_link_t* link) {
  if (!zathura_has_document(zathura) || !link) {
    return;
  }

#ifdef WITH_SANDBOX
  if (link->type != ZATHURA_LINK_GOTO_DEST) {
    girara_notify(zathura->ui.session, GIRARA_ERROR,
                  _("Opening external applications in strict sandbox mode is not permitted"));
    return;
  }
#endif

  switch (link->type) {
  case ZATHURA_LINK_GOTO_DEST:
    girara_debug("Going to link destination: page: %u", link->target.page_number);
    link_goto_dest(zathura, link);
    break;
#ifndef WITH_SANDBOX
  case ZATHURA_LINK_GOTO_REMOTE:
  case ZATHURA_LINK_URI:
  case ZATHURA_LINK_LAUNCH:
    bool confirm = true;
    girara_setting_get(zathura->ui.session, "open-link-confirm", &confirm);
    girara_debug("Opening link: %s (type = %u)", link->target.value, (unsigned int)link->target.destination_type);
    if (confirm) {
      link_confirm(zathura, link->type, link->target.value);
    } else if (link->type == ZATHURA_LINK_GOTO_REMOTE) {
      link_remote(zathura, link->target.value);
    } else {
      link_launch(zathura, link->target.value);
    }
    break;
#endif
  default:
    girara_error("Unhandled link type: %u", link->type);
    break;
  }
}

void zathura_link_display(zathura_t* zathura, zathura_link_t* link) {
  zathura_link_type_t type     = zathura_link_get_type(link);
  zathura_link_target_t target = zathura_link_get_target(link);
  switch (type) {
  case ZATHURA_LINK_GOTO_DEST:
    girara_notify(zathura->ui.session, GIRARA_INFO, _("Link: page %u"), target.page_number);
    break;
  case ZATHURA_LINK_GOTO_REMOTE:
  case ZATHURA_LINK_URI:
  case ZATHURA_LINK_LAUNCH:
  case ZATHURA_LINK_NAMED: {
    g_autofree gchar* escaped_value = g_markup_escape_text(target.value, -1);
    girara_notify(zathura->ui.session, GIRARA_INFO, _("Link: %s"), escaped_value);
    break;
  }
  default:
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("Link: Invalid"));
  }
}

void zathura_link_copy(zathura_t* zathura, zathura_link_t* link, GdkClipboard* selection) {
  zathura_link_type_t type     = zathura_link_get_type(link);
  zathura_link_target_t target = zathura_link_get_target(link);
  switch (type) {
  case ZATHURA_LINK_GOTO_DEST: {
    g_autofree gchar* tmp = g_strdup_printf("%u", target.page_number);
    gdk_clipboard_set_text(selection, tmp);
    girara_notify(zathura->ui.session, GIRARA_INFO, _("Copied page number: %u"), target.page_number);
    break;
  }
  case ZATHURA_LINK_GOTO_REMOTE:
  case ZATHURA_LINK_URI:
  case ZATHURA_LINK_LAUNCH:
  case ZATHURA_LINK_NAMED: {
    gdk_clipboard_set_text(selection, target.value);
    g_autofree gchar* escaped_value = g_markup_escape_text(target.value, -1);
    girara_notify(zathura->ui.session, GIRARA_INFO, _("Copied link: %s"), escaped_value);
    break;
  }
  default:
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("Link: Invalid"));
  }
}
