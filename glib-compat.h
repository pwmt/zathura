/* See LICENSE file for license and copyright information */

#ifndef GLIB_COMPAT_H
#define GLIB_COMPAT_H

#include <glib.h>

/* GStaticMutex is deprecated starting with glib 2.32 and got replaced with
 * GMutex */
#if GLIB_CHECK_VERSION(2, 32, 0)
#define mutex GMutex
#define mutex_init(m) g_mutex_init((m))
#define mutex_lock(m) g_mutex_lock((m))
#define mutex_unlock(m) g_mutex_unlock((m))
#define mutex_free(m) g_mutex_clear((m))
#else
#define mutex GStaticMutex
#define mutex_init(m) g_static_mutex_init((m))
#define mutex_lock(m) g_static_mutex_lock((m))
#define mutex_unlock(m) g_static_mutex_unlock((m))
#define mutex_free(m) g_static_mutex_free((m))
#endif

#endif
