/* SPDX-License-Identifier: Zlib */

#include "dbus-interface.h"
#include "adjustment.h"
#include "config.h"
#include "document.h"
#include "links.h"
#include "macros.h"
#include "resources.h"
#include "synctex.h"
#include "utils.h"
#include "zathura.h"

#include <gio/gio.h>
#include <girara/commands.h>
#include <girara/session.h>
#include <girara/settings.h>
#include <girara/utils.h>
#include <json-glib/json-glib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static const char DBUS_XML_FILENAME[] = "/org/pwmt/zathura/DBus/org.pwmt.zathura.xml";

static GBytes* load_xml_data(void) {
  GResource* resource = zathura_resources_get_resource();
  if (resource != NULL) {
    return g_resource_lookup_data(resource, DBUS_XML_FILENAME, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  }

  return NULL;
}

typedef struct private_s {
  zathura_t* zathura;
  GDBusNodeInfo* introspection_data;
  GDBusConnection* connection;
  guint owner_id;
  guint registration_id;
  char* bus_name;
} ZathuraDbusPrivate;

G_DEFINE_TYPE_WITH_CODE(ZathuraDbus, zathura_dbus, G_TYPE_OBJECT, G_ADD_PRIVATE(ZathuraDbus))

/* template for bus name */
static const char DBUS_NAME_TEMPLATE[] = "org.pwmt.zathura.PID-%d";
/* object path */
static const char DBUS_OBJPATH[] = "/org/pwmt/zathura";
/* interface name */
static const char DBUS_INTERFACE[] = "org.pwmt.zathura";

static const GDBusInterfaceVTable interface_vtable;

static void finalize(GObject* object) {
  ZathuraDbus* dbus        = ZATHURA_DBUS(object);
  ZathuraDbusPrivate* priv = zathura_dbus_get_instance_private(dbus);

  if (priv->connection != NULL && priv->registration_id > 0) {
    g_dbus_connection_unregister_object(priv->connection, priv->registration_id);
  }

  if (priv->owner_id > 0) {
    g_bus_unown_name(priv->owner_id);
  }

  if (priv->introspection_data != NULL) {
    g_dbus_node_info_unref(priv->introspection_data);
  }

  g_free(priv->bus_name);

  G_OBJECT_CLASS(zathura_dbus_parent_class)->finalize(object);
}

static void zathura_dbus_class_init(ZathuraDbusClass* class) {
  /* overwrite methods */
  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->finalize     = finalize;
}

static void zathura_dbus_init(ZathuraDbus* dbus) {
  ZathuraDbusPrivate* priv = zathura_dbus_get_instance_private(dbus);
  priv->zathura            = NULL;
  priv->introspection_data = NULL;
  priv->connection         = NULL;
  priv->owner_id           = 0;
  priv->registration_id    = 0;
  priv->bus_name           = NULL;
}

static void gdbus_connection_closed(GDBusConnection* UNUSED(connection), gboolean UNUSED(remote_peer_vanished),
                                    GError* error, void* UNUSED(data)) {
  if (error != NULL) {
    girara_debug("D-Bus connection closed: %s", error->message);
  }
}

static void bus_acquired(GDBusConnection* connection, const gchar* name, void* data) {
  girara_debug("Bus acquired at '%s'.", name);

  /* register callback for GDBusConnection's closed signal */
  g_signal_connect(G_OBJECT(connection), "closed", G_CALLBACK(gdbus_connection_closed), NULL);

  ZathuraDbus* dbus        = data;
  ZathuraDbusPrivate* priv = zathura_dbus_get_instance_private(dbus);

  g_autoptr(GError) error = NULL;
  priv->registration_id   = g_dbus_connection_register_object(
      connection, DBUS_OBJPATH, priv->introspection_data->interfaces[0], &interface_vtable, dbus, NULL, &error);
  if (priv->registration_id == 0) {
    girara_warning("Failed to register object on D-Bus connection: %s", error->message);
    return;
  }

  priv->connection = connection;
}

static void name_acquired(GDBusConnection* UNUSED(connection), const gchar* name, void* UNUSED(data)) {
  girara_debug("Acquired '%s' on session bus.", name);
}

static void name_lost(GDBusConnection* UNUSED(connection), const gchar* name, void* UNUSED(data)) {
  girara_debug("Lost connection or failed to acquire '%s' on session bus.", name);
}

ZathuraDbus* zathura_dbus_new(zathura_t* zathura) {
  g_autoptr(GObject) obj = g_object_new(ZATHURA_TYPE_DBUS, NULL);
  if (obj == NULL) {
    return NULL;
  }

  ZathuraDbus* dbus        = ZATHURA_DBUS(obj);
  ZathuraDbusPrivate* priv = zathura_dbus_get_instance_private(dbus);
  priv->zathura            = zathura;

  g_autoptr(GBytes) xml_data = load_xml_data();
  if (xml_data == NULL) {
    girara_warning("Failed to load introspection data.");
    return NULL;
  }

  g_autoptr(GError) error  = NULL;
  priv->introspection_data = g_dbus_node_info_new_for_xml((const char*)g_bytes_get_data(xml_data, NULL), &error);

  if (priv->introspection_data == NULL) {
    girara_warning("Failed to parse introspection data: %s", error->message);
    return NULL;
  }

  char* well_known_name = g_strdup_printf(DBUS_NAME_TEMPLATE, getpid());
  priv->bus_name        = well_known_name;
  priv->owner_id        = g_bus_own_name(G_BUS_TYPE_SESSION, well_known_name, G_BUS_NAME_OWNER_FLAGS_NONE, bus_acquired,
                                         name_acquired, name_lost, dbus, NULL);

  // dbus takes ownership of obj
  obj = NULL;
  return dbus;
}

const char* zathura_dbus_get_name(zathura_t* zathura) {
  ZathuraDbusPrivate* priv = zathura_dbus_get_instance_private(zathura->dbus);

  return priv->bus_name;
}

void zathura_dbus_edit(zathura_t* zathura, unsigned int page, unsigned int x, unsigned int y) {
  ZathuraDbus* edit        = zathura->dbus;
  ZathuraDbusPrivate* priv = zathura_dbus_get_instance_private(edit);

  const char* filename = zathura_document_get_path(zathura_get_document(priv->zathura));

  g_autofree char* input_file = NULL;
  unsigned int line           = 0;
  unsigned int column         = 0;

  if (synctex_get_input_line_column(zathura, filename, page, x, y, &input_file, &line, &column) == false) {
    return;
  }

  g_autoptr(GError) error = NULL;
  g_dbus_connection_emit_signal(priv->connection, NULL, DBUS_OBJPATH, DBUS_INTERFACE, "Edit",
                                g_variant_new("(suu)", input_file, line, column), &error);

  if (error != NULL) {
    girara_debug("Failed to emit 'Edit' signal: %s", error->message);
  }
}

/* D-Bus handler */

static void handle_open_document(zathura_t* zathura, GVariant* parameters, GDBusMethodInvocation* invocation) {
  g_autofree gchar* filename = NULL;
  g_autofree gchar* password = NULL;
  gint page                  = ZATHURA_PAGE_NUMBER_UNSPECIFIED;
  g_variant_get(parameters, "(ssi)", &filename, &password, &page);

  document_close(zathura, false);
  document_open_idle(zathura, filename, strlen(password) > 0 ? password : NULL, page, NULL, NULL, NULL, NULL);

  GVariant* result = g_variant_new("(b)", true);
  g_dbus_method_invocation_return_value(invocation, result);
}

static void handle_close_document(zathura_t* zathura, GVariant* UNUSED(parameters), GDBusMethodInvocation* invocation) {
  const bool ret = document_close(zathura, false);

  GVariant* result = g_variant_new("(b)", ret);
  g_dbus_method_invocation_return_value(invocation, result);
}

static void handle_goto_page(zathura_t* zathura, GVariant* parameters, GDBusMethodInvocation* invocation) {
  const unsigned int number_of_pages = zathura_document_get_number_of_pages(zathura_get_document(zathura));

  guint page = 0;
  g_variant_get(parameters, "(u)", &page);

  bool ret = true;
  if (page >= number_of_pages) {
    ret = false;
  } else {
    page_set(zathura, page);
  }

  GVariant* result = g_variant_new("(b)", ret);
  g_dbus_method_invocation_return_value(invocation, result);
}

typedef struct {
  zathura_t* zathura;
  girara_list_t** rectangles;
  unsigned int page;
  unsigned int number_of_pages;
} highlights_rect_data_t;

static gboolean synctex_highlight_rects_impl(gpointer ptr) {
  highlights_rect_data_t* data = ptr;

  synctex_highlight_rects(data->zathura, data->page, data->rectangles);

  for (unsigned int i = 0; i != data->number_of_pages; ++i) {
    girara_list_free(data->rectangles[i]);
  }
  g_free(data->rectangles);
  g_free(data);
  return false;
}

static void synctex_highlight_rects_idle(zathura_t* zathura, girara_list_t** rectangles, unsigned int page,
                                         unsigned number_of_pages) {
  highlights_rect_data_t* data = g_try_malloc0(sizeof(highlights_rect_data_t));
  data->zathura                = zathura;
  data->rectangles             = rectangles;
  data->page                   = page;
  data->number_of_pages        = number_of_pages;

  gdk_threads_add_idle(synctex_highlight_rects_impl, data);
}

static void handle_highlight_rects(zathura_t* zathura, GVariant* parameters, GDBusMethodInvocation* invocation) {
  const unsigned int number_of_pages = zathura_document_get_number_of_pages(zathura_get_document(zathura));

  guint page                             = 0;
  g_autoptr(GVariantIter) iter           = NULL;
  g_autoptr(GVariantIter) secondary_iter = NULL;
  g_variant_get(parameters, "(ua(dddd)a(udddd))", &page, &iter, &secondary_iter);

  if (page >= number_of_pages) {
    girara_debug("Got invalid page number.");
    GVariant* result = g_variant_new("(b)", false);
    g_dbus_method_invocation_return_value(invocation, result);
    return;
  }

  /* get rectangles */
  girara_list_t** rectangles = g_try_malloc0(number_of_pages * sizeof(girara_list_t*));
  if (rectangles == NULL) {
    g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, G_DBUS_ERROR_NO_MEMORY,
                                          "Failed to allocate memory.");
    return;
  }

  rectangles[page] = girara_list_new_with_free(g_free);
  if (rectangles[page] == NULL) {
    g_free(rectangles);
    g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, G_DBUS_ERROR_NO_MEMORY,
                                          "Failed to allocate memory.");
    return;
  }

  zathura_rectangle_t temp_rect = {0, 0, 0, 0};
  while (g_variant_iter_loop(iter, "(dddd)", &temp_rect.x1, &temp_rect.x2, &temp_rect.y1, &temp_rect.y2)) {
    zathura_rectangle_t* rect = g_try_malloc0(sizeof(zathura_rectangle_t));
    if (rect == NULL) {
      girara_list_free(rectangles[page]);
      g_free(rectangles);
      g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, G_DBUS_ERROR_NO_MEMORY,
                                            "Failed to allocate memory.");
      return;
    }

    *rect = temp_rect;
    girara_list_append(rectangles[page], rect);
  }

  /* get secondary rectangles */
  guint temp_page = 0;
  while (g_variant_iter_loop(secondary_iter, "(udddd)", &temp_page, &temp_rect.x1, &temp_rect.x2, &temp_rect.y1,
                             &temp_rect.y2)) {
    if (temp_page >= number_of_pages) {
      /* error out here? */
      girara_debug("Got invalid page number.");
      continue;
    }

    if (rectangles[temp_page] == NULL) {
      rectangles[temp_page] = girara_list_new_with_free(g_free);
    }

    zathura_rectangle_t* rect = g_try_malloc0(sizeof(zathura_rectangle_t));
    if (rect == NULL || rectangles[temp_page] == NULL) {
      for (unsigned int p = 0; p != number_of_pages; ++p) {
        girara_list_free(rectangles[p]);
      }
      g_free(rectangles);
      g_free(rect);
      g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, G_DBUS_ERROR_NO_MEMORY,
                                            "Failed to allocate memory.");
      return;
    }

    *rect = temp_rect;
    girara_list_append(rectangles[temp_page], rect);
  }

  /* run synctex_highlight_rects in main thread when idle */
  synctex_highlight_rects_idle(zathura, rectangles, page, number_of_pages);

  GVariant* result = g_variant_new("(b)", true);
  g_dbus_method_invocation_return_value(invocation, result);
}

