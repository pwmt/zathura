/* SPDX-License-Identifier: Zlib */

#include "session.h"

#include "callbacks.h"
#include "commands.h"
#include "config.h"
#include "resources.h"
#include "internal.h"
#include "settings.h"
#include "shortcuts.h"

#include <girara/datastructures.h>
#include <girara/input-history.h>
#include <girara/template.h>
#include <girara/utils.h>

#include <glib/gi18n-lib.h>
#include <pango/pango-font.h>
#include <stdlib.h>

static int cb_sort_settings(const void* data1, const void* data2) {
  const girara_setting_t* lhs = data1;
  const girara_setting_t* rhs = data2;

  return g_strcmp0(girara_setting_get_name(lhs), girara_setting_get_name(rhs));
}

static void init_template_engine(GiraraTemplate* csstemplate) {
  static const char variable_names[][24] = {
      "session",
      "default-fg",
      "default-bg",
      "inputbar-fg",
      "inputbar-bg",
      "statusbar-fg",
      "statusbar-bg",
      "completion-fg",
      "completion-bg",
      "completion-group-fg",
      "completion-group-bg",
      "completion-highlight-fg",
      "completion-highlight-bg",
      "notification-error-fg",
      "notification-error-bg",
      "notification-warning-fg",
      "notification-warning-bg",
      "notification-fg",
      "notification-bg",
      "bottombox-padding1",
      "bottombox-padding2",
      "bottombox-padding3",
      "bottombox-padding4",
      "font-family",
      "font-size",
      "font-weight",
      "font-style",
  };

  for (size_t idx = 0; idx < LENGTH(variable_names); ++idx) {
    girara_template_add_variable(csstemplate, variable_names[idx]);
  }
}

void css_template_fill_font(GiraraTemplate* csstemplate, const char* font) {
  PangoFontDescription* descr = pango_font_description_from_string(font);
  if (descr == NULL) {
    return;
  }

  girara_template_set_variable_value(csstemplate, "font-family", pango_font_description_get_family(descr));

  const int font_size = pango_font_description_get_size(descr);
  if (!font_size) {
    girara_debug("no font size given, defaulting to 9");
    girara_template_set_variable_value(csstemplate, "font-size", "9pt");
  } else {
    g_autofree char* size = g_strdup_printf("%d%s", pango_font_description_get_size(descr) / PANGO_SCALE,
                                            pango_font_description_get_size_is_absolute(descr) == FALSE ? "pt" : "");
    girara_template_set_variable_value(csstemplate, "font-size", size);
  }

  const unsigned int font_weight   = pango_font_description_get_weight(descr);
  g_autofree char* font_weight_str = g_strdup_printf("%u", font_weight);
  girara_template_set_variable_value(csstemplate, "font-weight", font_weight_str);

  static const char STYLE_TO_STR[3][8] = {
      [PANGO_STYLE_NORMAL]  = "normal",
      [PANGO_STYLE_ITALIC]  = "italic",
      [PANGO_STYLE_OBLIQUE] = "oblique",
  };
  const PangoStyle style = pango_font_description_get_style(descr);
  girara_template_set_variable_value(csstemplate, "font-style", STYLE_TO_STR[style]);

  pango_font_description_free(descr);
}

