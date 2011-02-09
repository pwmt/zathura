/* See LICENSE file for license and copyright information */

#define _BSD_SOURCE
#define _XOPEN_SOURCE 500
// TODO: Implement realpath

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "document.h"
#include "pdf/pdf.h"
#include "djvu/djvu.h"
#include "../utils.h"
#include "../zathura.h"

#define LENGTH(x) (sizeof(x)/sizeof((x)[0]))

zathura_document_plugin_t zathura_document_plugins[] = {
  { "pdf",  pdf_document_open },
  { "djvu", djvu_document_open },
};

zathura_document_t*
zathura_document_open(const char* path, const char* password)
{
  if (!path) {
    goto error_out;
  }

  if (!file_exists(path)) {
    fprintf(stderr, "error: file does not exist\n");
    goto error_out;
  }

  const char* file_extension = file_get_extension(path);
  if (!file_extension) {
    fprintf(stderr, "error: could not determine file type\n");
    goto error_out;
  }

  /* determine real path */
  size_t path_max;
#ifdef PATH_MAX
  path_max = PATH_MAX;
#else
  path_max = pathconf(path,_PC_PATH_MAX);
  if (path_max <= 0)
    path_max = 4096;
#endif

  char* real_path              = NULL;
  zathura_document_t* document = NULL;

  real_path = malloc(sizeof(char) * path_max);
  if (!real_path) {
    goto error_out;
  }

  if (!realpath(path, real_path)) {
    goto error_free;
  }

  document = malloc(sizeof(zathura_document_t));
  if (!document) {
    goto error_free;
  }

  document->file_path           = real_path;
  document->password            = password;
  document->current_page_number = 0;
  document->number_of_pages     = 0;
  document->scale               = 1.0;
  document->rotate              = 0;
  document->data                = NULL;
  document->pages               = NULL;

  document->functions.document_free            = NULL;
  document->functions.document_index_generate  = NULL;
  document->functions.document_save_as         = NULL;
  document->functions.document_attachments_get = NULL;
  document->functions.page_get                 = NULL;
  document->functions.page_free                = NULL;
  document->functions.page_search_text         = NULL;
  document->functions.page_links_get           = NULL;
  document->functions.page_form_fields_get     = NULL;
  document->functions.page_render              = NULL;

  /* init plugin with associated file type */
  for (unsigned int i = 0; i < LENGTH(zathura_document_plugins); i++)
  {
    if (!strcmp(file_extension, zathura_document_plugins[i].file_extension)) {
      if (zathura_document_plugins[i].open_function) {
        if (zathura_document_plugins[i].open_function(document)) {
          /* update statusbar */
          girara_statusbar_item_set_text(Zathura.UI.session, Zathura.UI.statusbar.file, real_path);

          /* read all pages */
          document->pages = calloc(document->number_of_pages, sizeof(zathura_page_t*));
          if (!document->pages) {
            goto error_free;
          }

          double offset = 0;
          for (unsigned int page_id = 0; page_id < document->number_of_pages; page_id++) {
            zathura_page_t* page = zathura_page_get(document, page_id);
            if (!page) {
              goto error_free;
            }

            page->offset = offset;
            offset += page->height;

            document->pages[page_id] = page;
          }

          return document;
        } else {
          fprintf(stderr, "error: could not open file\n");
          goto error_free;
        }
      }
    }
  }

  fprintf(stderr, "error: unknown file type\n");

error_free:

  free(real_path);

  if (document && document->pages) {
    for (unsigned int page_id = 0; page_id < document->number_of_pages; page_id++)
    {
      zathura_page_free(document->pages[page_id]);
    }

    free(document->pages);
  }

  free(document);

error_out:

  return NULL;
}

bool
zathura_document_free(zathura_document_t* document)
{
  if (!document) {
    return false;
  }

  /* free pages */
  for (unsigned int page_id = 0; page_id < document->number_of_pages; page_id++)
  {
    zathura_page_free(document->pages[page_id]);
  }

  free(document->pages);

  /* free document */
  if (!document->functions.document_free) {
    fprintf(stderr, "error: %s not implemented\n", __FUNCTION__);

    if (document->file_path) {
      free(document->file_path);
    }

    free(document);
    return true;
  }

  bool r = document->functions.document_free(document);

  if (document->file_path) {
    free(document->file_path);
  }

  free(document);

  return r;
}