typedef struct {
  zathura_t* zathura;
  gchar* input_file;
  unsigned int line;
  unsigned int column;
} view_data_t;

static gboolean synctex_view_impl(gpointer ptr) {
  view_data_t* data = ptr;

  synctex_view(data->zathura, data->input_file, data->line, data->column);

  g_free(data->input_file);
  g_free(data);
  return false;
}

static void synctex_view_idle(zathura_t* zathura, gchar* input_file, unsigned int line, unsigned int column) {
  view_data_t* data = g_try_malloc0(sizeof(view_data_t));
  data->zathura     = zathura;
  data->input_file  = input_file;
  data->line        = line;
  data->column      = column;

  gdk_threads_add_idle(synctex_view_impl, data);
}

static void handle_synctex_view(zathura_t* zathura, GVariant* parameters, GDBusMethodInvocation* invocation) {
  gchar* input_file = NULL;
  guint line        = 0;
  guint column      = 0;
  g_variant_get(parameters, "(suu)", &input_file, &line, &column);

  synctex_view_idle(zathura, input_file, line, column);

  GVariant* result = g_variant_new("(b)", true);
  g_dbus_method_invocation_return_value(invocation, result);
}

static void handle_execute_command(zathura_t* zathura, GVariant* parameters, GDBusMethodInvocation* invocation) {
  g_autofree gchar* input = NULL;
  g_variant_get(parameters, "(s)", &input);

  const bool ret   = girara_command_run(zathura->ui.session, input);
  GVariant* result = g_variant_new("(b)", ret);
  g_dbus_method_invocation_return_value(invocation, result);
}

