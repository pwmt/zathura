/* See LICENSE file for license and copyright information */

#ifndef ZATHURA_H
#define ZATHURA_H

#include <stdbool.h>
#include <girara.h>

struct
{
  struct 
  {
    girara_session_t* session; /**> girara interface session */
  } UI;
} Zathura;

/**
 * Initializes zathura
 *
 * @return If no error occured true, otherwise false, is returned.
 */

bool init_zathura();

#endif // ZATHURA_H
