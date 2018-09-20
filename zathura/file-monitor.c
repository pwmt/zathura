/* See LICENSE file for license and copyright information */

#include "file-monitor.h"
#include "file-monitor-glib.h"
#ifdef G_OS_UNIX
#include "file-monitor-signal.h"
#endif
#include "file-monitor-noop.h"
#include "macros.h"

#include <girara/utils.h>

typedef struct private_s {
  char* file_path;
} ZathuraFileMonitorPrivate;

G_DEFINE_TYPE_WITH_CODE(ZathuraFileMonitor, zathura_filemonitor, G_TYPE_OBJECT, G_ADD_PRIVATE(ZathuraFileMonitor))

enum {
  PROP_0,
  PROP_FILE_PATH
};

enum {
  RELOAD_FILE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
finalize(GObject* object)
{
  ZathuraFileMonitor* file_monitor = ZATHURA_FILEMONITOR(object);
  ZathuraFileMonitorPrivate* private               = zathura_filemonitor_get_instance_private(file_monitor);

  if (private->file_path != NULL) {
    g_free(private->file_path);
  }

  G_OBJECT_CLASS(zathura_filemonitor_parent_class)->finalize(object);
}

static void
set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
  ZathuraFileMonitor* file_monitor = ZATHURA_FILEMONITOR(object);
  ZathuraFileMonitorPrivate* private               = zathura_filemonitor_get_instance_private(file_monitor);

  switch (prop_id) {
    case PROP_FILE_PATH:
      if (private->file_path != NULL) {
        g_free(private->file_path);
      }
      private->file_path = g_value_dup_string(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec)
{
  ZathuraFileMonitor* file_monitor = ZATHURA_FILEMONITOR(object);
  ZathuraFileMonitorPrivate* private               = zathura_filemonitor_get_instance_private(file_monitor);

  switch (prop_id) {
    case PROP_FILE_PATH:
      g_value_set_string(value, private->file_path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
zathura_filemonitor_class_init(ZathuraFileMonitorClass* class)
{
  /* set up methods */
  class->start = NULL;
  class->stop  = NULL;

  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->finalize     = finalize;
  object_class->set_property = set_property;
  object_class->get_property = get_property;

  /* add properties */
  g_object_class_install_property(
    object_class, PROP_FILE_PATH,
    g_param_spec_string("file-path", "file-path", "file path to monitor", NULL,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  /* add signals */
  signals[RELOAD_FILE] =
    g_signal_new("reload-file", ZATHURA_TYPE_FILEMONITOR, G_SIGNAL_RUN_LAST, 0,
                 NULL, NULL, g_cclosure_marshal_generic, G_TYPE_NONE, 0);
}

static void
zathura_filemonitor_init(ZathuraFileMonitor* file_monitor)
{
  ZathuraFileMonitorPrivate* private = zathura_filemonitor_get_instance_private(file_monitor);
  private->file_path                 = NULL;
}

const char* zathura_filemonitor_get_filepath(ZathuraFileMonitor* file_monitor)
{
  ZathuraFileMonitorPrivate* private = zathura_filemonitor_get_instance_private(file_monitor);
  return private->file_path;
}

void zathura_filemonitor_start(ZathuraFileMonitor* file_monitor)
{
  ZATHURA_FILEMONITOR_GET_CLASS(file_monitor)->start(file_monitor);
}

void zathura_filemonitor_stop(ZathuraFileMonitor* file_monitor)
{
  ZATHURA_FILEMONITOR_GET_CLASS(file_monitor)->stop(file_monitor);
}

ZathuraFileMonitor*
zathura_filemonitor_new(const char*                file_path,
                        zathura_filemonitor_type_t filemonitor_type)
{
  g_return_val_if_fail(file_path != NULL, NULL);

  GObject* ret = NULL;
  switch (filemonitor_type) {
    case ZATHURA_FILEMONITOR_GLIB:
      girara_debug("using glib file monitor");
      ret = g_object_new(ZATHURA_TYPE_GLIBFILEMONITOR, "file-path", file_path,
                         NULL);
      break;
#ifdef G_OS_UNIX
    case ZATHURA_FILEMONITOR_SIGNAL:
      girara_debug("using SIGHUP file monitor");
      ret = g_object_new(ZATHURA_TYPE_SIGNALFILEMONITOR, "file-path", file_path,
                         NULL);
      break;
#endif
    case ZATHURA_FILEMONITOR_NOOP:
      girara_debug("using noop file monitor");
      ret = g_object_new(ZATHURA_TYPE_NOOPFILEMONITOR, "file-path", file_path,
                         NULL);
      break;
    default:
      girara_debug("invalid filemonitor type: %d", filemonitor_type);
      g_return_val_if_fail(false, NULL);
  }

  if (ret == NULL) {
    return NULL;
  }

  return ZATHURA_FILEMONITOR(ret);
}

