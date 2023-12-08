/* SPDX-License-Identifier: Zlib */

#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <stdbool.h>
#include <stdint.h>

#include <girara/types.h>

#include "types.h"

/**
 * Open the document
 *
 * @param plugin_manager The zathura instance
 * @param path Path to the document
 * @param password Password of the document or NULL
 * @param error Optional error parameter
 * @return The document object and NULL if an error occurs
 */
zathura_document_t* zathura_document_open(zathura_t* zathura,
    const char* path, const char *uri, const char* password, zathura_error_t*
    error);

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
ZATHURA_PLUGIN_API void zathura_document_set_number_of_pages(zathura_document_t* document, unsigned
    int number_of_pages);

/**
 * Returns the current page number
 *
 * @param document The document
 * @return Current page
 */
ZATHURA_PLUGIN_API unsigned int zathura_document_get_current_page_number(zathura_document_t* document);

/**
 * Sets the number of pages
 *
 * @param document The document
 * @param current_page The current page number
 */
ZATHURA_PLUGIN_API void zathura_document_set_current_page_number(zathura_document_t* document, unsigned
    int current_page);

/**
 * Returns the X position, as a value relative to the document width (0=left,
 * 1=right).
 *
 * @param document The document
 * @return X adjustment
 */
ZATHURA_PLUGIN_API double zathura_document_get_position_x(zathura_document_t* document);

/**
 * Returns the Y position as value relative to the document height (0=top,
 * 1=bottom)
 *
 * @param document The document
 * @return Y adjustment
 */
ZATHURA_PLUGIN_API double zathura_document_get_position_y(zathura_document_t* document);

/**
 * Sets the X position as a value relative to the document width (0=left,
 * 1=right)
 *
 * @param document The document
 * @param position_x the X adjustment
 */
ZATHURA_PLUGIN_API void zathura_document_set_position_x(zathura_document_t* document, double position_x);

/**
 * Sets the Y position as a value relative to the document height (0=top,
 * 1=bottom)
 *
 * @param document The document
 * @param position_y the Y adjustment
 */
ZATHURA_PLUGIN_API void zathura_document_set_position_y(zathura_document_t* document, double position_y);

/**
 * Returns the current zoom value of the document
 *
 * @param document The document
 * @return The current zoom value
 */
ZATHURA_PLUGIN_API double zathura_document_get_zoom(zathura_document_t* document);

/**
 * Returns the current scale value of the document (based on zoom and screen
 * PPI)
 *
 * @param document The document
 * @return The current scale value, in pixels per point
 */
ZATHURA_PLUGIN_API double zathura_document_get_scale(zathura_document_t* document);

/**
 * Sets the new zoom value of the document
 *
 * @param document The document
 * @param zoom The new zoom value
 */
ZATHURA_PLUGIN_API void zathura_document_set_zoom(zathura_document_t* document, double zoom);

/**
 * Returns the rotation value of zathura (0..360)
 *
 * @param document The document
 * @return The current rotation value
 */
ZATHURA_PLUGIN_API unsigned int zathura_document_get_rotation(zathura_document_t* document);

/**
 * Sets the new rotation value
 *
 * @param document The document
 * @param rotation The new rotation value
 */
ZATHURA_PLUGIN_API void zathura_document_set_rotation(zathura_document_t* document, unsigned int rotation);

/**
 * Returns the adjust mode of the document
 *
 * @param document The document
 * @return The adjust mode
 */
ZATHURA_PLUGIN_API zathura_adjust_mode_t zathura_document_get_adjust_mode(zathura_document_t* document);

/**
 * Sets the new adjust mode of the document
 *
 * @param document The document
 * @param mode The new adjust mode
 */
ZATHURA_PLUGIN_API void zathura_document_set_adjust_mode(zathura_document_t* document, zathura_adjust_mode_t mode);

/**
 * Returns the page offset of the document
 *
 * @param document The document
 * @return The page offset
 */
ZATHURA_PLUGIN_API int zathura_document_get_page_offset(zathura_document_t* document);

/**
 * Sets the new page offset of the document
 *
 * @param document The document
 * @param page_offset The new page offset
 */
ZATHURA_PLUGIN_API void zathura_document_set_page_offset(zathura_document_t* document, unsigned int page_offset);

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
 * Sets the width of the viewport in pixels.
 *
 * @param[in] document     The document instance
 * @param[in] width        The width of the viewport
 */
void
ZATHURA_PLUGIN_API zathura_document_set_viewport_width(zathura_document_t* document, unsigned int width);

/**
 * Sets the height of the viewport in pixels.
 *
 * @param[in] document     The document instance
 * @param[in] height       The height of the viewport
 */
void
ZATHURA_PLUGIN_API zathura_document_set_viewport_height(zathura_document_t* document, unsigned int height);

/**
 * Return the size of the viewport in pixels.
 *
 * @param[in]  document     The document instance
 * @param[out] height,width The width and height of the viewport
 */
void
ZATHURA_PLUGIN_API zathura_document_get_viewport_size(zathura_document_t* document,
                                   unsigned int *height, unsigned int* width);

