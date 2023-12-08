/* SPDX-License-Identifier: Zlib */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <glib/gi18n.h>

#include "bookmarks.h"
#include "document.h"
#include "completion.h"
#include "utils.h"
#include "page.h"
#include "database.h"

#include <girara/session.h>
#include <girara/settings.h>
#include <girara/completion.h>
#include <girara/utils.h>
#include <girara/datastructures.h>

static int
compare_case_insensitive(const char* str1, const char* str2)
{
  char* ustr1 = g_utf8_casefold(str1, -1);
  char* ustr2 = g_utf8_casefold(str2, -1);
  int res = g_utf8_collate(ustr1, ustr2);
  g_free(ustr1);
  g_free(ustr2);
  return res;
}

static girara_list_t*
list_files(zathura_t* zathura, const char* current_path, const char* current_file,
           size_t current_file_length, bool is_dir, bool check_file_ext)
{
  if (zathura == NULL || zathura->ui.session == NULL || current_path == NULL) {
    return NULL;
  }

  girara_debug("checking files in %s", current_path);

  /* read directory */
  GDir* dir = g_dir_open(current_path, 0, NULL);
  if (dir == NULL) {
    return NULL;
  }

  girara_list_t* res = girara_sorted_list_new2((girara_compare_function_t)compare_case_insensitive,
                       (girara_free_function_t)g_free);

  bool show_hidden = false;
  girara_setting_get(zathura->ui.session, "show-hidden", &show_hidden);
  bool show_directories = true;
  girara_setting_get(zathura->ui.session, "show-directories", &show_directories);

  /* read files */
  const char* name = NULL;
  while ((name = g_dir_read_name(dir)) != NULL) {
    char* e_name   = g_filename_display_name(name);
    if (e_name == NULL) {
      goto error_free;
    }

    size_t e_length = strlen(e_name);

    if (show_hidden == false && e_name[0] == '.') {
      g_free(e_name);
      continue;
    }

    if ((current_file_length > e_length) || strncmp(current_file, e_name, current_file_length)) {
      g_free(e_name);
      continue;
    }

    char* tmp = "/";
    if (is_dir == true || g_strcmp0(current_path, "/") == 0) {
      tmp = "";
    }

    char* full_path = g_strdup_printf("%s%s%s", current_path, tmp, e_name);
    g_free(e_name);

    if (g_file_test(full_path, G_FILE_TEST_IS_DIR) == true) {
      if (show_directories == false) {
        girara_debug("ignoring %s (directory)", full_path);
        g_free(full_path);
        continue;
      }
      girara_debug("adding %s (directory)", full_path);
      girara_list_append(res, full_path);
    } else if (check_file_ext == false || file_valid_extension(zathura, full_path) == true) {
      girara_debug("adding %s (file)", full_path);
      girara_list_append(res, full_path);
    } else {
      girara_debug("ignoring %s (file)", full_path);
      g_free(full_path);
    }
  }

  g_dir_close(dir);

  if (girara_list_size(res) == 1) {
    char* path = girara_list_nth(res, 0);
    if (g_file_test(path, G_FILE_TEST_IS_DIR) == true) {
      girara_debug("changing to directory %s", path);
      char* newpath = g_strdup_printf("%s/", path);
      girara_list_clear(res);
      girara_list_append(res, newpath);
    }
  }

  return res;

error_free:
  g_dir_close(dir);
  girara_list_free(res);
  return NULL;
}

static void
group_add_element(void* data, void* userdata)
{
  const char* element              = data;
  girara_completion_group_t* group = userdata;

  girara_completion_group_add_element(group, element, NULL);
}

