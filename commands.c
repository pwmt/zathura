/* See LICENSE file for license and copyright information */

#include <string.h>
#include <stdlib.h>
#include <glib/gi18n.h>

#include "commands.h"
#include "shortcuts.h"
#include "bookmarks.h"
#include "database.h"
#include "document.h"
#include "zathura.h"
#include "print.h"
#include "document.h"
#include "utils.h"
#include "page-widget.h"
#include "page.h"

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
    girara_notify(session, GIRARA_ERROR, _("No document opened."));
    return false;
  }

  const unsigned int argc = girara_list_size(argument_list);
  if (argc != 1) {
    girara_notify(session, GIRARA_ERROR, _("Invalid number of arguments given."));
    return false;
  }

  const char* bookmark_name = girara_list_nth(argument_list, 0);
  zathura_bookmark_t* bookmark = zathura_bookmark_get(zathura, bookmark_name);
  if (bookmark != NULL) {
    bookmark->page = zathura_document_get_current_page_number(zathura->document) + 1;
    girara_notify(session, GIRARA_INFO, _("Bookmark successfuly updated: %s"), bookmark_name);
    return true;
  }

  bookmark = zathura_bookmark_add(zathura, bookmark_name, zathura_document_get_current_page_number(zathura->document) + 1);
  if (bookmark == NULL) {
    girara_notify(session, GIRARA_ERROR, _("Could not create bookmark: %s"), bookmark_name);
    return false;
  }

  girara_notify(session, GIRARA_INFO, _("Bookmark successfuly created: %s"), bookmark_name);
  return true;
}

bool
cmd_bookmark_delete(girara_session_t* session, girara_list_t* argument_list)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  if (zathura->document == NULL) {
    girara_notify(session, GIRARA_ERROR, _("No document opened."));
    return false;
  }

  const unsigned int argc = girara_list_size(argument_list);
  if (argc != 1) {
    girara_notify(session, GIRARA_ERROR, _("Invalid number of arguments given."));
    return false;
  }

  const char* bookmark = girara_list_nth(argument_list, 0);
  if (zathura_bookmark_remove(zathura, bookmark)) {
    girara_notify(session, GIRARA_INFO, _("Removed bookmark: %s"), bookmark);
  } else {
    girara_notify(session, GIRARA_ERROR, _("Failed to remove bookmark: %s"), bookmark);
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
    girara_notify(session, GIRARA_ERROR, _("No document opened."));
    return false;
  }

  const unsigned int argc = girara_list_size(argument_list);
  if (argc != 1) {
    girara_notify(session, GIRARA_ERROR, _("Invalid number of arguments given."));
    return false;
  }

  const char* bookmark_name = girara_list_nth(argument_list, 0);
  zathura_bookmark_t* bookmark = zathura_bookmark_get(zathura, bookmark_name);
  if (bookmark == NULL) {
    girara_notify(session, GIRARA_ERROR, _("No such bookmark: %s"), bookmark_name);
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

  document_close(zathura, false);

  return true;
}

bool
cmd_info(girara_session_t* session, girara_list_t* UNUSED(argument_list))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  if (zathura->document == NULL) {
    girara_notify(session, GIRARA_ERROR, _("No document opened."));
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
      g_string_append(string, text);

      g_free(text);
      g_free(tmp);
    }
  }

  if (strlen(string->str) > 0) {
    g_string_erase(string, strlen(string->str) - 1, 1);
    girara_notify(session, GIRARA_INFO, "%s", string->str);
  } else {
    girara_notify(session, GIRARA_INFO, _("No information available."));
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
    girara_notify(session, GIRARA_ERROR, _("Too many arguments."));
    return false;
  } else if (argc >= 1) {
    if (zathura->document != NULL) {
      document_close(zathura, false);
    }

    document_open(zathura, girara_list_nth(argument_list, 0), (argc == 2) ? girara_list_nth(argument_list, 1) :  NULL);
  } else {
    girara_notify(session, GIRARA_ERROR, _("No arguments given."));
    return false;
  }

  return true;
}

bool
cmd_quit(girara_session_t* session, girara_list_t* UNUSED(argument_list))
{
  sc_quit(session, NULL, NULL, 0);

  return true;
}

