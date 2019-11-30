/* SPDX-License-Identifier: Zlib */

#include "content-type.h"
#include "macros.h"

#include <girara/utils.h>
#ifdef WITH_MAGIC
#include <magic.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#endif
#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>

struct zathura_content_type_context_s
{
#ifdef WITH_MAGIC
  magic_t magic;
#else
  void* magic;
#endif
};

zathura_content_type_context_t*
zathura_content_type_new(void)
{
  zathura_content_type_context_t* context =
    g_try_malloc0(sizeof(zathura_content_type_context_t));
  if (context == NULL) {
    return NULL;
  }

#ifdef WITH_MAGIC
  /* creat magic cookie */
  static const int flags =
    MAGIC_ERROR |
    MAGIC_MIME_TYPE |
    MAGIC_SYMLINK |
    MAGIC_NO_CHECK_APPTYPE |
    MAGIC_NO_CHECK_CDF |
    MAGIC_NO_CHECK_ELF |
    MAGIC_NO_CHECK_ENCODING;
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
#endif

  return context;
}

void
zathura_content_type_free(zathura_content_type_context_t* context)
{
  if (context == NULL) {
    return;
  }

#ifdef WITH_MAGIC
  if (context->magic != NULL) {
    magic_close(context->magic);
  }
#endif

  g_free(context);
}


/** Read a most GT_MAX_READ bytes before falling back to file. */
static const size_t GT_MAX_READ = 1 << 16;

#ifdef WITH_MAGIC
static char*
guess_type_magic(zathura_content_type_context_t* context, const char* path)
{
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

static char*
guess_type_file(const char* UNUSED(path))
{
  return NULL;
}
#else
static char*
guess_type_magic(zathura_content_type_context_t* UNUSED(context),
                 const char* UNUSED(path))
{
  return NULL;
}

static char*
guess_type_file(const char* path)
{
  /* g_spawn_async expects char** */
  static char cmd_file[] = "file";
  static char opt_b[] = "-b";
  static char opt_mime_type[] = "--mime-type";
  char* argv[] = { cmd_file, opt_b, opt_mime_type, g_strdup(path), NULL };

  GError* error = NULL;
  char* out = NULL;
  int ret = 0;
  const bool r = g_spawn_sync(NULL, argv, NULL,
                              G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
                              NULL, NULL, &out, NULL, &ret, &error);
  g_free(argv[3]);
  if (r == false || error != NULL) {
    girara_warning("failed to execute command: %s", error ? error->message : "unknown");
    g_error_free(error);
    g_free(out);
    return NULL;
  }
  if (g_spawn_check_exit_status(ret, &error) == false) {
    girara_warning("file failed: %s", error->message);
    g_error_free(error);
    g_free(out);
    return NULL;
  }

  g_strdelimit(out, "\n\r", '\0');
  girara_debug("file detected filetype: %s", out);

  char* content_type = g_content_type_from_mime_type(out);
  if (content_type == NULL) {
    girara_warning("failed to convert mime type to content type: %s", out);
    return out;
  }

  g_free(out);
  return content_type;
}
#endif

static char*
guess_type_glib(const char* path)
{
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

  FILE* f = fopen(path, "rb");
  if (f == NULL) {
    return NULL;
  }

  guchar* content = NULL;
  size_t length = 0;
  while (uncertain == TRUE && length < GT_MAX_READ) {
    g_free(content_type);
    content_type = NULL;

    guchar* temp_content = g_try_realloc(content, length + BUFSIZ);
    if (temp_content == NULL) {
      break;
    }
    content = temp_content;

    size_t bytes_read = fread(content + length, 1, BUFSIZ, f);
    if (bytes_read == 0) {
      break;
    }

    length += bytes_read;
    content_type = g_content_type_guess(NULL, content, length, &uncertain);
    girara_debug("new guess: %s uncertain: %d, read: %zu", content_type, uncertain, length);
  }

  fclose(f);
  g_free(content);
  if (uncertain == FALSE) {
    return content_type;
  }

  g_free(content_type);
  return NULL;
}

static int compare_content_types(const void* lhs, const void* rhs) {
  return g_strcmp0(lhs, rhs);
}

char*
zathura_content_type_guess(zathura_content_type_context_t* context,
                           const char* path,
                           const girara_list_t* supported_content_types)
{
  /* try libmagic first */
  char *content_type = guess_type_magic(context, path);
  if (content_type != NULL) {
    if (supported_content_types == NULL ||
        girara_list_find(supported_content_types, compare_content_types,
                         content_type) != NULL) {
      return content_type;
    }
    girara_debug("content type '%s' not supported, trying again", content_type);
    g_free(content_type);
  }
  /* else fallback to g_content_type_guess method */
  content_type = guess_type_glib(path);
  if (content_type != NULL) {
    if (supported_content_types == NULL ||
        girara_list_find(supported_content_types, compare_content_types,
                         content_type) != NULL) {
      return content_type;
    }
    girara_debug("content type '%s' not supported, trying again", content_type);
    g_free(content_type);
  }
  /* and if libmagic is not available, try file as last resort */
  return guess_type_file(path);
}
