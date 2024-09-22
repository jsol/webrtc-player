#pragma once

#include <glib-object.h>
#include <glib.h>
#include <gst/gst.h>
#include <gtk/gtk.h>

#include "webrtc_client.h"

G_BEGIN_DECLS

/*
 * Type declaration.
 */

#define WEBRTC_TYPE_GUI webrtc_gui_get_type()
G_DECLARE_FINAL_TYPE(WebrtcGui, webrtc_gui, WEBRTC, GUI, GObject)

/*
 * Method definitions.
 */
WebrtcGui *webrtc_gui_new(WebrtcClient *protocol);

void webrtc_gui_add_paintable(WebrtcGui *self,
                              const gchar *id,
                              GdkPaintable *paintable);

void webrtc_gui_remove_paintable(WebrtcGui *self, const gchar *id);

void webrtc_gui_activate(GtkApplication *app, G_GNUC_UNUSED gpointer user_data);

G_END_DECLS
