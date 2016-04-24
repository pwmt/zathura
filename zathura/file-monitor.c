/* See LICENSE file for license and copyright information */

#include "file-monitor.h"
#include "file-monitor-glib.h"
#include "file-monitor-signal.h"
#include "macros.h"

G_DEFINE_TYPE(ZathuraFileMonitor, zathura_filemonitor, G_TYPE_OBJECT)

typedef struct private_s {
  char* file_path;
} private_t;

#define GET_PRIVATE(obj)                                                       \
  (G_TYPE_INSTANCE_GET_PRIVATE((obj), ZATHURA_TYPE_FILEMONITOR, private_t))

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
  private_t* private               = GET_PRIVATE(file_monitor);

  if (private->file_path != NULL) {
    g_free(private->file_path);
  }

  G_OBJECT_CLASS(zathura_filemonitor_parent_class)->finalize(object);
}

static void
set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
  ZathuraFileMonitor* file_monitor = ZATHURA_FILEMONITOR(object);
  private_t* private               = GET_PRIVATE(file_monitor);

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
  private_t* private               = GET_PRIVATE(file_monitor);

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
  /* add private members */
  g_type_class_add_private(class, sizeof(private_t));

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
  private_t* private = GET_PRIVATE(file_monitor);
  private->file_path = NULL;
}

const char* zathura_filemonitor_get_filepath(ZathuraFileMonitor* filemonitor)
{
  private_t* private = GET_PRIVATE(filemonitor);

  return private->file_path;
}

ZathuraFileMonitor*
zathura_filemonitor_new(const char*                file_path,
                        zathura_filemonitor_type_t filemonitor_type)
{
  g_return_val_if_fail(file_path != NULL, NULL);

  GObject* ret = NULL;
  switch (filemonitor_type) {
    case ZATHURA_FILEMONITOR_GLIB:
      ret = g_object_new(ZATHURA_TYPE_GLIBFILEMONITOR, "file-path", file_path,
                         NULL);
      break;
    case ZATHURA_FILEMONITOR_SIGNAL:
      ret = g_object_new(ZATHURA_TYPE_SIGNALFILEMONITOR, "file-path", file_path,
                         NULL);
      break;
    default:
      g_return_val_if_fail(false, NULL);
  }

  if (ret == NULL) {
    return NULL;
  }

  ZathuraFileMonitor* file_monitor = ZATHURA_FILEMONITOR(ret);
  ZATHURA_FILEMONITOR_GET_CLASS(file_monitor)->start(file_monitor);

  return file_monitor;
}