bool
cmd_print(girara_session_t* session, girara_list_t* UNUSED(argument_list))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  if (zathura->document == NULL) {
    girara_notify(session, GIRARA_ERROR, _("No document opened."));
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
    girara_notify(session, GIRARA_ERROR, _("No document opened."));
    return false;
  }

  if (girara_list_size(argument_list) == 1) {
    if (document_save(zathura, girara_list_nth(argument_list, 0), false) == true) {
      girara_notify(session, GIRARA_INFO, _("Document saved."));
    } else {
      girara_notify(session, GIRARA_INFO, _("Failed to save document."));
    }
  } else {
    girara_notify(session, GIRARA_ERROR, _("Invalid number of arguments."));
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
    girara_notify(session, GIRARA_ERROR, _("No document opened."));
    return false;
  }

  if (girara_list_size(argument_list) == 1) {
    if (document_save(zathura, girara_list_nth(argument_list, 0), true) == true) {
      girara_notify(session, GIRARA_INFO, _("Document saved."));
    } else {
      girara_notify(session, GIRARA_INFO, _("Failed to save document."));
    }
  } else {
    girara_notify(session, GIRARA_ERROR, _("Invalid number of arguments."));
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
  zathura_error_t error = ZATHURA_ERROR_OK;


  unsigned int number_of_pages     = zathura_document_get_number_of_pages(zathura->document);
  unsigned int current_page_number = zathura_document_get_current_page_number(zathura->document);

  for (unsigned int page_id = 0; page_id < number_of_pages; ++page_id) {
		unsigned int index = (page_id + current_page_number) % number_of_pages;
    zathura_page_t* page = zathura_document_get_page(zathura->document, index);
    if (page == NULL) {
      continue;
    }

    GtkWidget* page_widget = zathura_page_get_widget(page);
    g_object_set(page_widget, "draw-links", FALSE, NULL);

    girara_list_t* result = zathura_page_search_text(page, input, &error);
    if (result == NULL || girara_list_size(result) == 0) {
      girara_list_free(result);
      g_object_set(page_widget, "search-results", NULL, NULL);

      if (error == ZATHURA_ERROR_NOT_IMPLEMENTED) {
        break;
      } else {
        continue;
      }
    }

    g_object_set(page_widget, "search-results", result, NULL);
    if (firsthit == true) {
      if (page_id != 0) {
        page_set_delayed(zathura, zathura_page_get_index(page));
      }
      g_object_set(page_widget, "search-current", 0, NULL);
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
    girara_notify(session, GIRARA_ERROR, _("No document opened."));
    return false;
  }

  if (girara_list_size(argument_list) != 2) {
    girara_notify(session, GIRARA_ERROR, _("Invalid number of arguments given."));
    return false;
  }

  const char* attachment_name = girara_list_nth(argument_list, 0);
  const char* file_name       = girara_list_nth(argument_list, 1);

  if (file_name == NULL || attachment_name == NULL) {
    return false;
  }
  char* file_name2 = girara_fix_path(file_name);

  if (zathura_document_attachment_save(zathura->document, attachment_name, file_name) == false) {
    girara_notify(session, GIRARA_ERROR, _("Couldn't write attachment '%s' to '%s'."), attachment_name, file_name);
  } else {
    girara_notify(session, GIRARA_INFO, _("Wrote attachment '%s' to '%s'."), attachment_name, file_name2);
  }

  g_free(file_name2);

  return true;
}

bool
cmd_offset(girara_session_t* session, girara_list_t* argument_list)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;
  if (zathura->document == NULL) {
    girara_notify(session, GIRARA_ERROR, _("No document opened."));
    return false;
  }

  /* no argument: take current page as offset */
  unsigned int page_offset = zathura_document_get_current_page_number(zathura->document);

  /* retrieve offset from argument */
  if (girara_list_size(argument_list) == 1) {
    const char* value = girara_list_nth(argument_list, 0);
    if (value != NULL) {
      page_offset = atoi(value);
      if (page_offset == 0 && strcmp(value, "0") != 0) {
        girara_notify(session, GIRARA_WARNING, _("Argument must be a number."));
        return false;
      }
    }
  }

  if (page_offset < zathura_document_get_number_of_pages(zathura->document)) {
    zathura_document_set_page_offset(zathura->document, page_offset);
  }

  return true;
}
