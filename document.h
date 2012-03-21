/* See LICENSE file for license and copyright information */

#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <cairo.h>
#include <gtk/gtk.h>
#include <stdbool.h>

#include <girara/types.h>
#include "zathura.h"
#include "version.h"

#define PLUGIN_REGISTER_FUNCTION "plugin_register"
#define PLUGIN_API_VERSION_FUNCTION "plugin_api_version"
#define PLUGIN_ABI_VERSION_FUNCTION "plugin_abi_version"

/**
 * Register a plugin.
 *
 * @param plugin_name the name of the plugin
 * @param major the plugin's major version
 * @param minor the plugin's minor version
 * @param rev the plugin's revision
 * @param plugin_open_function the plugin's open function
 * @param mimetypes a char array of mime types supported by the plugin
 */
#define PLUGIN_REGISTER(plugin_name, major, minor, rev, plugin_open_function, mimetypes) \
  unsigned int plugin_version_major() { return major; } \
  unsigned int plugin_version_minor() { return minor; } \
  unsigned int plugin_version_revision() { return rev; } \
  unsigned int plugin_api_version() { return ZATHURA_API_VERSION; } \
  unsigned int plugin_abi_version() { return ZATHURA_ABI_VERSION; } \
  \
  void plugin_register(zathura_document_plugin_t* plugin) \
  { \
    if (plugin == NULL) { \
      return; \
    } \
    plugin->open_function  = (plugin_open_function); \
    static const char* mime_types[] = mimetypes; \
    for (size_t s = 0; s != sizeof(mime_types) / sizeof(const char*); ++s) { \
      girara_list_append(plugin->content_types, g_content_type_from_mime_type(mime_types[s])); \
    } \
  } \

#define PLUGIN_MIMETYPES(...) __VA_ARGS__

/**
 * Error types for plugins
 */
typedef enum zathura_plugin_error_e
{
  ZATHURA_PLUGIN_ERROR_OK, /**< No error occured */
  ZATHURA_PLUGIN_ERROR_UNKNOWN, /**< An unknown error occured */
  ZATHURA_PLUGIN_ERROR_OUT_OF_MEMORY, /**< Out of memory */
  ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED, /**< The called function has not been implemented */
  ZATHURA_PLUGIN_ERROR_INVALID_ARGUMENTS, /**< Invalid arguments have been passed */
  ZATHURA_PLUGIN_ERROR_INVALID_PASSWORD /**< The provided password is invalid */
} zathura_plugin_error_t;

/**
 * Document open function
 *
 * @param document The document
 * @return true if no error occured otherwise false
 */
typedef zathura_plugin_error_t (*zathura_document_open_t)(zathura_document_t* document);

/**
 * Document plugin structure
 */
typedef struct zathura_document_plugin_s
{
  girara_list_t* content_types; /**< List of supported content types */
  zathura_document_open_t open_function; /**< Document open function */
  void* handle; /**< DLL handle */
} zathura_document_plugin_t;

/**
 * Plugin mapping
 */
typedef struct zathura_type_plugin_mapping_s
{
  const gchar* type; /**< Plugin type */
  zathura_document_plugin_t* plugin; /**< Mapped plugin */
} zathura_type_plugin_mapping_t;

/**
 * Meta data entries
 */
typedef enum zathura_document_meta_e
{
  ZATHURA_DOCUMENT_TITLE, /**< Title of the document */
  ZATHURA_DOCUMENT_AUTHOR, /**< Author of the document */
  ZATHURA_DOCUMENT_SUBJECT, /**< Subject of the document */
  ZATHURA_DOCUMENT_KEYWORDS, /**< Keywords of the document */
  ZATHURA_DOCUMENT_CREATOR, /**< Creator of the document */
  ZATHURA_DOCUMENT_PRODUCER, /**< Producer of the document */
  ZATHURA_DOCUMENT_CREATION_DATE, /**< Creation data */
  ZATHURA_DOCUMENT_MODIFICATION_DATE /**< Modification data */
} zathura_document_meta_t;

typedef struct zathura_password_dialog_info_s
{
  char* path; /**< Path to the file */
  zathura_t* zathura;  /**< Zathura session */
} zathura_password_dialog_info_t;

/**
 * Function prototype that is called to register a document plugin
 *
 * @param The document plugin
 */
typedef void (*zathura_plugin_register_service_t)(zathura_document_plugin_t*);

