/* See LICENSE file for license and copyright information */

#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <cairo.h>
#include <gtk/gtk.h>
#include <stdbool.h>

#include <girara-datastructures.h>
#include "zathura.h"

#define PLUGIN_REGISTER_FUNCTION "plugin_register"

typedef struct zathura_list_s zathura_list_t;
// typedef struct zathura_document_s zathura_document_t;

typedef bool (*zathura_document_open_t)(zathura_document_t* document);

/**
 * Document plugin structure
 */
typedef struct zathura_document_plugin_s
{
  girara_list_t* content_types; /**> List of supported content types */
  zathura_document_open_t open_function; /**> Document open function */
  void* handle; /**> DLL handle */
} zathura_document_plugin_t;

typedef struct zathura_type_plugin_mapping_s
{
  const gchar* type;
  zathura_document_plugin_t* plugin;
} zathura_type_plugin_mapping_t;

typedef enum zathura_document_meta_e
{
  ZATHURA_DOCUMENT_TITLE,
  ZATHURA_DOCUMENT_AUTHOR,
  ZATHURA_DOCUMENT_SUBJECT,
  ZATHURA_DOCUMENT_KEYWORDS,
  ZATHURA_DOCUMENT_CREATOR,
  ZATHURA_DOCUMENT_PRODUCER,
  ZATHURA_DOCUMENT_CREATION_DATE,
  ZATHURA_DOCUMENT_MODIFICATION_DATE
} zathura_document_meta_t;

/**
 * Function prototype that is called to register a document plugin
 *
 * @param The document plugin
 */
typedef void (*zathura_plugin_register_service_t)(zathura_document_plugin_t*);

/**
 * Structure for a list
 */
struct zathura_list_s
{
  void* data; /**> Data value */
  struct zathura_list_s* next; /**> Next element in the list */
};

/**
 * Image buffer
 */
