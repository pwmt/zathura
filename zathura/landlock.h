/* SPDX-License-Identifier: Zlib */

#ifndef ZATHURA_LANDLOCK_H
#define ZATHURA_LANDLOCK_H

/*
** remove write and execute permissions
*/
void landlock_drop_write (void);

/*
** restrict write permissions to XDG_DATA
*/
void landlock_restrict_write (void);

#endif
