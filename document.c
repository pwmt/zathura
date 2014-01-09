/* See LICENSE file for license and copyright information */

#define _BSD_SOURCE
#define _XOPEN_SOURCE 700

#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <glib.h>
#include <glib/gi18n.h>
#ifdef WITH_MAGIC
#include <magic.h>
#endif
#include <unistd.h>

#include <girara/datastructures.h>
#include <girara/utils.h>
#include <girara/statusbar.h>
#include <girara/session.h>
#include <girara/settings.h>

#include "adjustment.h"
#include "document.h"
#include "utils.h"
#include "zathura.h"
#include "render.h"
#include "database.h"
#include "page.h"
#include "page-widget.h"
#include "plugin.h"

/** Read a most GT_MAX_READ bytes before falling back to file. */
static const size_t GT_MAX_READ = 1 << 16;

static const char* guess_type(const char* path);

/**
 * Document
 */
struct zathura_document_s {
  char* file_path; /**< File path of the document */
  char* basename; /**< Basename of the document */
  const char* password; /**< Password of the document */
  unsigned int current_page_number; /**< Current page number */
  unsigned int number_of_pages; /**< Number of pages */
  double scale; /**< Scale value */
  unsigned int rotate; /**< Rotation */
  void* data; /**< Custom data */
  zathura_adjust_mode_t adjust_mode; /**< Adjust mode (best-fit, width) */
  int page_offset; /**< Page offset */
  double cell_width; /**< width of a page cell in the document (not ransformed by scale and rotation) */
  double cell_height; /**< height of a page cell in the document (not ransformed by scale and rotation) */
  unsigned int view_width; /**< width of current viewport */
  unsigned int view_height; /**< height of current viewport */
  unsigned int pages_per_row; /**< number of pages in a row */
  unsigned int first_page_column; /**< column of the first page */
  unsigned int page_padding; /**< padding between pages */
  double position_x; /**< X adjustment */
  double position_y; /**< Y adjustment */

  /**
   * Document pages
   */
  zathura_page_t** pages;

  /**
   * Used plugin
   */
  zathura_plugin_t* plugin;
};


zathura_document_t*
zathura_document_open(zathura_plugin_manager_t* plugin_manager, const char*
                      path, const char* password, zathura_error_t* error)
{
  if (path == NULL) {
    return NULL;
  }

  if (g_file_test(path, G_FILE_TEST_EXISTS) == FALSE) {
    girara_error("File '%s' does not exist", path);
    return NULL;
  }

  const char* content_type = guess_type(path);
  if (content_type == NULL) {
    girara_error("Could not determine file type.");
    return NULL;
  }

  /* determine real path */
  long path_max;
#ifdef PATH_MAX
  path_max = PATH_MAX;
#else
  path_max = pathconf(path,_PC_PATH_MAX);
  if (path_max <= 0)
    path_max = 4096;
#endif

  char* real_path              = NULL;
  zathura_document_t* document = NULL;

  real_path = malloc(sizeof(char) * path_max);
  if (real_path == NULL) {
    g_free((void*)content_type);
    return NULL;
  }

  if (realpath(path, real_path) == NULL) {
    g_free((void*)content_type);
    free(real_path);
    return NULL;
  }

  zathura_plugin_t* plugin = zathura_plugin_manager_get_plugin(plugin_manager, content_type);
  g_free((void*)content_type);

  if (plugin == NULL) {
    girara_error("unknown file type\n");
    *error = ZATHURA_ERROR_UNKNOWN;
    goto error_free;
  }

  document = g_malloc0(sizeof(zathura_document_t));

  document->file_path   = real_path;
  document->basename    = g_path_get_basename(real_path);
  document->password    = password;
  document->scale       = 1.0;
  document->plugin      = plugin;
  document->adjust_mode = ZATHURA_ADJUST_NONE;
  document->cell_width  = 0.0;
  document->cell_height = 0.0;
  document->view_height = 0;
  document->view_width  = 0;
  document->position_x  = 0.0;
  document->position_y  = 0.0;

  /* open document */
  zathura_plugin_functions_t* functions = zathura_plugin_get_functions(plugin);
  if (functions->document_open == NULL) {
    girara_error("plugin has no open function\n");
    goto error_free;
  }

  zathura_error_t int_error = functions->document_open(document);
  if (int_error != ZATHURA_ERROR_OK) {
    if (error != NULL) {
      *error = int_error;
    }

    girara_error("could not open document\n");
    goto error_free;
  }

  /* read all pages */
  document->pages = calloc(document->number_of_pages, sizeof(zathura_page_t*));
  if (document->pages == NULL) {
    goto error_free;
  }

  for (unsigned int page_id = 0; page_id < document->number_of_pages; page_id++) {
    zathura_page_t* page = zathura_page_new(document, page_id, NULL);
    if (page == NULL) {
      goto error_free;
    }

    document->pages[page_id] = page;

    /* cell_width and cell_height is the maximum of all the pages width and height */
    const double width = zathura_page_get_width(page);
    if (document->cell_width < width)
      document->cell_width = width;

    const double height = zathura_page_get_height(page);
    if (document->cell_height < height)
      document->cell_height = height;
  }

  return document;

error_free:

  free(real_path);

  if (document != NULL && document->pages != NULL) {
    for (unsigned int page_id = 0; page_id < document->number_of_pages; page_id++) {
      zathura_page_free(document->pages[page_id]);
    }

    free(document->pages);
  }

  g_free(document);
  return NULL;
}

