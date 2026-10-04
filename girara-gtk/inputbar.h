/* SPDX-License-Identifier: Zlib */

#ifndef GIRARA_INPUTBAR_H
#define GIRARA_INPUTBAR_H

#include <gtk/gtk.h>
#include "types.h"

#define GIRARA_TYPE_INPUTBAR (girara_inputbar_get_type())
G_DECLARE_DERIVABLE_TYPE(GiraraInputbar, girara_inputbar, GIRARA, INPUTBAR, GtkBox)

struct _GiraraInputbarClass {
  GtkBoxClass parent_class;
};

/**
 * Creates an inputbar widget for commands.
 *
 * @return The created inputbar widget
 */
GtkWidget* girara_inputbar_new(girara_session_t* session);

/**
 * Returns the entry owned by the inputbar.
 *
 * @param inputbar The inputbar widget
 * @return The entry, without transferring ownership
 */
GtkEntry* girara_inputbar_get_entry(GiraraInputbar* inputbar);

/* run a bound inputbar shortcut for the key */
gboolean girara_process_inputbar_key(girara_session_t* session, guint keyval, guint clean);

#endif
