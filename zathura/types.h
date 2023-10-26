/* SPDX-License-Identifier: Zlib */

#ifndef TYPES_H
#define TYPES_H

#include <girara/datastructures.h>
#include <glib.h>

#include "macros.h"

/**
 * Document
 */
typedef struct zathura_document_s zathura_document_t;
/**
 * Page
 */
typedef struct zathura_page_s zathura_page_t;
/**
 * Page widget
 */
typedef struct zathura_page_widget_s ZathuraPage;
typedef struct zathura_page_widget_class_s ZathuraPageClass;
/**
 * Zathura
 */
typedef struct zathura_s zathura_t;

/**
 * Plugin manager
 */
typedef struct zathura_plugin_manager_s zathura_plugin_manager_t;

/**
 * Renderer
 */
typedef struct zathura_renderer_s ZathuraRenderer;

/**
 * Render request
 */
typedef struct zathura_render_request_s ZathuraRenderRequest;

/**
 * D-Bus manager
 */
typedef struct zathura_dbus_s ZathuraDbus;

/**
 * Error types
 */
typedef enum zathura_plugin_error_e
{
  ZATHURA_ERROR_OK, /**< No error occurred */
  ZATHURA_ERROR_UNKNOWN, /**< An unknown error occurred */
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
  ZATHURA_DOCUMENT_INFORMATION_OTHER, /**< Any other information */
  ZATHURA_DOCUMENT_INFORMATION_FORMAT /**< Format of the document */
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
typedef struct zathura_document_information_entry_s zathura_document_information_entry_t;

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
  ZATHURA_ADJUST_WIDTH, /**< Adjust to width */
  ZATHURA_ADJUST_INPUTBAR, /**< Focusing the inputbar */
  ZATHURA_ADJUST_MODE_NUMBER /**< Number of adjust modes */
} zathura_adjust_mode_t;

/**
 * Creates an image buffer
 *
 * @param width Width of the image stored in the buffer
 * @param height Height of the image stored in the buffer
 * @return Image buffer or NULL if an error occurred
 */
ZATHURA_PLUGIN_API zathura_image_buffer_t* zathura_image_buffer_create(unsigned int width, unsigned int height);

/**
 * Frees the image buffer
 *
 * @param buffer The image buffer
 */
ZATHURA_PLUGIN_API void zathura_image_buffer_free(zathura_image_buffer_t* buffer);

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
  ZATHURA_LINK_NONE, /**< No action */
  ZATHURA_LINK_GOTO_DEST, /**< Links to a page */
  ZATHURA_LINK_GOTO_REMOTE, /**< Links to a page */
  ZATHURA_LINK_URI, /**< Links to an external source */
  ZATHURA_LINK_LAUNCH, /**< Links to an external source */
  ZATHURA_LINK_NAMED /**< Links to an external source */
} zathura_link_type_t;

typedef enum zathura_link_destination_type_e
{
  ZATHURA_LINK_DESTINATION_UNKNOWN,
  ZATHURA_LINK_DESTINATION_XYZ,
  ZATHURA_LINK_DESTINATION_FIT,
  ZATHURA_LINK_DESTINATION_FITH,
  ZATHURA_LINK_DESTINATION_FITV,
  ZATHURA_LINK_DESTINATION_FITR,
  ZATHURA_LINK_DESTINATION_FITB,
  ZATHURA_LINK_DESTINATION_FITBH,
  ZATHURA_LINK_DESTINATION_FITBV
} zathura_link_destination_type_t;

typedef struct zathura_link_target_s
{
  zathura_link_destination_type_t destination_type;
  char* value; /**< Value */
  unsigned int page_number; /**< Page number */
  double left; /**< Left coordinate */
  double right; /**< Right coordinate */
  double top; /**< Top coordinate */
  double bottom; /**< Bottom coordinate */
  double zoom; /**< Zoom */
} zathura_link_target_t;

/**
 * Link
 */
typedef struct zathura_link_s zathura_link_t;

/**
 * Index element
 */
typedef struct zathura_index_element_s
{
  char* title; /**< Title of the element */
  zathura_link_t* link;
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
 * Jump
 */
typedef struct zathura_jump_s
{
  unsigned int page;
  double x;
  double y;
} zathura_jump_t;

/**
 * Create new index element
 *
 * @param title Title of the index element
 * @return Index element
 */
ZATHURA_PLUGIN_API zathura_index_element_t* zathura_index_element_new(const char* title);

/**
 * Free index element
 *
 * @param index The index element
 */
ZATHURA_PLUGIN_API void zathura_index_element_free(zathura_index_element_t* index);

/**
 * Creates a list that should be used to store \ref
 * zathura_document_information_entry_t entries
 *
 * @return A list or NULL if an error occurred
 */
ZATHURA_PLUGIN_API girara_list_t* zathura_document_information_entry_list_new(void);

/**
 * Creates a new document information entry
 *
 * @param type The type
 * @param value The value
 *
 * @return A new entry or NULL if an error occurred
 */
ZATHURA_PLUGIN_API zathura_document_information_entry_t*
zathura_document_information_entry_new(zathura_document_information_type_t
    type, const char* value);

/**
 * Frees a document information entry
 *
 * @param entry The entry that should be freed
 */
ZATHURA_PLUGIN_API void zathura_document_information_entry_free(zathura_document_information_entry_t* entry);

/**
 * Context for MIME type detection
 */
typedef struct zathura_content_type_context_s zathura_content_type_context_t;

/**
 * Device scaling structure.
 */
typedef struct zathura_device_factors_s
{
  double x;
  double y;
} zathura_device_factors_t;

/**
 * Signature state
 */
typedef enum zathura_signature_state_e {
  ZATHURA_SIGNATURE_INVALID,
  ZATHURA_SIGNATURE_VALID,
  ZATHURA_SIGNATURE_CERTIFICATE_UNTRUSTED,
  ZATHURA_SIGNATURE_CERTIFICATE_EXPIRED,
  ZATHURA_SIGNATURE_CERTIFICATE_REVOKED,
  ZATHURA_SIGNATURE_CERTIFICATE_INVALID,
  ZATHURA_SIGNATURE_ERROR,
} zathura_signature_state_t;

/**
 * Signature information
 */
typedef struct zathura_signature_info_s {
  char*                     signer;
  GDateTime*                time;
  zathura_rectangle_t       position;
  zathura_signature_state_t state;
} zathura_signature_info_t;

/**
 *  Creates a new siganture info.
 *
 * @return A new signature info or NULL if an error occurred
 */
ZATHURA_PLUGIN_API zathura_signature_info_t* zathura_signature_info_new(void);

/**
 * Frees a signature info
 *
 * @param signature The signature info to be freed
 */
ZATHURA_PLUGIN_API void zathura_signature_info_free(zathura_signature_info_t* signature);

#endif // TYPES_H
