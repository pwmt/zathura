/* SPDX-License-Identifier: Zlib */

#ifndef ZATHURA_LANDLOCK_H
#define ZATHURA_LANDLOCK_H

/*
** Remove write and execute permissions
**
** returns 0 on success, 1 if landlock is unsupported, -1 on hard failure
*/
int landlock_drop_write(void);

/*
** Restrict write permissions to XDG_DATA
**
** returns 0 on success, 1 if landlock is unsupported, -1 on hard failure
*/
int landlock_restrict_write(void);

#endif