static void handle_source_config(zathura_t* zathura, GVariant* GIRARA_UNUSED(parameters),
                                 GDBusMethodInvocation* invocation) {
  config_load_files(zathura);

  GVariant* result = g_variant_new("(b)", true);
  g_dbus_method_invocation_return_value(invocation, result);
}

static void handle_source_config_from_dir(zathura_t* zathura, GVariant* parameters, GDBusMethodInvocation* invocation) {
  g_autofree gchar* input = NULL;
  g_variant_get(parameters, "(s)", &input);

  zathura_set_config_dir(zathura, input);
  config_load_files(zathura);

  GVariant* result = g_variant_new("(b)", true);
  g_dbus_method_invocation_return_value(invocation, result);
}

static void handle_method_call(GDBusConnection* UNUSED(connection), const gchar* UNUSED(sender),
                               const gchar* object_path, const gchar* interface_name, const gchar* method_name,
                               GVariant* parameters, GDBusMethodInvocation* invocation, void* data) {
  ZathuraDbus* dbus        = data;
  ZathuraDbusPrivate* priv = zathura_dbus_get_instance_private(dbus);

  girara_debug("Handling call '%s.%s' on '%s'.", interface_name, method_name, object_path);

  static const struct {
    const char* method;
    void (*handler)(zathura_t*, GVariant*, GDBusMethodInvocation*);
    bool needs_document;
    bool present_window;
  } handlers[] = {
      {"OpenDocument", handle_open_document, false, true},
      {"CloseDocument", handle_close_document, false, false},
      {"GotoPage", handle_goto_page, true, true},
      {"HighlightRects", handle_highlight_rects, true, true},
      {"SynctexView", handle_synctex_view, true, true},
      {"ExecuteCommand", handle_execute_command, false, false},
      {"SourceConfig", handle_source_config, false, false},
      {"SourceConfigFromDirectory", handle_source_config_from_dir, false, false},
  };

  for (size_t idx = 0; idx != sizeof(handlers) / sizeof(handlers[0]); ++idx) {
    if (g_strcmp0(method_name, handlers[idx].method) != 0) {
      continue;
    }

    if (handlers[idx].needs_document == true && zathura_has_document(priv->zathura) == false) {
      g_dbus_method_invocation_return_dbus_error(invocation, "org.pwmt.zathura.NoOpenDocument",
                                                 "No document has been opened.");
      return;
    }

    (*handlers[idx].handler)(priv->zathura, parameters, invocation);

    if (handlers[idx].present_window == true && priv->zathura->ui.session->gtk.embed == 0) {
      bool present_window = true;
      girara_setting_get(priv->zathura->ui.session, "dbus-raise-window", &present_window);
      if (present_window == true) {
        gtk_window_present(GTK_WINDOW(priv->zathura->ui.session->gtk.window));
      }
    }

    return;
  }
}

