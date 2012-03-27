/* See LICENSE file for license and copyright information */

#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <cairo.h>
#include <gtk/gtk.h>
#include <stdbool.h>
#include <girara/types.h>

#include "zathura.h"
#include "types.h"

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
 * Document
 */
struct zathura_document_s
{
  char* file_path; /**< File path of the document */
  const char* password; /**< Password of the document */
  unsigned int current_page_number; /**< Current page number */
  unsigned int number_of_pages; /**< Number of pages */
  double scale; /**< Scale value */
  unsigned int rotate; /**< Rotation */
  void* data; /**< Custom data */
  zathura_t* zathura; /** Zathura object */
  int adjust_mode; /**< Adjust mode (best-fit, width) */
  unsigned int page_offset; /**< Page offset */

  /**
   * Document pages
   */
  zathura_page_t** pages;

	/**
	 * Used plugin
	 */
	zathura_plugin_t* plugin;
};

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
 * @return ZATHURA_ERROR_OK when no error occured, otherwise see
 *    zathura_error_t
 */
zathura_error_t zathura_document_free(zathura_document_t* document);

/**
 * Save the document
 *
 * @param document The document object
 * @param path Path for the saved file
 * @return ZATHURA_ERROR_OK when no error occured, otherwise see
 *    zathura_error_t
 */
zathura_error_t zathura_document_save_as(zathura_document_t* document, const char* path);

/**
 * Generate the document index
 *
 * @param document The document object
 * @param error Set to an error value (see \ref zathura_error_t) if an
 *   error occured
 * @return Generated index
 */

girara_tree_node_t* zathura_document_index_generate(zathura_document_t* document, zathura_error_t* error);

/**
 * Get list of attachments
 *
 * @param document The document object
 * @param error Set to an error value (see \ref zathura_error_t) if an
 *   error occured
 * @return List of attachments
 */
girara_list_t* zathura_document_attachments_get(zathura_document_t* document, zathura_error_t* error);

/**
 * Save document attachment
 *
 * @param document The document objects
 * @param attachment name of the attachment
 * @param file the target filename
 * @return ZATHURA_ERROR_OK when no error occured, otherwise see
 *    zathura_error_t
 */
zathura_error_t zathura_document_attachment_save(zathura_document_t* document, const char* attachment, const char* file);

/**
 * Returns a string of the requested information
 *
 * @param document The zathura document
 * @param meta The information field
 * @param error Set to an error value (see \ref zathura_error_t) if an
 *   error occured
 * @return String or NULL if information could not be retreived
 */
char* zathura_document_meta_get(zathura_document_t* document, zathura_document_meta_t meta, zathura_error_t* error);

#endif // DOCUMENT_H