static void fill_template_with_values(girara_session_t* session) {
  GiraraTemplate* csstemplate = session->private_data->csstemplate;

  girara_template_set_variable_value(csstemplate, "session", session->private_data->session_name);

  g_autofree char* font = NULL;
  girara_setting_get(session, "font", &font);
  if (font != NULL) {
    css_template_fill_font(csstemplate, font);
  } else {
    girara_template_set_variable_value(csstemplate, "font-family", "monospace");
    girara_template_set_variable_value(csstemplate, "font-size", "9pt");
    girara_template_set_variable_value(csstemplate, "font-weight", "normal");
    girara_template_set_variable_value(csstemplate, "font-style", "normal");
  };

  /* parse color values */
  static const char* const color_settings[] = {
      "default-fg",
      "default-bg",
      "inputbar-fg",
      "inputbar-bg",
      "statusbar-fg",
      "statusbar-bg",
      "completion-fg",
      "completion-bg",
      "completion-group-fg",
      "completion-group-bg",
      "completion-highlight-fg",
      "completion-highlight-bg",
      "notification-error-fg",
      "notification-error-bg",
      "notification-warning-fg",
      "notification-warning-bg",
      "notification-fg",
      "notification-bg",
  };

  for (size_t i = 0; i < LENGTH(color_settings); ++i) {
    g_autofree char* tmp_value = NULL;
    girara_setting_get(session, color_settings[i], &tmp_value);

    GdkRGBA color = {0, 0, 0, 0};
    if (tmp_value != NULL) {
      gdk_rgba_parse(&color, tmp_value);
    }

    g_autofree char* colorstr = gdk_rgba_to_string(&color);
    girara_template_set_variable_value(csstemplate, color_settings[i], colorstr);
  }

  /* we want inputbar_entry the same height as notification_text and statusbar,
    so that when inputbar_entry is hidden, the size of the bottom_box remains
    the same. We need to get rid of the builtin padding in the GtkEntry
    widget. */

  int ypadding = 2; /* total amount of padding (top + bottom) */
  int xpadding = 8; /* total amount of padding (left + right) */
  girara_setting_get(session, "statusbar-h-padding", &xpadding);
  girara_setting_get(session, "statusbar-v-padding", &ypadding);

  typedef struct padding_mapping_s {
    const char* identifier;
    char* value;
  } padding_mapping_t;

  const padding_mapping_t padding_mapping[] = {
      {"bottombox-padding1", g_strdup_printf("%d", ypadding - ypadding / 2)},
      {"bottombox-padding2", g_strdup_printf("%d", xpadding / 2)},
      {"bottombox-padding3", g_strdup_printf("%d", ypadding / 2)},
      {"bottombox-padding4", g_strdup_printf("%d", xpadding - xpadding / 2)},
  };

  for (size_t i = 0; i < LENGTH(padding_mapping); ++i) {
    girara_template_set_variable_value(csstemplate, padding_mapping[i].identifier, padding_mapping[i].value);
    g_free(padding_mapping[i].value);
  }
}

static void css_template_changed(GiraraTemplate* csstemplate, girara_session_t* session) {
  GtkCssProvider* provider  = session->private_data->gtk.cssprovider;
  g_autofree char* css_data = girara_template_evaluate(csstemplate);
  if (css_data == NULL) {
    girara_error("Error while evaluating templates.");
    return;
  }

  if (provider == NULL) {
    provider = session->private_data->gtk.cssprovider = gtk_css_provider_new();

    /* add CSS style provider */
    GdkDisplay* display = gdk_display_get_default();
    GdkScreen* screen   = gdk_display_get_default_screen(display);
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  }

  g_autoptr(GError) error = NULL;
  if (gtk_css_provider_load_from_data(provider, css_data, -1, &error) == FALSE) {
    girara_error("Unable to load CSS: %s", error->message);
  }
}

static void mode_string_free(void* data) {
  if (data != NULL) {
    girara_mode_string_t* mode = data;

    g_free(mode->name);
    g_free(mode);
  }
}

