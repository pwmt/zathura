/* See LICENSE file for license and copyright information */

#include <glib.h>
#include <glib/gi18n.h>

#include "synctex.h"

#include "zathura.h"
#include "page.h"
#include "document.h"
#include "utils.h"

#include <girara/session.h>

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

static void synctex_record_hits(zathura_t* zathura, int page_idx, girara_list_t* hits, bool first);
static double scan_float(GScanner* scanner);
static bool synctex_view(zathura_t* zathura, char* position);

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

  int page_idx = zathura_page_get_index(page);
  char *buffer = g_strdup_printf("%d:%d:%d:%s", page_idx + 1, x, y, filename);

  if (zathura->synctex.editor != NULL) {
    char* argv[] = {"synctex", "edit", "-o", buffer, "-x", zathura->synctex.editor, NULL};
    g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
  } else {
    char* argv[] = {"synctex", "edit", "-o", buffer, NULL};
    g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
  }

  g_free(buffer);
}

static void
synctex_record_hits(zathura_t* zathura, int page_idx, girara_list_t* hits, bool first)
{
  zathura_page_t* page = zathura_document_get_page(zathura->document, page_idx-1);
  if (page == NULL)
    return;

  GtkWidget* page_widget = zathura_page_get_widget(zathura, page);
  g_object_set(page_widget, "draw-links", FALSE, NULL);
  g_object_set(page_widget, "search-results", hits, NULL);

  if (first) {
    page_set_delayed(zathura, zathura_page_get_index(page));
    g_object_set(page_widget, "search-current", 0, NULL);
  }
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

static bool
synctex_view(zathura_t* zathura, char* position)
{
  char* filename = g_strdup(zathura_document_get_path(zathura->document));
  char* argv[] = {"synctex", "view", "-i", position, "-o", filename, NULL};
  gint output;

  bool ret = g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL, &output, NULL, NULL);
  g_free(filename);

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

  if (found_begin == true) {
    unsigned int number_of_pages = zathura_document_get_number_of_pages(zathura->document);
    for (unsigned int page_id = 0; page_id < number_of_pages; ++page_id) {
      zathura_page_t* page = zathura_document_get_page(zathura->document, page_id);
      if (page == NULL) {
        continue;
      }
      g_object_set(zathura_page_get_widget(zathura, page), "search-results", NULL, NULL);
    }
  }

  ret = false;
  int page = -1, nextpage;
  girara_list_t* hitlist = NULL;
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
              nextpage = g_scanner_cur_value(scanner).v_int;
              if (page != nextpage) {
                if (hitlist) {
                  synctex_record_hits(zathura, page, hitlist, !ret);
                  ret = true;
                }
                hitlist = girara_list_new2((girara_free_function_t) g_free);
                page = nextpage;
              }
              rectangle = g_malloc0(sizeof(zathura_rectangle_t));
              girara_list_append(hitlist, rectangle);
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

  if (hitlist != NULL) {
    synctex_record_hits(zathura, page, hitlist, !ret);
    ret = true;
  }

  g_scanner_destroy(scanner);
  close(output);

  return ret;
}
