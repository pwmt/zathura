/* SPDX-License-Identifier: Zlib */

#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <string.h>
#include <json-glib/json-glib.h>
#include <girara/datastructures.h>
#include <girara/utils.h>

#include "settings.h"
#include "completion.h"
#include "session.h"
#include "internal.h"

/**
 * Structure of a settings entry
 */
struct girara_setting_s {
  char* name;        /**< Name of the setting */
  char* description; /**< Description of this setting */
  union {
    bool b;                           /**< Boolean */
    int i;                            /**< Integer */
    float f;                          /**< Floating number */
    char* s;                          /**< String */
  } value;                            /**< Value of the setting */
  girara_setting_callback_t callback; /**< Callback that gets executed when the value of the setting changes */
  void* data;                         /**< Arbitrary data that can be used by callbacks */
  girara_setting_type_t type;         /**< Type identifier */
  bool init_only;                     /**< Option can be set only before girara gets initialized */
};

void girara_setting_set_value(girara_session_t* session, girara_setting_t* setting, const void* value) {
  g_return_if_fail(setting && (value || setting->type == STRING));

  switch (setting->type) {
  case BOOLEAN:
    setting->value.b = *((const bool*)value);
    break;
  case FLOAT:
    setting->value.f = *((const float*)value);
    break;
  case INT:
    setting->value.i = *((const int*)value);
    break;
  case STRING:
    if (setting->value.s != NULL) {
      g_free(setting->value.s);
    }
    setting->value.s = value ? g_strdup(value) : NULL;
    break;
  default:
    g_assert(false);
  }

  if (session && setting->callback != NULL) {
    setting->callback(session, setting->name, setting->type, value, setting->data);
  }
}

bool girara_setting_add(girara_session_t* session, const char* name, const void* value, girara_setting_type_t type,
                        bool init_only, const char* description, girara_setting_callback_t callback, void* data) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(name != NULL, false);
  g_return_val_if_fail(type != UNKNOWN, false);
  if (type != STRING && value == NULL) {
    return false;
  }

  /* search for existing setting */
  if (girara_setting_find(session, name) != NULL) {
    return false;
  }

  /* add new setting */
  girara_setting_t* setting = g_malloc0(sizeof(girara_setting_t));

  setting->name        = g_strdup(name);
  setting->type        = type;
  setting->init_only   = init_only;
  setting->description = description ? g_strdup(description) : NULL;
  setting->callback    = callback;
  setting->data        = data;
  girara_setting_set_value(NULL, setting, value);

  girara_list_append(session->private_data->settings, setting);

  return true;
}

bool girara_setting_set(girara_session_t* session, const char* name, const void* value) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(name != NULL, false);

  girara_setting_t* setting = girara_setting_find(session, name);
  if (setting == NULL) {
    return false;
  }

  girara_setting_set_value(session, setting, value);
  return true;
}

bool girara_setting_get_value(girara_setting_t* setting, void* dest) {
  g_return_val_if_fail(setting != NULL && dest != NULL, false);

  switch (setting->type) {
  case BOOLEAN: {
    bool* bvalue = (bool*)dest;
    *bvalue      = setting->value.b;
    break;
  }
  case FLOAT: {
    float* fvalue = (float*)dest;
    *fvalue       = setting->value.f;
    break;
  }
  case INT: {
    int* ivalue = (int*)dest;
    *ivalue     = setting->value.i;
    break;
  }
  case STRING: {
    char** svalue = (char**)dest;
    *svalue       = setting->value.s ? g_strdup(setting->value.s) : NULL;
    break;
  }
  default:
    g_assert(false);
  }

  return true;
}

bool girara_setting_get(girara_session_t* session, const char* name, void* dest) {
  g_return_val_if_fail(session != NULL && name != NULL && dest != NULL, false);

  girara_setting_t* setting = girara_setting_find(session, name);
  if (setting == NULL) {
    return false;
  }

  return girara_setting_get_value(setting, dest);
}

void girara_setting_free(girara_setting_t* setting) {
  if (setting != NULL) {
    if (setting->type == STRING) {
      g_free(setting->value.s);
    }
    g_free(setting->description);
    g_free(setting->name);
    g_free(setting);
  }
}

