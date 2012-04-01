/* See LICENSE file for license and copyright information */

#ifndef TYPES_H
#define TYPES_H

#include "zathura.h"

/**
 * Error types
 */
typedef enum zathura_plugin_error_e
{
  ZATHURA_ERROR_OK, /**< No error occured */
  ZATHURA_ERROR_UNKNOWN, /**< An unknown error occured */
  ZATHURA_ERROR_OUT_OF_MEMORY, /**< Out of memory */
  ZATHURA_ERROR_NOT_IMPLEMENTED, /**< The called function has not been implemented */
  ZATHURA_ERROR_INVALID_ARGUMENTS, /**< Invalid arguments have been passed */
  ZATHURA_ERROR_INVALID_PASSWORD /**< The provided password is invalid */
} zathura_error_t;

/**
 * Possible information entry types
 */
typedef enum zathura_document_information_type_e
{
  ZATHURA_DOCUMENT_INFORMATION_TITLE, /**< Title of the document */
  ZATHURA_DOCUMENT_INFORMATION_AUTHOR, /**< Author of the document */
  ZATHURA_DOCUMENT_INFORMATION_SUBJECT, /**< Subject of the document */
  ZATHURA_DOCUMENT_INFORMATION_KEYWORDS, /**< Keywords of the document */
  ZATHURA_DOCUMENT_INFORMATION_CREATOR, /**< Creator of the document */
  ZATHURA_DOCUMENT_INFORMATION_PRODUCER, /**< Producer of the document */
  ZATHURA_DOCUMENT_INFORMATION_CREATION_DATE, /**< Creation data */
  ZATHURA_DOCUMENT_INFORMATION_MODIFICATION_DATE, /**< Modification data */
  ZATHURA_DOCUMENT_INFORMATION_OTHER /**< Any other information */
} zathura_document_information_type_t;

/**
  * Plugin
  */
typedef struct zathura_plugin_s zathura_plugin_t;

/**
 * Document information entry
 *
 * Represents a single entry in the returned list from the \ref
 * zathura_document_get_information function
 */
typedef struct zathura_document_information_entry_s
{
  zathura_document_information_type_t type; /**< Type of the information */
  char* value; /**< Value */
} zathura_document_information_entry_t;

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
 * Adjust mode
 */
typedef enum zathura_adjust_mode_e
{
  ZATHURA_ADJUST_NONE, /**< No adjustment */
  ZATHURA_ADJUST_BESTFIT, /**< Adjust to best-fit */
  ZATHURA_ADJUST_WIDTH /**< Adjust to width */
} zathura_adjust_mode_t;

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
  ZATHURA_LINK_INVALID, /**< Invalid type */
  ZATHURA_LINK_TO_PAGE, /**< Links to a page */
  ZATHURA_LINK_EXTERNAL, /**< Links to an external source */
} zathura_link_type_t;

typedef union zathura_link_target_u
{
  unsigned int page_number; /**< Page number */
  char* uri; /**< Value */
} zathura_link_target_t;

/**
 * Link
 */
typedef struct zathura_link_s
{
  zathura_rectangle_t position; /**< Position of the link */
  zathura_link_type_t type; /**< Link type */
  zathura_link_target_t target; /**< Link target */
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
 * Creates a new zathura link
 *
 * @param type Type of the link
 * @param position Position of the link
 * @param target Target
 * @return New zathura link
 */
zathura_link_t* zathura_link_new(zathura_link_type_t type, zathura_rectangle_t position,
    zathura_link_target_t target);

/**
 * Free link
 *
 * @param link The link
 */
void zathura_link_free(zathura_link_t* link);

/**
 * Returns the type of the link
 *
 * @param link The link
 * @return The target type of the link
 */
zathura_link_type_t zathura_link_get_type(zathura_link_t* link);

/**
 * Returns the position of the link
 *
 * @param link The link
 * @return The position of the link
 */
zathura_rectangle_t zathura_link_get_position(zathura_link_t* link);

/**
 * The target value of the link
 *
 * @param link The link
 * @return Returns the target of the link (depends on the link type)
 */
zathura_link_target_t zathura_link_get_target(zathura_link_t* link);

/**
 * Creates a list that should be used to store \ref
 * zathura_document_information_entry_t entries
 *
 * @return A list or NULL if an error occured
 */
girara_list_t* zathura_document_information_entry_list_new();

/**
 * Creates a new document information entry
 *
 * @param type The type
 * @param value The value
 *
 * @return A new entry or NULL if an error occured
 */
zathura_document_information_entry_t*
zathura_document_information_entry_new(zathura_document_information_type_t
    type, const char* value);

/**
 * Frees a document information entry
 *
 * @param entry The entry that should be freed
 */
void zathura_document_information_entry_free(zathura_document_information_entry_t* entry);

#endif // TYPES_H