zathura_error_t
zathura_document_free(zathura_document_t* document)
{
  if (document == NULL || document->plugin == NULL) {
    return ZATHURA_ERROR_INVALID_ARGUMENTS;
  }

  /* free pages */
  for (unsigned int page_id = 0; page_id < document->number_of_pages; page_id++) {
    zathura_page_free(document->pages[page_id]);
    document->pages[page_id] = NULL;
  }

  free(document->pages);

  /* free document */
  zathura_error_t error = ZATHURA_ERROR_OK;
  zathura_plugin_functions_t* functions = zathura_plugin_get_functions(document->plugin);
  if (functions->document_free == NULL) {
    error = ZATHURA_ERROR_NOT_IMPLEMENTED;
  } else {
    error = functions->document_free(document, document->data);
  }

  if (document->file_path != NULL) {
    free(document->file_path);
  }
  g_free(document->basename);

  g_free(document);

  return error;
}

const char*
zathura_document_get_path(zathura_document_t* document)
{
  if (document == NULL) {
    return NULL;
  }

  return document->file_path;
}

const char*
zathura_document_get_basename(zathura_document_t* document)
{
  if (document == NULL) {
    return NULL;
  }

  return document->basename;
}

const char*
zathura_document_get_password(zathura_document_t* document)
{
  if (document == NULL) {
    return NULL;
  }

  return document->password;
}

zathura_page_t*
zathura_document_get_page(zathura_document_t* document, unsigned int index)
{
  if (document == NULL || document->pages == NULL || (document->number_of_pages <= index)) {
    return NULL;
  }

  return document->pages[index];
}

void*
zathura_document_get_data(zathura_document_t* document)
{
  if (document == NULL) {
    return NULL;
  }

  return document->data;
}

void
zathura_document_set_data(zathura_document_t* document, void* data)
{
  if (document == NULL) {
    return;
  }

  document->data = data;
}

unsigned int
zathura_document_get_number_of_pages(zathura_document_t* document)
{
  if (document == NULL) {
    return 0;
  }

  return document->number_of_pages;
}

void
zathura_document_set_number_of_pages(zathura_document_t* document, unsigned int number_of_pages)
{
  if (document == NULL) {
    return;
  }

  document->number_of_pages = number_of_pages;
}

unsigned int
zathura_document_get_current_page_number(zathura_document_t* document)
{
  if (document == NULL) {
    return 0;
  }

  return document->current_page_number;
}

void
zathura_document_set_current_page_number(zathura_document_t* document, unsigned int
    current_page)
{
  if (document == NULL) {
    return;
  }

  document->current_page_number = current_page;
}

double
zathura_document_get_position_x(zathura_document_t* document)
{
  if (document == NULL) {
    return 0;
  }

  return document->position_x;
}

