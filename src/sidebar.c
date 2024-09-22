#include <adwaita.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "sidebar.h"

#define APP_ID "com.github.com.jsol.webrtc_player"

#if ADW_MINOR_VERSION >= 4 && ADW_MAJOR_VERSION >= 1
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

#if ADW_MINOR_VERSION >= 4 && ADW_MAJOR_VERSION >= 1
static void
on_active_change(GObject *self,
                 G_GNUC_UNUSED GParamSpec *pspec,
                 gpointer user_data)
{
  void (*callback)(GObject*, gpointer);

  g_assert(self);

  if (adw_switch_row_get_active(ADW_SWITCH_ROW(self))) {
    callback = g_object_get_data(self, "activate");
  } else {
    callback = g_object_get_data(self, "deactivate");
  }
  callback(self, user_data);
}

GtkWidget *
get_button(const gchar *title,
           const gchar *subtitle,
           GCallback activate,
           GCallback deactivate,
           gpointer user_data)
{
  GtkWidget *button;

  g_return_val_if_fail(title != NULL, NULL);

  button = adw_switch_row_new();

  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(button), title);
  adw_preferences_row_set_title_selectable(ADW_PREFERENCES_ROW(button), TRUE);
  if (subtitle) {
    adw_action_row_set_subtitle(ADW_ACTION_ROW(button), subtitle);
  }

  g_object_set_data(G_OBJECT(button), "activate", activate);
  g_object_set_data(G_OBJECT(button), "deactivate", deactivate);

  g_signal_connect(button,
                   "notify::active",
                   G_CALLBACK(on_active_change),
                   user_data);

  return button;
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