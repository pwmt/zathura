/* See LICENSE file for license and copyright information */

#include "commands.h"
#include "bookmarks.h"
#include "database.h"
#include "zathura.h"
#include "print.h"
#include "document.h"

bool
cmd_bookmark_create(girara_session_t* UNUSED(session), girara_list_t*
    UNUSED(argument_list))
{
  return true;
}

bool
cmd_bookmark_delete(girara_session_t* session, girara_list_t* argument_list)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  if (zathura->document == NULL) {
    girara_notify(session, GIRARA_ERROR, "No document opened.");
    return false;
  }

  const unsigned int argc = girara_list_size(argument_list);
  if (argc != 1) {
    girara_notify(session, GIRARA_ERROR, "Invalid number of arguments given.");
    return false;
  }

  const char* bookmark = girara_list_nth(argument_list, 0);
  if (zathura_bookmark_remove(zathura, bookmark)) {
    girara_notify(session, GIRARA_INFO, "Removed bookmark: %s", bookmark);
  } else {
    girara_notify(session, GIRARA_ERROR, "Failed to remove bookmark: %s", bookmark);
  }

  return true;
}

bool
cmd_bookmark_open(girara_session_t* UNUSED(session), girara_list_t*
    UNUSED(argument_list))
{
  return true;
}

bool
cmd_close(girara_session_t* session, girara_list_t* UNUSED(argument_list))
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
cmd_info(girara_session_t* UNUSED(session), girara_list_t*
    UNUSED(argument_list))
{
  return true;
}

bool
cmd_help(girara_session_t* UNUSED(session), girara_list_t*
    UNUSED(argument_list))
{
  return true;
}

bool
cmd_open(girara_session_t* session, girara_list_t* argument_list)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  const int argc = girara_list_size(argument_list);
  if (argc > 2) {
    girara_notify(session, GIRARA_ERROR, "Too many arguments.");
    return false;
  }
  else if (argc >= 1) {
    if (zathura->document) {
      document_close(zathura);
    }

    document_open(zathura, girara_list_nth(argument_list, 0), (argc == 2) ? girara_list_nth(argument_list, 1) :  NULL);
  }
  else {
    girara_notify(session, GIRARA_ERROR, "No arguments given.");
    return false;
  }

  return true;
}

bool
cmd_print(girara_session_t* session, girara_list_t* UNUSED(argument_list))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  if (zathura->document == NULL) {
    girara_notify(session, GIRARA_ERROR, "No open document.");
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
    girara_notify(session, GIRARA_ERROR, "No open document.");
    return false;
  }

  if (girara_list_size(argument_list) == 1) {
    document_save(zathura, girara_list_nth(argument_list, 0), false);
  }
  else {
    girara_notify(session, GIRARA_ERROR, "Invalid number of arguments.");
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
    girara_notify(session, GIRARA_ERROR, "No open document.");
    return false;
  }

  if (girara_list_size(argument_list) == 1) {
    document_save(zathura, girara_list_nth(argument_list, 0), true);
  }
  else {
    girara_notify(session, GIRARA_ERROR, "Invalid number of arguments.");
    return false;
  }

  return true;
}
