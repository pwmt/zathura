/* See LICENSE file for license and copyright information */

#include <stdlib.h>
#include <poppler/glib/poppler.h>

#include "pdf.h"
#include "../../zathura.h"

bool
pdf_document_open(zathura_document_t* document)
{
  if(!document) {
    goto error_out;
  }

  document->functions.document_free             = pdf_document_free;
  document->functions.document_index_generate   = pdf_document_index_generate;;
  document->functions.document_save_as          = pdf_document_save_as;
  document->functions.document_attachments_get  = pdf_document_attachments_get;
  document->functions.page_get                  = pdf_page_get;
  document->functions.page_search_text          = pdf_page_search_text;
  document->functions.page_links_get            = pdf_page_links_get;
  document->functions.page_form_fields_get      = pdf_page_form_fields_get;
  document->functions.page_render               = pdf_page_render;
  document->functions.page_free                 = pdf_page_free;

  document->data = malloc(sizeof(pdf_document_t));
  if(!document->data) {
    goto error_out;
  }

  /* format path */
  GError* error  = NULL;
  char* file_uri = g_filename_to_uri(document->file_path, NULL, &error);

  if(!file_uri) {
    fprintf(stderr, "error: could not open file: %s\n", error->message);
    goto error_free;
  }

  pdf_document_t* pdf_document = (pdf_document_t*) document->data;
  pdf_document->document       = poppler_document_new_from_file(file_uri, document->password, &error);

  if(!pdf_document->document) {
    fprintf(stderr, "error: could not open file: %s\n", error->message);
    goto error_free;
  }

  document->number_of_pages = poppler_document_get_n_pages(pdf_document->document);

  return true;

error_free:

    if(error) {
      g_error_free(error);
    }

    if(file_uri) {
      g_free(file_uri);
    }

    free(document->data);
    document->data = NULL;

error_out:

  return false;
}

bool
pdf_document_free(zathura_document_t* document)
{
  if(!document) {
    return false;
  }

  if(document->data) {
    pdf_document_t* pdf_document = (pdf_document_t*) document->data;
    g_object_unref(pdf_document->document);
    free(document->data);
    document->data = NULL;
  }

  return true;
}

static void
build_index(pdf_document_t* pdf, girara_tree_node_t* root, PopplerIndexIter* iter)
{
  if(!root || !iter) {
    return;
  }

  do
  {
    PopplerAction* action = poppler_index_iter_get_action(iter);

    if(!action) {
      continue;
    }

    gchar* markup = g_markup_escape_text(action->any.title, -1);
    zathura_index_element_t* indexelement = zathura_index_element_new(markup);

    if(action->type == POPPLER_ACTION_URI) {
      indexelement->type = ZATHURA_LINK_EXTERNAL;
      indexelement->target.uri = g_strdup(action->uri.uri);
    } else if (action->type == POPPLER_ACTION_GOTO_DEST) {
      indexelement->type = ZATHURA_LINK_TO_PAGE;

      if(action->goto_dest.dest->type == POPPLER_DEST_NAMED) {
        PopplerDest* dest = poppler_document_find_dest(pdf->document, action->goto_dest.dest->named_dest);
        if(dest) {
          indexelement->target.page_number = dest->page_num - 1;
          poppler_dest_free(dest);
        }
      } else {
        indexelement->target.page_number = action->goto_dest.dest->page_num - 1;
      }
    } else {
      poppler_action_free(action);
      zathura_index_element_free(indexelement);
      continue;
    }

    poppler_action_free(action);

    girara_tree_node_t* node = girara_node_append_data(root, indexelement);
    PopplerIndexIter* child  = poppler_index_iter_get_child(iter);

    if(child) {
      build_index(pdf, node, child);
    }

    poppler_index_iter_free(child);

  } while (poppler_index_iter_next(iter));
}

girara_tree_node_t*
pdf_document_index_generate(zathura_document_t* document)
{
  if(!document || !document->data) {
    return NULL;
  }

  pdf_document_t* pdf_document = (pdf_document_t*) document->data;
  PopplerIndexIter* iter       = poppler_index_iter_new(pdf_document->document);

  if(!iter) {
    // XXX: error message?
    return NULL;
  }

  girara_tree_node_t* root = girara_node_new(zathura_index_element_new("ROOT"));
  girara_node_set_free_function(root, (girara_free_function_t)zathura_index_element_free);
  build_index(pdf_document, root, iter);

  poppler_index_iter_free(iter);
  return root;
}

bool
pdf_document_save_as(zathura_document_t* document, const char* path)
{
  if(!document || !document->data || !path) {
    return false;
  }

  pdf_document_t* pdf_document = (pdf_document_t*) document->data;

  char* file_path = g_strdup_printf("file://%s", path);
  poppler_document_save(pdf_document->document, file_path, NULL);
  g_free(file_path);

  return false;
}

zathura_list_t*
pdf_document_attachments_get(zathura_document_t* document)
{
  return NULL;
}

zathura_page_t*
pdf_page_get(zathura_document_t* document, unsigned int page)
{
  if(!document || !document->data) {
    return NULL;
  }

  pdf_document_t* pdf_document  = (pdf_document_t*) document->data;
  zathura_page_t* document_page = malloc(sizeof(zathura_page_t));

  if(!document_page) {
    return NULL;
  }

  document_page->document = document;
  document_page->data     = poppler_document_get_page(pdf_document->document, page);

  if(!document_page->data) {
    free(document_page);
    return NULL;
  }

  poppler_page_get_size(document_page->data, &(document_page->width), &(document_page->height));

  return document_page;
}

bool
pdf_page_free(zathura_page_t* page)
{
  if(!page) {
    return false;
  }

  g_object_unref(page->data);
  free(page);

  return true;
}

zathura_list_t*
pdf_page_search_text(zathura_page_t* page, const char* text)
{
  return NULL;
}

zathura_list_t*
pdf_page_links_get(zathura_page_t* page)
{
  return NULL;
}

zathura_list_t*
pdf_page_form_fields_get(zathura_page_t* page)
{
  return NULL;
}

GtkWidget*
pdf_page_render(zathura_page_t* page)
{
  if(!Zathura.document || !page || !page->data || !page->document) {
    return NULL;
  }

  /* calculate sizes */
  unsigned int page_width  = Zathura.document->scale * page->width;
  unsigned int page_height = Zathura.document->scale * page->height;

  if(Zathura.document->rotate == 90 || Zathura.document->rotate == 270) {
      unsigned int dim_temp = 0;
      dim_temp    = page_width;
      page_width  = page_height;
      page_height = dim_temp;
  }

  /* create pixbuf */
  GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
      page_width, page_height);

  if(!pixbuf) {
    return NULL;
  }

  poppler_page_render_to_pixbuf(page->data, 0, 0, page_width, page_height, Zathura.document->scale,
      Zathura.document->rotate, pixbuf);

  /* write pixbuf */
  GtkWidget* image = gtk_image_new();

  if(!image) {
    g_object_unref(pixbuf);
    return NULL;
  }

  gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);
  gtk_widget_show(image);

  return image;
}
