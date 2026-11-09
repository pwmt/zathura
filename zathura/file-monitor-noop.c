/* SPDX-License-Identifier: Zlib */

#include "file-monitor-noop.h"

#include <girara/macros.h>

struct zathura_noopfilemonitor_s {
  ZathuraFileMonitor parent;
};

G_DEFINE_TYPE(ZathuraNoopFileMonitor, zathura_noopfilemonitor, ZATHURA_TYPE_FILEMONITOR)

static void noop(ZathuraFileMonitor* GIRARA_UNUSED(file_monitor)) {}

static void zathura_noopfilemonitor_class_init(ZathuraNoopFileMonitorClass* class) {
  ZathuraFileMonitorClass* filemonitor_class = ZATHURA_FILEMONITOR_CLASS(class);
  filemonitor_class->start                   = noop;
  filemonitor_class->stop                    = noop;
}

static void zathura_noopfilemonitor_init(ZathuraNoopFileMonitor* GIRARA_UNUSED(noopfilemonitor)) {}
