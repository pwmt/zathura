/* See LICENSE file for license and copyright information */

#ifndef INTERNAL_H
#define INTERNAL_H

#include "zathura.h"
#include "plugin.h"

/**
 * Zathura password dialog
 */
typedef struct zathura_password_dialog_info_s
{
  char* path; /**< Path to the file */
  char* uri; /**< URI to the file */
  zathura_t* zathura;  /**< Zathura session */
} zathura_password_dialog_info_t;

struct zathura_document_information_entry_s
{
  zathura_document_information_type_t type; /**< Type of the information */
  char* value; /**< Value */
};

/**
 * Returns the associated plugin
 *
 * @param document The document
 * @return The plugin or NULL
 */
zathura_plugin_t* zathura_document_get_plugin(zathura_document_t* document);

#endif // INTERNAL_H
