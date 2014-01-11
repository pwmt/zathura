/* See LICENSE file for license and copyright information */

#ifndef SYNCTEX_DBUS_H
#define SYNCTEX_DBUS_H

#include <stdbool.h>
#include <girara/types.h>
#include <glib-object.h>
#include "types.h"

typedef struct zathura_synctex_dbus_class_s ZathuraSynctexDbusClass;

struct zathura_synctex_dbus_s
{
  GObject parent;
};

struct zathura_synctex_dbus_class_s
{
  GObjectClass parent_class;
};

#define ZATHURA_TYPE_SYNCTEX_DBUS \
  (zathura_synctex_dbus_get_type())
#define ZATHURA_SYNCTEX_DBUS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATHURA_TYPE_SYNCTEX_DBUS, \
                              ZathuraSynctexDbus))
#define ZATHURA_SYNCTEX_DBUS_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_CAST((obj), ZATHURA_TYPE_SYNCTEX_DBUS, \
                           ZathuraSynctexDbus))
#define ZATHURA_IS_SYNCTEX_DBUS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATHURA_TYPE_SYNCTEX_DBUS))
#define ZATHURA_IS_SYNCTEX_DBUS_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((obj), ZATHURA_TYPE_SYNCTEX_DBUS))
#define ZATHURA_SYNCTEX_DBUS_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ZATHURA_TYPE_SYNCTEX_DBUS, \
                             ZathuraSynctexDbusClass))

GType zathura_synctex_dbus_get_type(void);

ZathuraSynctexDbus* zathura_synctex_dbus_new(zathura_t* zathura);

/**
 * Look for zathura instance having filename open and cause it to open give page
 * and move to the given position on that page.
 *
 * @param filename filename
 * @param page page number
 * @param position_x x coordinate on the page
 * @param position_y y coordinate on the page
 * @returns true if a instance was found that has the given filename open, false
 * otherwise
 */
bool zathura_dbus_goto_page_and_highlight(const char* filename, int page, girara_list_t* rectangles);

bool zathura_dbus_synctex_position(const char* filename, const char* position);

#endif
