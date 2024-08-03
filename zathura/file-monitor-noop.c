/* SPDX-License-Identifier: Zlib */

#include "file-monitor-noop.h"

#include <girara/utils.h>
#ifdef G_OS_UNIX
#include <glib-unix.h>
#endif

struct zathura_noopfilemonitor_s {
  ZathuraFileMonitor parent;
};

G_DEFINE_TYPE(ZathuraNoopFileMonitor, zathura_noopfilemonitor, ZATHURA_TYPE_FILEMONITOR)

static void start(ZathuraFileMonitor* GIRARA_UNUSED(file_monitor)) {}

static void stop(ZathuraFileMonitor* GIRARA_UNUSED(file_monitor)) {}

static void zathura_noopfilemonitor_finalize(GObject* object) {
  stop(ZATHURA_FILEMONITOR(object));

  G_OBJECT_CLASS(zathura_noopfilemonitor_parent_class)->finalize(object);
}

static void zathura_noopfilemonitor_class_init(ZathuraNoopFileMonitorClass* class) {
  ZathuraFileMonitorClass* filemonitor_class = ZATHURA_FILEMONITOR_CLASS(class);
  filemonitor_class->start                   = start;
  filemonitor_class->stop                    = stop;

  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->finalize     = zathura_noopfilemonitor_finalize;
}

static void zathura_noopfilemonitor_init(ZathuraNoopFileMonitor* GIRARA_UNUSED(noopfilemonitor)) {}
