/* SPDX-License-Identifier: Zlib */

#ifndef INDEX_ELEMENT_OBJECT_H
#define INDEX_ELEMENT_OBJECT_H

#include "types.h"

#include <gtk/gtk.h>

#define ZATHURA_TYPE_INDEX_ELEMENT_OBJECT (zathura_index_element_object_get_type())

/* GObject wrapping zathura_index_element_t so it can live in a GListModel */
G_DECLARE_FINAL_TYPE(ZathuraIndexElementObject, zathura_index_element_object, ZATHURA, INDEX_ELEMENT_OBJECT, GObject)

struct _ZathuraIndexElementObject {
  GObject parent_instance;
  char* title;                      /* escaped markup for column 1 */
  char* page_label;                 /* primary page string for column 2 */
  char* page_alt;                   /* alt page string for column 3 */
  zathura_index_element_t* element; /* link target */
  GListStore* children;             /* NULL for leaves */
};

#endif