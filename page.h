/* See LICENSE file for license and copyright information */

#ifndef PAGE_H
#define PAGE_H

#include <girara/datastructures.h>
#include <cairo.h>

#include "types.h"

/**
 * Get the page object
 *
 * @param document The document
 * @param index Page number
 * @param error Optional error
 * @return Page object or NULL if an error occured
 */
zathura_page_t* zathura_page_new(zathura_document_t* document, unsigned int
    index, zathura_error_t* error);

/**
 * Frees the page object
 *
 * @param page The page object
 * @return ZATHURA_ERROR_OK when no error occured, otherwise see
 *    zathura_error_t
 */
zathura_error_t zathura_page_free(zathura_page_t* page);

/**
 * Returns the associated document
 *
 * @param page The page object
 * @return The associated document
 * @return NULL if an error occured
 */
zathura_document_t* zathura_page_get_document(zathura_page_t* page);

/**
 * Returns the set id of the page
 *
 * @param page The page object
 * @return The id of the page
 */
unsigned int zathura_page_get_index(zathura_page_t* page);

/**
 * Returns the width of the page
 *
 * @param page The page object
 * @return Width of the page
 * @return -1 If an error occured
 */
double zathura_page_get_width(zathura_page_t* page);

/**
 * Sets the new width of the page
 *
 * @param page The page object
 * @param width The new width of the page
 */
void zathura_page_set_width(zathura_page_t* page, double width);

/**
 * Returns the height of the page
 *
 * @param page The page object
 * @return Height of the page
 * @return -1 If an error occured
 */
double zathura_page_get_height(zathura_page_t* page);

/**
 * Sets the new height of the page
 *
 * @param page The page object
 * @param height The new height of the page
 */
void zathura_page_set_height(zathura_page_t* page, double height);

/**
 * Returns the visibility of the page
 *
 * @param page The page object
 * @return true if the page is visible
 * @return false if the page is hidden
 */
bool zathura_page_get_visibility(zathura_page_t* page);

/**
 * Sets the visibility of the page
 *
 * @param page The page object
 * @param visibility The new visibility value
 */
void zathura_page_set_visibility(zathura_page_t* page, bool visibility);

/**
 * Returns the custom data
 *
 * @param page The page object
 * @return The custom data or NULL
 */
void* zathura_page_get_data(zathura_page_t* page);

/**
 * Sets the custom data
 *
 * @param page The page object
 * @param data The custom data
 */
void zathura_page_set_data(zathura_page_t* page, void* data);

/**
 * Search page
 *
 * @param page The page object
 * @param text Search item
 * @param error Set to an error value (see \ref zathura_error_t) if an
 *   error occured
 * @return List of results
 */
girara_list_t* zathura_page_search_text(zathura_page_t* page, const char* text, zathura_error_t* error);

/**
 * Get page links
 *
 * @param page The page object
 * @param error Set to an error value (see \ref zathura_error_t) if an
 *   error occured
 * @return List of links
 */
girara_list_t* zathura_page_links_get(zathura_page_t* page, zathura_error_t* error);

/**
 * Free page links
 *
 * @param list List of links
 * @return ZATHURA_ERROR_OK when no error occured, otherwise see
 *    zathura_error_t
 */
zathura_error_t zathura_page_links_free(girara_list_t* list);

/**
 * Get list of form fields
 *
 * @param page The page object
 * @param error Set to an error value (see \ref zathura_error_t) if an
 *   error occured
 * @return List of form fields
 */
girara_list_t* zathura_page_form_fields_get(zathura_page_t* page, zathura_error_t* error);

/**
 * Free list of form fields
 *
 * @param list List of form fields
 * @return ZATHURA_ERROR_OK when no error occured, otherwise see
 *    zathura_error_t
 */
zathura_error_t zathura_page_form_fields_free(girara_list_t* list);

/**
 * Get list of images
 *
 * @param page Page
 * @param error Set to an error value (see \ref zathura_error_t) if an
 *   error occured
 * @return List of images or NULL if an error occured
 */
girara_list_t* zathura_page_images_get(zathura_page_t* page, zathura_error_t* error);

/**
 * Get image
 *
 * @param page Page
 * @param image Image identifier
 * @param error Set to an error value (see \ref zathura_error_t) if an
 *   error occured
 * @return The cairo image surface or NULL if an error occured
 */
cairo_surface_t* zathura_page_image_get_cairo(zathura_page_t* page, zathura_image_t* image, zathura_error_t* error);

/**
 * Get text for selection
 * @param page Page
 * @param rectangle Selection
 * @param error Set to an error value (see \ref zathura_error_t) if an error
 * occured
 * @return The selected text (needs to be deallocated with g_free)
 */
char* zathura_page_get_text(zathura_page_t* page, zathura_rectangle_t rectangle, zathura_error_t* error);

/**
 * Returns a list of annotations (see \ref zathura_annotation_t)
 *
 * @param page Page
 * @param error Set to an error value (see \ref zathura_error_t) if an
 *   error occured
 * @return List of annotations or NULL if an error occured
 */
girara_list_t* zathura_page_get_annotations(zathura_page_t* page, zathura_error_t* error);

/**
 * Render page
 *
 * @param page The page object
 * @param cairo Cairo object
 * @param printing render for printing
 * @return ZATHURA_ERROR_OK when no error occured, otherwise see
 *    zathura_error_t
 */
zathura_error_t zathura_page_render(zathura_page_t* page, cairo_t* cairo, bool printing);

#endif // PAGE_H
