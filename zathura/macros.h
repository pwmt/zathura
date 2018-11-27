/* SPDX-License-Identifier: Zlib */

#ifndef ZATHURA_MACROS_H
#define ZATHURA_MACROS_H

#include <girara/macros.h>

#define UNUSED(x) GIRARA_UNUSED(x)
#define DEPRECATED(x) GIRARA_DEPRECATED(x)
#define ZATHURA_PLUGIN_API GIRARA_VISIBLE

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#endif
