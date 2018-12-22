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
  const int flags =
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
  if (mime_type == NULL) {
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
  GString* command = g_string_new("file -b --mime-type ");
  char* tmp        = g_shell_quote(path);

  g_string_append(command, tmp);
  g_free(tmp);

  GError* error = NULL;
  char* out = NULL;
  int ret = 0;
  g_spawn_command_line_sync(command->str, &out, NULL, &ret, &error);
  g_string_free(command, TRUE);
  if (error != NULL) {
    girara_warning("failed to execute command: %s", error->message);
    g_error_free(error);
    g_free(out);
    return NULL;
  }
  if (WEXITSTATUS(ret) != 0) {
    girara_warning("file failed with error code: %d", WEXITSTATUS(ret));
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

  const int fd = fileno(f);
  guchar* content = NULL;
  size_t length = 0u;
  ssize_t bytes_read = -1;
  while (uncertain == TRUE && length < GT_MAX_READ && bytes_read != 0) {
    g_free((void*)content_type);
    content_type = NULL;

    guchar* temp_content = g_try_realloc(content, length + BUFSIZ);
    if (temp_content == NULL) {
      break;
    }
    content = temp_content;

    bytes_read = read(fd, content + length, BUFSIZ);
    if (bytes_read == -1) {
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

  g_free((void*)content_type);
  return NULL;
}

char*
zathura_content_type_guess(zathura_content_type_context_t* context,
                           const char* path)
{
  /* try libmagic first */
  char* content_type = guess_type_magic(context, path);
  if (content_type != NULL) {
    return content_type;
  }
  /* else fallback to g_content_type_guess method */
  content_type = guess_type_glib(path);
  if (content_type != NULL) {
    return content_type;
  }
  /* and if libmagic is not available, try file as last resort */
  return guess_type_file(path);
}