static void json_document_info_add_node(JsonBuilder* builder, girara_tree_node_t* index) {
  girara_list_t* list = girara_node_get_children(index);
  for (size_t idx = 0; idx != girara_list_size(list); ++idx) {
    girara_tree_node_t* node               = girara_list_nth(list, idx);
    zathura_index_element_t* index_element = girara_node_get_data(node);

    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "title");
    json_builder_add_string_value(builder, index_element->title);

    zathura_link_type_t type     = zathura_link_get_type(index_element->link);
    zathura_link_target_t target = zathura_link_get_target(index_element->link);
    if (type == ZATHURA_LINK_GOTO_DEST) {
      json_builder_set_member_name(builder, "page");
      json_builder_add_int_value(builder, target.page_number + 1);
    } else {
      json_builder_set_member_name(builder, "target");
      json_builder_add_string_value(builder, target.value);
    }

    if (girara_node_get_num_children(node) > 0) {
      json_builder_set_member_name(builder, "sub-index");
      json_builder_begin_array(builder);
      json_document_info_add_node(builder, node);
      json_builder_end_array(builder);
    }
    json_builder_end_object(builder);
  }
}

static GVariant* json_document_info(zathura_t* zathura) {
  zathura_document_t* document = zathura_get_document(zathura);

  g_autoptr(JsonBuilder) builder = json_builder_new();
  json_builder_begin_object(builder);
  json_builder_set_member_name(builder, "filename");
  json_builder_add_string_value(builder, zathura_document_get_path(document));
  json_builder_set_member_name(builder, "number-of-pages");
  json_builder_add_int_value(builder, zathura_document_get_number_of_pages(document));

  json_builder_set_member_name(builder, "index");
  json_builder_begin_array(builder);
  g_autoptr(girara_tree_node_t) index = zathura_document_index_generate(document, NULL);
  if (index != NULL) {
    json_document_info_add_node(builder, index);
  }
  json_builder_end_array(builder);

  json_builder_end_object(builder);

  g_autoptr(JsonNode) root        = json_builder_get_root(builder);
  char* serialized_root = json_to_string(root, true);

  return g_variant_new_take_string(serialized_root);
}

