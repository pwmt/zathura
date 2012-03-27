/* See LICENSE file for license and copyright information */

#ifndef INTERNAL_H
#define INTERNAL_H

#include "zathura.h"

/**
 * Zathura password dialog
 */
typedef struct zathura_password_dialog_info_s
{
  char* path; /**< Path to the file */
  zathura_t* zathura;  /**< Zathura session */
} zathura_password_dialog_info_t;

#endif // INTERNAL_H
