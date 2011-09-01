/* See LICENSE file for license and copyright information */

#include "commands.h"
#include "zathura.h"
#include "print.h"

bool
cmd_bookmark_create(girara_session_t* session, girara_list_t* argument_list)
{
  return true;
}

bool
cmd_bookmark_delete(girara_session_t* session, girara_list_t* argument_list)
{
  return true;
}

bool
cmd_bookmark_open(girara_session_t* session, girara_list_t* argument_list)
{
  return true;
}

bool
cmd_close(girara_session_t* session, girara_list_t* argument_list)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  if (zathura->document == NULL) {
    // nothing needs to be done
    return true;
  }

  document_close(zathura);

  return true;
}

bool
cmd_info(girara_session_t* session, girara_list_t* argument_list)
{
  return true;
}

bool
cmd_open(girara_session_t* session, girara_list_t* argument_list)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  if (zathura->document) {
    document_close(zathura);
  }

  const int argc = girara_list_size(argument_list);
  if (argc > 2) {
    girara_error("too many arguments");
    return false;
  }
  else if (argc >= 1) {
    document_open(zathura, girara_list_nth(argument_list, 0), (argc == 2) ? girara_list_nth(argument_list, 1) :  NULL);
  }
  else {
    girara_error("no arguments");
    return false;
  }

  return true;
}

bool
cmd_print(girara_session_t* session, girara_list_t* argument_list)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

	if (zathura->document == NULL) {
		girara_error("no document as been opened");
		return false;
	}

	print((zathura_t*) session->global.data);

  return true;
}

bool
cmd_save(girara_session_t* session, girara_list_t* argument_list)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

	if (zathura->document == NULL) {
		girara_error("no document as been opened");
		return false;
	}

  if (girara_list_size(argument_list) == 1) {
    document_save(zathura, girara_list_nth(argument_list, 0), false);
  }
  else {
    girara_error("invalid arguments");
    return false;
  }

  return true;
}

bool
cmd_savef(girara_session_t* session, girara_list_t* argument_list)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

	if (zathura->document == NULL) {
		girara_error("no document as been opened");
		return false;
	}

  if (girara_list_size(argument_list) == 1) {
    document_save(zathura, girara_list_nth(argument_list, 0), true);
  }
  else {
    girara_error("invalid arguments");
    return false;
  }

  return true;
}
