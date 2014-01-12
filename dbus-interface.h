/* See LICENSE file for license and copyright information */

#ifndef DBUS_INTERFACE_H
#define DBUS_INTERFACE_H

#include <stdbool.h>
#include <girara/types.h>
#include <glib-object.h>
#include "types.h"

typedef struct zathura_dbus_class_s ZathuraDbusClass;

struct zathura_dbus_s
{
  GObject parent;
};

struct zathura_dbus_class_s
{
  GObjectClass parent_class;
};

#define ZATHURA_TYPE_DBUS \
  (zathura_dbus_get_type())
#define ZATHURA_DBUS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATHURA_TYPE_DBUS, \
                              ZathuraDbus))
#define ZATHURA_DBUS_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_CAST((obj), ZATHURA_TYPE_DBUS, \
                           ZathuraDbus))
#define ZATHURA_IS_DBUS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATHURA_TYPE_DBUS))
#define ZATHURA_IS_DBUS_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((obj), ZATHURA_TYPE_DBUS))
#define ZATHURA_DBUS_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ZATHURA_TYPE_DBUS, \
                             ZathuraDbusClass))

GType zathura_dbus_get_type(void);

ZathuraDbus* zathura_dbus_new(zathura_t* zathura);

/**
 * Look for zathura instance having filename open and cause it to open give page
 * and highlight rectangles on the given page
 *
 * @param filename filename
 * @param page page number
 * @param rectangles list of rectangles to highlight
 * @returns true if a instance was found that has the given filename open, false
 * otherwise
 */
bool zathura_dbus_goto_page_and_highlight(const char* filename, int page,
    girara_list_t* rectangles);

bool zathura_dbus_synctex_position(const char* filename, const char* position);

#endif
