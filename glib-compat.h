/* See LICENSE file for license and copyright information */

#ifndef GLIB_COMPAT_H
#define GLIB_COMPAT_H

#include <glib.h>

/* GStaticMutex is deprecated starting with glib 2.32 */
#if !GLIB_CHECK_VERSION(2, 31, 0)
#define mutex GStaticMutex
#define mutex_init(m) g_static_mutex_init((m))
#define mutex_lock(m) g_static_mutex_lock((m))
#define mutex_unlock(m) g_static_mutex_unlock((m))
#define mutex_free(m) g_static_mutex_free((m))
#else
#define mutex GMutex
#define mutex_init(m) g_mutex_init((m))
#define mutex_lock(m) g_mutex_lock((m))
#define mutex_unlock(m) g_mutex_unlock((m))
#define mutex_free(m) g_mutex_clear((m))
#endif

/* g_get_real_time appeared in 2.28 */
#if !GLIB_CHECK_VERSION(2, 27, 0)
inline static gint64 g_get_real_time(void)
{
  GTimeVal tv;
  g_get_current_time(&tv);
  return (((gint64) tv.tv_sec) * 1000000) + tv.tv_usec;
}
#endif

#endif
