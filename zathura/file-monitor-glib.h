/* See LICENSE file for license and copyright information */

#ifndef FILEMONITOR_GLIB_H
#define FILEMONITOR_GLIB_H

#include "file-monitor.h"

#define ZATHURA_TYPE_GLIBFILEMONITOR (zathura_glibfilemonitor_get_type())
#define ZATHURA_GLIBFILEMONITOR(obj)                                           \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATHURA_TYPE_GLIBFILEMONITOR,             \
                              ZathuraGLibFileMonitor))
#define ZATHURA_GLIBFILEMONITOR_CLASS(obj)                                     \
  (G_TYPE_CHECK_CLASS_CAST((obj), ZATHURA_TYPE_GLIBFILEMONITOR,                \
                           ZathuraGLibFileMonitorClass))
#define ZATHURA_IS_GLIBFILEMONITOR(obj)                                        \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATHURA_TYPE_GLIBFILEMONITOR))
#define ZATHURA_IS_GLIBFILEMONITOR_CLASS(obj)                                  \
  (G_TYPE_CHECK_CLASS_TYPE((obj), ZATHURA_TYPE_GLIBFILEMONITOR))
#define ZATHURA_GLIBFILEMONITOR_GET_CLASS(obj)                                 \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ZATHURA_TYPE_GLIBFILEMONITOR,              \
                             ZathuraGLibFileMonitorClass))

typedef struct zathura_glibfilemonitor_s       ZathuraGLibFileMonitor;
typedef struct zathura_glibfilemonitor_class_s ZathuraGLibFileMonitorClass;

struct zathura_glibfilemonitor_class_s
{
  ZathuraFileMonitorClass parent_class;
};

GType zathura_glibfilemonitor_get_type(void) G_GNUC_CONST;

#endif