double
zathura_document_get_position_y(zathura_document_t* document)
{
  if (document == NULL) {
    return 0;
  }

  return document->position_y;
}

void
zathura_document_set_position_x(zathura_document_t* document, double position_x)
{
  if (document == NULL) {
    return;
  }

  document->position_x = position_x;
}

void
zathura_document_set_position_y(zathura_document_t* document, double position_y)
{
  if (document == NULL) {
    return;
  }

  document->position_y = position_y;
}

double
zathura_document_get_scale(zathura_document_t* document)
{
  if (document == NULL) {
    return 0;
  }

  return document->scale;
}

void
zathura_document_set_scale(zathura_document_t* document, double scale)
{
  if (document == NULL) {
    return;
  }

  document->scale = scale;
}

unsigned int
zathura_document_get_rotation(zathura_document_t* document)
{
  if (document == NULL) {
    return 0;
  }

  return document->rotate;
}

void
zathura_document_set_rotation(zathura_document_t* document, unsigned int rotation)
{
  if (document == NULL) {
    return;
  }

  document->rotate = rotation % 360;

  if (document->rotate > 0 && document->rotate <= 90) {
    document->rotate = 90;
  } else if (document->rotate > 0 && document->rotate <= 180) {
    document->rotate = 180;
  } else if (document->rotate > 0 && document->rotate <= 270) {
    document->rotate = 270;
  } else {
    document->rotate = 0;
  }
}

zathura_adjust_mode_t
zathura_document_get_adjust_mode(zathura_document_t* document)
{
  if (document == NULL) {
    return ZATHURA_ADJUST_NONE;
  }

  return document->adjust_mode;
}

void
zathura_document_set_adjust_mode(zathura_document_t* document, zathura_adjust_mode_t mode)
{
  if (document == NULL) {
    return;
  }

  document->adjust_mode = mode;
}

int
zathura_document_get_page_offset(zathura_document_t* document)
{
  if (document == NULL) {
    return 0;
  }

  return document->page_offset;
}

void
zathura_document_set_page_offset(zathura_document_t* document, unsigned int page_offset)
{
  if (document == NULL) {
    return;
  }

  document->page_offset = page_offset;
}

void
zathura_document_set_viewport_width(zathura_document_t* document, unsigned int width)
{
  if (document == NULL) {
    return;
  }
  document->view_width = width;
}

void
zathura_document_set_viewport_height(zathura_document_t* document, unsigned int height)
{
  if (document == NULL) {
    return;
  }
  document->view_height = height;
}

void
zathura_document_get_viewport_size(zathura_document_t* document,
                                   unsigned int *height, unsigned int* width)
{
  g_return_if_fail(document != NULL && height != NULL && width != NULL);
  *height = document->view_height;
  *width = document->view_width;
}

void
zathura_document_get_cell_size(zathura_document_t* document,
                               unsigned int* height, unsigned int* width)
{
  g_return_if_fail(document != NULL && height != NULL && width != NULL);

  page_calc_height_width(document, document->cell_height, document->cell_width,
                         height, width, true);
}

void
zathura_document_get_document_size(zathura_document_t* document,
                                   unsigned int* height, unsigned int* width)
{
  g_return_if_fail(document != NULL && height != NULL && width != NULL);

  const unsigned int npag  = zathura_document_get_number_of_pages(document);
  const unsigned int ncol  = zathura_document_get_pages_per_row(document);
  const unsigned int c0    = zathura_document_get_first_page_column(document);
  const unsigned int nrow  = (npag + c0 - 1 + ncol - 1) / ncol;  /* number of rows */
  const unsigned int pad   = zathura_document_get_page_padding(document);

  unsigned int cell_height = 0;
  unsigned int cell_width  = 0;
  zathura_document_get_cell_size(document, &cell_height, &cell_width);

  *width  = ncol * cell_width + (ncol - 1) * pad;
  *height = nrow * cell_height + (nrow - 1) * pad;
}