girara_session_t* girara_session_create(void) {
  girara_session_t* session = g_malloc0(sizeof(girara_session_t));
  session->private_data     = g_malloc0(sizeof(girara_session_private_t));

  girara_session_private_t* session_private = session->private_data;

  /* init values */
  session->bindings.mouse_events            = girara_list_new_with_free(g_free);
  session->bindings.commands                = girara_list_new_with_free((girara_free_function_t)girara_command_free);
  session->bindings.special_commands        = girara_list_new_with_free(g_free);
  session->bindings.shortcuts               = girara_list_new_with_free((girara_free_function_t)girara_shortcut_free);
  session->bindings.inputbar_shortcuts      = girara_list_new_with_free(g_free);
  session_private->elements.statusbar_items = girara_list_new_with_free(g_free);

  g_mutex_init(&session_private->feedkeys_mutex);

  /* settings */
  session_private->settings =
      girara_sorted_list_new_with_free(cb_sort_settings, (girara_free_function_t)girara_setting_free);

  /* CSS style provider */
  GResource* css_resource = zathura_resources_get_resource();
  GBytes* css_data =
      g_resource_lookup_data(css_resource, "/org/pwmt/girara/CSS/girara.css_t", G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  if (css_data != NULL) {
    session_private->csstemplate = girara_template_new(g_bytes_get_data(css_data, NULL));
    g_bytes_unref(css_data);
  }

  session_private->gtk.cssprovider = NULL;
  init_template_engine(session_private->csstemplate);

  /* init modes */
  session->modes.identifiers  = girara_list_new_with_free(mode_string_free);
  girara_mode_t normal_mode   = girara_mode_add(session, "normal");
  girara_mode_t inputbar_mode = girara_mode_add(session, "inputbar");
  session->modes.normal       = normal_mode;
  session->modes.current_mode = normal_mode;
  session->modes.inputbar     = inputbar_mode;

  /* config handles */
  session_private->config.handles = girara_list_new_with_free(girara_config_handle_free);
  session_private->config.shortcut_mappings =
      girara_list_new_with_free((girara_free_function_t)girara_shortcut_mapping_free);
  session_private->config.argument_mappings =
      girara_list_new_with_free((girara_free_function_t)girara_argument_mapping_free);

  girara_config_handle_add(session, "map", girara_cmd_map);
  girara_config_handle_add(session, "set", girara_cmd_set);
  girara_config_handle_add(session, "unmap", girara_cmd_unmap);

  /* completion state */
  session_private->completion.command_mode = true;

  /* command history */
  session->command_history = girara_input_history_new(NULL);

  /* other values */
  session->global.autohide_inputbar = true;

  /* create widgets */
  session->gtk.box                = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
  session_private->gtk.overlay    = gtk_overlay_new();
  session_private->gtk.bottom_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
  session->gtk.statusbar_entries  = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
  session->gtk.inputbar_box       = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
  gtk_box_set_homogeneous(session->gtk.inputbar_box, TRUE);
  session->gtk.view = gtk_stack_new();
  gtk_widget_add_events(session->gtk.view, GDK_SCROLL_MASK);
  gtk_widget_set_can_focus(session->gtk.view, true);
  session->gtk.statusbar         = gtk_event_box_new();
  session->gtk.notification_area = gtk_event_box_new();
  session->gtk.notification_text = gtk_label_new(NULL);
  session->gtk.inputbar_dialog   = GTK_LABEL(gtk_label_new(NULL));
  session->gtk.inputbar_entry    = GTK_ENTRY(gtk_entry_new());
  session->gtk.inputbar          = gtk_event_box_new();

  /* make notification text selectable */
  gtk_label_set_selectable(GTK_LABEL(session->gtk.notification_text), TRUE);
  /* ellipsize notification text */
  gtk_label_set_ellipsize(GTK_LABEL(session->gtk.notification_text), PANGO_ELLIPSIZE_END);

  return session;
}

static void screen_changed(GtkWidget* widget, GdkScreen* GIRARA_UNUSED(old_screen), gpointer GIRARA_UNUSED(userdata)) {
  GdkScreen* screen = gtk_widget_get_screen(widget);
  GdkVisual* visual = gdk_screen_get_rgba_visual(screen);
  if (!visual) {
    visual = gdk_screen_get_system_visual(screen);
  }
  gtk_widget_set_visual(widget, visual);
}

bool girara_session_init(girara_session_t* session, const char* sessionname) {
  if (session == NULL) {
    return false;
  }

  /* set session name */
  session->private_data->session_name = g_strdup((sessionname == NULL) ? "girara" : sessionname);

  /* enable smooth scroll events */
  gtk_widget_add_events(session->gtk.view, GDK_SMOOTH_SCROLL_MASK);

  /* load CSS style */
  fill_template_with_values(session);
  g_signal_connect(G_OBJECT(session->private_data->csstemplate), "changed", G_CALLBACK(css_template_changed), session);

  /* window */
#ifdef GDK_WINDOWING_X11
  if (session->gtk.embed != 0) {
    session->gtk.window = gtk_plug_new(session->gtk.embed);
  } else {
#endif
    session->gtk.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
#ifdef GDK_WINDOWING_X11
  }
#endif

  gtk_widget_set_name(session->gtk.window, session->private_data->session_name);

  g_signal_connect(G_OBJECT(session->gtk.window), "screen-changed", G_CALLBACK(screen_changed), NULL);
  screen_changed(GTK_WIDGET(session->gtk.window), NULL, NULL);

  /* apply CSS style */
  css_template_changed(session->private_data->csstemplate, session);

  GdkGeometry hints = {
      .base_height = 1,
      .base_width  = 1,
      .height_inc  = 0,
      .max_aspect  = 0,
      .max_height  = 0,
      .max_width   = 0,
      .min_aspect  = 0,
      .min_height  = 0,
      .min_width   = 0,
      .width_inc   = 0,
  };

  gtk_window_set_geometry_hints(GTK_WINDOW(session->gtk.window), NULL, &hints, GDK_HINT_MIN_SIZE);

  /* view */
  session->signals.view_key_pressed = g_signal_connect(G_OBJECT(session->gtk.view), "key-press-event",
                                                       G_CALLBACK(girara_callback_view_key_press_event), session);

  session->signals.view_button_press_event = g_signal_connect(
      G_OBJECT(session->gtk.view), "button-press-event", G_CALLBACK(girara_callback_view_button_press_event), session);

  session->signals.view_button_release_event =
      g_signal_connect(G_OBJECT(session->gtk.view), "button-release-event",
                       G_CALLBACK(girara_callback_view_button_release_event), session);

  session->signals.view_motion_notify_event =
      g_signal_connect(G_OBJECT(session->gtk.view), "motion-notify-event",
                       G_CALLBACK(girara_callback_view_button_motion_notify_event), session);

  session->signals.view_scroll_event = g_signal_connect(G_OBJECT(session->gtk.view), "scroll-event",
                                                        G_CALLBACK(girara_callback_view_scroll_event), session);

  /* statusbar */
  gtk_container_add(GTK_CONTAINER(session->gtk.statusbar), GTK_WIDGET(session->gtk.statusbar_entries));

  /* notification area */
  gtk_container_add(GTK_CONTAINER(session->gtk.notification_area), session->gtk.notification_text);
  gtk_widget_set_halign(session->gtk.notification_text, GTK_ALIGN_START);
  gtk_widget_set_valign(session->gtk.notification_text, GTK_ALIGN_CENTER);
  gtk_label_set_use_markup(GTK_LABEL(session->gtk.notification_text), TRUE);

  /* inputbar */
  gtk_entry_set_has_frame(session->gtk.inputbar_entry, FALSE);
  gtk_editable_set_editable(GTK_EDITABLE(session->gtk.inputbar_entry), TRUE);

  widget_add_class(GTK_WIDGET(session->gtk.inputbar_entry), "bottom_box");
  widget_add_class(session->gtk.notification_text, "bottom_box");
  widget_add_class(GTK_WIDGET(session->gtk.statusbar_entries), "bottom_box");

  session->signals.inputbar_key_pressed =
      g_signal_connect(G_OBJECT(session->gtk.inputbar_entry), "key-press-event",
                       G_CALLBACK(girara_callback_inputbar_key_press_event), session);

  session->signals.inputbar_changed = g_signal_connect(G_OBJECT(session->gtk.inputbar_entry), "changed",
                                                       G_CALLBACK(girara_callback_inputbar_changed_event), session);

  session->signals.inputbar_activate = g_signal_connect(G_OBJECT(session->gtk.inputbar_entry), "activate",
                                                        G_CALLBACK(girara_callback_inputbar_activate), session);

  gtk_box_set_homogeneous(session->gtk.inputbar_box, FALSE);
  gtk_box_set_spacing(session->gtk.inputbar_box, 5);

  /* inputbar box */
  gtk_box_pack_start(GTK_BOX(session->gtk.inputbar_box), GTK_WIDGET(session->gtk.inputbar_dialog), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(session->gtk.inputbar_box), GTK_WIDGET(session->gtk.inputbar_entry), TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(session->gtk.inputbar), GTK_WIDGET(session->gtk.inputbar_box));

  /* bottom box */
  gtk_box_set_spacing(session->private_data->gtk.bottom_box, 0);

  gtk_box_pack_end(GTK_BOX(session->private_data->gtk.bottom_box), GTK_WIDGET(session->gtk.inputbar), TRUE, TRUE, 0);
  gtk_box_pack_end(GTK_BOX(session->private_data->gtk.bottom_box), GTK_WIDGET(session->gtk.notification_area), TRUE,
                   TRUE, 0);
  gtk_box_pack_end(GTK_BOX(session->private_data->gtk.bottom_box), GTK_WIDGET(session->gtk.statusbar), TRUE, TRUE, 0);

  /* packing */
  gtk_box_set_spacing(session->gtk.box, 0);
  gtk_box_pack_start(session->gtk.box, GTK_WIDGET(session->gtk.view), TRUE, TRUE, 0);

  /* box */
  gtk_container_add(GTK_CONTAINER(session->private_data->gtk.overlay), GTK_WIDGET(session->gtk.box));
  /* bottom_box */
  g_object_set(session->private_data->gtk.bottom_box, "halign", GTK_ALIGN_FILL, NULL);
  g_object_set(session->private_data->gtk.bottom_box, "valign", GTK_ALIGN_END, NULL);

  gtk_container_add(GTK_CONTAINER(session->gtk.box), GTK_WIDGET(session->private_data->gtk.bottom_box));

  gtk_container_add(GTK_CONTAINER(session->gtk.window), GTK_WIDGET(session->private_data->gtk.overlay));

  /* statusbar */
  widget_add_class(GTK_WIDGET(session->gtk.statusbar), "statusbar");

  /* inputbar */
  widget_add_class(GTK_WIDGET(session->gtk.inputbar_box), "inputbar");
  widget_add_class(GTK_WIDGET(session->gtk.inputbar_entry), "inputbar");
  widget_add_class(GTK_WIDGET(session->gtk.inputbar), "inputbar");
  widget_add_class(GTK_WIDGET(session->gtk.inputbar_dialog), "inputbar");

  /* notification area */
  widget_add_class(session->gtk.notification_area, "notification");
  widget_add_class(session->gtk.notification_text, "notification");

  /* set window size */
  int window_width  = 0;
  int window_height = 0;
  girara_setting_get(session, "window-width", &window_width);
  girara_setting_get(session, "window-height", &window_height);

  if (window_width > 0 && window_height > 0) {
    gtk_window_set_default_size(GTK_WINDOW(session->gtk.window), window_width, window_height);
  }

  gtk_widget_show_all(GTK_WIDGET(session->gtk.window));
  gtk_widget_hide(GTK_WIDGET(session->gtk.notification_area));
  gtk_widget_hide(GTK_WIDGET(session->gtk.inputbar_dialog));

  if (session->global.autohide_inputbar == true) {
    gtk_widget_hide(GTK_WIDGET(session->gtk.inputbar));
  }

  if (session->global.hide_statusbar == true) {
    gtk_widget_hide(GTK_WIDGET(session->gtk.statusbar));
  }

  g_autofree char* window_icon = NULL;
  girara_setting_get(session, "window-icon", &window_icon);
  if (window_icon != NULL && strlen(window_icon) != 0) {
    girara_set_window_icon(session, window_icon);
  }

  gtk_widget_grab_focus(GTK_WIDGET(session->gtk.view));

  return true;
}

static void girara_session_private_free(girara_session_private_t* session) {
  g_return_if_fail(session != NULL);

  g_free(session->session_name);
  session->session_name = NULL;

  // clean up completation state
  g_list_free(session->completion.entries);
  session->completion.entries         = NULL;
  session->completion.entries_current = NULL;

  g_free(session->completion.previous_command);
  session->completion.previous_command = NULL;

  g_free(session->completion.previous_parameter);
  session->completion.previous_parameter = NULL;

  /* clean up config handles */
  girara_list_free(session->config.handles);
  session->config.handles = NULL;

  /* clean up shortcut mappings */
  girara_list_free(session->config.shortcut_mappings);
  session->config.shortcut_mappings = NULL;

  /* clean up argument mappings */
  girara_list_free(session->config.argument_mappings);
  session->config.argument_mappings = NULL;

  /* clean up buffer */
  if (session->buffer.command) {
    g_string_free(session->buffer.command, TRUE);
  }
  session->buffer.command = NULL;

  /* clean up statusbar items */
  girara_list_free(session->elements.statusbar_items);
  session->elements.statusbar_items = NULL;

  /* clean up CSS style provider */
  g_clear_object(&session->gtk.cssprovider);
  g_clear_object(&session->csstemplate);

  /* clean up settings */
  girara_list_free(session->settings);
  session->settings = NULL;

  /* clean up mutex */
  g_mutex_clear(&session->feedkeys_mutex);

  g_free(session);
}

bool girara_session_destroy(girara_session_t* session) {
  g_return_val_if_fail(session != NULL, FALSE);

  /* clean up shortcuts */
  girara_list_free(session->bindings.shortcuts);
  session->bindings.shortcuts = NULL;

  /* clean up inputbar shortcuts */
  girara_list_free(session->bindings.inputbar_shortcuts);
  session->bindings.inputbar_shortcuts = NULL;

  /* clean up commands */
  girara_list_free(session->bindings.commands);
  session->bindings.commands = NULL;

  /* clean up special commands */
  girara_list_free(session->bindings.special_commands);
  session->bindings.special_commands = NULL;

  /* clean up mouse events */
  girara_list_free(session->bindings.mouse_events);
  session->bindings.mouse_events = NULL;

  /* clean up input histry */
  g_clear_object(&session->command_history);

  /* clean up modes */
  girara_list_free(session->modes.identifiers);
  session->modes.identifiers = NULL;

  /* clean up buffer */
  if (session->global.buffer) {
    g_string_free(session->global.buffer, TRUE);
  }
  session->global.buffer = NULL;

  /* clean up private data */
  girara_session_private_free(session->private_data);
  session->private_data = NULL;

  /* clean up session */
  g_free(session);

  return TRUE;
}

char* girara_buffer_get(girara_session_t* session) {
  g_return_val_if_fail(session != NULL, NULL);

  return (session->global.buffer) ? g_strdup(session->global.buffer->str) : NULL;
}

void girara_notify(girara_session_t* session, int level, const char* format, ...) {
  if (session == NULL || session->gtk.notification_text == NULL || session->gtk.notification_area == NULL ||
      session->gtk.inputbar == NULL || session->gtk.view == NULL) {
    return;
  }

  if (level == GIRARA_ERROR) {
    widget_add_class(session->gtk.notification_area, "notification-error");
    widget_add_class(session->gtk.notification_text, "notification-error");
  } else {
    widget_remove_class(session->gtk.notification_area, "notification-error");
    widget_remove_class(session->gtk.notification_text, "notification-error");
  }
  if (level == GIRARA_WARNING) {
    widget_add_class(session->gtk.notification_area, "notification-warning");
    widget_add_class(session->gtk.notification_text, "notification-warning");
  } else {
    widget_remove_class(session->gtk.notification_area, "notification-warning");
    widget_remove_class(session->gtk.notification_text, "notification-warning");
  }

  /* prepare message */
  va_list ap;
  va_start(ap, format);
  g_autofree char* message = g_strdup_vprintf(format, ap);
  va_end(ap);

  gtk_label_set_markup(GTK_LABEL(session->gtk.notification_text), message);

  /* update visibility */
  gtk_widget_show(GTK_WIDGET(session->gtk.notification_area));
  gtk_widget_hide(GTK_WIDGET(session->gtk.inputbar));

  gtk_widget_grab_focus(GTK_WIDGET(session->gtk.view));
}

void girara_dialog(girara_session_t* session, const char* dialog, bool invisible,
                   girara_callback_inputbar_key_press_event_t key_press_event,
                   girara_callback_inputbar_activate_t activate_event, void* data) {
  if (session == NULL || session->gtk.inputbar == NULL || session->gtk.inputbar_dialog == NULL ||
      session->gtk.inputbar_entry == NULL) {
    return;
  }

  gtk_widget_show(GTK_WIDGET(session->gtk.inputbar_dialog));

  /* set dialog message */
  if (dialog != NULL) {
    gtk_label_set_markup(session->gtk.inputbar_dialog, dialog);
  }

  /* set input visibility */
  if (invisible == true) {
    gtk_entry_set_visibility(session->gtk.inputbar_entry, FALSE);
  } else {
    gtk_entry_set_visibility(session->gtk.inputbar_entry, TRUE);
  }

  /* set handler */
  session->signals.inputbar_custom_activate        = activate_event;
  session->signals.inputbar_custom_key_press_event = key_press_event;
  session->signals.inputbar_custom_data            = data;

  /* focus inputbar */
  girara_sc_focus_inputbar(session, NULL, NULL, 0);
}

bool girara_set_view(girara_session_t* session, GtkWidget* widget) {
  g_return_val_if_fail(session != NULL, false);

  GtkWidget* parent = gtk_widget_get_parent(widget);
  if (parent != session->gtk.view) {
    gtk_container_add(GTK_CONTAINER(session->gtk.view), widget);
  }

  gtk_widget_set_visible(widget, true);
  gtk_stack_set_visible_child(GTK_STACK(session->gtk.view), widget);
  gtk_widget_grab_focus(GTK_WIDGET(session->gtk.view));

  return true;
}

void girara_mode_set(girara_session_t* session, girara_mode_t mode) {
  g_return_if_fail(session != NULL);

  session->modes.current_mode = mode;
}

girara_mode_t girara_mode_add(girara_session_t* session, const char* name) {
  g_return_val_if_fail(session != NULL, FALSE);
  g_return_val_if_fail(name != NULL && name[0] != '\0', FALSE);

  girara_mode_t last_index = 0;
  for (size_t idx = 0; idx != girara_list_size(session->modes.identifiers); ++idx) {
    girara_mode_string_t* mode = girara_list_nth(session->modes.identifiers, idx);
    if (mode->index > last_index) {
      last_index = mode->index;
    }
  }

  /* create new mode identifier */
  girara_mode_string_t* mode = g_malloc(sizeof(girara_mode_string_t));
  mode->index                = last_index + 1;
  mode->name                 = g_strdup(name);
  girara_list_append(session->modes.identifiers, mode);

  return mode->index;
}

girara_mode_t girara_mode_get(girara_session_t* session) {
  g_return_val_if_fail(session != NULL, 0);

  return session->modes.current_mode;
}

bool girara_set_window_title(girara_session_t* session, const char* name) {
  if (session == NULL || session->gtk.window == NULL || name == NULL) {
    return false;
  }

  gtk_window_set_title(GTK_WINDOW(session->gtk.window), name);

  return true;
}

bool girara_set_window_icon(girara_session_t* session, const char* name) {
  if (session == NULL || session->gtk.window == NULL || name == NULL) {
    return false;
  }

  if (strlen(name) == 0) {
    girara_warning("Empty icon name.");
    return false;
  }

  GtkWindow* window     = GTK_WINDOW(session->gtk.window);
  g_autofree char* path = girara_fix_path(name);
  bool success          = true;

  if (g_file_test(path, G_FILE_TEST_EXISTS) == TRUE) {
    girara_debug("Loading window icon from file: %s", path);

    g_autoptr(GError) error = NULL;
    success                 = gtk_window_set_icon_from_file(window, path, &error);

    if (success == false) {
      girara_debug("Failed to load window icon (file): %s", error->message);
    }
  } else {
    girara_debug("Loading window icon with name: %s", name);
    gtk_window_set_icon_name(window, name);
  }

  return success;
}

girara_list_t* girara_get_command_history(girara_session_t* session) {
  g_return_val_if_fail(session != NULL, NULL);
  return girara_input_history_list(session->command_history);
}

GiraraTemplate* girara_session_get_template(girara_session_t* session) {
  g_return_val_if_fail(session != NULL, NULL);

  return session->private_data->csstemplate;
}

void girara_session_set_template(girara_session_t* session, GiraraTemplate* template, bool init_variables) {
  g_return_if_fail(session != NULL);
  g_return_if_fail(template != NULL);

  g_clear_object(&session->private_data->csstemplate);

  session->private_data->csstemplate = template;
  if (init_variables == true) {
    init_template_engine(template);
    fill_template_with_values(session);
  }

  css_template_changed(template, session);
}