typedef struct zathura_image_buffer_s
{
  unsigned char* data; /**> Image buffer data */
  unsigned int height; /**> Height of the image */
  unsigned int width; /**> Width of the image */
  unsigned int rowstride; /**> Rowstride of the image */
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
 * @param zathura_image_buffer_t
 */
void zathura_image_buffer_free(zathura_image_buffer_t*);

/**
 * Rectangle structure
 */
typedef struct zathura_rectangle_s
{
  double x1; /**> X coordinate of point 1 */
  double y1; /**> Y coordinate of point 1 */
  double x2; /**> X coordinate of point 2 */
  double y2; /**> Y coordinate of point 2 */
} zathura_rectangle_t;

/**
 * Possible link types
 */
typedef enum zathura_link_type_e
{
  ZATHURA_LINK_TO_PAGE, /**> Links to a page */
  ZATHURA_LINK_EXTERNAL, /**> Links to an external source */
} zathura_link_type_t;

/**
 * Link
 */
typedef struct zathura_link_s
{
  zathura_rectangle_t position; /**> Position of the link */
  zathura_link_type_t type; /**> Link type */
  union
  {
    unsigned int page_number; /**> Page number */
    char* value; /**> Value */
  } target;
} zathura_link_t;

/**
 * Index element
 */
typedef struct zathura_index_element_s
{
  char* title; /**> Title of the element */
  zathura_link_type_t type; /**> Type */
  union
  {
    unsigned int page_number; /**> Page number */
    char* uri; /**> Uri */
  } target;
} zathura_index_element_t;

/**
 * Form type
 */
typedef enum zathura_form_type_e
{
  ZATHURA_FORM_CHECKBOX, /**> Checkbox */
  ZATHURA_FORM_TEXTFIELD /**> Textfield */
} zathura_form_type_t;

/**
 * Form element
 */
typedef struct zathura_form_s
{
  zathura_rectangle_t position; /**> Position */
  zathura_form_type_t type; /**> Type */
} zathura_form_t;

/**
 * Page
 */
struct zathura_page_s
{
  double height; /**> Page height */
  double width; /**> Page width */
  unsigned int number; /**> Page number */
  zathura_document_t* document; /**> Document */
  void* data; /**> Custom data */
  bool visible; /**> Page is visible */
  GtkWidget* event_box; /**> Widget wrapper for mouse events */
  GtkWidget* drawing_area; /**> Drawing area */
  GStaticMutex lock; /**> Lock */
  cairo_surface_t* surface; /** Cairo surface */
};

/**
 * Document
 */
struct zathura_document_s
{
  char* file_path; /**> File path of the document */
  const char* password; /**> Password of the document */
  unsigned int current_page_number; /**> Current page number */
  unsigned int number_of_pages; /**> Number of pages */
  double scale; /**> Scale value */
  int rotate; /**> Rotation */
  void* data; /**> Custom data */
  zathura_t* zathura; /** Zathura object */

  struct
  {
    /**
     * Frees the document
     */
    bool (*document_free)(zathura_document_t* document);

    /**
     * Generates the document index
     */
    girara_tree_node_t* (*document_index_generate)(zathura_document_t* document);

    /**
     * Save the document
     */
    bool (*document_save_as)(zathura_document_t* document, const char* path);

    /**
     * Get list of attachments
     */
    zathura_list_t* (*document_attachments_get)(zathura_document_t* document);

    /**
     * Get document information
     */
    char* (*document_meta_get)(zathura_document_t* document, zathura_document_meta_t info);

    /**
     * Gets the page object
     */
    zathura_page_t* (*page_get)(zathura_document_t* document, unsigned int page_id);

    /**
     * Search text
     */
    zathura_list_t* (*page_search_text)(zathura_page_t* page, const char* text);

    /**
     * Get links on a page
     */
    zathura_list_t* (*page_links_get)(zathura_page_t* page);

    /**
     * Get form fields
     */
    zathura_list_t* (*page_form_fields_get)(zathura_page_t* page);

    /**
     * Renders the page
     */
    zathura_image_buffer_t* (*page_render)(zathura_page_t* page);

    /**
     * Renders the page
     */
    bool (*page_render_cairo)(zathura_page_t* page, cairo_t* cairo);

    /**
     * Free page
     */
    bool (*page_free)(zathura_page_t* page);
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
 * Register document plugin
 */
bool zathura_document_plugin_register(zathura_t* zathura, zathura_document_plugin_t* new_plugin, void* handle);

/**
 * Open the document
 *
 * @param path Path to the document
 * @param password Password of the document or NULL
 * @return The document object
 */
zathura_document_t* zathura_document_open(zathura_t* zathura, const char* path, const char* password);

/**
 * Free the document
 *
 * @param document
 * @return true if no error occured, otherwise false
 */
bool zathura_document_free(zathura_document_t* document);

/**
 * Save the document
 *
 * @param document The document object
 * @param path Path for the saved file
 * @return true if no error occured, otherwise false
 */
bool zathura_document_save_as(zathura_document_t* document, const char* path);

/**
 * Generate the document index
 *
 * @param document The document object
 * @return Generated index
 */

girara_tree_node_t* zathura_document_index_generate(zathura_document_t* document);

/**
 * Get list of attachments
 *
 * @param document The document object
 * @return List of attachments
 */
zathura_list_t* zathura_document_attachments_get(zathura_document_t* document);

/**
 * Free document attachments
 *
 * @param list list of document attachments
 * @return
 */
bool zathura_document_attachments_free(zathura_list_t* list);

/**
 * Returns a string of the requested information
 *
 * @param document The zathura document
 * @param meta The information field
 * @return String or NULL if information could not be retreived
 */
char* zathura_document_meta_get(zathura_document_t* document, zathura_document_meta_t meta);

/**
 * Get the page object
 *
 * @param document The document
 * @param page_id Page number
 * @return Page object or NULL if an error occured
 */
zathura_page_t* zathura_page_get(zathura_document_t* document, unsigned int page_id);

/**
 * Frees the page object
 *
 * @param page The page object
 * @return true if no error occured, otherwise false
 */
bool zathura_page_free(zathura_page_t* page);

/**
 * Search page
 *
 * @param page The page object
 * @param text Search item
 * @return List of results
 */
zathura_list_t* zathura_page_search_text(zathura_page_t* page, const char* text);

/**
 * Get page links
 *
 * @param page The page object
 * @return List of links
 */
zathura_list_t* zathura_page_links_get(zathura_page_t* page);

/**
 * Free page links
 *
 * @param list List of links
 * @return true if no error occured, otherwise false
 */
bool zathura_page_links_free(zathura_list_t* list);

/**
 * Get list of form fields
 *
 * @param page The page object
 * @return List of form fields
 */
zathura_list_t* zathura_page_form_fields_get(zathura_page_t* page);

/**
 * Free list of form fields
 *
 * @param list List of form fields
 * @return true if no error occured, otherwise false
 */
bool zathura_page_form_fields_free(zathura_list_t* list);

/**
 * Render page
 *
 * @param page The page object
 * @param cairo Cairo object
 * @return True if no error occured, otherwise false
 */
bool zathura_page_render(zathura_page_t* page, cairo_t* cairo);

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