/**
 * Function prototype that is called to get the plugin's API version.
 *
 * @return plugin's API version
 */
typedef unsigned int (*zathura_plugin_api_version_t)();

/**
 * Function prototype that is called to get the ABI version the plugin is built
 * against.
 *
 * @return plugin's ABI version
 */
typedef unsigned int (*zathura_plugin_abi_version_t)();

/**
 * Image buffer
 */
typedef struct zathura_image_buffer_s
{
  unsigned char* data; /**< Image buffer data */
  unsigned int height; /**< Height of the image */
  unsigned int width; /**< Width of the image */
  unsigned int rowstride; /**< Rowstride of the image */
} zathura_image_buffer_t;

/**
 * Creates an image buffer
 *
 * @param width Width of the image stored in the buffer
 * @param height Height of the image stored in the buffer
 * @return Image buffer or NULL if an error occured
 */
zathura_image_buffer_t* zathura_image_buffer_create(unsigned int width, unsigned int height);

/**
 * Frees the image buffer
 *
 * @param buffer The image buffer
 */
void zathura_image_buffer_free(zathura_image_buffer_t* buffer);

/**
 * Rectangle structure.
 * The coordinate system has its origin in the left upper corner. The x axes
 * goes to the right, the y access goes down.
 */
typedef struct zathura_rectangle_s
{
  double x1; /**< X coordinate of point 1 */
  double y1; /**< Y coordinate of point 1 */
  double x2; /**< X coordinate of point 2 */
  double y2; /**< Y coordinate of point 2 */
} zathura_rectangle_t;

/**
 * Image structure
 */
typedef struct zathura_image_s
{
  zathura_rectangle_t position; /**< Coordinates of the image */
  void* data; /**< Custom data of the plugin */
} zathura_image_t;

/**
 * Possible link types
 */
typedef enum zathura_link_type_e
{
  ZATHURA_LINK_TO_PAGE, /**< Links to a page */
  ZATHURA_LINK_EXTERNAL, /**< Links to an external source */
} zathura_link_type_t;

/**
 * Link
 */
typedef struct zathura_link_s
{
  zathura_rectangle_t position; /**< Position of the link */
  zathura_link_type_t type; /**< Link type */
  union
  {
    unsigned int page_number; /**< Page number */
    char* value; /**< Value */
  } target;
} zathura_link_t;

/**
 * Index element
 */
typedef struct zathura_index_element_s
{
  char* title; /**< Title of the element */
  zathura_link_type_t type; /**< Type */
  union
  {
    unsigned int page_number; /**< Page number */
    char* uri; /**< Uri */
  } target;
} zathura_index_element_t;

/**
 * Form type
 */
typedef enum zathura_form_type_e
{
  ZATHURA_FORM_CHECKBOX, /**< Checkbox */
  ZATHURA_FORM_TEXTFIELD /**< Textfield */
} zathura_form_type_t;

/**
 * Form element
 */
typedef struct zathura_form_s
{
  zathura_rectangle_t position; /**< Position */
  zathura_form_type_t type; /**< Type */
} zathura_form_t;

/**
 * Page
 */
struct zathura_page_s
{
  double height; /**< Page height */
  double width; /**< Page width */
  unsigned int number; /**< Page number */
  zathura_document_t* document; /**< Document */
  void* data; /**< Custom data */
  bool visible; /**< Page is visible */
  GtkWidget* drawing_area; /**< Drawing area */
};

/**
 * Document
 */
struct zathura_document_s
{
  char* file_path; /**< File path of the document */
  const char* password; /**< Password of the document */
  unsigned int current_page_number; /**< Current page number */
  unsigned int number_of_pages; /**< Number of pages */
  double scale; /**< Scale value */
  int rotate; /**< Rotation */
  void* data; /**< Custom data */
  zathura_t* zathura; /** Zathura object */
  int adjust_mode; /**< Adjust mode (best-fit, width) */

  struct
  {
    /**
     * Frees the document
     */
    zathura_plugin_error_t (*document_free)(zathura_document_t* document);

    /**
     * Generates the document index
     */
    girara_tree_node_t* (*document_index_generate)(zathura_document_t* document, zathura_plugin_error_t* error);

    /**
     * Save the document
     */
    zathura_plugin_error_t (*document_save_as)(zathura_document_t* document, const char* path);

    /**
     * Get list of attachments
     */
    girara_list_t* (*document_attachments_get)(zathura_document_t* document, zathura_plugin_error_t* error);