static GVariant* handle_get_property(GDBusConnection* UNUSED(connection), const gchar* UNUSED(sender),
                                     const gchar* UNUSED(object_path), const gchar* UNUSED(interface_name),
                                     const gchar* property_name, GError** error, void* data) {
  ZathuraDbus* dbus            = data;
  ZathuraDbusPrivate* priv     = zathura_dbus_get_instance_private(dbus);
  zathura_document_t* document = zathura_get_document(priv->zathura);

  if (document == NULL) {
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "No document open.");
    return NULL;
  }

  if (g_strcmp0(property_name, "filename") == 0) {
    return g_variant_new_string(zathura_document_get_path(document));
  } else if (g_strcmp0(property_name, "pagenumber") == 0) {
    return g_variant_new_uint32(zathura_document_get_current_page_number(document));
  } else if (g_strcmp0(property_name, "numberofpages") == 0) {
    return g_variant_new_uint32(zathura_document_get_number_of_pages(document));
  } else if (g_strcmp0(property_name, "documentinfo") == 0) {
    return json_document_info(priv->zathura);
  }

  return NULL;
}

static const GDBusInterfaceVTable interface_vtable = {
    .method_call  = handle_method_call,
    .get_property = handle_get_property,
    .set_property = NULL,
};

static const unsigned int TIMEOUT = 3000;

