/* SPDX-License-Identifier: Zlib */

#include "file-monitor-glib.h"
#include "macros.h"

#include <girara/utils.h>
#include <gio/gio.h>

struct zathura_glibfilemonitor_s
{
  ZathuraFileMonitor parent;
  GFileMonitor*      monitor; /**< File monitor */
  GFile*             file;    /**< File for file monitor */
};

G_DEFINE_TYPE(ZathuraGLibFileMonitor, zathura_glibfilemonitor,
              ZATHURA_TYPE_FILEMONITOR)

static void
file_changed(GFileMonitor* UNUSED(monitor), GFile* file,
             GFile* UNUSED(other_file), GFileMonitorEvent event,
             ZathuraGLibFileMonitor* file_monitor)
{
  switch (event) {
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    case G_FILE_MONITOR_EVENT_CREATED: {
      char* uri = g_file_get_uri(file);
      girara_debug("received file-monitor event for %s", uri);
      g_free(uri);

      g_signal_emit_by_name(file_monitor, "reload-file");
      break;
    }
    default:
      return;
  }
}

static void
start(ZathuraFileMonitor* file_monitor)
{
  ZathuraGLibFileMonitor* glib_file_monitor =
    ZATHURA_GLIBFILEMONITOR(file_monitor);

  const char* file_path =
    zathura_filemonitor_get_filepath(file_monitor);

  /* install file monitor */
  glib_file_monitor->file = g_file_new_for_path(file_path);
  if (glib_file_monitor->file == NULL) {
    return;
  }

  glib_file_monitor->monitor = g_file_monitor_file(
    glib_file_monitor->file, G_FILE_MONITOR_WATCH_HARD_LINKS, NULL, NULL);
  if (glib_file_monitor->monitor != NULL) {
    g_signal_connect_object(G_OBJECT(glib_file_monitor->monitor), "changed",
                            G_CALLBACK(file_changed), glib_file_monitor, 0);
  }
}

static void
stop(ZathuraFileMonitor* file_monitor)
{
  ZathuraGLibFileMonitor* glib_file_monitor =
    ZATHURA_GLIBFILEMONITOR(file_monitor);

  if (glib_file_monitor->monitor != NULL) {
    g_file_monitor_cancel(glib_file_monitor->monitor);
  }

  g_clear_object(&glib_file_monitor->monitor);
  g_clear_object(&glib_file_monitor->file);
}

static void
dispose(GObject* object)
{
  stop(ZATHURA_FILEMONITOR(object));

  G_OBJECT_CLASS(zathura_glibfilemonitor_parent_class)->dispose(object);
}

static void
finalize(GObject* object)
{
  G_OBJECT_CLASS(zathura_glibfilemonitor_parent_class)->finalize(object);
}

static void
zathura_glibfilemonitor_class_init(ZathuraGLibFileMonitorClass* class)
{
  ZathuraFileMonitorClass* filemonitor_class = ZATHURA_FILEMONITOR_CLASS(class);
  filemonitor_class->start                   = start;
  filemonitor_class->stop                    = stop;

  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->dispose      = dispose;
  object_class->finalize     = finalize;
}

static void
zathura_glibfilemonitor_init(ZathuraGLibFileMonitor* glibfilemonitor)
{
  glibfilemonitor->monitor = NULL;
  glibfilemonitor->file    = NULL;
}
