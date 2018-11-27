/* SPDX-License-Identifier: Zlib */

#include "file-monitor-signal.h"

#include <girara/utils.h>
#ifdef G_OS_UNIX
#include <glib-unix.h>
#endif

struct zathura_signalfilemonitor_s
{
  ZathuraFileMonitor parent;
  gint               handle;
};

G_DEFINE_TYPE(ZathuraSignalFileMonitor, zathura_signalfilemonitor,
              ZATHURA_TYPE_FILEMONITOR)

static gboolean
signal_handler(gpointer data)
{
  if (data == NULL) {
    return TRUE;
  }

  ZathuraSignalFileMonitor* signalfilemonitor = data;

  girara_debug("SIGHUP received");
  g_signal_emit_by_name(signalfilemonitor, "reload-file");

  return TRUE;
}

static void
start(ZathuraFileMonitor* file_monitor)
{
#ifdef G_OS_UNIX
  ZathuraSignalFileMonitor* signal_file_monitor =
    ZATHURA_SIGNALFILEMONITOR(file_monitor);

  signal_file_monitor->handle =
    g_unix_signal_add(SIGHUP, signal_handler, signal_file_monitor);
#endif
}

static void
stop(ZathuraFileMonitor* file_monitor)
{
#ifdef G_OS_UNIX
  ZathuraSignalFileMonitor* signal_file_monitor =
    ZATHURA_SIGNALFILEMONITOR(file_monitor);

  if (signal_file_monitor->handle > 0) {
    g_source_remove(signal_file_monitor->handle);
  }
#endif
}

static void
zathura_signalfilemonitor_finalize(GObject* object)
{
  stop(ZATHURA_FILEMONITOR(object));

  G_OBJECT_CLASS(zathura_signalfilemonitor_parent_class)->finalize(object);
}

static void
zathura_signalfilemonitor_class_init(ZathuraSignalFileMonitorClass* class)
{
  ZathuraFileMonitorClass* filemonitor_class = ZATHURA_FILEMONITOR_CLASS(class);
  filemonitor_class->start                   = start;
  filemonitor_class->stop                    = stop;

  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->finalize     = zathura_signalfilemonitor_finalize;
}

static void
zathura_signalfilemonitor_init(ZathuraSignalFileMonitor* signalfilemonitor)
{
  signalfilemonitor->handle = 0;
}