bool
zathura_document_save_as(zathura_document_t* document, const char* path)
{
  if (!document || !path) {
    return false;
  }

  if (!document->functions.document_save_as) {
    fprintf(stderr, "error: %s not implemented\n", __FUNCTION__);
    return false;
  }

  return document->functions.document_save_as(document, path);
}

girara_tree_node_t*
zathura_document_index_generate(zathura_document_t* document)
{
  if (!document) {
    return NULL;
  }

  if (!document->functions.document_index_generate) {
    fprintf(stderr, "error: %s not implemented\n", __FUNCTION__);
    return NULL;
  }

  return document->functions.document_index_generate(document);
}

bool
zathura_document_index_free(zathura_list_t* list)
{
  return false;
}

zathura_list_t*
zathura_document_attachments_get(zathura_document_t* document)
{
  if (!document) {
    return NULL;
  }

  if (!document->functions.document_attachments_get) {
    fprintf(stderr, "error: %s not implemented\n", __FUNCTION__);
    return NULL;
  }

  return document->functions.document_attachments_get(document);
}

bool
zathura_document_attachments_free(zathura_list_t* list)
{
  return false;
}

zathura_page_t*
zathura_page_get(zathura_document_t* document, unsigned int page_id)
{
  if (!document) {
    return NULL;
  }

  if (!document->functions.page_get) {
    fprintf(stderr, "error: %s not implemented\n", __FUNCTION__);
    return NULL;
  }

  zathura_page_t* page = document->functions.page_get(document, page_id);

  if (page) {
    page->number   = page_id;
    page->rendered = false;
    g_static_mutex_init(&(page->lock));
  }

  return page;
}

bool
zathura_page_free(zathura_page_t* page)
{
  if (!page || !page->document) {
    return false;
  }

  if (!page->document->functions.page_free) {
    fprintf(stderr, "error: %s not implemented\n", __FUNCTION__);
    return false;
  }

  return page->document->functions.page_free(page);
}

zathura_list_t*
zathura_page_search_text(zathura_page_t* page, const char* text)
{
  if (!page || !page->document || !text) {
    return NULL;
  }

  if (!page->document->functions.page_search_text) {
    fprintf(stderr, "error: %s not implemented\n", __FUNCTION__);
    return NULL;
  }

  return page->document->functions.page_search_text(page, text);
}

zathura_list_t*
zathura_page_links_get(zathura_page_t* page)
{
  if (!page || !page->document) {
    return NULL;
  }

  if (!page->document->functions.page_links_get) {
    fprintf(stderr, "error: %s not implemented\n", __FUNCTION__);
    return NULL;
  }

  return page->document->functions.page_links_get(page);
}

bool
zathura_page_links_free(zathura_list_t* list)
{
  return false;
}

zathura_list_t*
zathura_page_form_fields_get(zathura_page_t* page)
{
  if (!page || !page->document) {
    return NULL;
  }

  if (!page->document->functions.page_form_fields_get) {
    fprintf(stderr, "error: %s not implemented\n", __FUNCTION__);
    return NULL;
  }

  return page->document->functions.page_form_fields_get(page);
}

bool
zathura_page_form_fields_free(zathura_list_t* list)
{
  return false;
}

GtkWidget*
zathura_page_render(zathura_page_t* page)
{
  if (!page || !page->document) {
    return NULL;
  }

  if (!page->document->functions.page_render) {
    fprintf(stderr, "error: %s not implemented\n", __FUNCTION__);
    return NULL;
  }

  GtkWidget* widget = page->document->functions.page_render(page);

  if (widget) {
    page->rendered = true;
  }

  return widget;
}

zathura_index_element_t*
zathura_index_element_new(const char* title)
{
  if (!title) {
    return NULL;
  }

  zathura_index_element_t* res = g_malloc0(sizeof(zathura_index_element_t));

  if (!res) {
    return NULL;
  }

  res->title = g_strdup(title);

  return res;
}

void
zathura_index_element_free(zathura_index_element_t* index)
{
  if (!index) {
    return;
  }

  g_free(index->title);

  if (index->type == ZATHURA_LINK_EXTERNAL) {
    g_free(index->target.uri);
  }

  g_free(index);
}
