/* See LICENSE file for license and copyright information */

#include <string.h>

#include "commands.h"
#include "bookmarks.h"
#include "database.h"
#include "document.h"
#include "zathura.h"
#include "print.h"
#include "document.h"
#include "utils.h"
#include "page_widget.h"


#include <girara/session.h>
#include <girara/datastructures.h>
#include <girara/utils.h>

bool
cmd_bookmark_create(girara_session_t* session, girara_list_t* argument_list)
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

  const char* bookmark_name = girara_list_nth(argument_list, 0);
  zathura_bookmark_t* bookmark = zathura_bookmark_get(zathura, bookmark_name);
  if (bookmark != NULL) {
    bookmark->page = zathura->document->current_page_number + 1;
    girara_notify(session, GIRARA_INFO, "Bookmark successfuly updated: %s", bookmark_name);
    return true;
  }

  bookmark = zathura_bookmark_add(zathura, bookmark_name, zathura->document->current_page_number + 1);
  if (bookmark == NULL) {
    girara_notify(session, GIRARA_ERROR, "Could not create bookmark: %s", bookmark_name);
    return false;
  }

  girara_notify(session, GIRARA_INFO, "Bookmark successfuly created: %s", bookmark_name);
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
cmd_bookmark_open(girara_session_t* session, girara_list_t* argument_list)
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

  const char* bookmark_name = girara_list_nth(argument_list, 0);
  zathura_bookmark_t* bookmark = zathura_bookmark_get(zathura, bookmark_name);
  if (bookmark == NULL) {
    girara_notify(session, GIRARA_ERROR, "No such bookmark: %s", bookmark_name);
    return false;
  }

  return page_set(zathura, bookmark->page - 1);
}

bool
cmd_close(girara_session_t* session, girara_list_t* UNUSED(argument_list))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  if (zathura->document == NULL) {
    return true;
  }

  document_close(zathura);

  return true;
}

bool
cmd_info(girara_session_t* session, girara_list_t* UNUSED(argument_list))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  if (zathura->document == NULL) {
    girara_notify(session, GIRARA_ERROR, "No document opened.");
    return false;
  }

  struct meta_field {
    char* name;
    zathura_document_meta_t field;
  };

  struct meta_field meta_fields[] = {
    { "Title",            ZATHURA_DOCUMENT_TITLE },
    { "Author",           ZATHURA_DOCUMENT_AUTHOR },
    { "Subject",          ZATHURA_DOCUMENT_SUBJECT },
    { "Keywords",         ZATHURA_DOCUMENT_KEYWORDS },
    { "Creator",          ZATHURA_DOCUMENT_CREATOR },
    { "Producer",         ZATHURA_DOCUMENT_PRODUCER },
    { "Creation date",    ZATHURA_DOCUMENT_CREATION_DATE },
    { "Modiciation date", ZATHURA_DOCUMENT_MODIFICATION_DATE }
  };

  GString* string = g_string_new(NULL);
  if (string == NULL) {
    return true;
  }

  for (unsigned int i = 0; i < LENGTH(meta_fields); i++) {
    char* tmp = zathura_document_meta_get(zathura->document, meta_fields[i].field, NULL);
    if (tmp != NULL) {
      char* text = g_strdup_printf("<b>%s:</b> %s\n", meta_fields[i].name, tmp);
      if (text == NULL) {
        g_free(tmp);
        return true;
      }

      g_string_append(string, text);

      g_free(text);
      g_free(tmp);
    }
  }

  if (strlen(string->str) > 0) {
    g_string_erase(string, strlen(string->str) - 1, 1);
    girara_notify(session, GIRARA_INFO, "%s", string->str);
  }

  g_string_free(string, TRUE);

  return false;
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
  } else if (argc >= 1) {
    if (zathura->document) {
      document_close(zathura);
    }

    document_open(zathura, girara_list_nth(argument_list, 0), (argc == 2) ? girara_list_nth(argument_list, 1) :  NULL);
  } else {
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
  } else {
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
  } else {
    girara_notify(session, GIRARA_ERROR, "Invalid number of arguments.");
    return false;
  }

  return true;
}

bool
cmd_search(girara_session_t* session, const char* input, girara_argument_t* argument)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(input != NULL, false);
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  if (zathura->document == NULL || strlen(input) == 0) {
    return false;
  }

  bool firsthit = true;
  zathura_plugin_error_t error = ZATHURA_PLUGIN_ERROR_OK;

  for (unsigned int page_id = 0; page_id < zathura->document->number_of_pages; ++page_id) {
    zathura_page_t* page = zathura->document->pages[(page_id + zathura->document->current_page_number) % zathura->document->number_of_pages];
    if (page == NULL) {
      continue;
    }

    g_object_set(page->drawing_area, "draw-links", FALSE, NULL);

    girara_list_t* result = zathura_page_search_text(page, input, &error);
    if (result == NULL || girara_list_size(result) == 0) {
      girara_list_free(result);
      g_object_set(page->drawing_area, "search-results", NULL, NULL);

      if (error == ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED) {
        break;
      } else {
        continue;
      }
    }

    g_object_set(page->drawing_area, "search-results", result, NULL);
    if (firsthit == true) {
      if (page_id != 0) {
        page_set_delayed(zathura, page->number);
      }
      g_object_set(page->drawing_area, "search-current", 0, NULL);
      firsthit = false;
    }
  }

  return true;
}

bool
cmd_export(girara_session_t* session, girara_list_t* argument_list)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  if (zathura->document == NULL) {
    girara_notify(session, GIRARA_ERROR, "No document opened.");
    return false;
  }

  const unsigned int argc = girara_list_size(argument_list);
  if (argc != 2) {
    girara_notify(session, GIRARA_ERROR, "Invalid number of arguments given.");
    return false;
  }

  const char* attachment_name = girara_list_nth(argument_list, 0);
  const char* file_name       = girara_list_nth(argument_list, 1);

  if (file_name == NULL || attachment_name == NULL) {
    return false;
  }
  char* file_name2 = girara_fix_path(file_name);

  if (zathura_document_attachment_save(zathura->document, attachment_name, file_name) == false) {
    girara_notify(session, GIRARA_ERROR, "Couldn't write attachment '%s' to '%s'.", attachment_name, file_name);
  } else {
    girara_notify(session, GIRARA_INFO, "Wrote attachment '%s' to '%s'.", attachment_name, file_name2);
  }

  g_free(file_name2);

  return true;
}
