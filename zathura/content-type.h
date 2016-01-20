/* See LICENSE file for license and copyright information */

#ifndef ZATHURA_CONTENT_TYPE_H
#define ZATHURA_CONTENT_TYPE_H

#include "types.h"

/**
 * Create new context for MIME type detection.
 *
 * @return new context
 */
zathura_content_type_context_t* zathura_content_type_new(void);

/**
 * Free MIME type detection context.
 *
 * @param context The context.
 */
void zathura_content_type_free(zathura_content_type_context_t* context);

/**
 * "Guess" the content type of a file. Various methods are tried depending on
 * the available libraries.
 *
 * @param path file name
 * @return content type of path
 */
const char* zathura_content_type_guess(zathura_content_type_context_t* context,
                                       const char* path);

#endif
