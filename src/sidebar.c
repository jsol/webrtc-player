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
  gtk_widget_set_vexpand(scroll, TRUE);

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
  void (*callback)(GObject *, gpointer);

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
           const gchar *session_id,
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

  g_object_set_data_full(G_OBJECT(button),
                         "target",
                         g_strdup(subtitle),
                         g_free);
  g_object_set_data_full(G_OBJECT(button),
                         "session_id",
                         g_strdup(session_id),
                         g_free);

  g_signal_connect(button,
                   "notify::active",
                   G_CALLBACK(on_active_change),
                   user_data);

  return button;
}

#else

static void
on_active_change(GObject *self,
                 G_GNUC_UNUSED GParamSpec *pspec,
                 gpointer user_data)
{
  void (*callback)(GObject *, gpointer);

  g_assert(self);

  if (gtk_switch_get_active(GTK_SWITCH(self))) {
    callback = g_object_get_data(self, "activate");
  } else {
    callback = g_object_get_data(self, "deactivate");
  }
  callback(self, user_data);
}

GtkWidget *
get_button(const gchar *title,
           const gchar *subtitle,
           const gchar *session_id,
           GCallback activate,
           GCallback deactivate,
           gpointer user_data)
{
  GtkWidget *onoff;
  GtkWidget *row;

  row = adw_action_row_new();
  onoff = gtk_switch_new();

  gtk_widget_set_valign(onoff, GTK_ALIGN_CENTER);
  gtk_accessible_update_state(GTK_ACCESSIBLE(row),
                              GTK_ACCESSIBLE_STATE_CHECKED,
                              FALSE,
                              -1);
  gtk_widget_set_can_focus(onoff, FALSE);
  gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), TRUE);
  adw_action_row_add_suffix(ADW_ACTION_ROW(row), onoff);
  adw_action_row_set_activatable_widget(ADW_ACTION_ROW(row), onoff);

  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);
  adw_preferences_row_set_title_selectable(ADW_PREFERENCES_ROW(row), TRUE);
  if (subtitle) {
    adw_action_row_set_subtitle(ADW_ACTION_ROW(row), subtitle);
  }

  g_object_set_data(G_OBJECT(onoff), "activate", activate);
  g_object_set_data(G_OBJECT(onoff), "deactivate", deactivate);

  g_object_set_data_full(G_OBJECT(onoff), "target", g_strdup(subtitle), g_free);
  g_object_set_data_full(G_OBJECT(onoff),
                         "session_id",
                         g_strdup(session_id),
                         g_free);

  g_signal_connect(onoff,
                   "notify::active",
                   G_CALLBACK(on_active_change),
                   user_data);

  return row;
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