/**
 Sets the viewport PPI (pixels per inch: the resolution of the monitor, after
 scaling with the device factor).
 *
 * @param[in] document     The document instance
 * @param[in] height       The viewport PPI
 */
void
ZATHURA_PLUGIN_API zathura_document_set_viewport_ppi(zathura_document_t* document, double ppi);

/**
 * Return the viewport PPI (pixels per inch: the resolution of the monitor,
 * after scaling with the device factor).
 *
 * @param[in] document     The document instance
 * @return    The viewport PPI
 */
double
ZATHURA_PLUGIN_API zathura_document_get_viewport_ppi(zathura_document_t* document);

/**
 * Set the device scale factors (e.g. for HiDPI). These are generally integers
 * and equal for x and y. These scaling factors are only used when rendering to
 * the screen.
 *
 * @param[in] x_factor,yfactor The x and y scale factors
 */
void
ZATHURA_PLUGIN_API zathura_document_set_device_factors(zathura_document_t* document,
                                  double x_factor, double y_factor);
/**
 * Return the current device scale factors (guaranteed to be non-zero).
 *
 * @return The x and y device scale factors
 */
ZATHURA_PLUGIN_API zathura_device_factors_t
zathura_document_get_device_factors(zathura_document_t* document);

/**
 * Return the size of a cell from the document's layout table in pixels. Assumes
 * that the table is homogeneous (i.e. every cell has the same dimensions). It
 * takes the current scale into account.
 *
 * @param[in]  document     The document instance
 * @param[out] height,width The computed height and width of the cell
 */
ZATHURA_PLUGIN_API void zathura_document_get_cell_size(zathura_document_t* document,
                                    unsigned int* height, unsigned int* width);

/**
 * Compute the size of the entire document to be displayed in pixels. Takes into
 * account the scale, the layout of the pages, and the padding between them. It
 * should be equal to the allocation of zathura->ui.page_widget once it's shown.
 *
 * @param[in]  document               The document
 * @param[out] height,width           The height and width of the document
 */
ZATHURA_PLUGIN_API void zathura_document_get_document_size(zathura_document_t* document,
                                        unsigned int* height, unsigned int* width);

/**
 * Sets the cell height and width of the document
 *
 * @param[in]  document          The document instance
 * @param[in]  cell_height       The desired cell height
 * @param[in]  cell_width        The desired cell width
 */
ZATHURA_PLUGIN_API void zathura_document_set_cell_size(zathura_document_t *document,
                                      unsigned int cell_height,
                                      unsigned int cell_width);

/**
 * Sets the layout of the pages in the document
 *
 * @param[in]  document          The document instance
 * @param[in]  page_padding      pixels of padding between pages
 * @param[in]  pages_per_row     number of pages per row
 * @param[in]  first_page_column column of the first page (first column is 1)
 */
ZATHURA_PLUGIN_API void zathura_document_set_page_layout(zathura_document_t* document, unsigned int page_padding,
                                      unsigned int pages_per_row, unsigned int first_page_column);

/**
 * Returns the padding in pixels between pages
 *
 * @param document The document
 * @return The padding in pixels between pages
 */
ZATHURA_PLUGIN_API unsigned int zathura_document_get_page_padding(zathura_document_t* document);

/**
 * Returns the number of pages per row
 *
 * @param document The document
 * @return The number of pages per row
 */
ZATHURA_PLUGIN_API unsigned int zathura_document_get_pages_per_row(zathura_document_t* document);

/**
 * Returns the column for the first page (first column = 1)
 *
 * @param document The document
 * @return The column for the first page
 */
ZATHURA_PLUGIN_API unsigned int zathura_document_get_first_page_column(zathura_document_t* document);

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
 * Generate the document index
 *
 * @param document The document object
 * @param error Set to an error value (see \ref zathura_error_t) if an
 *   error occurred
 * @return Generated index
 */
ZATHURA_PLUGIN_API girara_tree_node_t* zathura_document_index_generate(zathura_document_t* document, zathura_error_t* error);

/**
 * Get list of attachments
 *
 * @param document The document object
 * @param error Set to an error value (see \ref zathura_error_t) if an
 *   error occurred
 * @return List of attachments
 */
ZATHURA_PLUGIN_API girara_list_t* zathura_document_attachments_get(zathura_document_t* document, zathura_error_t* error);

/**
 * Save document attachment
 *
 * @param document The document objects
 * @param attachment name of the attachment
 * @param file the target filename
 * @return ZATHURA_ERROR_OK when no error occurred, otherwise see
 *    zathura_error_t
 */
ZATHURA_PLUGIN_API zathura_error_t zathura_document_attachment_save(zathura_document_t* document, const char* attachment, const char* file);

/**
 * Returns a string of the requested information
 *
 * @param document The zathura document
 * @param error Set to an error value (see \ref zathura_error_t) if an
 *   error occurred
 * @return List of document information entries or NULL if information could not be retrieved
 */
ZATHURA_PLUGIN_API girara_list_t* zathura_document_get_information(zathura_document_t* document, zathura_error_t* error);

#endif // DOCUMENT_H
