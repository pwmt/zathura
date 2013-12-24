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
 * Forward synctex position to zathura instance having the right file open.
 * @param filename filename
 * @param position synctex position
 * @returns true if a instance was found that has the given filename open, false
 * otherwise
 */
bool synctex_forward_position(const char* filename, const char* position);

#endif
