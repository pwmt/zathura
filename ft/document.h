/* See LICENSE file for license and copyright information */

#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <gtk/gtk.h>
#include <stdbool.h>

#include <girara-datastructures.h>

typedef struct zathura_list_s zathura_list_t;
typedef struct zathura_document_s zathura_document_t;

typedef bool (*zathura_document_open_t)(zathura_document_t* document);

typedef struct zathura_document_plugin_s
{
  const char* file_extension;
  zathura_document_open_t open_function;
} zathura_document_plugin_t;

struct zathura_list_s
{
  void* data;
  struct zathura_list_s* next;
};

typedef struct zathura_rectangle_s
{
  double x1;
  double y1;
  double x2;
  double y2;
} zathura_rectangle_t;

typedef enum zathura_link_type_e
{
  ZATHURA_LINK_TO_PAGE,
  ZATHURA_LINK_EXTERNAL,
} zathura_link_type_t;

typedef struct zathura_link_s
{
  zathura_rectangle_t position;
  zathura_link_type_t type;
  union
  {
    unsigned int page_number;
    char* value;
  } target;
} zathura_link_t;

typedef struct zathura_index_element_s
{
  char* title;
  zathura_link_type_t type;
  union
  {
    unsigned int page_number;
    char* uri;
  } target;
} zathura_index_element_t;

typedef enum zathura_form_type_e
{
  ZATHURA_FORM_CHECKBOX,
  ZATHURA_FORM_TEXTFIELD
} zathura_form_type_t;

typedef struct zathura_form_s
{
  zathura_rectangle_t position;
  zathura_form_type_t type;
} zathura_form_t;

typedef struct zathura_page_s
{
  double height;
  double width;
  zathura_document_t* document;
  void* data;
} zathura_page_t;

struct zathura_document_s
{
  char* file_path;
  const char* password;
  unsigned int current_page_number;
  unsigned int number_of_pages;
  double scale;
  int rotate;
  void* data;

  struct
  {
    bool (*document_free)(zathura_document_t* document);
    girara_tree_node_t* (*document_index_generate)(zathura_document_t* document);
    bool (*document_save_as)(zathura_document_t* document, const char* path);
    zathura_list_t* (*document_attachments_get)(zathura_document_t* document);

    zathura_page_t* (*page_get)(zathura_document_t* document, unsigned int page);
    zathura_list_t* (*page_search_text)(zathura_page_t* page, const char* text);
    zathura_list_t* (*page_links_get)(zathura_page_t* page);
    zathura_list_t* (*page_form_fields_get)(zathura_page_t* page);
    GtkWidget* (*page_render)(zathura_page_t* page);
    bool (*page_free)(zathura_page_t* page);
  } functions;

  zathura_page_t** pages;
};

zathura_document_t* zathura_document_open(const char* path, const char* password);
bool zathura_document_free(zathura_document_t* document);
bool zathura_document_save_as(zathura_document_t* document, const char* path);
girara_tree_node_t* zathura_document_index_generate(zathura_document_t* document);
bool zathura_document_index_free(zathura_list_t* list);
zathura_list_t* zathura_document_attachments_get(zathura_document_t* document);
bool zathura_document_attachments_free(zathura_list_t* list);

zathura_page_t* zathura_page_get(zathura_document_t* document, unsigned int page);
bool zathura_page_free(zathura_page_t* page);
zathura_list_t* zathura_page_search_text(zathura_page_t* page, const char* text);
zathura_list_t* zathura_page_links_get(zathura_page_t* page);
bool zathura_page_links_free(zathura_list_t* list);
zathura_list_t* zathura_page_form_fields_get(zathura_page_t* page);
bool zathura_page_form_fields_free(zathura_list_t* list);
GtkWidget* zathura_page_render(zathura_page_t* page);

zathura_index_element_t* zathura_index_element_new(const char* title);
void zathura_index_element_free(zathura_index_element_t* index);

#endif // DOCUMENT_H
