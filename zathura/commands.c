/* SPDX-License-Identifier: Zlib */

#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

#include "adjustment.h"
#include "bookmarks.h"
#include "commands.h"
#include "config.h"
#include "database.h"
#include "dbus-interface.h"
#include "document.h"
#include "internal.h"
#include "page-widget.h"
#include "page.h"
#include "plugin.h"
#include "print.h"
#include "render.h"
#include "shortcuts.h"
#include "utils.h"
#include "zathura.h"

#include <girara/commands.h>
#include <girara/datastructures.h>
#include <girara/session.h>
#include <girara/settings.h>
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

  const char*         bookmark_name = girara_list_nth(argument_list, 0);
  zathura_bookmark_t* bookmark      = zathura_bookmark_get(zathura, bookmark_name);
  bool                update        = bookmark != NULL ? true : false;

  bookmark =
    zathura_bookmark_add(zathura, bookmark_name, zathura_document_get_current_page_number(zathura->document) + 1);
  if (bookmark == NULL) {
    if (update == true) {
      girara_notify(session, GIRARA_ERROR, _("Could not update bookmark: %s"), bookmark_name);
    } else {
      girara_notify(session, GIRARA_ERROR, _("Could not create bookmark: %s"), bookmark_name);
    }
    return false;
  } else {
    if (update == true) {
      girara_notify(session, GIRARA_INFO, _("Bookmark successfully updated: %s"), bookmark_name);
    } else {
      girara_notify(session, GIRARA_INFO, _("Bookmark successfully created: %s"), bookmark_name);
    }
  }

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
    GString* string = g_string_new(NULL);
    for (size_t idx = 0; idx != girara_list_size(zathura->bookmarks.bookmarks); ++idx) {
      zathura_bookmark_t* bookmark = girara_list_nth(zathura->bookmarks.bookmarks, idx);
      g_string_append_printf(string, "<b>%s</b>: %u\n", bookmark->id, bookmark->page);
    }

    if (strlen(string->str) > 0) {
      g_string_erase(string, strlen(string->str) - 1, 1);
      girara_notify(session, GIRARA_INFO, "%s", string->str);
    } else {
      girara_notify(session, GIRARA_INFO, _("No bookmarks available."));
    }

    g_string_free(string, TRUE);
    return false;
  }

  const char*         bookmark_name = girara_list_nth(argument_list, 0);
  zathura_bookmark_t* bookmark      = zathura_bookmark_get(zathura, bookmark_name);
  if (bookmark == NULL) {
    girara_notify(session, GIRARA_ERROR, _("No such bookmark: %s"), bookmark_name);
    return false;
  }

  zathura_jumplist_add(zathura);
  page_set(zathura, bookmark->page - 1);
  if (bookmark->x != DBL_MIN && bookmark->y != DBL_MIN) {
    position_set(zathura, bookmark->x, bookmark->y);
  }
  zathura_jumplist_add(zathura);

  return true;
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
    const char*                         name;
    zathura_document_information_type_t field;
  };

  const struct meta_field meta_fields[] = {{_("Title"), ZATHURA_DOCUMENT_INFORMATION_TITLE},
                                           {_("Subject"), ZATHURA_DOCUMENT_INFORMATION_SUBJECT},
                                           {_("Keywords"), ZATHURA_DOCUMENT_INFORMATION_KEYWORDS},
                                           {_("Author"), ZATHURA_DOCUMENT_INFORMATION_AUTHOR},
                                           {_("Creator"), ZATHURA_DOCUMENT_INFORMATION_CREATOR},
                                           {_("Producer"), ZATHURA_DOCUMENT_INFORMATION_PRODUCER},
                                           {_("Creation date"), ZATHURA_DOCUMENT_INFORMATION_CREATION_DATE},
                                           {_("Modification date"), ZATHURA_DOCUMENT_INFORMATION_MODIFICATION_DATE},
                                           {_("Format"), ZATHURA_DOCUMENT_INFORMATION_FORMAT},
                                           {_("Other"), ZATHURA_DOCUMENT_INFORMATION_OTHER}};

  girara_list_t* information = zathura_document_get_information(zathura->document, NULL);
  if (information == NULL) {
    girara_notify(session, GIRARA_INFO, _("No information available."));
    return false;
  }

  GString* string = g_string_new(NULL);

  for (unsigned int i = 0; i < LENGTH(meta_fields); i++) {
    for (size_t idx = 0; idx != girara_list_size(information); ++idx) {
      zathura_document_information_entry_t* entry = girara_list_nth(information, idx);
      if (entry != NULL) {
        if (meta_fields[i].field == entry->type) {
          g_string_append_printf(string, "<b>%s:</b> %s\n", meta_fields[i].name, entry->value);
        }
      }
    }
  }

  if (string->len > 0) {
    g_string_erase(string, string->len - 1, 1);
    girara_notify(session, GIRARA_INFO, "%s", string->str);
  } else {
    girara_notify(session, GIRARA_INFO, _("No information available."));
  }

  g_string_free(string, TRUE);

  return false;
}

