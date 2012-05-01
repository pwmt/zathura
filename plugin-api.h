/* See LICENSE file for license and copyright information */

#ifndef PLUGIN_API_H
#define PLUGIN_API_H

#include "page.h"
#include "annotations.h"
#include "document.h"
#include "version.h"

typedef struct zathura_plugin_functions_s zathura_plugin_functions_t;

/**
 * Functions register function
 *
 * @param functions The functions struct
 */
typedef void (*zathura_plugin_register_function_t)(zathura_plugin_functions_t* functions);

/**
 * Sets the functions register function of the plugin
 *
 * @param plugin The plugin
 * @param register_function The register function that registers the document
 * functions
 */
void zathura_plugin_set_register_functions_function(zathura_plugin_t* plugin,
    zathura_plugin_register_function_t register_function);

/**
 * Sets the functions register function of the plugin
 *
 * @param plugin The plugin
 * @param mime_type The mime type that should be added
 */
void zathura_plugin_add_mimetype(zathura_plugin_t* plugin, const char* mime_type);

/**
 * Register a plugin.
 *
 * @param plugin_name the name of the plugin
 * @param major the plugin's major version
 * @param minor the plugin's minor version
 * @param rev the plugin's revision
 * @param register_functions function to register the plugin's document functions
 * @param mimetypes a char array of mime types supported by the plugin
 */
#define ZATHURA_PLUGIN_REGISTER(plugin_name, major, minor, rev, register_functions, mimetypes) \
  unsigned int zathura_plugin_version_major(void) { return major; } \
  unsigned int zathura_plugin_version_minor(void) { return minor; } \
  unsigned int zathura_plugin_version_revision(void) { return rev; } \
  unsigned int zathura_plugin_api_version(void) { return ZATHURA_API_VERSION; } \
  unsigned int zathura_plugin_abi_version(void) { return ZATHURA_ABI_VERSION; } \
  \
  void zathura_plugin_register(zathura_plugin_t* plugin) \
  { \
    if (plugin == NULL) { \
      return; \
    } \
    zathura_plugin_set_register_functions_function(plugin, register_functions); \
    static const char* mime_types[] = mimetypes; \
    for (size_t s = 0; s != sizeof(mime_types) / sizeof(const char*); ++s) { \
      zathura_plugin_add_mimetype(plugin, mime_types[s]); \
    } \
  } \

#define ZATHURA_PLUGIN_MIMETYPES(...) __VA_ARGS__

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
 * Renders the page
 */
typedef zathura_image_buffer_t* (*zathura_plugin_page_render_t)(zathura_page_t* page, void* data, zathura_error_t* error);

/**
 * Renders the page
 */
typedef zathura_error_t (*zathura_plugin_page_render_cairo_t)(zathura_page_t* page, void* data, cairo_t* cairo, bool printing);


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
   * Renders the page
   */
  zathura_plugin_page_render_t page_render;

  /**
   * Renders the page
   */
  zathura_plugin_page_render_cairo_t page_render_cairo;
};


#endif // PLUGIN_API_H
