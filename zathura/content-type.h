/* SPDX-License-Identifier: Zlib */

#ifndef ZATHURA_CONTENT_TYPE_H
#define ZATHURA_CONTENT_TYPE_H

#include "types.h"

/**
 * Create new context for content type detection.
 *
 * @return new context
 */
zathura_content_type_context_t* zathura_content_type_new(void);

/**
 * Free content type detection context.
 *
 * @param context The context.
 */
void zathura_content_type_free(zathura_content_type_context_t* context);

/**
 * "Guess" the content type of a file. Various methods are tried depending on
 * the available libraries.
 *
 * @param path file name
 * @return content type of path, needs to freeed with g_free.
 */
char* zathura_content_type_guess(zathura_content_type_context_t* context,
                                 const char* path);

#endif
