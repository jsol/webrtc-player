#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define USE_DEFAULT "use default"

GtkWidget *get_framed_content(GtkWidget *menu, GtkWidget *content);

GtkWidget *
get_window(const gchar *title_str, GtkApplication *app, GtkWidget *content);

GtkWidget *get_button(const gchar *title,
                      const gchar *subtitle,
                      const gchar *session_id,
                      GCallback activate,
                      GCallback deactivate,
                      gpointer user_data);

GtkApplication *get_application(void);

G_END_DECLS