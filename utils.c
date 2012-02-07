/* See LICENSE file for license and copyright information */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "utils.h"
#include "zathura.h"
#include "document.h"

#include <girara/datastructures.h>

#define BLOCK_SIZE 64

bool
file_exists(const char* path)
{
  if (!access(path, F_OK)) {
    return true;
  } else {
    return false;
  }
}

const char*
file_get_extension(const char* path)
{
  if (!path) {
    return NULL;
  }

  unsigned int i = strlen(path);
  for (; i > 0; i--)
  {
    if (*(path + i) != '.') {
      continue;
    } else {
      break;
    }
  }

  if (!i) {
    return NULL;
  }

  return path + i + 1;
}

bool
file_valid_extension(zathura_t* zathura, const char* path)
{
  if (path == NULL) {
    return false;
  }

  const gchar* content_type = g_content_type_guess(path, NULL, 0, NULL);
  if (content_type == NULL) {
    return false;
  }

  bool result = false;
  GIRARA_LIST_FOREACH(zathura->plugins.type_plugin_mapping, zathura_type_plugin_mapping_t*, iter, mapping)
    if (g_content_type_equals(content_type, mapping->type)) {
      result = true;
      break;
    }
  GIRARA_LIST_FOREACH_END(zathura->plugins.type_plugin_mapping, zathura_type_plugin_mapping_t*, iter, mapping);

  g_free((void*)content_type);
  return result;
}

bool
execute_command(char* const argv[], char** output)
{
  if (!output) {
    return false;
  }

  int p[2];
  if (pipe(p)) {
    return -1;
  }

  pid_t pid = fork();

  if (pid == -1) { // failure
    return false;
  } else if (pid == 0) { // child
    dup2(p[1], 1);
    close(p[0]);

    if (execvp(argv[0], argv) == -1) {
      return false;
    }
  } else { // parent
    dup2(p[0], 0);
    close(p[1]);

    /* read output */
    unsigned int bc = BLOCK_SIZE;
    unsigned int i  = 0;
    char* buffer    = malloc(sizeof(char) * bc);
    *output = NULL;

    if (!buffer) {
      close(p[0]);
      return false;
    }

    char c;
    while (1 == read(p[0], &c, 1)) {
      buffer[i++] = c;

      if (i == bc) {
        bc += BLOCK_SIZE;
        char* tmp = realloc(buffer, sizeof(char) * bc);

        if (!tmp) {
          free(buffer);
          close(p[0]);
          return false;
        }

        buffer = tmp;
      }
    }

    char* tmp = realloc(buffer, sizeof(char) * (bc + 1));
    if (!tmp) {
      free(buffer);
      close(p[0]);
      return false;
    }

    buffer = tmp;
    buffer[i] = '\0';

    *output = buffer;

    /* wait for child to terminate */
    waitpid(pid, NULL, 0);
    close(p[0]);
  }

  return true;
}

void
document_index_build(GtkTreeModel* model, GtkTreeIter* parent,
    girara_tree_node_t* tree)
{
  girara_list_t* list        = girara_node_get_children(tree);
  GIRARA_LIST_FOREACH(list, girara_tree_node_t*, iter, node)
    zathura_index_element_t* index_element = (zathura_index_element_t*)girara_node_get_data(node);

    gchar* description = NULL;
    if (index_element->type == ZATHURA_LINK_TO_PAGE) {
      description = g_strdup_printf("Page %d", index_element->target.page_number);
    } else {
      description = g_strdup(index_element->target.uri);
    }

    GtkTreeIter tree_iter;
    gtk_tree_store_append(GTK_TREE_STORE(model), &tree_iter, parent);
    gtk_tree_store_set(GTK_TREE_STORE(model), &tree_iter, 0, index_element->title, 1, description, 2, index_element, -1);
    g_object_weak_ref(G_OBJECT(model), (GWeakNotify) zathura_index_element_free, index_element);
    g_free(description);

    if (girara_node_get_num_children(node) > 0) {
      document_index_build(model, &tree_iter, node);
    }

  GIRARA_LIST_FOREACH_END(list, gchar*, iter, name);
}

void
page_calculate_offset(zathura_page_t* page, page_offset_t* offset)
{
  g_return_if_fail(page != NULL && page->document != NULL && page->document->zathura != NULL && offset != NULL);
  zathura_document_t* document = page->document;
  zathura_t* zathura           = document->zathura;

  g_return_if_fail(gtk_widget_translate_coordinates(page->drawing_area, zathura->ui.page_view, 0, 0, &(offset->x), &(offset->y)) == true);
}

zathura_rectangle_t
recalc_rectangle(zathura_page_t* page, zathura_rectangle_t rectangle)
{
  if (page == NULL || page->document == NULL) {
    return rectangle;
  }

  zathura_rectangle_t tmp;

  switch (page->document->rotate) {
    case 90:
      tmp.x1 = rectangle.y2 * page->document->scale;
      tmp.x2 = rectangle.y1 * page->document->scale;
      tmp.y1 = rectangle.x1 * page->document->scale;
      tmp.y2 = rectangle.x2 * page->document->scale;
      break;
    case 180:
      tmp.x1 = (page->width - rectangle.x2) * page->document->scale;
      tmp.x2 = (page->width - rectangle.x1) * page->document->scale;
      tmp.y1 = rectangle.y2 * page->document->scale;
      tmp.y2 = rectangle.y1 * page->document->scale;
      break;
    case 270:
      tmp.x1 = (page->height - rectangle.y1) * page->document->scale;
      tmp.x2 = (page->height - rectangle.y2) * page->document->scale;
      tmp.y1 = (page->width  - rectangle.x2) * page->document->scale;
      tmp.y2 = (page->width  - rectangle.x1) * page->document->scale;
      break;
    default:
      tmp.x1 = rectangle.x1 * page->document->scale;
      tmp.x2 = rectangle.x2 * page->document->scale;
      tmp.y1 = (page->height - rectangle.y1) * page->document->scale;
      tmp.y2 = (page->height - rectangle.y2) * page->document->scale;
  }

  return tmp;
}
