/* SPDX-License-Identifier: Zlib */

#ifndef GIRARA_DIALOG_H
#define GIRARA_DIALOG_H

#include "inputbar.h"
#include "types.h"

#define GIRARA_TYPE_DIALOG (girara_dialog_get_type())
G_DECLARE_FINAL_TYPE(GiraraDialog, girara_dialog, GIRARA, DIALOG, GiraraInputbar)

GiraraDialog* girara_dialog_new(girara_session_t* session, const char* prompt, bool invisible);

#endif
