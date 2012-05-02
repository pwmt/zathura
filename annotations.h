/* See LICENSE file for license and copyright information */

#ifndef ANNOTATION_H
#define ANNOTATION_H

#include <time.h>

#include "page.h"
#include "types.h"

typedef struct zathura_annotation_s zathura_annotation_t;

typedef enum zathura_annotation_type_s {
  ZATHURA_ANNOTATION_UNKNOWN,
  ZATHURA_ANNOTATION_TEXT,
  ZATHURA_ANNOTATION_LINK,
  ZATHURA_ANNOTATION_FREE_TEXT,
  ZATHURA_ANNOTATION_LINE,
  ZATHURA_ANNOTATION_SQUARE,
  ZATHURA_ANNOTATION_CIRCLE,
  ZATHURA_ANNOTATION_POLYGON,
  ZATHURA_ANNOTATION_POLY_LINE,
  ZATHURA_ANNOTATION_HIGHLIGHT,
  ZATHURA_ANNOTATION_UNDERLINE,
  ZATHURA_ANNOTATION_SQUIGGLY,
  ZATHURA_ANNOTATION_STRIKE_OUT,
  ZATHURA_ANNOTATION_STAMP,
  ZATHURA_ANNOTATION_CARET,
  ZATHURA_ANNOTATION_INK,
  ZATHURA_ANNOTATION_POPUP,
  ZATHURA_ANNOTATION_FILE_ATTACHMENT,
  ZATHURA_ANNOTATION_SOUND,
  ZATHURA_ANNOTATION_MOVIE,
  ZATHURA_ANNOTATION_WIDGET,
  ZATHURA_ANNOTATION_SCREEN,
  ZATHURA_ANNOTATION_PRINTER_MARK,
  ZATHURA_ANNOTATION_TRAP_NET,
  ZATHURA_ANNOTATION_WATERMARK,
  ZATHURA_ANNOTATION_3D,
  ZATHURA_ANNOTATION_MARKUP
} zathura_annotation_type_t;

typedef enum zathura_annotation_flag_s {
  ZATHURA_ANNOTATION_FLAG_UNKNOWN         = 0,
  ZATHURA_ANNOTATION_FLAG_INVISIBLE       = 1 << 0,
  ZATHURA_ANNOTATION_FLAG_HIDDEN          = 1 << 1,
  ZATHURA_ANNOTATION_FLAG_PRINT           = 1 << 2,
  ZATHURA_ANNOTATION_FLAG_NO_ZOOM         = 1 << 3,
  ZATHURA_ANNOTATION_FLAG_NO_ROTATE       = 1 << 4,
  ZATHURA_ANNOTATION_FLAG_NO_VIEW         = 1 << 5,
  ZATHURA_ANNOTATION_FLAG_READ_ONLY       = 1 << 6,
  ZATHURA_ANNOTATION_FLAG_LOCKED          = 1 << 7,
  ZATHURA_ANNOTATION_FLAG_TOGGLE_NO_VIEW  = 1 << 8,
  ZATHURA_ANNOTATION_FLAG_LOCKED_CONTENTS = 1 << 9,
} zathura_annotation_flag_t;

/**
 * Creates a new annotation
 *
 * @param type The type of the annotation
 * @return Pointer to the created annotation object or NULL if an error occured
 */
zathura_annotation_t* zathura_annotation_new(zathura_annotation_type_t type);

/**
 * Frees the given annotation
 *
 * @param annotation The annotation
 */
void zathura_annotation_free(zathura_annotation_t* annotation);

/**
 * Returns the pointer to the custom data for the given annotation
 *
 * @param annotation The annotation
 * @return Pointer to the custom data or NULL
 */
void* zathura_annotation_get_data(zathura_annotation_t* annotation);

/**
 * Sets the custom data for the given annotation
 *
 * @param annotation The annotation
 * @param data Custom data
 */
void zathura_annotation_set_data(zathura_annotation_t* annotation, void* data);

/**
 * Returns the type of the annotation
 *
 * @param annotation The annotation
 * @return The type of the annotation (\ref zathura_annotation_type_t)
 */
zathura_annotation_type_t zathura_annotation_get_type(zathura_annotation_t* annotation);

/**
 * Returns the flags of the annotation
 *
 * @param annotation The annotation
 * @return The set flags of the annotation (\ref zathura_annotation_flag_t)
 */
zathura_annotation_flag_t zathura_annotation_get_flags(zathura_annotation_t* annotation);

/**
 * Sets the flags of the annotation
 *
 * @param annotation The annotation
 * @param flags The flags that should be set to the annotation
 */
void zathura_annotation_set_flags(zathura_annotation_t* annotation, zathura_annotation_flag_t flags);

/**
 * Returns the content of the annotation
 *
 * @param annotation The annotation
 * @return The content of the annotation or NULL
 */
char* zathura_annotation_get_content(zathura_annotation_t* annotation);

/**
 * Sets the content of the annotation
 *
 * @param annotation The annotation
 * @param content The new content of the annotation
 */
void zathura_annotation_set_content(zathura_annotation_t* annotation, char* content);

/**
 * Returns the name of the annotation
 *
 * @param annotation The annotation
 * @return The name of the annotation or NULL
 */
char* zathura_annotation_get_name(zathura_annotation_t* annotation);

/**
 * Sets the name of the annotation
 *
 * @param annotation The annotation
 * @param name The new name of the annotation
 */
void zathura_annotation_set_name(zathura_annotation_t* annotation, const char* name);

/**
 * Returns the modification date of the annotation
 *
 * @param annotation The annotation
 * @return The date on which the annotation has been modified the last time
 */
time_t zathura_annotation_get_modified(zathura_annotation_t* annotation);

/**
 * Sets the date on which the annotation has been modified the last time
 *
 * @param annotation The annotation
 * @param modification_date The modification date
 */
void zathura_annotation_set_modified(zathura_annotation_t* annotation, time_t modification_date);

/**
 * Returns the page of the annotation
 *
 * @param annotation The annotation
 * @return The page of the annotation
 */
zathura_page_t* zathura_annotation_get_page(zathura_annotation_t* annotation);

/**
 * Sets the page of the annotation
 *
 * @param annotation The annotation
 * @param page The page of the annotation
 */
void zathura_annotation_set_page_index(zathura_annotation_t* annotation, zathura_page_t* page);

/**
 * Retrieves the position of the annotation and saves it into the given
 * rectangle
 *
 * @param annotation The annotation
 * @param rectangle The coordinates of the annotation
 * @return true if the position could be retrieved otherwise false
 */
bool zathura_annotation_get_position(zathura_annotation_t* annotation, zathura_rectangle_t* rectangle);

/**
 * Sets the new position of the annotation
 *
 * @param annotation The annotation
 * @param rectangle The rectangle containing the new position information
 */
void zathura_annotation_set_position(zathura_annotation_t* annotation, zathura_rectangle_t rectangle);

char* zathura_annotation_markup_get_label(zathura_annotation_t* annotation);
void zathura_annotation_markup_set_label(zathura_annotation_t* annotation, const char* label);

#endif // ANNOTATION_H
