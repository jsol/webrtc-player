#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

GtkWidget *get_framed_content(GtkWidget *menu, GtkWidget *content);

GtkWidget *
get_window(const gchar *title_str, GtkApplication *app, GtkWidget *content);

GtkWidget *get_button(const gchar *title,
                      const gchar *subtitle,
                      GCallback activate,
                      GCallback deactivate,
                      gpointer user_data);

GtkApplication *get_application(void);

G_END_DECLS