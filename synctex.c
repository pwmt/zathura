/* See LICENSE file for license and copyright information */

#include <glib.h>
#include <girara/utils.h>

#ifdef WITH_SYSTEM_SYNCTEX
#include <synctex/synctex_parser.h>
#else
#include "synctex/synctex_parser.h"
#endif

#include "synctex.h"
#include "zathura.h"
#include "page.h"
#include "document.h"
#include "utils.h"

bool
synctex_get_input_line_column(const char* filename, unsigned int page, int x, int y,
    char** input_file, unsigned int* line, unsigned int* column)
{
  if (filename == NULL) {
    return false;
  }

  synctex_scanner_t scanner = synctex_scanner_new_with_output_file(filename, NULL, 1);
  if (scanner == NULL) {
    girara_debug("Failed to create synctex scanner.");
    return false;
  }

  synctex_scanner_t temp = synctex_scanner_parse(scanner);
  if (temp == NULL) {
    girara_debug("Failed to parse synctex file.");
    synctex_scanner_free(scanner);
    return false;
  }

  bool ret = false;

  if (synctex_edit_query(scanner, page + 1u, x, y) > 0) {
    /* Assume that a backward search returns at most one result. */
    synctex_node_t node = synctex_next_result(scanner);
    if (node != NULL) {
      if (input_file != NULL) {
        *input_file = g_strdup(synctex_scanner_get_name(scanner, synctex_node_tag(node)));
      }
      if (line != NULL) {
        *line = synctex_node_line(node);
      }
      if (column != NULL) {
        *column = synctex_node_column(node);
      }

      ret = true;
    }
  }

  synctex_scanner_free(scanner);

  return ret;
}

void
synctex_edit(const char* editor, zathura_page_t* page, int x, int y)
{
  if (editor == NULL || page == NULL) {
    return;
  }

  zathura_document_t* document = zathura_page_get_document(page);
  if (document == NULL) {
    return;
  }

  const char* filename = zathura_document_get_path(document);
  if (filename == NULL) {
    return;
  }

  unsigned int line = 0;
  unsigned int column = 0;
  char* input_file = NULL;

  if (synctex_get_input_line_column(filename, zathura_page_get_index(page), x, y,
        &input_file, &line, &column) == true) {
    char* linestr = g_strdup_printf("%d", line);
    char* columnstr = g_strdup_printf("%d", column);

    gchar** argv = NULL;
    gint    argc = 0;
    if (g_shell_parse_argv(editor, &argc, &argv, NULL) == TRUE) {
      for (gint i = 0; i != argc; ++i) {
        char* temp = girara_replace_substring(argv[i], "%{line}", linestr);
        g_free(argv[i]);
        argv[i] = temp;
        temp = girara_replace_substring(argv[i], "%{column}", columnstr);
        g_free(argv[i]);
        argv[i] = temp;
        temp = girara_replace_substring(argv[i], "%{input}", input_file);
        g_free(argv[i]);
        argv[i] = temp;
      }

      g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
      g_strfreev(argv);
    }

    g_free(linestr);
    g_free(columnstr);

    g_free(input_file);
  }
}

girara_list_t*
synctex_rectangles_from_position(const char* filename, const char* input_file,
                                 int line, int column, unsigned int* page,
                                 girara_list_t** secondary_rects)
{
  if (filename == NULL || input_file == NULL || page == NULL) {
    return NULL;
  }

  synctex_scanner_t scanner = synctex_scanner_new_with_output_file(filename, NULL, 1);
  if (scanner == NULL) {
    girara_debug("Failed to create synctex scanner.");
    return NULL;
  }

  synctex_scanner_t temp = synctex_scanner_parse(scanner);
  if (temp == NULL) {
    girara_debug("Failed to parse synctex file.");
    synctex_scanner_free(scanner);
    return NULL;
  }

  girara_list_t* hitlist     = girara_list_new2(g_free);
  girara_list_t* other_rects = girara_list_new2(g_free);

  if (synctex_display_query(scanner, input_file, line, column) > 0) {
    synctex_node_t node        = NULL;
    bool got_page              = false;

    while ((node = synctex_next_result (scanner)) != NULL) {
      const unsigned int current_page = synctex_node_page(node) - 1;
      if (got_page == false) {
        got_page = true;
        *page = current_page;
      }

      zathura_rectangle_t rect = { 0, 0, 0, 0 };
      rect.x1 = synctex_node_box_visible_h(node);
      rect.y1 = synctex_node_box_visible_v(node) - synctex_node_box_visible_height(node);
      rect.x2 = rect.x1 + synctex_node_box_visible_width(node);
      rect.y2 = synctex_node_box_visible_depth(node) + synctex_node_box_visible_height (node) + rect.y1;

      if (*page == current_page) {
        zathura_rectangle_t* real_rect = g_try_malloc(sizeof(zathura_rectangle_t));
        if (real_rect == NULL) {
          continue;
        }

        *real_rect = rect;
        girara_list_append(hitlist, real_rect);
      } else {
        synctex_page_rect_t* page_rect = g_try_malloc(sizeof(synctex_page_rect_t));
        if (page_rect == NULL) {
          continue;
        }

        page_rect->page = current_page;
        page_rect->rect = rect;

        girara_list_append(other_rects, page_rect);
      }
    }
  }

  synctex_scanner_free(scanner);

  if (secondary_rects != NULL) {
    *secondary_rects = other_rects;
  } else {
    girara_list_free(other_rects);
  }

  return hitlist;
}
