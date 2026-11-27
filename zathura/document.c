/* SPDX-License-Identifier: Zlib */

#include "document.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <glib.h>
#include <gio/gio.h>
#include <math.h>
#include <xxhash.h>

#include <girara/datastructures.h>
#include <girara/log.h>
#include <girara/utils.h>

#include "zathura.h"
#include "page.h"
#include "plugin.h"
#include "content-type.h"
#include "internal.h"

G_DEFINE_AUTOPTR_CLEANUP_FUNC(XXH3_state_t, XXH3_freeState)

/**
 * Document
 */
struct zathura_document_s {
  void* data;                     /**< Custom data */
  zathura_page_t** pages;         /**< All pages of the document */
  GMutex lock;                    /**< Document lock */
  const zathura_plugin_t* plugin; /**< Used plugin */
  char* file_path;                /**< File path of the document */
  char* uri;                      /**< URI of the document */
  char* basename;                 /**< Basename of the document */
  const char* password;           /**< Password of the document */
  XXH128_canonical_t hash;        /**< XXH3 hash of the document */
  unsigned int number_of_pages;   /**< Number of pages */
  bool hash_computed;             /**< Whether the hash has been computed yet */
  bool modified;                  /**< Changed in memory since it was opened or last saved */
};

static bool hash_file(XXH128_canonical_t* dst, const char* path) {
  g_autoptr(GFile) f = g_file_new_for_path(path);
  if (!f) {
    return false;
  }

  g_autoptr(GFileInputStream) stream = g_file_read(f, NULL, NULL);
  if (!stream) {
    return false;
  }

  g_autoptr(XXH3_state_t) state = XXH3_createState();
  if (!state) {
    return false;
  }
  XXH3_128bits_reset(state);

  uint8_t buf[MAX(BUFSIZ, 4096)];
  gssize read;
  while ((read = g_input_stream_read(G_INPUT_STREAM(stream), buf, sizeof(buf), NULL, NULL)) > 0) {
    XXH3_128bits_update(state, buf, read);
  }

  /* read is zero on a clean end of stream and negative on an error */
  if (read < 0) {
    return false;
  }

  XXH128_canonicalFromHash(dst, XXH3_128bits_digest(state));
  return true;
}

