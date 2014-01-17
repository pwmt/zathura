/* See LICENSE file for license and copyright information */

#include <glib.h>

#include "synctex.h"
#include "zathura.h"
#include "page.h"
#include "document.h"
#include "utils.h"

enum {
  SYNCTEX_RESULT_BEGIN = 1,
  SYNCTEX_RESULT_END,
  SYNCTEX_PROP_PAGE,
  SYNCTEX_PROP_H,
  SYNCTEX_PROP_V,
  SYNCTEX_PROP_WIDTH,
  SYNCTEX_PROP_HEIGHT,
};

typedef struct token_s {
  const char* name;
  guint token;
} token_t;

static token_t scanner_tokens[] = {
  {"SyncTeX result begin", SYNCTEX_RESULT_BEGIN},
  {"SyncTeX result end", SYNCTEX_RESULT_END},
  {"Page:", SYNCTEX_PROP_PAGE},
  {"h:", SYNCTEX_PROP_H},
  {"v:", SYNCTEX_PROP_V},
  {"W:", SYNCTEX_PROP_WIDTH},
  {"H:", SYNCTEX_PROP_HEIGHT},
  {NULL, 0}
};

static GScannerConfig scanner_config = {
  .cset_skip_characters  = "\n\r",
  .cset_identifier_first = G_CSET_a_2_z G_CSET_A_2_Z,
  .cset_identifier_nth   = G_CSET_a_2_z G_CSET_A_2_Z ": ",
  .cpair_comment_single  = NULL,
  .case_sensitive        = TRUE,
  .scan_identifier       = TRUE,
  .scan_symbols          = TRUE,
  .scan_float            = TRUE,
  .numbers_2_int         = TRUE,
};

void
synctex_edit(zathura_t* zathura, zathura_page_t* page, int x, int y)
{
  if (zathura == NULL || page == NULL) {
    return;
  }

  zathura_document_t* document = zathura_page_get_document(page);
  if (document == NULL) {
    return;
  }

  const char *filename = zathura_document_get_path(document);
  if (filename == NULL) {
    return;
  }

  char** argv = g_malloc0(sizeof(char*) * (zathura->synctex.editor != NULL ?
      6 : 4));
  argv[0] = g_strdup("synctex");
  argv[1] = g_strdup("edit");
  argv[2] = g_strdup("-o");
  argv[3] = g_strdup_printf("%d:%d:%d:%s", zathura_page_get_index(page) + 1, x,
      y, filename);
  if (zathura->synctex.editor != NULL) {
    argv[4] = g_strdup("-x");
    argv[5] = g_strdup(zathura->synctex.editor);
  }

  g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
  g_strfreev(argv);
}

static double
scan_float(GScanner* scanner)
{
  switch (g_scanner_get_next_token(scanner)) {
    case G_TOKEN_FLOAT:
      return g_scanner_cur_value(scanner).v_float;
    case G_TOKEN_INT:
      return g_scanner_cur_value(scanner).v_int;
    default:
      return 0.0;
  }
}

girara_list_t*
synctex_rectangles_from_position(const char* filename, const char* position,
                                 int* page, girara_list_t** secondary_rects)
{
  if (filename == NULL || position == NULL || page == NULL) {
    return NULL;
  }

  char** argv = g_malloc0(sizeof(char*) * 6);
  argv[0] = g_strdup("synctex");
  argv[1] = g_strdup("view");
  argv[2] = g_strdup("-i");
  argv[3] = g_strdup(position);
  argv[4] = g_strdup("-o");
  argv[5] = g_strdup(filename);

  gint output = -1;
  bool ret = g_spawn_async_with_pipes(NULL, argv, NULL,
      G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL,
      &output, NULL, NULL);
  g_strfreev(argv);

  if (ret == false) {
    return false;
  }

  GScanner* scanner = g_scanner_new(&scanner_config);
  token_t* tokens = scanner_tokens;
  while (tokens->name != NULL) {
    g_scanner_add_symbol(scanner, tokens->name, GINT_TO_POINTER(tokens->token));
    tokens++;
  }

  g_scanner_input_file(scanner, output);

  bool found_begin = false, found_end = false;
  while (found_begin == false && found_end == false) {
    switch (g_scanner_get_next_token(scanner)) {
      case G_TOKEN_EOF:
        found_end = true;
        break;

      case G_TOKEN_SYMBOL:
        switch (GPOINTER_TO_INT(g_scanner_cur_value(scanner).v_identifier)) {
          case SYNCTEX_RESULT_BEGIN:
            found_begin = true;
            break;
        }
        break;

      default:
        /* skip everything else */
        break;
    }
  }

  int rpage = ZATHURA_PAGE_NUMBER_UNSPECIFIED;
  int current_page = ZATHURA_PAGE_NUMBER_UNSPECIFIED;
  girara_list_t* hitlist = girara_list_new2(g_free);
  girara_list_t* other_rects = girara_list_new2(g_free);
  zathura_rectangle_t* rectangle = NULL;

  while (found_end == false) {
    switch (g_scanner_get_next_token(scanner)) {
      case G_TOKEN_EOF:
        found_end = true;
        break;

      case G_TOKEN_SYMBOL:
        switch (GPOINTER_TO_INT(g_scanner_cur_value(scanner).v_identifier)) {
          case SYNCTEX_RESULT_END:
            found_end = true;
            break;

          case SYNCTEX_PROP_PAGE:
            if (g_scanner_get_next_token(scanner) == G_TOKEN_INT) {
              current_page = g_scanner_cur_value(scanner).v_int - 1;
              if (rpage == ZATHURA_PAGE_NUMBER_UNSPECIFIED) {
                rpage = current_page;
              }

              if (*page == current_page && rectangle != NULL) {
                girara_list_append(hitlist, rectangle);
                rectangle = NULL;
              } else if (rectangle != NULL) {
                synctex_page_rect_t* page_rect = g_malloc0(sizeof(synctex_page_rect_t));
                page_rect->page = current_page;
                page_rect->rect = *rectangle;
                girara_list_append(other_rects, page_rect);
              }

              g_free(rectangle);
              rectangle = g_malloc0(sizeof(zathura_rectangle_t));
            }
            break;

          case SYNCTEX_PROP_H:
            rectangle->x1 = scan_float(scanner);
            break;

          case SYNCTEX_PROP_V:
            rectangle->y2 = scan_float(scanner);
            break;

          case SYNCTEX_PROP_WIDTH:
            rectangle->x2 = rectangle->x1 + scan_float(scanner);
            break;

          case SYNCTEX_PROP_HEIGHT:
            rectangle->y1 = rectangle->y2 - scan_float(scanner);
            break;
        }
        break;

      default:
        break;
    }
  }

  if (rectangle != NULL) {
    if (current_page == rpage) {
      girara_list_append(hitlist, rectangle);
    } else {
      synctex_page_rect_t* page_rect = g_malloc0(sizeof(synctex_page_rect_t));
      page_rect->page = current_page;
      page_rect->rect = *rectangle;
      girara_list_append(other_rects, page_rect);
      g_free(rectangle);
    }
  }

  g_scanner_destroy(scanner);
  close(output);

  if (page != NULL) {
    *page = rpage;
  }
  if (secondary_rects != NULL) {
    *secondary_rects = other_rects;
  } else {
    girara_list_free(other_rects);
  }

  return hitlist;
}
