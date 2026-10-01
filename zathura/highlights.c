/* SPDX-License-Identifier: Zlib */

#include "highlights.h"

#include <girara-gtk/session.h>
#include <girara/datastructures.h>
#include <girara/log.h>
#include <girara/utils.h>
#include <string.h>

#include "database.h"
#include "document.h"

static void zathura_highlight_free(void* data) {
  if (data != NULL) {
    zathura_highlight_t* highlight = data;
    g_free(highlight->id);
    g_free(highlight->text);
    girara_list_free(highlight->rectangles);
    g_free(highlight);
  }
}

zathura_highlight_t* zathura_highlight_add(zathura_t* zathura, unsigned int page, girara_list_t* rectangles,
                                           GdkRGBA color, const gchar* text) {
  g_return_val_if_fail(zathura_has_document(zathura) == true && zathura->highlights.highlights, NULL);
  g_return_val_if_fail(rectangles != NULL && girara_list_size(rectangles) != 0, NULL);

  zathura_highlight_t* highlight = g_try_malloc0(sizeof(zathura_highlight_t));
  if (highlight == NULL) {
    return NULL;
  }

  g_autofree gchar* uuid = g_uuid_string_random();
  highlight->id          = g_strdup(uuid);
  highlight->page        = page;
  highlight->rectangles  = rectangles;
  highlight->color       = color;
  highlight->text        = g_strdup(text);
  highlight->created     = g_get_real_time();

  girara_list_append(zathura->highlights.highlights, highlight);

  zathura_document_t* document = zathura_get_document(zathura);
  const char* path             = zathura_document_get_path(document);
  if (zathura_db_add_highlight(zathura->database, path, highlight) == false) {
    girara_warning("Failed to add highlight to database.");
  }

  return highlight;
}

bool zathura_highlight_remove(zathura_t* zathura, const gchar* id, unsigned int* page) {
  g_return_val_if_fail(zathura && zathura->highlights.highlights, false);
  g_return_val_if_fail(id != NULL && *id != '\0', false);

  zathura_highlight_t* highlight = NULL;
  for (size_t idx = 0; idx != girara_list_size(zathura->highlights.highlights); ++idx) {
    zathura_highlight_t* it = girara_list_nth(zathura->highlights.highlights, idx);
    if (g_str_has_prefix(it->id, id) == TRUE) {
      if (highlight != NULL) {
        /* more than one highlight matches this prefix; refuse instead of
         * guessing which one the caller meant */
        girara_warning("Ambiguous highlight id prefix: %s", id);
        return false;
      }
      highlight = it;
    }
  }

  if (highlight == NULL) {
    return false;
  }

  if (page != NULL) {
    *page = highlight->page;
  }

  zathura_document_t* document = zathura_get_document(zathura);
  const char* path             = zathura_document_get_path(document);
  if (zathura_db_remove_highlight(zathura->database, path, highlight->id) == false) {
    girara_warning("Failed to remove highlight from database.");
  }

  girara_list_remove(zathura->highlights.highlights, highlight);

  return true;
}

girara_list_t* zathura_highlight_get_for_page(zathura_t* zathura, unsigned int page) {
  g_return_val_if_fail(zathura && zathura->highlights.highlights, NULL);

  girara_list_t* list = girara_list_new();
  if (list == NULL) {
    return NULL;
  }

  for (size_t idx = 0; idx != girara_list_size(zathura->highlights.highlights); ++idx) {
    zathura_highlight_t* highlight = girara_list_nth(zathura->highlights.highlights, idx);
    if (highlight->page == page) {
      girara_list_append(list, highlight);
    }
  }

  return list;
}

#define HIGHLIGHT_PALETTE_PRESETS 3

bool zathura_highlights_init(zathura_t* zathura) {
  if (!zathura) {
    return false;
  }

  zathura->highlights.highlights = girara_list_new_with_free(zathura_highlight_free);

  // preset palette
  zathura->highlights.palette[0] =
      (GdkRGBA){.red = 251 / 255.0, .green = 247 / 255.0, .blue = 25 / 255.0, .alpha = 1.0}; /* yellow */
  zathura->highlights.palette[1] =
      (GdkRGBA){.red = 93 / 255.0, .green = 226 / 255.0, .blue = 60 / 255.0, .alpha = 1.0}; /* green */
  zathura->highlights.palette[2] =
      (GdkRGBA){.red = 247 / 255.0, .green = 47 / 255.0, .blue = 53 / 255.0, .alpha = 1.0}; /* red */
  zathura->highlights.palette[3]     = (GdkRGBA){.alpha = -1.0};                            /* unset */
  zathura->highlights.palette_active = 0;

  return zathura->highlights.highlights != NULL;
}

GdkRGBA zathura_highlight_get_active_color(zathura_t* zathura) {
  g_return_val_if_fail(zathura, ((GdkRGBA){0}));
  return zathura->highlights.palette[zathura->highlights.palette_active];
}

void zathura_highlight_cycle_color(zathura_t* zathura) {
  g_return_if_fail(zathura);

  const bool has_custom              = zathura->highlights.palette[3].alpha >= 0.0;
  const unsigned int slots           = has_custom ? 4 : HIGHLIGHT_PALETTE_PRESETS;
  zathura->highlights.palette_active = (zathura->highlights.palette_active + 1) % slots;
}

void zathura_highlight_set_custom_color(zathura_t* zathura, GdkRGBA color) {
  g_return_if_fail(zathura);

  zathura->highlights.palette[3]     = color;
  zathura->highlights.palette_active = 3;
}

const char* zathura_highlight_get_active_color_name(zathura_t* zathura) {
  g_return_val_if_fail(zathura, "");

  static const char* names[4] = {"yellow", "green", "red", "custom"};
  return names[zathura->highlights.palette_active];
}

bool zathura_highlights_load(zathura_t* zathura, const gchar* file) {
  g_return_val_if_fail(zathura && zathura->database, false);
  g_return_val_if_fail(file, false);

  girara_list_clear(zathura->highlights.highlights);
  return zathura_db_load_highlights(zathura->database, file, zathura->highlights.highlights);
}

void zathura_highlights_free(zathura_t* zathura) {
  if (zathura) {
    girara_list_free(zathura->highlights.highlights);
  }
}
