/* See LICENSE file for license and copyright information */

#include <glib.h>
#include <girara/utils.h>
#include <girara/settings.h>

#ifdef WITH_SYNCTEX
#include <synctex/synctex_parser.h>
#endif

#include "synctex.h"
#include "zathura.h"
#include "page.h"
#include "document.h"
#include "utils.h"
#include "adjustment.h"

#ifdef WITH_SYNCTEX
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

// NEOVIM
void
synctex_edit_msg(int fd, zathura_page_t* page, int x, int y)
{
  if (fd == 0 || page == NULL) {
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

	char* forkstr = g_strdup_printf("edit +%d %s\n", line, input_file);

	write(fd, forkstr, strlen(forkstr));

    g_free(forkstr);

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

  /* We use indexes starting at 0 but SyncTeX uses 1 */
  ++line;
  ++column;

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
#else
bool
synctex_get_input_line_column(const char* UNUSED(filename),
                              unsigned int UNUSED(page), int UNUSED(x),
                              int UNUSED(y), char** UNUSED(input_file),
                              unsigned int* UNUSED(line),
                              unsigned int* UNUSED(column))
{
  return false;
}

void
synctex_edit(const char* UNUSED(editor), zathura_page_t* UNUSED(page),
             int UNUSED(x), int UNUSED(y))
{}

girara_list_t*
synctex_rectangles_from_position(const char* UNUSED(filename),
                                 const char* UNUSED(input_file),
                                 int UNUSED(line), int UNUSED(column),
                                 unsigned int* UNUSED(page),
                                 girara_list_t** UNUSED(secondary_rects))
{
  return NULL;
}
#endif

bool
synctex_parse_input(const char* synctex, char** input_file, int* line,
                    int* column)
{
  if (synctex == NULL || input_file == NULL || line == NULL || column == NULL) {
    return false;
  }

  char** split_fwd = g_strsplit(synctex, ":", 0);
  if (split_fwd == NULL || split_fwd[0] == NULL || split_fwd[1] == NULL ||
      split_fwd[2] == NULL || split_fwd[3] != NULL) {
    g_strfreev(split_fwd);
    return false;
  }

  *line = MIN(INT_MAX, g_ascii_strtoll(split_fwd[0], NULL, 10));
  *column = MIN(INT_MAX, g_ascii_strtoll(split_fwd[1], NULL, 10));
  /* SyncTeX starts indexing at 1, but we use 0 */
  if (*line > 0) {
    --*line;
  }
  if (*column > 0) {
    --*column;
  }
  *input_file = g_strdup(split_fwd[2]);

  g_strfreev(split_fwd);
  return true;
}

void
synctex_highlight_rects(zathura_t* zathura, unsigned int page,
                        girara_list_t** rectangles)
{
  const unsigned int number_of_pages = zathura_document_get_number_of_pages(zathura->document);

  for (unsigned int p = 0; p != number_of_pages; ++p) {
    GObject* widget = G_OBJECT(zathura->pages[p]);

    g_object_set(widget, "draw-links", FALSE, "search-results", rectangles[p],
                 NULL);
    if (p == page) {
      g_object_set(widget, "search-current", 0, NULL);
    }
  }

  document_draw_search_results(zathura, true);

  girara_list_t* rect_list = rectangles[page];
  if (rect_list == NULL || girara_list_size(rect_list) == 0) {
    girara_debug("No rectangles for the given page. Jumping to page %u.", page);
    page_set(zathura, page);
    return;
  }

  bool search_hadjust = true;
  girara_setting_get(zathura->ui.session, "search-hadjust", &search_hadjust);

  /* compute the position of the center of the page */
  double pos_x = 0;
  double pos_y = 0;
  page_number_to_position(zathura->document, page, 0.5, 0.5, &pos_x, &pos_y);

  /* correction to center the current result                          */
  /* NOTE: rectangle is in viewport units, already scaled and rotated */
  unsigned int cell_height = 0;
  unsigned int cell_width = 0;
  zathura_document_get_cell_size(zathura->document, &cell_height, &cell_width);

  unsigned int doc_height = 0;
  unsigned int doc_width = 0;
  zathura_document_get_document_size(zathura->document, &doc_height, &doc_width);

  /* Need to adjust rectangle to page scale and orientation */
  zathura_page_t* doc_page = zathura_document_get_page(zathura->document, page);
  zathura_rectangle_t* rect = girara_list_nth(rect_list, 0);
  if (rect == NULL) {
    girara_debug("List of rectangles is broken. Jumping to page %u.", page);
    page_set(zathura, page);
    return;
  }

  zathura_rectangle_t rectangle = recalc_rectangle(doc_page, *rect);

  /* compute the center of the rectangle, which will be aligned to the center
     of the viewport */
  double center_x = (rectangle.x1 + rectangle.x2) / 2;
  double center_y = (rectangle.y1 + rectangle.y2) / 2;

  pos_y += (center_y - (double)cell_height/2) / (double)doc_height;
  if (search_hadjust == true) {
    pos_x += (center_x - (double)cell_width/2) / (double)doc_width;
  }

  /* move to position */
  girara_debug("Jumping to page %u position (%f, %f).", page, pos_x, pos_y);
  zathura_jumplist_add(zathura);
  position_set(zathura, pos_x, pos_y);
  zathura_jumplist_add(zathura);
}

bool
synctex_view(zathura_t* zathura, const char* input_file,
             unsigned int line, unsigned int column)
{
  if (zathura == NULL || input_file == NULL) {
    return false;
  }

  const unsigned int number_of_pages = zathura_document_get_number_of_pages(zathura->document);

  unsigned int page = 0;
  girara_list_t* secondary_rects = NULL;
  girara_list_t* rectangles = synctex_rectangles_from_position(
    zathura_document_get_path(zathura->document), input_file, line,
    column, &page, &secondary_rects);

  if (rectangles == NULL) {
    return false;
  }

  girara_list_t** all_rectangles = g_try_malloc0(number_of_pages * sizeof(girara_list_t*));
  if (all_rectangles == NULL) {
    girara_list_free(rectangles);
    return false;
  }

  for (unsigned int p = 0; p != number_of_pages; ++p) {
    if (p == page) {
      all_rectangles[p] = rectangles;
    } else {
      all_rectangles[p] = girara_list_new2(g_free);
    }
  }

  if (secondary_rects != NULL) {
    GIRARA_LIST_FOREACH(secondary_rects, synctex_page_rect_t*, iter, rect)
      zathura_rectangle_t* newrect = g_try_malloc0(sizeof(zathura_rectangle_t));
      if (newrect != NULL) {
        *newrect = rect->rect;
        girara_list_append(all_rectangles[rect->page], newrect);
      }
    GIRARA_LIST_FOREACH_END(secondary_rects, synctex_page_rect_t*, iter, rect);
  }

  synctex_highlight_rects(zathura, page, all_rectangles);

  girara_list_free(secondary_rects);
  g_free(all_rectangles);

  return true;
}