static girara_completion_t*
list_files_for_cc(zathura_t* zathura, const char* input, bool check_file_ext, int show_recent)
{
  girara_completion_t* completion  = girara_completion_init();
  girara_completion_group_t* group = girara_completion_group_create(zathura->ui.session, "files");
  girara_completion_group_t* history_group = NULL;

  gchar* path         = NULL;
  gchar* current_path = NULL;

  if (show_recent > 0) {
    history_group = girara_completion_group_create(zathura->ui.session, "recent files");
  }

  if (completion == NULL || group == NULL || (show_recent > 0 && history_group == NULL)) {
    goto error_free;
  }

  path = girara_fix_path(input);
  if (path == NULL) {
    goto error_free;
  }

  /* If the path does not begin with a slash we update the path with the current
   * working directory */
  if (strlen(path) == 0 || path[0] != '/') {
    char* cwd = g_get_current_dir();
    if (cwd == NULL) {
      goto error_free;
    }

    char* tmp_path = g_strdup_printf("%s/%s", cwd, path);
    g_free(cwd);
    if (tmp_path == NULL) {
      goto error_free;
    }

    g_free(path);
    path = tmp_path;
  }

  /* Append a slash if the given argument is a directory */
  bool is_dir = (path[strlen(path) - 1] == '/') ? true : false;
  if ((g_file_test(path, G_FILE_TEST_IS_DIR) == TRUE) && is_dir == false) {
    char* tmp_path = g_strdup_printf("%s/", path);
    g_free(path);
    path = tmp_path;
    is_dir = true;
  }

  /* get current path */
  current_path = is_dir ? g_strdup(path) : g_path_get_dirname(path);

  /* get current file */
  gchar* current_file              = is_dir ? "" : basename(path);
  const size_t current_file_length = strlen(current_file);

  /* read directory */
  if (g_file_test(current_path, G_FILE_TEST_IS_DIR) == TRUE) {
    girara_list_t* names = list_files(zathura, current_path, current_file, current_file_length, is_dir, check_file_ext);
    if (names == NULL) {
      goto error_free;
    }

    girara_list_foreach(names, group_add_element, group);
    girara_list_free(names);
  }

  if (show_recent > 0 && zathura->database != NULL) {
    girara_list_t* recent_files = zathura_db_get_recent_files(zathura->database, show_recent, path);
    if (recent_files == NULL) {
      goto error_free;
    }

    if (girara_list_size(recent_files) != 0) {
      girara_list_foreach(recent_files, group_add_element, history_group);
      girara_list_free(recent_files);
    } else {
      girara_completion_group_free(history_group);
      history_group = NULL;
    }
  }

  g_free(path);
  g_free(current_path);

  if (history_group != NULL) {
    girara_completion_add_group(completion, history_group);
  }
  girara_completion_add_group(completion, group);

  return completion;

error_free:

  if (completion) {
    girara_completion_free(completion);
  }
  if (history_group) {
    girara_completion_group_free(history_group);
  }
  if (group) {
    girara_completion_group_free(group);
  }

  g_free(current_path);
  g_free(path);

  return NULL;
}

girara_completion_t*
cc_open(girara_session_t* session, const char* input)
{
  g_return_val_if_fail(session != NULL, NULL);
  g_return_val_if_fail(session->global.data != NULL, NULL);
  zathura_t* zathura = session->global.data;

  int show_recent = 0;
  girara_setting_get(zathura->ui.session, "show-recent", &show_recent);

  return list_files_for_cc(zathura, input, true, show_recent);
}

girara_completion_t*
cc_write(girara_session_t* session, const char* input)
{
  g_return_val_if_fail(session != NULL, NULL);
  g_return_val_if_fail(session->global.data != NULL, NULL);
  zathura_t* zathura = session->global.data;

  return list_files_for_cc(zathura, input, false, false);
}

