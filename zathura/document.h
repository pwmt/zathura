/* SPDX-License-Identifier: Zlib */

#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <girara/types.h>
#include <stdbool.h>
#include <stdint.h>

#include "types.h"

#define DOCUMENT_DIGEST_SIZE 16

/**
 * Open the document
 *
 * @param plugin_manager The zathura instance
 * @param path Path to the document
 * @param password Password of the document or NULL
 * @param error Optional error parameter
 * @return The document object and NULL if an error occurs
 */
zathura_document_t* zathura_document_open(zathura_t* zathura, const char* path, const char* uri, const char* password,
                                          zathura_error_t* error);

/**
 * Free the document
 *
 * @param document
 * @return ZATHURA_ERROR_OK when no error occurred, otherwise see
 *    zathura_error_t
 */
ZATHURA_PLUGIN_API zathura_error_t zathura_document_free(zathura_document_t* document);

/**
 * Returns the path of the document
 *
 * @param document The document
 * @return The file path of the document
 */
ZATHURA_PLUGIN_API const char* zathura_document_get_path(zathura_document_t* document);

/**
 * Returns the URI of the document
 *
 * @param document The document
 * @return The URI of the document
 */
ZATHURA_PLUGIN_API const char* zathura_document_get_uri(zathura_document_t* document);

/**
 * Returns the basename of the document
 *
 * @param document The document
 * @return The basename of the document
 */
ZATHURA_PLUGIN_API const char* zathura_document_get_basename(zathura_document_t* document);

/**
 * Returns the SHA256 hash of the document
 *
 * @param document The document
 * @return The SHA256 hash of the document
 */
ZATHURA_PLUGIN_API const uint8_t* zathura_document_get_hash(zathura_document_t* document);

/**
 * Returns the password of the document
 *
 * @param document The document
 * @return Returns the password of the document
 */
ZATHURA_PLUGIN_API const char* zathura_document_get_password(zathura_document_t* document);

/**
 * Returns the page at the given index
 *
 * @param document The document
 * @param index The index of the page
 * @return The page or NULL if an error occurred
 */
ZATHURA_PLUGIN_API zathura_page_t* zathura_document_get_page(zathura_document_t* document, unsigned int index);

/**
 * Returns the number of pages
 *
 * @param document The document
 * @return Number of pages
 */
ZATHURA_PLUGIN_API unsigned int zathura_document_get_number_of_pages(zathura_document_t* document);

/**
 * Sets the number of pages
 *
 * @param document The document
 * @param number_of_pages Number of pages
 */
ZATHURA_PLUGIN_API void zathura_document_set_number_of_pages(zathura_document_t* document,
                                                             unsigned int number_of_pages);

/**
 * Returns the private data of the document
 *
 * @param document The document
 * @return The private data or NULL
 */
ZATHURA_PLUGIN_API void* zathura_document_get_data(zathura_document_t* document);

/**
 * Sets the private data of the document
 *
 * @param document The document
 * @param data The new private data
 */
ZATHURA_PLUGIN_API void zathura_document_set_data(zathura_document_t* document, void* data);

/**
 * Save the document
 *
 * @param document The document object
 * @param path Path for the saved file
 * @return ZATHURA_ERROR_OK when no error occurred, otherwise see
 *    zathura_error_t
 */
ZATHURA_PLUGIN_API zathura_error_t zathura_document_save_as(zathura_document_t* document, const char* path);

/**
 * Whether the document was changed in memory, e.g. by annotating it, and not saved since
 *
 * @param document The document
 * @return true if there are unsaved changes
 */
ZATHURA_PLUGIN_API bool zathura_document_is_modified(zathura_document_t* document);

/**
 * Mark the document as changed or saved
 *
 * @param document The document
 * @param modified Whether there are unsaved changes
 */
ZATHURA_PLUGIN_API void zathura_document_set_modified(zathura_document_t* document, bool modified);

/**
 * Generate the document index
 *
 * @param document The document object
 * @param error Set to an error value (see \ref zathura_error_t) if an
 *   error occurred
 * @return Generated index
 */
ZATHURA_PLUGIN_API girara_tree_node_t* zathura_document_index_generate(zathura_document_t* document,
                                                                       zathura_error_t* error);

/**
 * Get list of attachments
 *
 * @param document The document object
 * @param error Set to an error value (see \ref zathura_error_t) if an
 *   error occurred
 * @return List of attachments
 */
ZATHURA_PLUGIN_API girara_list_t* zathura_document_attachments_get(zathura_document_t* document,
                                                                   zathura_error_t* error);

/**
 * Save document attachment
 *
 * @param document The document objects
 * @param attachment name of the attachment
 * @param file the target filename
 * @return ZATHURA_ERROR_OK when no error occurred, otherwise see
 *    zathura_error_t
 */
ZATHURA_PLUGIN_API zathura_error_t zathura_document_attachment_save(zathura_document_t* document,
                                                                    const char* attachment, const char* file);

/**
 * Returns a string of the requested information
 *
 * @param document The zathura document
 * @param error Set to an error value (see \ref zathura_error_t) if an
 *   error occurred
 * @return List of document information entries or NULL if information could not be retrieved
 */
ZATHURA_PLUGIN_API girara_list_t* zathura_document_get_information(zathura_document_t* document,
                                                                   zathura_error_t* error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(zathura_document_t, zathura_document_free)

#endif // DOCUMENT_H
