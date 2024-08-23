#include <adwaita.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "sidebar.h"

#define APP_ID "com.github.com.jsol.webrtc_player"

#ifdef NO_FLAP
GtkWidget *
get_framed_content(GtkWidget *menu, GtkWidget *content)
{
  GtkWidget *flap;

  g_return_val_if_fail(menu != NULL, NULL);
  g_return_val_if_fail(content != NULL, NULL);

  flap = adw_overlay_split_view_new();

  adw_overlay_split_view_set_sidebar(ADW_OVERLAY_SPLIT_VIEW(flap), menu);
  adw_overlay_split_view_set_content(ADW_OVERLAY_SPLIT_VIEW(flap), content);

  return flap;
}
#else
GtkWidget *
get_framed_content(GtkWidget *menu, GtkWidget *content)
{
  GtkWidget *flap;
  GtkWidget *scroll;

  g_return_val_if_fail(menu != NULL, NULL);
  g_return_val_if_fail(content != NULL, NULL);

  scroll = gtk_scrolled_window_new();
  gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(scroll), 250);

  flap = adw_flap_new();

  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), menu);
  adw_flap_set_flap(ADW_FLAP(flap), scroll);
  adw_flap_set_content(ADW_FLAP(flap), content);

  return flap;
}
#endif

GtkWidget *
get_window(const gchar *title_str, GtkApplication *app, GtkWidget *content)
{
  GtkWidget *title;
  GtkWidget *header;
  GtkWidget *window;
  GtkWidget *box;

  window = adw_application_window_new(app);
  box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

  title = adw_window_title_new(title_str, NULL);
  header = adw_header_bar_new();
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(header), title);

  gtk_box_append(GTK_BOX(box), header);
  gtk_box_append(GTK_BOX(box), content);

  adw_application_window_set_content(ADW_APPLICATION_WINDOW(window), box);

  return window;
}

GtkApplication *
get_application(void)
{
  return GTK_APPLICATION(
          adw_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS));
}