girara_setting_t* girara_setting_find(girara_session_t* session, const char* name) {
  g_return_val_if_fail(session != NULL, NULL);
  g_return_val_if_fail(name != NULL, NULL);

  for (size_t idx = 0; idx != girara_list_size(session->private_data->settings); ++idx) {
    girara_setting_t* setting = girara_list_nth(session->private_data->settings, idx);
    if (g_strcmp0(setting->name, name) == 0) {
      return setting;
    }
  }

  return NULL;
}

const char* girara_setting_get_name(const girara_setting_t* setting) {
  g_return_val_if_fail(setting, NULL);
  return setting->name;
}

girara_setting_type_t girara_setting_get_type(girara_setting_t* setting) {
  g_return_val_if_fail(setting, UNKNOWN);
  return setting->type;
}

girara_completion_t* girara_cc_set(girara_session_t* session, const char* input) {
  if (input == NULL) {
    return NULL;
  }

  girara_completion_t* completion = girara_completion_init();
  if (completion == NULL) {
    return NULL;
  }
  girara_completion_group_t* group = girara_completion_group_create(session, NULL);
  if (group == NULL) {
    girara_completion_free(completion);
    return NULL;
  }
  girara_completion_add_group(completion, group);

  unsigned int input_length = strlen(input);

  for (size_t idx = 0; idx != girara_list_size(session->private_data->settings); ++idx) {
    girara_setting_t* setting = girara_list_nth(session->private_data->settings, idx);
    if ((setting->init_only == false) && (input_length <= strlen(setting->name)) &&
        !strncmp(input, setting->name, input_length)) {
      girara_completion_group_add_element(group, setting->name, setting->description);
    }
  }

  return completion;
}

static void dump_setting(JsonBuilder* builder, const girara_setting_t* setting) {
  json_builder_set_member_name(builder, setting->name);
  json_builder_begin_object(builder);
  json_builder_set_member_name(builder, "value");
  const char* type = NULL;
  switch (setting->type) {
  case BOOLEAN:
    json_builder_add_boolean_value(builder, setting->value.b);
    type = "boolean";
    break;
  case FLOAT:
    json_builder_add_double_value(builder, setting->value.f);
    type = "float";
    break;
  case INT:
    json_builder_add_int_value(builder, setting->value.i);
    type = "int";
    break;
  case STRING:
    json_builder_add_string_value(builder, setting->value.s ? setting->value.s : "");
    type = "string";
    break;
  default:
    girara_debug("Invalid setting: %s", setting->name);
  }

  if (type != NULL) {
    json_builder_set_member_name(builder, "type");
    json_builder_add_string_value(builder, type);
  }

  if (setting->description != NULL) {
    json_builder_set_member_name(builder, "description");
    json_builder_add_string_value(builder, setting->description);
  }
  json_builder_set_member_name(builder, "init-only");
  json_builder_add_boolean_value(builder, setting->init_only);
  json_builder_end_object(builder);
}

bool girara_cmd_dump_config(girara_session_t* session, girara_list_t* argument_list) {
  if (session == NULL || argument_list == NULL) {
    return false;
  }

  const size_t number_of_arguments = girara_list_size(argument_list);
  if (number_of_arguments != 1) {
    girara_warning("Invalid number of arguments passed: %zu instead of 1", number_of_arguments);
    girara_notify(session, GIRARA_ERROR, _("Invalid number of arguments passed: %zu instead of 1"),
                  number_of_arguments);
    return false;
  }

  JsonBuilder* builder = json_builder_new();
  json_builder_begin_object(builder);
  for (size_t idx = 0; idx != girara_list_size(session->private_data->settings); ++idx) {
    girara_setting_t* setting = girara_list_nth(session->private_data->settings, idx);
    dump_setting(builder, setting);
  }
  json_builder_end_object(builder);

  JsonGenerator* gen = json_generator_new();
  json_generator_set_pretty(gen, true);
  JsonNode* root = json_builder_get_root(builder);
  json_generator_set_root(gen, root);

  bool ret                = true;
  g_autoptr(GError) error = NULL;
  if (!json_generator_to_file(gen, girara_list_nth(argument_list, 0), &error)) {
    girara_warning("Failed to write JSON: %s", error->message);
    girara_notify(session, GIRARA_ERROR, _("Failed to write JSON: %s"), error->message);
    ret = false;
  }

  json_node_free(root);
  g_object_unref(gen);
  g_object_unref(builder);

  return ret;
}
