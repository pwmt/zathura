/* SPDX-License-Identifier: Zlib */

#ifndef ZATHURA_SECCOMP_FILTERS_H
#define ZATHURA_SECCOMP_FILTERS_H

#include "zathura.h"

/* strict filter before document parsing */
/* this filter is to be enabled after most of the initialisation of zathura has finished */
int seccomp_enable_strict_filter(zathura_t* zathura);

#endif