void
zathura_document_set_page_layout(zathura_document_t* document, unsigned int page_padding,
                                 unsigned int pages_per_row, unsigned int first_page_column)
{
  g_return_if_fail(document != NULL);

  document->page_padding = page_padding;
  document->pages_per_row = pages_per_row;

  if (first_page_column < 1) {
    first_page_column = 1;
  } else if (first_page_column > pages_per_row) {
    first_page_column = ((first_page_column - 1) % pages_per_row) + 1;
  }

  document->first_page_column = first_page_column;
}

unsigned int
zathura_document_get_page_padding(zathura_document_t* document)
{
  if (document == NULL) {
    return 0;
  }
  return document->page_padding;
}

unsigned int
zathura_document_get_pages_per_row(zathura_document_t* document)
{
  if (document == NULL) {
    return 0;
  }
  return document->pages_per_row;
}

unsigned int
zathura_document_get_first_page_column(zathura_document_t* document)
{
  if (document == NULL) {
    return 0;
  }
  return document->first_page_column;
}

zathura_error_t
zathura_document_save_as(zathura_document_t* document, const char* path)
{
  if (document == NULL || document->plugin == NULL || path == NULL) {
    return ZATHURA_ERROR_UNKNOWN;
  }

  zathura_plugin_functions_t* functions = zathura_plugin_get_functions(document->plugin);
  if (functions->document_save_as == NULL) {
    return ZATHURA_ERROR_NOT_IMPLEMENTED;
  }

  return functions->document_save_as(document, document->data, path);
}

girara_tree_node_t*
zathura_document_index_generate(zathura_document_t* document, zathura_error_t* error)
{
  if (document == NULL || document->plugin == NULL) {
    if (error != NULL) {
      *error = ZATHURA_ERROR_INVALID_ARGUMENTS;
    }
    return NULL;
  }

  zathura_plugin_functions_t* functions = zathura_plugin_get_functions(document->plugin);
  if (functions->document_index_generate == NULL) {
    if (error != NULL) {
      *error = ZATHURA_ERROR_NOT_IMPLEMENTED;
    }
    return NULL;
  }

  return functions->document_index_generate(document, document->data, error);
}

girara_list_t*
zathura_document_attachments_get(zathura_document_t* document, zathura_error_t* error)
{
  if (document == NULL || document->plugin == NULL) {
    if (error != NULL) {
      *error = ZATHURA_ERROR_INVALID_ARGUMENTS;
    }
    return NULL;
  }

  zathura_plugin_functions_t* functions = zathura_plugin_get_functions(document->plugin);
  if (functions->document_attachments_get == NULL) {
    if (error != NULL) {
      *error = ZATHURA_ERROR_NOT_IMPLEMENTED;
    }
    return NULL;
  }

  return functions->document_attachments_get(document, document->data, error);
}

zathura_error_t
zathura_document_attachment_save(zathura_document_t* document, const char* attachment, const char* file)
{
  if (document == NULL || document->plugin == NULL) {
    return ZATHURA_ERROR_INVALID_ARGUMENTS;
  }

  zathura_plugin_functions_t* functions = zathura_plugin_get_functions(document->plugin);
  if (functions->document_attachment_save == NULL) {
    return ZATHURA_ERROR_NOT_IMPLEMENTED;
  }

  return functions->document_attachment_save(document, document->data, attachment, file);
}

girara_list_t*
zathura_document_get_information(zathura_document_t* document, zathura_error_t* error)
{
  if (document == NULL || document->plugin == NULL) {
    if (error != NULL) {
      *error = ZATHURA_ERROR_INVALID_ARGUMENTS;
    }
    return NULL;
  }

  zathura_plugin_functions_t* functions = zathura_plugin_get_functions(document->plugin);
  if (functions->document_get_information == NULL) {
    if (error != NULL) {
      *error = ZATHURA_ERROR_NOT_IMPLEMENTED;
    }
    return NULL;
  }

  girara_list_t* result = functions->document_get_information(document, document->data, error);
  if (result != NULL) {
    girara_list_set_free_function(result, (girara_free_function_t) zathura_document_information_entry_free);
  }

  return result;
}

