/* See LICENSE file for license and copyright information */

#ifndef ZATHURA_CONTENT_TYPE_H
#define ZATHURA_CONTENT_TYPE_H

/**
 * "Guess" the content type of a file. Various methods are tried depending on
 * the available libraries.
 *
 * @param path file name
 * @return content type of path
 */
const char* guess_content_type(const char* path);

#endif
