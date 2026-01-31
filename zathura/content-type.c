/* SPDX-License-Identifier: Zlib */

#include "content-type.h"
#include "macros.h"

#include <gio/gio.h>
#include <girara/log.h>
#include <girara/utils.h>
#include <glib.h>
#include <magic.h>
#include <stdio.h>

struct zathura_content_type_context_s {
  magic_t magic;
};

zathura_content_type_context_t* zathura_content_type_new(void) {
  zathura_content_type_context_t* context = g_try_malloc0(sizeof(zathura_content_type_context_t));
  if (context == NULL) {
    return NULL;
  }

  /* creat magic cookie */
  static const int flags = MAGIC_ERROR | MAGIC_MIME_TYPE | MAGIC_SYMLINK | MAGIC_NO_CHECK_APPTYPE | MAGIC_NO_CHECK_CDF |
                           MAGIC_NO_CHECK_ELF | MAGIC_NO_CHECK_ENCODING;
  magic_t magic = magic_open(flags);
  if (magic == NULL) {
    girara_debug("failed creating the magic cookie");
    return context;
  }

  /* ... and load mime database */
  if (magic_load(magic, NULL) < 0) {
    girara_debug("failed loading the magic database: %s", magic_error(magic));
    magic_close(magic);
    return context;
  }

  context->magic = magic;
  return context;
}

void zathura_content_type_free(zathura_content_type_context_t* context) {
  if (context != NULL && context->magic != NULL) {
    magic_close(context->magic);
  }

  g_free(context);
}

/** Read a most GT_MAX_READ bytes before falling back to file. */
static const size_t GT_MAX_READ = 1 << 16;

static char* guess_type_magic(zathura_content_type_context_t* context, const char* path) {
  if (context == NULL || context->magic == NULL) {
    return NULL;
  }

  const char* mime_type = NULL;

  /* get the mime type */
  mime_type = magic_file(context->magic, path);
  if (mime_type == NULL || magic_errno(context->magic) != 0) {
    girara_debug("failed guessing filetype: %s", magic_error(context->magic));
    return NULL;
  }
  girara_debug("magic detected filetype: %s", mime_type);

  char* content_type = g_content_type_from_mime_type(mime_type);
  if (content_type == NULL) {
    girara_warning("failed to convert mime type to content type: %s", mime_type);
    /* dup so we own the memory */
    return g_strdup(mime_type);
  }

  return content_type;
}

static char* guess_type_glib(const char* path) {
  gboolean uncertain = FALSE;
  char* content_type = g_content_type_guess(path, NULL, 0, &uncertain);
  if (content_type == NULL) {
    girara_debug("g_content_type failed\n");
  } else {
    if (uncertain == FALSE) {
      girara_debug("g_content_type detected filetype: %s", content_type);
      return content_type;
    }
    girara_debug("g_content_type is uncertain, guess: %s", content_type);
  }

  g_free(content_type);
  content_type = NULL;

  GMappedFile* f = g_mapped_file_new(path, FALSE, NULL);
  if (f == NULL) {
    return NULL;
  }

  content_type = g_content_type_guess(NULL, (const guchar*)g_mapped_file_get_contents(f),
                                      MIN(g_mapped_file_get_length(f), GT_MAX_READ), &uncertain);
  girara_debug("new guess: %s uncertain: %d", content_type, uncertain);
  g_mapped_file_unref(f);
  if (uncertain == FALSE) {
    return content_type;
  }

  g_free(content_type);
  return NULL;
}

static int compare_content_types(const void* lhs, const void* rhs) {
  return g_strcmp0(lhs, rhs);
}

char* zathura_content_type_guess(zathura_content_type_context_t* context, const char* path,
                                 const girara_list_t* supported_content_types) {
  /* try libmagic first */
  char* content_type = guess_type_magic(context, path);
  if (content_type != NULL) {
    if (supported_content_types == NULL ||
        girara_list_find(supported_content_types, compare_content_types, content_type) != NULL) {
      return content_type;
    }
    girara_debug("content type '%s' not supported, trying again", content_type);
    g_free(content_type);
  }
  /* else fallback to g_content_type_guess method */
  content_type = guess_type_glib(path);
  if (content_type != NULL) {
    if (supported_content_types == NULL ||
        girara_list_find(supported_content_types, compare_content_types, content_type) != NULL) {
      return content_type;
    }
    girara_debug("content type '%s' not supported", content_type);
    g_free(content_type);
  }
  return NULL;
}
