/* See LICENSE file for license and copyright information */

#ifndef FILEMONITOR_H
#define FILEMONITOR_H

#include <stdbool.h>
#include <girara/types.h>
#include <glib-object.h>

#define ZATHURA_TYPE_FILEMONITOR (zathura_filemonitor_get_type())
#define ZATHURA_FILEMONITOR(obj)                                               \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATHURA_TYPE_FILEMONITOR,                 \
                              ZathuraFileMonitor))
#define ZATHURA_FILEMONITOR_CLASS(obj)                                         \
  (G_TYPE_CHECK_CLASS_CAST((obj), ZATHURA_TYPE_FILEMONITOR,                    \
                           ZathuraFileMonitorClass))
#define ZATHURA_IS_FILEMONITOR(obj)                                            \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATHURA_TYPE_FILEMONITOR))
#define ZATHURA_IS_FILEMONITOR_CLASS(obj)                                      \
  (G_TYPE_CHECK_CLASS_TYPE((obj), ZATHURA_TYPE_FILEMONITOR))
#define ZATHURA_FILEMONITOR_GET_CLASS(obj)                                     \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ZATHURA_TYPE_FILEMONITOR,                  \
                             ZathuraFileMonitorClass))

typedef struct zathura_filemonitor_s       ZathuraFileMonitor;
typedef struct zathura_filemonitor_class_s ZathuraFileMonitorClass;

/**
 * Base class for all file monitors.
 *
 * The signal 'reload-file' is emitted if the monitored file changed.
 */
struct zathura_filemonitor_s
{
  GObject parent;
};

struct zathura_filemonitor_class_s
{
  GObjectClass parent_class;

  void (*start)(ZathuraFileMonitor*);
  void (*stop)(ZathuraFileMonitor*);
};

/**
 * Get the type of the filemonitor.
 *
 * @return the type
 */
GType zathura_filemonitor_get_type(void) G_GNUC_CONST;

/**
 * Type of file monitor.
 */
typedef enum zathura_filemonitor_type_e {
  ZATHURA_FILEMONITOR_GLIB, /**< Use filemonitor from GLib */
  ZATHURA_FILEMONITOR_SIGNAL /**< Reload when receiving SIGHUP */
} zathura_filemonitor_type_t;

/**
 * Create a new file monitor.
 *
 * @param file_path file to monitor
 * @param filemonitor_type type of file monitor
 * @return new file monitor instance
 */
ZathuraFileMonitor*
zathura_filemonitor_new(const char*                file_path,
                        zathura_filemonitor_type_t filemonitor_type);

/**
 * Get path of the monitored file.
 *
 * @return path of monitored file
 */
const char* zathura_filemonitor_get_filepath(ZathuraFileMonitor* file_monitor);

/**
 * Start file monitor.
 */
void zathura_filemonitor_start(ZathuraFileMonitor* file_monitor);

/**
 * Stop file monitor.
 */
void zathura_filemonitor_stop(ZathuraFileMonitor* file_monitor);

#endif