zathura_document_t* zathura_document_open(zathura_t* zathura, const char* path, const char* uri, const char* password,
                                          zathura_error_t* error) {
  if (zathura == NULL || path == NULL) {
    return NULL;
  }

  g_autoptr(GFile) file = g_file_new_for_path(path);
  if (file == NULL) {
    girara_error("Error while handling path '%s'.", path);
    zathura_check_set_error(error, ZATHURA_ERROR_UNKNOWN);
    return NULL;
  }

  g_autofree char* real_path = g_file_get_path(file);
  if (real_path == NULL) {
    girara_error("Error while handling path '%s'.", path);
    zathura_check_set_error(error, ZATHURA_ERROR_UNKNOWN);
    return NULL;
  }

  g_autofree char* content_type = zathura_content_type_guess(
      zathura->content_type_context, real_path, zathura_plugin_manager_get_content_types(zathura->plugins.manager));
  if (content_type == NULL) {
    girara_error("Could not determine file type.");
    zathura_check_set_error(error, ZATHURA_ERROR_UNKNOWN);
    return NULL;
  }

  const zathura_plugin_t* plugin = zathura_plugin_manager_get_plugin(zathura->plugins.manager, content_type);
  if (plugin == NULL) {
    girara_error("Unknown file type: '%s'", content_type);
    zathura_check_set_error(error, ZATHURA_ERROR_UNKNOWN);
    return NULL;
  }

  g_autoptr(zathura_document_t) document = g_try_malloc0(sizeof(zathura_document_t));
  if (document == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  g_mutex_init(&document->lock);

  document->file_path = g_steal_pointer(&real_path);
  document->uri       = g_strdup(uri);
  if (document->uri == NULL) {
    document->basename = g_file_get_basename(file);
  } else {
    g_autoptr(GFile) gf = g_file_new_for_uri(document->uri);
    document->basename  = g_file_get_basename(gf);
  }
  document->password = password;
  document->plugin   = plugin;

  /* open document */
  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(plugin);

  zathura_error_t int_error = functions->document_open(document);
  if (int_error != ZATHURA_ERROR_OK) {
    zathura_check_set_error(error, int_error);
    girara_error("could not open document\n");
    return NULL;
  }

  /* allocate the pages without parsing them */
  document->pages = g_try_malloc0_n(document->number_of_pages, sizeof(zathura_page_t*));
  if (document->pages == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  for (unsigned int page_id = 0; page_id < document->number_of_pages; page_id++) {
    zathura_page_t* page = zathura_page_new(document, page_id, NULL);
    if (page == NULL) {
      zathura_check_set_error(error, ZATHURA_ERROR_OUT_OF_MEMORY);
      return NULL;
    }

    document->pages[page_id] = page;
  }

  return g_steal_pointer(&document);
}

zathura_error_t zathura_document_free(zathura_document_t* document) {
  if (!document || !document->plugin) {
    g_free(document);
    return ZATHURA_ERROR_INVALID_ARGUMENTS;
  }

  if (document->pages) {
    /* free pages */
    for (unsigned int page_id = 0; page_id < document->number_of_pages; page_id++) {
      zathura_page_free(document->pages[page_id]);
      document->pages[page_id] = NULL;
    }
    g_free(document->pages);
  }

  /* free document */
  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(document->plugin);

  zathura_error_t error = functions->document_free(document, document->data);

  g_free(document->file_path);
  g_free(document->uri);
  g_free(document->basename);
  g_mutex_clear(&document->lock);
  g_free(document);

  return error;
}

const char* zathura_document_get_path(zathura_document_t* document) {
  if (document == NULL) {
    return NULL;
  }

  return document->file_path;
}

const uint8_t* zathura_document_get_hash(zathura_document_t* document) {
  if (document == NULL) {
    return NULL;
  }

  if (!document->hash_computed) {
    if (!hash_file(&document->hash, document->file_path)) {
      girara_warning("Failed to hash file '%s'; fileinfo lookup may be unreliable.", document->file_path);
    }
    document->hash_computed = true;
  }

  return document->hash.digest;
}

const char* zathura_document_get_uri(zathura_document_t* document) {
  if (document == NULL) {
    return NULL;
  }

  return document->uri;
}

const char* zathura_document_get_basename(zathura_document_t* document) {
  if (document == NULL) {
    return NULL;
  }

  return document->basename;
}

const char* zathura_document_get_password(zathura_document_t* document) {
  if (document == NULL) {
    return NULL;
  }

  return document->password;
}

zathura_page_t* zathura_document_get_page(zathura_document_t* document, unsigned int index) {
  if (document == NULL || document->pages == NULL) {
    return NULL;
  }

  g_return_val_if_fail(index < document->number_of_pages, NULL);
  return document->pages[index];
}

void* zathura_document_get_data(zathura_document_t* document) {
  if (document == NULL) {
    return NULL;
  }

  return document->data;
}

void zathura_document_set_data(zathura_document_t* document, void* data) {
  if (document == NULL) {
    return;
  }

  document->data = data;
}

unsigned int zathura_document_get_number_of_pages(zathura_document_t* document) {
  if (document == NULL) {
    return 0;
  }

  return document->number_of_pages;
}

void zathura_document_set_number_of_pages(zathura_document_t* document, unsigned int number_of_pages) {
  if (document == NULL) {
    return;
  }

  document->number_of_pages = number_of_pages;
}

zathura_error_t zathura_document_save_as(zathura_document_t* document, const char* path) {
  if (document == NULL || document->plugin == NULL || path == NULL) {
    return ZATHURA_ERROR_UNKNOWN;
  }

  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(document->plugin);
  if (functions->document_save_as == NULL) {
    return ZATHURA_ERROR_NOT_IMPLEMENTED;
  }

  const zathura_error_t error = functions->document_save_as(document, document->data, path);
  if (error == ZATHURA_ERROR_OK) {
    document->modified = false;
  }
  return error;
}

bool zathura_document_is_modified(zathura_document_t* document) {
  return document != NULL && document->modified;
}

void zathura_document_set_modified(zathura_document_t* document, bool modified) {
  if (document != NULL) {
    document->modified = modified;
  }
}

girara_tree_node_t* zathura_document_index_generate(zathura_document_t* document, zathura_error_t* error) {
  if (document == NULL || document->plugin == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(document->plugin);
  if (functions->document_index_generate == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  return functions->document_index_generate(document, document->data, error);
}

girara_list_t* zathura_document_attachments_get(zathura_document_t* document, zathura_error_t* error) {
  if (document == NULL || document->plugin == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(document->plugin);
  if (functions->document_attachments_get == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  return functions->document_attachments_get(document, document->data, error);
}

zathura_error_t zathura_document_attachment_save(zathura_document_t* document, const char* attachment,
                                                 const char* file) {
  if (document == NULL || document->plugin == NULL) {
    return ZATHURA_ERROR_INVALID_ARGUMENTS;
  }

  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(document->plugin);
  if (functions->document_attachment_save == NULL) {
    return ZATHURA_ERROR_NOT_IMPLEMENTED;
  }

  return functions->document_attachment_save(document, document->data, attachment, file);
}

girara_list_t* zathura_document_get_information(zathura_document_t* document, zathura_error_t* error) {
  if (document == NULL || document->plugin == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(document->plugin);
  if (functions->document_get_information == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  girara_list_t* result = functions->document_get_information(document, document->data, error);
  if (result != NULL) {
    girara_list_set_free_function(result, zathura_document_information_entry_free);
  }

  return result;
}

const zathura_plugin_t* zathura_document_get_plugin(zathura_document_t* document) {
  g_return_val_if_fail(document != NULL, NULL);

  return document->plugin;
}

void zathura_document_lock(zathura_document_t* document) {
  g_return_if_fail(document != NULL);

  g_mutex_lock(&document->lock);
}

void zathura_document_unlock(zathura_document_t* document) {
  g_return_if_fail(document != NULL);

  g_mutex_unlock(&document->lock);
}
