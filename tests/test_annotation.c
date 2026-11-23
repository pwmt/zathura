/* SPDX-License-Identifier: Zlib */

#include <glib/gstdio.h>

#include "content-type.h"
#include "document.h"
#include "page.h"
#include "plugin.h"
#include "types.h"
#include "zathura.h"

typedef struct {
  zathura_t zathura;
  zathura_document_t* document;
  zathura_page_t* page;
  char* dir;
  char* file;
} fixture_t;

static void fixture_set_up(fixture_t* fixture, gconstpointer UNUSED(data)) {
  memset(&fixture->zathura, 0, sizeof(fixture->zathura));
  fixture->zathura.plugins.manager = zathura_plugin_manager_new();
  zathura_plugin_manager_set_dir(fixture->zathura.plugins.manager, g_getenv("G_TEST_BUILDDIR"));
  g_assert_true(zathura_plugin_manager_load(fixture->zathura.plugins.manager));
  fixture->zathura.content_type_context = zathura_content_type_new();

  g_autoptr(GError) error = NULL;
  fixture->dir            = g_dir_make_tmp("zathura-annotation-XXXXXX", &error);
  g_assert_no_error(error);
  fixture->file = g_build_filename(fixture->dir, "document.txt", NULL);
  g_assert_true(g_file_set_contents(fixture->file, "some plain text\n", -1, &error));

  zathura_error_t zerror = ZATHURA_ERROR_OK;
  fixture->document      = zathura_document_open(&fixture->zathura, fixture->file, NULL, NULL, &zerror);
  g_assert_nonnull(fixture->document);
  fixture->page = zathura_document_get_page(fixture->document, 0);
  g_assert_nonnull(fixture->page);
}

static void fixture_tear_down(fixture_t* fixture, gconstpointer UNUSED(data)) {
  zathura_document_free(fixture->document);
  zathura_content_type_free(fixture->zathura.content_type_context);
  zathura_plugin_manager_free(fixture->zathura.plugins.manager);
  g_remove(fixture->file);
  g_rmdir(fixture->dir);
  g_free(fixture->file);
  g_free(fixture->dir);
}

static zathura_annotation_t highlight(zathura_rectangle_t* quad, const char* contents) {
  return (zathura_annotation_t){
      .type      = ZATHURA_ANNOTATION_HIGHLIGHT,
      .quads     = quad,
      .n_quads   = 1,
      .rect      = *quad,
      .has_color = true,
      .color     = {1, 1, 0},
      .contents  = (char*)contents,
  };
}

static void test_annotation_free(void) {
  zathura_annotation_free(NULL);

  zathura_annotation_t* annotation = g_malloc0(sizeof(zathura_annotation_t));
  annotation->quads                = g_malloc0(2 * sizeof(zathura_rectangle_t));
  annotation->n_quads              = 2;
  annotation->contents             = g_strdup("contents");
  annotation->author               = g_strdup("author");
  zathura_annotation_free(annotation);
}

static void test_modified_flag(void) {
  g_assert_false(zathura_document_is_modified(NULL));
  zathura_document_set_modified(NULL, true);
}

static void test_invalid_arguments(void) {
  zathura_error_t error           = ZATHURA_ERROR_OK;
  zathura_rectangle_t quad        = {0, 0, 10, 10};
  zathura_annotation_t annotation = highlight(&quad, NULL);

  g_assert_null(zathura_page_annotations_get(NULL, &error));
  g_assert_cmpint(error, ==, ZATHURA_ERROR_INVALID_ARGUMENTS);
  g_assert_cmpint(zathura_page_annotation_add(NULL, &annotation), ==, ZATHURA_ERROR_INVALID_ARGUMENTS);
  g_assert_cmpint(zathura_page_annotation_delete(NULL, 0), ==, ZATHURA_ERROR_INVALID_ARGUMENTS);
  g_assert_cmpint(zathura_page_annotation_set_contents(NULL, 0, "x"), ==, ZATHURA_ERROR_INVALID_ARGUMENTS);
}

static void test_add_and_get(fixture_t* fixture, gconstpointer UNUSED(data)) {
  g_autoptr(girara_list_t) list = zathura_page_annotations_get(fixture->page, NULL);
  g_assert_nonnull(list);
  g_assert_cmpuint(girara_list_size(list), ==, 0);
  g_assert_false(zathura_document_is_modified(fixture->document));

  zathura_rectangle_t quad        = {10, 20, 30, 40};
  zathura_annotation_t annotation = highlight(&quad, "a comment");
  g_assert_cmpint(zathura_page_annotation_add(fixture->page, NULL), ==, ZATHURA_ERROR_INVALID_ARGUMENTS);
  g_assert_false(zathura_document_is_modified(fixture->document));
  g_assert_cmpint(zathura_page_annotation_add(fixture->page, &annotation), ==, ZATHURA_ERROR_OK);
  g_assert_true(zathura_document_is_modified(fixture->document));

  zathura_annotation_t note = {.type = ZATHURA_ANNOTATION_TEXT, .rect = {50, 60, 70, 80}, .contents = "a note"};
  g_assert_cmpint(zathura_page_annotation_add(fixture->page, &note), ==, ZATHURA_ERROR_OK);

  g_autoptr(girara_list_t) annotations = zathura_page_annotations_get(fixture->page, NULL);
  g_assert_nonnull(annotations);
  g_assert_cmpuint(girara_list_size(annotations), ==, 2);

  const zathura_annotation_t* first = girara_list_nth(annotations, 0);
  g_assert_cmpint(first->type, ==, ZATHURA_ANNOTATION_HIGHLIGHT);
  g_assert_cmpuint(first->index, ==, 0);
  g_assert_cmpuint(first->n_quads, ==, 1);
  g_assert_cmpfloat(first->quads[0].x1, ==, 10);
  g_assert_cmpfloat(first->quads[0].y2, ==, 40);
  g_assert_true(first->has_color);
  g_assert_cmpfloat(first->color[0], ==, 1);
  g_assert_cmpstr(first->contents, ==, "a comment");

  const zathura_annotation_t* second = girara_list_nth(annotations, 1);
  g_assert_cmpint(second->type, ==, ZATHURA_ANNOTATION_TEXT);
  g_assert_cmpuint(second->index, ==, 1);
  g_assert_cmpstr(second->contents, ==, "a note");
}

