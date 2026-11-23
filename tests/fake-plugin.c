/* SPDX-License-Identifier: Zlib */

/* A plugin for text/plain that keeps the annotations of its only page in memory, for testing the annotation API */

#include <string.h>
#include <zathura/plugin-api.h>

typedef struct {
  GPtrArray* annotations; /**< zathura_annotation_t */
} fake_document_t;

static zathura_annotation_t* annotation_copy(const zathura_annotation_t* source) {
  zathura_annotation_t* copy = g_malloc0(sizeof(zathura_annotation_t));
  *copy                      = *source;
  copy->quads    = source->n_quads != 0 ? g_memdup2(source->quads, source->n_quads * sizeof(*source->quads)) : NULL;
  copy->contents = g_strdup(source->contents);
  copy->author   = g_strdup(source->author);
  return copy;
}

static zathura_error_t fake_document_open(zathura_document_t* document) {
  fake_document_t* data = g_malloc0(sizeof(fake_document_t));
  data->annotations     = g_ptr_array_new_with_free_func((GDestroyNotify)zathura_annotation_free);
  zathura_document_set_data(document, data);
  zathura_document_set_number_of_pages(document, 1);
  return ZATHURA_ERROR_OK;
}

static zathura_error_t fake_document_free(zathura_document_t* UNUSED(document), void* data) {
  fake_document_t* fake = data;
  g_ptr_array_unref(fake->annotations);
  g_free(fake);
  return ZATHURA_ERROR_OK;
}

static zathura_error_t fake_document_save_as(zathura_document_t* UNUSED(document), void* UNUSED(data),
                                             const char* path) {
  /* saving to a path in a directory that does not exist fails */
  return g_str_has_prefix(path, "/nonexistent/") == TRUE ? ZATHURA_ERROR_UNKNOWN : ZATHURA_ERROR_OK;
}

static zathura_error_t fake_page_init(zathura_page_t* page) {
  zathura_page_set_width(page, 100);
  zathura_page_set_height(page, 100);
  zathura_page_set_data(page, zathura_document_get_data(zathura_page_get_document(page)));
  return ZATHURA_ERROR_OK;
}

static zathura_error_t fake_page_clear(zathura_page_t* UNUSED(page), void* UNUSED(data)) {
  return ZATHURA_ERROR_OK;
}

static zathura_error_t fake_page_render_cairo(zathura_page_t* UNUSED(page), void* UNUSED(data), cairo_t* UNUSED(cairo),
                                              bool UNUSED(printing)) {
  return ZATHURA_ERROR_OK;
}

static girara_list_t* fake_page_annotations_get(zathura_page_t* UNUSED(page), void* data, zathura_error_t* error) {
  fake_document_t* fake = data;
  girara_list_t* list   = girara_list_new_with_free((girara_free_function_t)zathura_annotation_free);
  for (guint i = 0; i < fake->annotations->len; ++i) {
    zathura_annotation_t* copy = annotation_copy(g_ptr_array_index(fake->annotations, i));
    copy->index                = i;
    girara_list_append(list, copy);
  }
  if (error != NULL) {
    *error = ZATHURA_ERROR_OK;
  }
  return list;
}

static zathura_error_t fake_page_annotation_add(zathura_page_t* UNUSED(page), void* data,
                                                const zathura_annotation_t* annotation) {
  fake_document_t* fake = data;
  if (annotation->type == ZATHURA_ANNOTATION_OTHER) {
    return ZATHURA_ERROR_NOT_IMPLEMENTED;
  }
  if (annotation->type != ZATHURA_ANNOTATION_TEXT && annotation->n_quads == 0) {
    return ZATHURA_ERROR_INVALID_ARGUMENTS;
  }
  g_ptr_array_add(fake->annotations, annotation_copy(annotation));
  return ZATHURA_ERROR_OK;
}

static zathura_error_t fake_page_annotation_delete(zathura_page_t* UNUSED(page), void* data, unsigned int index) {
  fake_document_t* fake = data;
  if (index >= fake->annotations->len) {
    return ZATHURA_ERROR_INVALID_ARGUMENTS;
  }
  g_ptr_array_remove_index(fake->annotations, index);
  return ZATHURA_ERROR_OK;
}

static zathura_error_t fake_page_annotation_set_contents(zathura_page_t* UNUSED(page), void* data, unsigned int index,
                                                         const char* contents) {
  fake_document_t* fake = data;
  if (index >= fake->annotations->len) {
    return ZATHURA_ERROR_INVALID_ARGUMENTS;
  }
  zathura_annotation_t* annotation = g_ptr_array_index(fake->annotations, index);
  g_free(annotation->contents);
  annotation->contents = g_strdup(contents);
  return ZATHURA_ERROR_OK;
}

ZATHURA_PLUGIN_REGISTER_WITH_FUNCTIONS("fake", "0",
                                       ZATHURA_PLUGIN_FUNCTIONS({
                                           .document_open                = fake_document_open,
                                           .document_free                = fake_document_free,
                                           .document_save_as             = fake_document_save_as,
                                           .page_init                    = fake_page_init,
                                           .page_clear                   = fake_page_clear,
                                           .page_render_cairo            = fake_page_render_cairo,
                                           .page_annotations_get         = fake_page_annotations_get,
                                           .page_annotation_add          = fake_page_annotation_add,
                                           .page_annotation_delete       = fake_page_annotation_delete,
                                           .page_annotation_set_contents = fake_page_annotation_set_contents,
                                       }),
                                       ZATHURA_PLUGIN_MIMETYPES({"text/plain"}))
