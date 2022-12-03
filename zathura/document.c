/* SPDX-License-Identifier: Zlib */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <glib.h>
#include <gio/gio.h>
#include <math.h>

#include <girara/datastructures.h>
#include <girara/utils.h>

#include "adjustment.h"
#include "document.h"
#include "utils.h"
#include "zathura.h"
#include "page.h"
#include "plugin.h"
#include "content-type.h"

/**
 * Document
 */
struct zathura_document_s {
  char* file_path; /**< File path of the document */
  char* uri; /**< URI of the document */
  char* basename; /**< Basename of the document */
  uint8_t hash_sha256[32]; /**< SHA256 hash of the document */
  const char* password; /**< Password of the document */
  unsigned int current_page_number; /**< Current page number */
  unsigned int number_of_pages; /**< Number of pages */
  double zoom; /**< Zoom value */
  unsigned int rotate; /**< Rotation */
  void* data; /**< Custom data */
  zathura_adjust_mode_t adjust_mode; /**< Adjust mode (best-fit, width) */
  int page_offset; /**< Page offset */
  double cell_width; /**< width of a page cell in the document (not transformed by scale and rotation) */
  double cell_height; /**< height of a page cell in the document (not transformed by scale and rotation) */
  unsigned int view_width; /**< width of current viewport */
  unsigned int view_height; /**< height of current viewport */
  double view_ppi; /**< PPI of the current viewport */
  zathura_device_factors_t device_factors; /**< x and y device scale factors (for e.g. HiDPI) */
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

static void
check_set_error(zathura_error_t* error, zathura_error_t code) {
  if (error != NULL) {
    *error = code;
  }
}

static bool
hash_file_sha256(uint8_t* dst, const char* path)
{
  FILE* f = fopen(path, "rb");
  if (f == NULL) {
    return false;
  }

  GChecksum* checksum = g_checksum_new(G_CHECKSUM_SHA256);
  if (checksum == NULL) {
    fclose(f);
    return false;
  }

  uint8_t buf[BUFSIZ];
  size_t read;
  while ((read = fread(buf, 1, sizeof(buf), f)) != 0) {
    g_checksum_update(checksum, buf, read);
  }

  if (ferror(f) != 0) {
    g_checksum_free(checksum);
    fclose(f);
    return false;
  }

  fclose(f);
  gsize dst_size = 32;
  g_checksum_get_digest(checksum, dst, &dst_size);
  g_checksum_free(checksum);
  return true;
}

zathura_document_t*
zathura_document_open(zathura_t* zathura, const char* path, const char* uri,
                      const char* password, zathura_error_t* error)
{
  if (zathura == NULL || path == NULL) {
    return NULL;
  }

  GFile* file = g_file_new_for_path(path);
  char* real_path = NULL;
  char* content_type = NULL;
  zathura_plugin_t* plugin = NULL;
  zathura_document_t* document = NULL;

  if (file == NULL) {
    girara_error("Error while handling path '%s'.", path);
    check_set_error(error, ZATHURA_ERROR_UNKNOWN);
    goto error_free;
  }

  real_path = g_file_get_path(file);
  if (real_path == NULL) {
    girara_error("Error while handling path '%s'.", path);
    check_set_error(error, ZATHURA_ERROR_UNKNOWN);
    goto error_free;
  }

  content_type = zathura_content_type_guess(zathura->content_type_context, real_path, zathura_plugin_manager_get_content_types(zathura->plugins.manager));
  if (content_type == NULL) {
    girara_error("Could not determine file type.");
    check_set_error(error, ZATHURA_ERROR_UNKNOWN);
    goto error_free;
  }

  plugin = zathura_plugin_manager_get_plugin(zathura->plugins.manager, content_type);
  if (plugin == NULL) {
    girara_error("Unknown file type: '%s'", content_type);
    check_set_error(error, ZATHURA_ERROR_UNKNOWN);

    g_free((void*)content_type);
    content_type = NULL;

    goto error_free;
  }

  g_free(content_type);
  content_type = NULL;

  document = g_try_malloc0(sizeof(zathura_document_t));
  if (document == NULL) {
    check_set_error(error, ZATHURA_ERROR_OUT_OF_MEMORY);
    goto error_free;
  }

  document->file_path   = real_path;
  document->uri         = g_strdup(uri);
  if (document->uri == NULL) {
    document->basename    = g_file_get_basename(file);
  } else {
    GFile*gf = g_file_new_for_uri(document->uri);
    document->basename = g_file_get_basename(gf);
    g_object_unref(gf);
  }
  hash_file_sha256(document->hash_sha256, document->file_path);
  document->password    = password;
  document->zoom        = 1.0;
  document->plugin      = plugin;
  document->adjust_mode = ZATHURA_ADJUST_NONE;
  document->cell_width  = 0.0;
  document->cell_height = 0.0;
  document->view_height = 0;
  document->view_width  = 0;
  document->view_ppi    = 0.0;
  document->device_factors.x = 1.0;
  document->device_factors.y = 1.0;
  document->position_x  = 0.0;
  document->position_y  = 0.0;

  real_path = NULL;
  g_object_unref(file);
  file = NULL;

  /* open document */
  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(plugin);
  if (functions->document_open == NULL) {
    girara_error("plugin has no open function\n");
    goto error_free;
  }

  zathura_error_t int_error = functions->document_open(document);
  if (int_error != ZATHURA_ERROR_OK) {
    check_set_error(error, int_error);
    girara_error("could not open document\n");
    goto error_free;
  }

  /* read all pages */
  document->pages = calloc(document->number_of_pages, sizeof(zathura_page_t*));
  if (document->pages == NULL) {
    check_set_error(error, ZATHURA_ERROR_OUT_OF_MEMORY);
    goto error_free;
  }

  for (unsigned int page_id = 0; page_id < document->number_of_pages; page_id++) {
    zathura_page_t* page = zathura_page_new(document, page_id, NULL);
    if (page == NULL) {
      check_set_error(error, ZATHURA_ERROR_OUT_OF_MEMORY);
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

  if (file != NULL) {
    g_object_unref(file);
  }

  g_free(real_path);

  if (document != NULL) {
    zathura_document_free(document);
    document = NULL; /* prevent double-free */
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

  if (document->pages != NULL) {
    /* free pages */
    for (unsigned int page_id = 0; page_id < document->number_of_pages; page_id++) {
      zathura_page_free(document->pages[page_id]);
      document->pages[page_id] = NULL;
    }
    free(document->pages);
  }

  /* free document */
  zathura_error_t error = ZATHURA_ERROR_OK;
  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(document->plugin);
  if (functions->document_free == NULL) {
    error = ZATHURA_ERROR_NOT_IMPLEMENTED;
  } else {
    error = functions->document_free(document, document->data);
  }

  g_free(document->file_path);
  g_free(document->uri);
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

const uint8_t*
zathura_document_get_hash(zathura_document_t* document)
{
  if (document == NULL) {
    return NULL;
  }

  return document->hash_sha256;
}

const char*
zathura_document_get_uri(zathura_document_t* document)
{
  if (document == NULL) {
    return NULL;
  }

  return document->uri;
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
zathura_document_get_zoom(zathura_document_t* document)
{
  if (document == NULL) {
    return 0;
  }

  return document->zoom;
}

void
zathura_document_set_zoom(zathura_document_t* document, double zoom)
{
  if (document == NULL) {
    return;
  }

  document->zoom = zoom;
}

double
zathura_document_get_scale(zathura_document_t* document)
{
  if (document == NULL) {
    return 0;
  }

  double ppi = document->view_ppi;
  if (ppi < DBL_EPSILON) {
    /* No PPI information -> use a typical value */
    ppi = 100;
  }

  /* scale = pixels per point, and there are 72 points in one inch */
  return document->zoom * ppi / 72.0;
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

  rotation = rotation % 360;
  if (rotation == 0 || rotation > 270) {
    document->rotate = 0;
  } else if (rotation <= 90) {
    document->rotate = 90;
  } else if (rotation <= 180) {
    document->rotate = 180;
  } else {
    document->rotate = 270;
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
zathura_document_set_viewport_ppi(zathura_document_t* document, double ppi)
{
  if (document == NULL) {
    return;
  }
  document->view_ppi = ppi;
}

void
zathura_document_get_viewport_size(zathura_document_t* document,
                                   unsigned int *height, unsigned int* width)
{
  g_return_if_fail(document != NULL && height != NULL && width != NULL);
  *height = document->view_height;
  *width = document->view_width;
}

double
zathura_document_get_viewport_ppi(zathura_document_t* document)
{
  if (document == NULL) {
    return 0.0;
  }
  return document->view_ppi;
}

void
zathura_document_set_device_factors(zathura_document_t* document,
                                    double x_factor, double y_factor)
{
  if (document == NULL) {
    return;
  }
  if (fabs(x_factor) < DBL_EPSILON || fabs(y_factor) < DBL_EPSILON) {
    girara_debug("Ignoring new device factors %0.2f and %0.2f: too small",
        x_factor, y_factor);
    return;
  }

  document->device_factors.x = x_factor;
  document->device_factors.y = y_factor;
}

zathura_device_factors_t
zathura_document_get_device_factors(zathura_document_t* document)
{
  if (document == NULL) {
    /* The function is guaranteed to not return zero values */
    return (zathura_device_factors_t){1.0, 1.0};
  }

  return document->device_factors;
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

  const unsigned int npag = zathura_document_get_number_of_pages(document);
  const unsigned int ncol = zathura_document_get_pages_per_row(document);

  if (npag == 0 || ncol == 0) {
    return;
  }

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
zathura_document_set_cell_size(zathura_document_t* document, unsigned int cell_height,
                               unsigned int cell_width)
{
  document->cell_width = cell_width;
  document->cell_height = cell_height;
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

  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(document->plugin);
  if (functions->document_save_as == NULL) {
    return ZATHURA_ERROR_NOT_IMPLEMENTED;
  }

  return functions->document_save_as(document, document->data, path);
}

girara_tree_node_t*
zathura_document_index_generate(zathura_document_t* document, zathura_error_t* error)
{
  if (document == NULL || document->plugin == NULL) {
    check_set_error(error, ZATHURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(document->plugin);
  if (functions->document_index_generate == NULL) {
    check_set_error(error, ZATHURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  return functions->document_index_generate(document, document->data, error);
}

girara_list_t*
zathura_document_attachments_get(zathura_document_t* document, zathura_error_t* error)
{
  if (document == NULL || document->plugin == NULL) {
    check_set_error(error, ZATHURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(document->plugin);
  if (functions->document_attachments_get == NULL) {
    check_set_error(error, ZATHURA_ERROR_NOT_IMPLEMENTED);
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

  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(document->plugin);
  if (functions->document_attachment_save == NULL) {
    return ZATHURA_ERROR_NOT_IMPLEMENTED;
  }

  return functions->document_attachment_save(document, document->data, attachment, file);
}

girara_list_t*
zathura_document_get_information(zathura_document_t* document, zathura_error_t* error)
{
  if (document == NULL || document->plugin == NULL) {
    check_set_error(error, ZATHURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(document->plugin);
  if (functions->document_get_information == NULL) {
    check_set_error(error, ZATHURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  girara_list_t* result = functions->document_get_information(document, document->data, error);
  if (result != NULL) {
    girara_list_set_free_function(result, (girara_free_function_t) zathura_document_information_entry_free);
  }

  return result;
}

zathura_plugin_t*
zathura_document_get_plugin(zathura_document_t* document)
{
  if (document == NULL) {
    return NULL;
  }

  return document->plugin;
}