static void test_failed_changes_do_not_modify(fixture_t* fixture, gconstpointer UNUSED(data)) {
  zathura_annotation_t other = {.type = ZATHURA_ANNOTATION_OTHER};
  g_assert_cmpint(zathura_page_annotation_add(fixture->page, &other), !=, ZATHURA_ERROR_OK);

  zathura_annotation_t no_quads = {.type = ZATHURA_ANNOTATION_UNDERLINE};
  g_assert_cmpint(zathura_page_annotation_add(fixture->page, &no_quads), !=, ZATHURA_ERROR_OK);

  g_assert_cmpint(zathura_page_annotation_delete(fixture->page, 0), !=, ZATHURA_ERROR_OK);
  g_assert_cmpint(zathura_page_annotation_set_contents(fixture->page, 0, "x"), !=, ZATHURA_ERROR_OK);
  g_assert_cmpint(zathura_page_annotation_set_contents(fixture->page, 0, NULL), ==, ZATHURA_ERROR_INVALID_ARGUMENTS);

  g_assert_false(zathura_document_is_modified(fixture->document));
  g_autoptr(girara_list_t) list = zathura_page_annotations_get(fixture->page, NULL);
  g_assert_cmpuint(girara_list_size(list), ==, 0);
}

static void test_set_contents_and_delete(fixture_t* fixture, gconstpointer UNUSED(data)) {
  zathura_rectangle_t quad = {0, 0, 10, 10};
  zathura_annotation_t one = highlight(&quad, "one");
  zathura_annotation_t two = highlight(&quad, "two");
  g_assert_cmpint(zathura_page_annotation_add(fixture->page, &one), ==, ZATHURA_ERROR_OK);
  g_assert_cmpint(zathura_page_annotation_add(fixture->page, &two), ==, ZATHURA_ERROR_OK);
  zathura_document_set_modified(fixture->document, false);

  g_assert_cmpint(zathura_page_annotation_set_contents(fixture->page, 1, "changed"), ==, ZATHURA_ERROR_OK);
  g_assert_true(zathura_document_is_modified(fixture->document));
  {
    g_autoptr(girara_list_t) list = zathura_page_annotations_get(fixture->page, NULL);
    g_assert_cmpstr(((zathura_annotation_t*)girara_list_nth(list, 0))->contents, ==, "one");
    g_assert_cmpstr(((zathura_annotation_t*)girara_list_nth(list, 1))->contents, ==, "changed");
  }

  zathura_document_set_modified(fixture->document, false);
  g_assert_cmpint(zathura_page_annotation_delete(fixture->page, 0), ==, ZATHURA_ERROR_OK);
  g_assert_true(zathura_document_is_modified(fixture->document));
  {
    g_autoptr(girara_list_t) list = zathura_page_annotations_get(fixture->page, NULL);
    g_assert_cmpuint(girara_list_size(list), ==, 1);
    /* the remaining annotation moved to the front */
    g_assert_cmpuint(((zathura_annotation_t*)girara_list_nth(list, 0))->index, ==, 0);
    g_assert_cmpstr(((zathura_annotation_t*)girara_list_nth(list, 0))->contents, ==, "changed");
  }
}

static void test_save_clears_modified(fixture_t* fixture, gconstpointer UNUSED(data)) {
  zathura_rectangle_t quad        = {0, 0, 10, 10};
  zathura_annotation_t annotation = highlight(&quad, NULL);
  g_assert_cmpint(zathura_page_annotation_add(fixture->page, &annotation), ==, ZATHURA_ERROR_OK);
  g_assert_true(zathura_document_is_modified(fixture->document));

  /* a failed save keeps the changes unsaved */
  g_assert_cmpint(zathura_document_save_as(fixture->document, "/nonexistent/document.txt"), !=, ZATHURA_ERROR_OK);
  g_assert_true(zathura_document_is_modified(fixture->document));

  g_assert_cmpint(zathura_document_save_as(fixture->document, fixture->file), ==, ZATHURA_ERROR_OK);
  g_assert_false(zathura_document_is_modified(fixture->document));
}

int main(int argc, char* argv[]) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/annotation/free", test_annotation_free);
  g_test_add_func("/annotation/modified_flag", test_modified_flag);
  g_test_add_func("/annotation/invalid_arguments", test_invalid_arguments);
  g_test_add("/annotation/add_and_get", fixture_t, NULL, fixture_set_up, test_add_and_get, fixture_tear_down);
  g_test_add("/annotation/failed_changes_do_not_modify", fixture_t, NULL, fixture_set_up,
             test_failed_changes_do_not_modify, fixture_tear_down);
  g_test_add("/annotation/set_contents_and_delete", fixture_t, NULL, fixture_set_up, test_set_contents_and_delete,
             fixture_tear_down);
  g_test_add("/annotation/save_clears_modified", fixture_t, NULL, fixture_set_up, test_save_clears_modified,
             fixture_tear_down);
  return g_test_run();
}
