/* SPDX-License-Identifier: Zlib */

#ifndef DBUS_INTERFACE_H
#define DBUS_INTERFACE_H

#include <stdbool.h>
#include <girara/types.h>
#include <glib-object.h>
#include <sys/types.h>
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

GType zathura_dbus_get_type(void) G_GNUC_CONST;

ZathuraDbus* zathura_dbus_new(zathura_t* zathura);
const char* zathura_dbus_get_name(zathura_t* zathura);

/**
 * Emit the 'Edit' signal on the D-Bus connection.
 *
 * @param dbus ZathuraDbus instance
 * @param page page
 * @param x x coordinate
 * @param y y coordinate
 */
void zathura_dbus_edit(ZathuraDbus* dbus, unsigned int page, unsigned int x, unsigned int y);

/**
 * Highlight rectangles in a zathura instance that has filename open.
 * input_file, line and column determine the rectangles to display and are
 * passed to SyncTeX.
 *
 * @param filename path of the document
 * @param input_file path of the input file
 * @param line line index (starts at 0)
 * @param column column index (starts at 0)
 * @param hint zathura process ID that has filename open
 */
int zathura_dbus_synctex_position(const char* filename, const char* input_file,
    int line, int column, pid_t hint);

#endif
