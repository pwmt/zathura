/* SPDX-License-Identifier: Zlib */

#ifndef FILEMONITOR_SIGNAL_H
#define FILEMONITOR_SIGNAL_H

#include "file-monitor.h"

#define ZATHURA_TYPE_SIGNALFILEMONITOR (zathura_signalfilemonitor_get_type())
#define ZATHURA_SIGNALFILEMONITOR(obj)                                         \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATHURA_TYPE_SIGNALFILEMONITOR,           \
                              ZathuraSignalFileMonitor))
#define ZATHURA_SIGNALFILEMONITOR_CLASS(obj)                                   \
  (G_TYPE_CHECK_CLASS_CAST((obj), ZATHURA_TYPE_SIGNALFILEMONITOR,              \
                           ZathuraSignalFileMonitorClass))
#define ZATHURA_IS_SIGNALFILEMONITOR(obj)                                      \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATHURA_TYPE_SIGNALFILEMONITOR))
#define ZATHURA_IS_SIGNALFILEMONITOR_CLASS(obj)                                \
  (G_TYPE_CHECK_CLASS_TYPE((obj), ZATHURA_TYPE_SIGNALFILEMONITOR))
#define ZATHURA_SIGNALFILEMONITOR_GET_CLASS(obj)                               \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ZATHURA_TYPE_SIGNALFILEMONITOR,            \
                             ZathuraSignalFileMonitorClass))

typedef struct zathura_signalfilemonitor_s       ZathuraSignalFileMonitor;
typedef struct zathura_signalfilemonitor_class_s ZathuraSignalFileMonitorClass;

struct zathura_signalfilemonitor_class_s
{
  ZathuraFileMonitorClass parent_class;
};

GType zathura_signalfilemonitor_get_type(void) G_GNUC_CONST;

#endif