static bool call_synctex_view(GDBusConnection* connection, const char* filename, const char* name,
                              const char* input_file, unsigned int line, unsigned int column) {
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) vfilename =
      g_dbus_connection_call_sync(connection, name, DBUS_OBJPATH, "org.freedesktop.DBus.Properties", "Get",
                                  g_variant_new("(ss)", DBUS_INTERFACE, "filename"), G_VARIANT_TYPE("(v)"),
                                  G_DBUS_CALL_FLAGS_NONE, TIMEOUT, NULL, &error);
  if (vfilename == NULL) {
    girara_error("Failed to query 'filename' property from '%s': %s", name, error->message);
    return false;
  }

  g_autoptr(GVariant) tmp = NULL;
  g_variant_get(vfilename, "(v)", &tmp);
  g_autofree gchar* remote_filename = g_variant_dup_string(tmp, NULL);
  girara_debug("Filename from '%s': %s", name, remote_filename);

  if (g_strcmp0(filename, remote_filename) != 0) {
    return false;
  }

  g_autoptr(GVariant) ret = g_dbus_connection_call_sync(
      connection, name, DBUS_OBJPATH, DBUS_INTERFACE, "SynctexView", g_variant_new("(suu)", input_file, line, column),
      G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, TIMEOUT, NULL, &error);
  if (ret == NULL) {
    girara_error("Failed to run SynctexView on '%s': %s", name, error->message);
    return false;
  }
  return true;
}

static int iterate_instances_call_synctex_view(const char* filename, const char* input_file, unsigned int line,
                                               unsigned int column, pid_t hint) {
  if (filename == NULL) {
    return -1;
  }

  g_autoptr(GError) error               = NULL;
  g_autoptr(GDBusConnection) connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
  if (connection == NULL) {
    girara_error("Could not connect to session bus: %s", error->message);
    return -1;
  }

  if (hint != -1) {
    g_autofree char* well_known_name = g_strdup_printf(DBUS_NAME_TEMPLATE, hint);
    const bool ret = call_synctex_view(connection, filename, well_known_name, input_file, line, column);
    return ret ? 1 : -1;
  }

  g_autoptr(GVariant) vnames = g_dbus_connection_call_sync(
      connection, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames", NULL,
      G_VARIANT_TYPE("(as)"), G_DBUS_CALL_FLAGS_NONE, TIMEOUT, NULL, &error);
  if (vnames == NULL) {
    girara_error("Could not list available names: %s", error->message);
    return -1;
  }

  g_autoptr(GVariantIter) iter = NULL;
  g_variant_get(vnames, "(as)", &iter);

  gchar* name    = NULL;
  bool found_one = false;
  while (found_one == false && g_variant_iter_loop(iter, "s", &name) == TRUE) {
    if (g_str_has_prefix(name, "org.pwmt.zathura.PID") == FALSE) {
      continue;
    }
    girara_debug("Found name: %s", name);

    found_one = call_synctex_view(connection, filename, name, input_file, line, column);
  }

  return found_one ? 1 : 0;
}

int zathura_dbus_synctex_position(const char* filename, const char* input_file, int line, int column, pid_t hint) {
  if (filename == NULL || input_file == NULL || line < 0 || column < 0) {
    return -1;
  }

  return iterate_instances_call_synctex_view(filename, input_file, line, column, hint);
}