bool
cmd_help(girara_session_t* UNUSED(session), girara_list_t* UNUSED(argument_list))
{
  return true;
}

bool
cmd_hlsearch(girara_session_t* session, girara_list_t* UNUSED(argument_list))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  document_draw_search_results(zathura, true);
  render_all(zathura);

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

    document_open_idle(zathura, girara_list_nth(argument_list, 0),
                       (argc == 2) ? girara_list_nth(argument_list, 1) : NULL, ZATHURA_PAGE_NUMBER_UNSPECIFIED, NULL,
                       NULL, NULL, NULL);
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

  if (zathura->global.sandbox == ZATHURA_SANDBOX_STRICT) {
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("Printing is not permitted in strict sandbox mode"));
    return false;
  }

  print(zathura);

  return true;
}

bool
cmd_nohlsearch(girara_session_t* session, girara_list_t* UNUSED(argument_list))
{
  sc_nohlsearch(session, NULL, NULL, 0);

  return true;
}

bool
cmd_save(girara_session_t* session, girara_list_t* argument_list)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  if (zathura->global.sandbox == ZATHURA_SANDBOX_STRICT) {
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("Saving is not permitted in strict sandbox mode"));
    return false;
  }

  if (zathura->document == NULL) {
    girara_notify(session, GIRARA_ERROR, _("No document opened."));
    return false;
  }

  if (girara_list_size(argument_list) == 1) {
    document_save(zathura, girara_list_nth(argument_list, 0), false);
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

  if (zathura->global.sandbox == ZATHURA_SANDBOX_STRICT) {
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("Saving is not permitted in strict sandbox mode"));
    return false;
  }

  if (zathura->document == NULL) {
    girara_notify(session, GIRARA_ERROR, _("No document opened."));
    return false;
  }

  if (girara_list_size(argument_list) == 1) {
    document_save(zathura, girara_list_nth(argument_list, 0), true);
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

  zathura_error_t error = ZATHURA_ERROR_OK;

  /* set search direction */
  zathura->global.search_direction = argument->n;

  unsigned int number_of_pages     = zathura_document_get_number_of_pages(zathura->document);
  unsigned int current_page_number = zathura_document_get_current_page_number(zathura->document);

  /* reset search highlighting */
  bool nohlsearch = false;
  girara_setting_get(session, "nohlsearch", &nohlsearch);

  /* search pages */
  for (unsigned int page_id = 0; page_id < number_of_pages; ++page_id) {
    unsigned int    index = (page_id + current_page_number) % number_of_pages;
    zathura_page_t* page  = zathura_document_get_page(zathura->document, index);
    if (page == NULL) {
      continue;
    }

    GtkWidget* page_widget     = zathura_page_get_widget(zathura, page);
    GObject*   obj_page_widget = G_OBJECT(page_widget);
    g_object_set(obj_page_widget, "draw-links", FALSE, NULL);

    zathura_renderer_lock(zathura->sync.render_thread);
    girara_list_t* result = zathura_page_search_text(page, input, &error);
    zathura_renderer_unlock(zathura->sync.render_thread);

    if (result == NULL || girara_list_size(result) == 0) {
      girara_list_free(result);
      g_object_set(obj_page_widget, "search-results", NULL, NULL);

      if (error == ZATHURA_ERROR_NOT_IMPLEMENTED) {
        break;
      } else {
        continue;
      }
    }

    g_object_set(obj_page_widget, "search-results", result, NULL);

    if (argument->n == BACKWARD) {
      /* start at bottom hit in page */
      g_object_set(obj_page_widget, "search-current", girara_list_size(result) - 1, NULL);
    } else {
      g_object_set(obj_page_widget, "search-current", 0, NULL);
    }
  }

  girara_argument_t* arg = g_try_malloc0(sizeof(girara_argument_t));
  if (arg == NULL) {
    return false;
  }

  arg->n    = FORWARD;
  arg->data = (void*)input;
  sc_search(session, arg, NULL, 0);
  g_free(arg);

  return true;
}

