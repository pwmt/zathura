/* SPDX-License-Identifier: Zlib */

#ifndef GIRARA_CALLBACKS_H
#define GIRARA_CALLBACKS_H

#include "types.h"
#include <gtk/gtk.h>

/**
 * Default callback for key press events in the view area
 *
 * @param widget The used widget
 * @param event The occurred event
 * @param session The used girara session
 * @return TRUE No error occurred
 * @return FALSE An error occurred
 */
gboolean girara_callback_view_key_press_event(GtkEventControllerKey* controller, guint keyval, guint keycode,
                                              GdkModifierType state, girara_session_t* session);

/**
 * Default callback when a button (typically a mouse button) has been pressed
 *
 * @param widget The used widget
 * @param button The button that triggered the event
 * @param session The used girara session
 * @return true to stop other handlers from being invoked for the event.
 * @return false to propagate the event further.
 */
gboolean girara_callback_view_button_press_event(GtkGestureClick* gesture, gint n_press, gdouble x, gdouble y,
                                                 girara_session_t* session);

/**
 * Default callback when a button (typically a mouse button) has been released
 *
 * @param widget The used widget
 * @param button The button that triggered the event
 * @param session The used girara session
 * @return true to stop other handlers from being invoked for the event.
 * @return false to propagate the event further.
 */
gboolean girara_callback_view_button_release_event(GtkGestureClick* gesture, gint n_press, gdouble x, gdouble y,
                                                   girara_session_t* session);

/**
 * Default callback when the pointer moves over the widget
 *
 * @param widget The used widget
 * @param button The event motion that triggered the event
 * @param session The used girara session
 * @return true to stop other handlers from being invoked for the event.
 * @return false to propagate the event further.
 */
gboolean girara_process_view_key(girara_session_t* session, guint keyval, guint clean);

gboolean girara_callback_view_button_motion_notify_event(GtkEventControllerMotion* controller, gdouble x, gdouble y,
                                                         girara_session_t* session);

/* return true if a mouse binding matches the given event in the current mode (state is masked internally) */
bool girara_has_mouse_event(girara_session_t* session, girara_event_type_t type, guint button, GdkModifierType state);

/**
 * Default callback then a scroll event is triggered by the view
 *
 * @param widget The widget
 * @param event The event motion
 * @param session The girara session
 * @return true to stop other handlers from being invoked for the event.
 * @return false to propagate the event further.
 */
gboolean girara_callback_view_scroll_event(GtkEventControllerScroll* controller, gdouble dx, gdouble dy,
                                           girara_session_t* session);

#endif
