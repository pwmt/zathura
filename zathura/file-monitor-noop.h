/* See LICENSE file for license and copyright information */

#ifndef FILEMONITOR_NOOP_H
#define FILEMONITOR_NOOP_H

#include "file-monitor.h"

#define ZATHURA_TYPE_NOOPFILEMONITOR (zathura_noopfilemonitor_get_type())
#define ZATHURA_NOOPFILEMONITOR(obj)                                         \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATHURA_TYPE_NOOPFILEMONITOR,           \
                              ZathuraNoopFileMonitor))
#define ZATHURA_NOOPFILEMONITOR_CLASS(obj)                                   \
  (G_TYPE_CHECK_CLASS_CAST((obj), ZATHURA_TYPE_NOOPFILEMONITOR,              \
                           ZathuraNoopFileMonitorClass))
#define ZATHURA_IS_NOOPFILEMONITOR(obj)                                      \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATHURA_TYPE_NOOPFILEMONITOR))
#define ZATHURA_IS_NOOPFILEMONITOR_CLASS(obj)                                \
  (G_TYPE_CHECK_CLASS_TYPE((obj), ZATHURA_TYPE_NOOPFILEMONITOR))
#define ZATHURA_NOOPFILEMONITOR_GET_CLASS(obj)                               \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ZATHURA_TYPE_NOOPFILEMONITOR,            \
                             ZathuraNoopFileMonitorClass))

typedef struct zathura_noopfilemonitor_s       ZathuraNoopFileMonitor;
typedef struct zathura_noopfilemonitor_class_s ZathuraNoopFileMonitorClass;

struct zathura_noopfilemonitor_class_s
{
  ZathuraFileMonitorClass parent_class;
};

GType zathura_noopfilemonitor_get_type(void) G_GNUC_CONST;

#endif