    /**
     * Save attachment to a file
     */
    zathura_plugin_error_t (*document_attachment_save)(zathura_document_t* document, const char* attachment, const char* file);

    /**
     * Get document information
     */
    char* (*document_meta_get)(zathura_document_t* document, zathura_document_meta_t info, zathura_plugin_error_t* error);

    /**
     * Gets the page object
     */
    zathura_page_t* (*page_get)(zathura_document_t* document, unsigned int page_id, zathura_plugin_error_t* error);

    /**
     * Search text
     */
    girara_list_t* (*page_search_text)(zathura_page_t* page, const char* text, zathura_plugin_error_t* error);

    /**
     * Get links on a page
     */
    girara_list_t* (*page_links_get)(zathura_page_t* page, zathura_plugin_error_t* error);

    /**
     * Get form fields
     */
    girara_list_t* (*page_form_fields_get)(zathura_page_t* page, zathura_plugin_error_t* error);

    /**
     * Get list of images
     */
    girara_list_t* (*page_images_get)(zathura_page_t* page, zathura_plugin_error_t* error);

    /**
     * Get the image
     */
    cairo_surface_t* (*page_image_get_cairo)(zathura_page_t* page, zathura_image_t* image, zathura_plugin_error_t* error);

    /**
     * Get text for selection
     */
    char* (*page_get_text)(zathura_page_t* page, zathura_rectangle_t rectangle, zathura_plugin_error_t* error);

    /**
     * Renders the page
     */
    zathura_image_buffer_t* (*page_render)(zathura_page_t* page, zathura_plugin_error_t* error);

    /**
     * Renders the page
     */
    zathura_plugin_error_t (*page_render_cairo)(zathura_page_t* page, cairo_t* cairo, bool printing);

    /**
     * Free page
     */
    zathura_plugin_error_t (*page_free)(zathura_page_t* page);
  } functions;

  /**
   * Document pages
   */
  zathura_page_t** pages;
};

/**
 * Load all document plugins
 *
 * @param zathura the zathura session
 */
void zathura_document_plugins_load(zathura_t* zathura);

/**
 * Free a document plugin
 *
 * @param plugin The plugin
 */
void zathura_document_plugin_free(zathura_document_plugin_t* plugin);

/**
 * Open the document
 *
 * @param zathura Zathura object
 * @param path Path to the document
 * @param password Password of the document or NULL
 * @return The document object
 */
zathura_document_t* zathura_document_open(zathura_t* zathura, const char* path,
    const char* password);

/**
 * Free the document
 *
 * @param document
 * @return ZATHURA_PLUGIN_ERROR_OK when no error occured, otherwise see
 *    zathura_plugin_error_t
 */
zathura_plugin_error_t zathura_document_free(zathura_document_t* document);

/**
 * Save the document
 *
 * @param document The document object
 * @param path Path for the saved file
 * @return ZATHURA_PLUGIN_ERROR_OK when no error occured, otherwise see
 *    zathura_plugin_error_t
 */
zathura_plugin_error_t zathura_document_save_as(zathura_document_t* document, const char* path);

/**
 * Generate the document index
 *
 * @param document The document object
 * @param error Set to an error value (see \ref zathura_plugin_error_t) if an
 *   error occured
 * @return Generated index
 */

girara_tree_node_t* zathura_document_index_generate(zathura_document_t* document, zathura_plugin_error_t* error);

/**
 * Get list of attachments
 *
 * @param document The document object
 * @param error Set to an error value (see \ref zathura_plugin_error_t) if an
 *   error occured
 * @return List of attachments
 */
girara_list_t* zathura_document_attachments_get(zathura_document_t* document, zathura_plugin_error_t* error);

/**
 * Save document attachment
 *
 * @param document The document objects
 * @param attachment name of the attachment
 * @param file the target filename
 * @return ZATHURA_PLUGIN_ERROR_OK when no error occured, otherwise see
 *    zathura_plugin_error_t
 */
zathura_plugin_error_t zathura_document_attachment_save(zathura_document_t* document, const char* attachment, const char* file);

/**
 * Returns a string of the requested information
 *
 * @param document The zathura document
 * @param meta The information field
 * @param error Set to an error value (see \ref zathura_plugin_error_t) if an
 *   error occured
 * @return String or NULL if information could not be retreived
 */
char* zathura_document_meta_get(zathura_document_t* document, zathura_document_meta_t meta, zathura_plugin_error_t* error);

