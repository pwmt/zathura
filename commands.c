/* See LICENSE file for license and copyright information */

#include "commands.h"
#include "zathura.h"

bool
cmd_bookmark_create(girara_session_t* session, int argc, char** argv)
{
  return true;
}

bool
cmd_bookmark_delete(girara_session_t* session, int argc, char** argv)
{
  return true;
}

bool
cmd_bookmark_open(girara_session_t* session, int argc, char** argv)
{
  return true;
}

bool
cmd_close(girara_session_t* session, int argc, char** argv)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  g_return_val_if_fail(zathura->document != NULL, false);

  document_close(zathura);

  return true;
}

bool
cmd_info(girara_session_t* session, int argc, char** argv)
{
  return true;
}

bool
cmd_print(girara_session_t* session, int argc, char** argv)
{
  return true;
}

bool
cmd_save(girara_session_t* session, int argc, char** argv)
{
  return true;
}