bool
cmd_export(girara_session_t* session, girara_list_t* argument_list)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  if (zathura->global.sandbox == ZATHURA_SANDBOX_STRICT) {
    girara_notify(zathura->ui.session, GIRARA_ERROR,
                  _("Exporting attachments is not permitted in strict sandbox mode"));
    return false;
  }

  if (zathura->document == NULL) {
    girara_notify(session, GIRARA_ERROR, _("No document opened."));
    return false;
  }

  if (girara_list_size(argument_list) != 2) {
    girara_notify(session, GIRARA_ERROR, _("Invalid number of arguments given."));
    return false;
  }

  const char* file_identifier = girara_list_nth(argument_list, 0);
  const char* file_name       = girara_list_nth(argument_list, 1);

  if (file_name == NULL || file_identifier == NULL) {
    return false;
  }

  char* export_path = girara_fix_path(file_name);
  if (export_path == NULL) {
    return false;
  }

  /* attachment */
  if (strncmp(file_identifier, "attachment-", strlen("attachment-")) == 0) {
    if (zathura_document_attachment_save(zathura->document, file_identifier + strlen("attachment-"), export_path) ==
        false) {
      girara_notify(session, GIRARA_ERROR, _("Couldn't write attachment '%s' to '%s'."), file_identifier, file_name);
    } else {
      girara_notify(session, GIRARA_INFO, _("Wrote attachment '%s' to '%s'."), file_identifier, export_path);
    }
    /* image */
  } else if (strncmp(file_identifier, "image-p", strlen("image-p")) == 0 && strlen(file_identifier) >= 10) {
    /* parse page id */
    const char* input   = file_identifier + strlen("image-p");
    int         page_id = atoi(input);
    if (page_id == 0) {
      goto image_error;
    }

    /* parse image id */
    input = strstr(input, "-");
    if (input == NULL) {
      goto image_error;
    }

    int image_id = atoi(input + 1);
    if (image_id == 0) {
      goto image_error;
    }

    /* get image */
    zathura_page_t* page = zathura_document_get_page(zathura->document, page_id - 1);
    if (page == NULL) {
      goto image_error;
    }

    girara_list_t* images = zathura_page_images_get(page, NULL);
    if (images == NULL) {
      goto image_error;
    }

    zathura_image_t* image = girara_list_nth(images, image_id - 1);
    if (image == NULL) {
      goto image_error;
    }

    cairo_surface_t* surface = zathura_page_image_get_cairo(page, image, NULL);
    if (surface == NULL) {
      goto image_error;
    }

    if (cairo_surface_write_to_png(surface, export_path) == CAIRO_STATUS_SUCCESS) {
      girara_notify(session, GIRARA_INFO, _("Wrote image '%s' to '%s'."), file_identifier, export_path);
    } else {
      girara_notify(session, GIRARA_ERROR, _("Couldn't write image '%s' to '%s'."), file_identifier, file_name);
    }

    goto error_ret;

  image_error:

    girara_notify(session, GIRARA_ERROR, _("Unknown image '%s'."), file_identifier);
    goto error_ret;
    /* unknown */
  } else {
    girara_notify(session, GIRARA_ERROR, _("Unknown attachment or image '%s'."), file_identifier);
  }

error_ret:

  g_free(export_path);

  return true;
}

bool cmd_exec(girara_session_t* session, girara_list_t* argument_list) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  if (zathura->global.sandbox == ZATHURA_SANDBOX_STRICT) {
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("Exec is not permitted in strict sandbox mode"));
    return false;
  }

  const char* bus_name = zathura_dbus_get_name(zathura);
  for (size_t idx = 0; idx != girara_list_size(argument_list); ++idx) {
    char* value = girara_list_nth(argument_list, idx);
    char* s     = girara_replace_substring(value, "$DBUS", bus_name);
    if (s != NULL) {
      girara_list_set_nth(argument_list, idx, s);
    }
  }

  if (zathura->document != NULL) {
    const char* path  = zathura_document_get_path(zathura->document);
    unsigned int page = zathura_document_get_current_page_number(zathura->document);
    char page_buf[G_ASCII_DTOSTR_BUF_SIZE];
    g_ascii_dtostr(page_buf, G_ASCII_DTOSTR_BUF_SIZE, page + 1);

    for (size_t idx = 0; idx != girara_list_size(argument_list); ++idx) {
      char* value = girara_list_nth(argument_list, idx);
      char* r     = girara_replace_substring(value, "$PAGE", page_buf);
      if (r != NULL) {
        char* s = girara_replace_substring(r, "$FILE", path);
        g_free(r);

        if (s != NULL) {
          girara_list_set_nth(argument_list, idx, s);
        }
      }
    };
  }

  return girara_exec_with_argument_list(session, argument_list);
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
  int page_offset = zathura_document_get_current_page_number(zathura->document);

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

  zathura_document_set_page_offset(zathura->document, page_offset);

  return true;
}

bool
cmd_version(girara_session_t* session, girara_list_t* UNUSED(argument_list))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  char* string = zathura_get_version_string(zathura, true);
  if (string == NULL) {
    return false;
  }

  /* display information */
  girara_notify(session, GIRARA_INFO, "%s", string);

  g_free(string);

  return true;
}

bool
cmd_source(girara_session_t* session, girara_list_t* argument_list)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->global.data != NULL, false);
  zathura_t* zathura = session->global.data;

  const int argc = girara_list_size(argument_list);
  if (argc > 1) {
    girara_notify(session, GIRARA_ERROR, _("Too many arguments."));
    return false;
  } else if (argc == 1) {
    zathura_set_config_dir(zathura, girara_list_nth(argument_list, 0));
  }
  config_load_files(zathura);

  return true;
}
