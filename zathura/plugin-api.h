/* SPDX-License-Identifier: Zlib */

#ifndef PLUGIN_API_H
#define PLUGIN_API_H

#include <cairo.h>

#include "types.h"
#include "page.h"
#include "document.h"
#include "links.h"
#include "zathura-version.h"

typedef struct zathura_plugin_functions_s zathura_plugin_functions_t;

/**
 * Opens a document
 */
typedef zathura_error_t (*zathura_plugin_document_open_t)(zathura_document_t* document);

/**
 * Frees the document
 */
typedef zathura_error_t (*zathura_plugin_document_free_t)(zathura_document_t* document, void* data);

/**
 * Generates the document index
 */
typedef girara_tree_node_t* (*zathura_plugin_document_index_generate_t)(zathura_document_t* document, void* data, zathura_error_t* error);

/**
 * Save the document
 */
typedef zathura_error_t (*zathura_plugin_document_save_as_t)(zathura_document_t* document, void* data, const char* path);

/**
 * Get list of attachments
 */
typedef girara_list_t* (*zathura_plugin_document_attachments_get_t)(zathura_document_t* document, void* data, zathura_error_t* error);

/**
 * Save attachment to a file
 */
typedef zathura_error_t (*zathura_plugin_document_attachment_save_t)(zathura_document_t* document, void* data, const char* attachment, const char* file);

/**
 * Get document information
 */
typedef girara_list_t* (*zathura_plugin_document_get_information_t)(zathura_document_t* document, void* data, zathura_error_t* error);

/**
 * Gets the page object
 */
typedef zathura_error_t (*zathura_plugin_page_init_t)(zathura_page_t* page);

/**
 * Free page
 */
typedef zathura_error_t (*zathura_plugin_page_clear_t)(zathura_page_t* page, void* data);

/**
 * Search text
 */
typedef girara_list_t* (*zathura_plugin_page_search_text_t)(zathura_page_t* page, void* data, const char* text, zathura_error_t* error);

/**
 * Get links on a page
 */
typedef girara_list_t* (*zathura_plugin_page_links_get_t)(zathura_page_t* page, void* data, zathura_error_t* error);

/**
 * Get form fields
 */
typedef girara_list_t* (*zathura_plugin_page_form_fields_get_t)(zathura_page_t* page, void* data, zathura_error_t* error);

/**
 * Get list of images
 */
typedef girara_list_t* (*zathura_plugin_page_images_get_t)(zathura_page_t* page, void* data, zathura_error_t* error);

/**
 * Get the image
 */
typedef cairo_surface_t* (*zathura_plugin_page_image_get_cairo_t)(zathura_page_t* page, void* data, zathura_image_t* image, zathura_error_t* error);

/**
 * Get text for selection
 */
typedef char* (*zathura_plugin_page_get_text_t)(zathura_page_t* page, void* data, zathura_rectangle_t rectangle, zathura_error_t* error);

/**
 * Get rectangles from selection
 */
typedef girara_list_t* (*zathura_plugin_page_get_selection_t)(zathura_page_t* page, void* data, zathura_rectangle_t rectangle, zathura_error_t* error);

/**
 * Renders the page
 */
typedef zathura_image_buffer_t* (*zathura_plugin_page_render_t)(zathura_page_t* page, void* data, zathura_error_t* error);

/**
 * Renders the page to a cairo surface.
 */
typedef zathura_error_t (*zathura_plugin_page_render_cairo_t)(zathura_page_t* page, void* data, cairo_t* cairo, bool printing);

/**
 * Get page label.
 */
typedef zathura_error_t (*zathura_plugin_page_get_label_t)(zathura_page_t* page, void* data, char** label);

/**
 * Get signatures
 */
typedef girara_list_t* (*zathura_plugin_page_get_signatures)(zathura_page_t* page, void* data, zathura_error_t* error);

struct zathura_plugin_functions_s
{
  /**
   * Opens a document
   */
  zathura_plugin_document_open_t document_open;

