/* SPDX-License-Identifier: Zlib */

#ifndef GIRARA_ENTRY_H
#define GIRARA_ENTRY_H

#include <gtk/gtk.h>

/*
 * Type macros.
 */
#define GIRARA_TYPE_ENTRY (girara_entry_get_type())
#define GIRARA_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GIRARA_TYPE_ENTRY, GiraraEntry))
#define GIRARA_IS_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GIRARA_TYPE_ENTRY))
#define GIRARA_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GIRARA_TYPE_ENTRY, GiraraEntryClass))
#define GIRARA_IS_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GIRARA_TYPE_ENTRY))
#define GIRARA_ENTRY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GIRARA_TYPE_ENTRY, GiraraEntryClass))

typedef struct girara_entry_s GiraraEntry;
typedef struct girara_entry_class_s GiraraEntryClass;

struct girara_entry_s {
  /* Parent instance structure */
  GtkEntry parent_instance;

  /* instance members */
};

struct girara_entry_class_s {
  /* Parent class structure */
  GtkEntryClass parent_class;

  /* class members */
  void (*paste_primary)(GiraraEntry*);
};

GType girara_entry_get_type(void) G_GNUC_CONST;

GiraraEntry* girara_entry_new(void);

#endif