/**
 * Get the page object
 *
 * @param document The document
 * @param page_id Page number
 * @param error Set to an error value (see \ref zathura_plugin_error_t) if an
 *   error occured
 * @return Page object or NULL if an error occured
 */
zathura_page_t* zathura_page_get(zathura_document_t* document, unsigned int page_id, zathura_plugin_error_t* error);

/**
 * Frees the page object
 *
 * @param page The page object
 * @return ZATHURA_PLUGIN_ERROR_OK when no error occured, otherwise see
 *    zathura_plugin_error_t
 */
zathura_plugin_error_t zathura_page_free(zathura_page_t* page);

/**
 * Search page
 *
 * @param page The page object
 * @param text Search item
 * @param error Set to an error value (see \ref zathura_plugin_error_t) if an
 *   error occured
 * @return List of results
 */
girara_list_t* zathura_page_search_text(zathura_page_t* page, const char* text, zathura_plugin_error_t* error);

/**
 * Get page links
 *
 * @param page The page object
 * @param error Set to an error value (see \ref zathura_plugin_error_t) if an
 *   error occured
 * @return List of links
 */
girara_list_t* zathura_page_links_get(zathura_page_t* page, zathura_plugin_error_t* error);

/**
 * Free page links
 *
 * @param list List of links
 * @return ZATHURA_PLUGIN_ERROR_OK when no error occured, otherwise see
 *    zathura_plugin_error_t
 */
zathura_plugin_error_t zathura_page_links_free(girara_list_t* list);

/**
 * Get list of form fields
 *
 * @param page The page object
 * @param error Set to an error value (see \ref zathura_plugin_error_t) if an
 *   error occured
 * @return List of form fields
 */
girara_list_t* zathura_page_form_fields_get(zathura_page_t* page, zathura_plugin_error_t* error);

/**
 * Free list of form fields
 *
 * @param list List of form fields
 * @return ZATHURA_PLUGIN_ERROR_OK when no error occured, otherwise see
 *    zathura_plugin_error_t
 */
zathura_plugin_error_t zathura_page_form_fields_free(girara_list_t* list);

/**
 * Get list of images
 *
 * @param page Page
 * @param error Set to an error value (see \ref zathura_plugin_error_t) if an
 *   error occured
 * @return List of images or NULL if an error occured
 */
girara_list_t* zathura_page_images_get(zathura_page_t* page, zathura_plugin_error_t* error);

/**
 * Get image
 *
 * @param page Page
 * @param image Image identifier
 * @param error Set to an error value (see \ref zathura_plugin_error_t) if an
 *   error occured
 * @return The cairo image surface or NULL if an error occured
 */
cairo_surface_t* zathura_page_image_get_cairo(zathura_page_t* page, zathura_image_t* image, zathura_plugin_error_t* error);

/**
 * Get text for selection
 * @param page Page
 * @param rectangle Selection
 * @param error Set to an error value (see \ref zathura_plugin_error_t) if an error
 * occured
 * @return The selected text (needs to be deallocated with g_free)
 */
char* zathura_page_get_text(zathura_page_t* page, zathura_rectangle_t rectangle, zathura_plugin_error_t* error);

/**
 * Render page
 *
 * @param page The page object
 * @param cairo Cairo object
 * @param printing render for printing
 * @return ZATHURA_PLUGIN_ERROR_OK when no error occured, otherwise see
 *    zathura_plugin_error_t
 */
zathura_plugin_error_t zathura_page_render(zathura_page_t* page, cairo_t* cairo, bool printing);

/**
 * Create new index element
 *
 * @param title Title of the index element
 * @return Index element
 */
zathura_index_element_t* zathura_index_element_new(const char* title);

/**
 * Free index element
 *
 * @param index The index element
 */
void zathura_index_element_free(zathura_index_element_t* index);

/**
 * Free link
 *
 * @param link The link
 */
void zathura_link_free(zathura_link_t* link);

/**
 * Add type -> plugin mapping
 * @param zathura zathura instance
 * @param type content type
 * @param plugin plugin instance
 * @return true on success, false if another plugin is already registered for
 * that type
 */
bool zathura_type_plugin_mapping_new(zathura_t* zathura, const gchar* type, zathura_document_plugin_t* plugin);

/**
 * Free type -> plugin mapping
 * @param mapping To be freed
 */
void zathura_type_plugin_mapping_free(zathura_type_plugin_mapping_t* mapping);

#endif // DOCUMENT_H