#ifdef WITH_MAGIC
static const char*
guess_type_magic(const char* path) {
  const char* mime_type = NULL;

  /* creat magic cookie */
  const int flags =
    MAGIC_MIME_TYPE |
    MAGIC_SYMLINK |
    MAGIC_NO_CHECK_APPTYPE |
    MAGIC_NO_CHECK_CDF |
    MAGIC_NO_CHECK_ELF |
    MAGIC_NO_CHECK_ENCODING;
  magic_t magic = magic_open(flags);
  if (magic == NULL) {
    girara_debug("failed creating the magic cookie");
    goto cleanup;
  }

  /* ... and load mime database */
  if (magic_load(magic, NULL) < 0) {
    girara_debug("failed loading the magic database: %s", magic_error(magic));
    goto cleanup;
  }

  /* get the mime type */
  mime_type = magic_file(magic, path);
  if (mime_type == NULL) {
    girara_debug("failed guessing filetype: %s", magic_error(magic));
    goto cleanup;
  }
  /* dup so we own the memory */
  mime_type = g_strdup(mime_type);

  girara_debug("magic detected filetype: %s", mime_type);

cleanup:
  if (magic != NULL) {
    magic_close(magic);
  }

  return mime_type;
}

static const char*
guess_type_file(const char* UNUSED(path))
{
  return NULL;
}
#else
static const char*
guess_type_magic(const char* UNUSED(path)) {
  return NULL;
}

static const char*
guess_type_file(const char* path)
{
  GString* command = g_string_new("file -b --mime-type ");
  char* tmp        = g_shell_quote(path);

  g_string_append(command, tmp);
  g_free(tmp);

  GError* error = NULL;
  char* out = NULL;
  int ret = 0;
  g_spawn_command_line_sync(command->str, &out, NULL, &ret, &error);
  g_string_free(command, TRUE);
  if (error != NULL) {
    girara_warning("failed to execute command: %s", error->message);
    g_error_free(error);
    g_free(out);
    return NULL;
  }
  if (WEXITSTATUS(ret) != 0) {
    girara_warning("file failed with error code: %d", WEXITSTATUS(ret));
    g_free(out);
    return NULL;
  }

  g_strdelimit(out, "\n\r", '\0');
  return out;
}
#endif

static const char*
guess_type_glib(const char* path)
{
  gboolean uncertain = FALSE;
  const char* content_type = g_content_type_guess(path, NULL, 0, &uncertain);
  if (content_type == NULL) {
    girara_debug("g_content_type failed\n");
  } else {
    if (uncertain == FALSE) {
      girara_debug("g_content_type detected filetype: %s", content_type);
      return content_type;
    }
    girara_debug("g_content_type is uncertain, guess: %s", content_type);
  }

  FILE* f = fopen(path, "rb");
  if (f == NULL) {
    return NULL;
  }

  const int fd = fileno(f);
  guchar* content = NULL;
  size_t length = 0u;
  ssize_t bytes_read = -1;
  while (uncertain == TRUE && length < GT_MAX_READ && bytes_read != 0) {
    g_free((void*)content_type);
    content_type = NULL;

    content = g_realloc(content, length + BUFSIZ);
    bytes_read = read(fd, content + length, BUFSIZ);
    if (bytes_read == -1) {
      break;
    }

    length += bytes_read;
    content_type = g_content_type_guess(NULL, content, length, &uncertain);
    girara_debug("new guess: %s uncertain: %d, read: %zu", content_type, uncertain, length);
  }

  fclose(f);
  g_free(content);
  if (uncertain == FALSE) {
    return content_type;
  }

  g_free((void*)content_type);
  return NULL;
}

static const char*
guess_type(const char* path)
{
  /* try libmagic first */
  const char* content_type = guess_type_magic(path);
  if (content_type != NULL) {
    return content_type;
  }
  /* else fallback to g_content_type_guess method */
  content_type = guess_type_glib(path);
  if (content_type != NULL) {
    return content_type;
  }
  /* and if libmagic is not available, try file as last resort */
  return guess_type_file(path);
}

zathura_plugin_t*
zathura_document_get_plugin(zathura_document_t* document)
{
  if (document == NULL) {
    return NULL;
  }

  return document->plugin;
}