girara_completion_t* cc_bookmarks(girara_session_t* session, const char* input) {
  if (input == NULL) {
    return NULL;
  }

  g_return_val_if_fail(session != NULL, NULL);
  g_return_val_if_fail(session->global.data != NULL, NULL);
  zathura_t* zathura = session->global.data;

  girara_completion_t* completion  = girara_completion_init();
  girara_completion_group_t* group = girara_completion_group_create(session, NULL);

  if (completion == NULL || group == NULL) {
    goto error_free;
  }

  const size_t input_length = strlen(input);
  for (size_t idx = 0; idx != girara_list_size(zathura->bookmarks.bookmarks); ++idx) {
    zathura_bookmark_t* bookmark = girara_list_nth(zathura->bookmarks.bookmarks, idx);
    if (input_length <= strlen(bookmark->id) && !strncmp(input, bookmark->id, input_length)) {
      gchar* paged = g_strdup_printf(_("Page %d"), bookmark->page);
      girara_completion_group_add_element(group, bookmark->id, paged);
      g_free(paged);
    }
  }

  girara_completion_add_group(completion, group);

  return completion;

error_free:

  if (completion != NULL) {
    girara_completion_free(completion);
  }

  if (group != NULL) {
    girara_completion_group_free(group);
  }

  return NULL;
}

girara_completion_t* cc_export(girara_session_t* session, const char* input) {
  g_return_val_if_fail(session != NULL, NULL);
  g_return_val_if_fail(session->global.data != NULL, NULL);
  zathura_t* zathura = session->global.data;

  if (input == NULL || zathura->document == NULL) {
    goto error_ret;
  }

  girara_completion_t* completion             = NULL;
  girara_completion_group_t* attachment_group = NULL;
  girara_completion_group_t* image_group      = NULL;

  completion = girara_completion_init();
  if (completion == NULL) {
    goto error_free;
  }

  attachment_group = girara_completion_group_create(session, _("Attachments"));
  if (attachment_group == NULL) {
    goto error_free;
  }

  /* add attachments */
  const size_t input_length  = strlen(input);
  girara_list_t* attachments = zathura_document_attachments_get(zathura->document, NULL);
  if (attachments != NULL) {
    bool added = false;

    for (size_t idx = 0; idx != girara_list_size(attachments); ++idx) {
      const char* attachment = girara_list_nth(attachments, idx);
      if (input_length <= strlen(attachment) && !strncmp(input, attachment, input_length)) {
        char* attachment_string = g_strdup_printf("attachment-%s", attachment);
        girara_completion_group_add_element(attachment_group, attachment_string, NULL);
        g_free(attachment_string);
        added = true;
      }
    }

    if (added == true) {
      girara_completion_add_group(completion, attachment_group);
    } else {
      girara_completion_group_free(attachment_group);
      attachment_group = NULL;
    }

    girara_list_free(attachments);
  }

  /* add images */
  image_group = girara_completion_group_create(session, _("Images"));
  if (image_group == NULL) {
    goto error_free;
  }

  bool added = false;

  unsigned int number_of_pages = zathura_document_get_number_of_pages(zathura->document);
  for (unsigned int page_id = 0; page_id < number_of_pages; page_id++) {
    zathura_page_t* page = zathura_document_get_page(zathura->document, page_id);
    if (page == NULL) {
      continue;
    }

    girara_list_t* images = zathura_page_images_get(page, NULL);
    if (images != NULL) {
      unsigned int image_number = 1;
      for (size_t idx = 0; idx != girara_list_size(images); ++idx) {
        char* image_string = g_strdup_printf("image-p%d-%d", page_id + 1, image_number);
        girara_completion_group_add_element(image_group, image_string, NULL);
        g_free(image_string);

        added = true;
        image_number++;
      }
      girara_list_free(images);
    }
  }

  if (added == true) {
    girara_completion_add_group(completion, image_group);
  } else {
    girara_completion_group_free(image_group);
    image_group = NULL;
  }

  return completion;

error_free:

  if (completion != NULL) {
    girara_completion_free(completion);
  }

  if (attachment_group != NULL) {
    girara_completion_group_free(attachment_group);
  }

  if (image_group != NULL) {
    girara_completion_group_free(image_group);
  }

error_ret:

  return NULL;
}
