/* SPDX-License-Identifier: Zlib */

#include "index-element-object.h"

G_DEFINE_TYPE(ZathuraIndexElementObject, zathura_index_element_object, G_TYPE_OBJECT)

static void zathura_index_element_object_dispose(GObject* object) {
  ZathuraIndexElementObject* self = ZATHURA_INDEX_ELEMENT_OBJECT(object);
  g_clear_object(&self->children);
  G_OBJECT_CLASS(zathura_index_element_object_parent_class)->dispose(object);
}

static void zathura_index_element_object_finalize(GObject* object) {
  ZathuraIndexElementObject* self = ZATHURA_INDEX_ELEMENT_OBJECT(object);
  g_clear_pointer(&self->title, g_free);
  g_clear_pointer(&self->page_label, g_free);
  g_clear_pointer(&self->page_alt, g_free);
  if (self->element != NULL) {
    zathura_index_element_free(self->element);
    self->element = NULL;
  }
  G_OBJECT_CLASS(zathura_index_element_object_parent_class)->finalize(object);
}

static void zathura_index_element_object_class_init(ZathuraIndexElementObjectClass* klass) {
  GObjectClass* object_class = G_OBJECT_CLASS(klass);
  object_class->dispose      = zathura_index_element_object_dispose;
  object_class->finalize     = zathura_index_element_object_finalize;
}

static void zathura_index_element_object_init(ZathuraIndexElementObject* UNUSED(self)) {}
