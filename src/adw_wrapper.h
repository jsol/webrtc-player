/*
 * Seems like adwaita collides with gstreamer, can't build when both are
 * included in the same file. Also, libawaita differs a lot between the versions
 * available on ubuntu 22.04 / 24.04 and debian 10, so this hides a lot of
 * different implementations
 */

#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>
#include "webrtc_settings.h"

G_BEGIN_DECLS

#define USE_DEFAULT "use default"

GtkWidget *get_framed_content(GtkWidget *menu, GtkWidget *content);

GtkWidget *get_window(const gchar *title_str,
                      GtkApplication *app,
                      GtkWidget *content,
                      WebrtcSettings *settings,
                      GObject *emit);

GtkWidget *get_button(const gchar *title,
                      const gchar *subtitle,
                      const gchar *session_id,
                      GObject *client,
                      GCallback activate,
                      GCallback deactivate,
                      gpointer user_data);

GtkApplication *get_application(void);

GtkWidget *get_button_list(const gchar *title, const gchar *subtitle);
void add_button_to_list(GtkWidget *list, GtkWidget *button);

void show_toast(GtkApplication *app, const gchar *msg);
G_END_DECLS