  /**
   * Frees the document
   */
  zathura_plugin_document_free_t document_free;

  /**
   * Generates the document index
   */
  zathura_plugin_document_index_generate_t document_index_generate;

  /**
   * Save the document
   */
  zathura_plugin_document_save_as_t document_save_as;

  /**
   * Get list of attachments
   */
  zathura_plugin_document_attachments_get_t document_attachments_get;

  /**
   * Save attachment to a file
   */
  zathura_plugin_document_attachment_save_t document_attachment_save;

  /**
   * Get document information
   */
  zathura_plugin_document_get_information_t document_get_information;

  /**
   * Gets the page object
   */
  zathura_plugin_page_init_t page_init;

  /**
   * Free page
   */
  zathura_plugin_page_clear_t page_clear;

  /**
   * Search text
   */
  zathura_plugin_page_search_text_t page_search_text;

  /**
   * Get links on a page
   */
  zathura_plugin_page_links_get_t page_links_get;

  /**
   * Get form fields
   */
  zathura_plugin_page_form_fields_get_t page_form_fields_get;

  /**
   * Get list of images
   */
  zathura_plugin_page_images_get_t page_images_get;

  /**
   * Get the image
   */
  zathura_plugin_page_image_get_cairo_t page_image_get_cairo;

  /**
   * Get text for selection
   */
  zathura_plugin_page_get_text_t page_get_text;

  /**
   * Get text for selection
   */
  zathura_plugin_page_get_selection_t page_get_selection;

  /**
   * Renders the page
   */
  zathura_plugin_page_render_t page_render;

  /**
   * Renders the page to a cairo surface.
   */
  zathura_plugin_page_render_cairo_t page_render_cairo;

  /**
   * Get page label.
   */
  zathura_plugin_page_get_label_t page_get_label;

  /**
   * Get signatures.
   */
  zathura_plugin_page_get_signatures page_get_signatures;
};

typedef struct zathura_plugin_version_s {
  unsigned int major; /**< Major */
  unsigned int minor; /**< Minor */
  unsigned int rev;   /**< Revision */
} zathura_plugin_version_t;

typedef struct zathura_plugin_definition_s {
  const char* name;
  const zathura_plugin_version_t version;
  zathura_plugin_functions_t functions;
  const size_t mime_types_size;
  const char** mime_types;
} zathura_plugin_definition_t;

#define JOIN(x, y) JOIN2(x, y)
#define JOIN2(x, y) x ## _ ## y

#define ZATHURA_PLUGIN_DEFINITION_SYMBOL \
  JOIN(zathura_plugin, JOIN(ZATHURA_API_VERSION, ZATHURA_ABI_VERSION))

/**
 * Register a plugin.
 *
 * @param plugin_name the name of the plugin
 * @param major the plugin's major version
 * @param minor the plugin's minor version
 * @param rev the plugin's revision
 * @param plugin_functions function to register the plugin's document functions
 * @param mimetypes a char array of mime types supported by the plugin
 */
#define ZATHURA_PLUGIN_REGISTER_WITH_FUNCTIONS(plugin_name, major, minor, rev, plugin_functions, mimetypes) \
  static const char* zathura_plugin_mime_types[] = mimetypes; \
  \
  ZATHURA_PLUGIN_API const zathura_plugin_definition_t ZATHURA_PLUGIN_DEFINITION_SYMBOL = { \
    .name = plugin_name, \
    .version = { major, minor, rev }, \
    .functions = plugin_functions, \
    .mime_types_size = sizeof(zathura_plugin_mime_types) / sizeof(zathura_plugin_mime_types[0]), \
    .mime_types = zathura_plugin_mime_types \
  }; \


#define ZATHURA_PLUGIN_MIMETYPES(...) __VA_ARGS__
#define ZATHURA_PLUGIN_FUNCTIONS(...) __VA_ARGS__

#endif // PLUGIN_API_H
