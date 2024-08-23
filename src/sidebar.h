#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

GtkWidget *get_framed_content(GtkWidget *menu, GtkWidget *content);

GtkWidget *get_window(const gchar *title_str, GtkApplication *app, GtkWidget *content);


GtkApplication  *
get_application(void);

G_END_DECLS