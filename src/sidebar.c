#include <adwaita.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "sidebar.h"

#define APP_ID "com.github.com.jsol.webrtc_player"

static const gchar *audio_codec_list[] = { USE_DEFAULT, "aac", "opus", NULL };
static const gchar *boolean_options_list[] = { USE_DEFAULT,
                                               "disabled",
                                               "enabled",
                                               NULL };

#ifndef G_APPLICATION_DEFAULT_FLAGS
#define G_APPLICATION_DEFAULT_FLAGS 0
#endif

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

static void
on_selected_change(GObject *self,
                   G_GNUC_UNUSED GParamSpec *pspec,
                   gpointer user_data)
{
  GObject *app = G_OBJECT(user_data);
  const gchar *name;
  GObject *sel;

  name = g_object_get_data(self, "pref_name");
  sel = adw_combo_row_get_selected_item(ADW_COMBO_ROW(self));

  g_object_set_data_full(app, name, g_object_ref(sel), g_object_unref);
}

static void
add_pref_dropdown(AdwPreferencesGroup *group,
                  GListModel *model,
                  const gchar *title,
                  const gchar *pref_name,
                  gpointer data)
{
  GtkWidget *combo;

  combo = adw_combo_row_new();
  adw_combo_row_set_model(ADW_COMBO_ROW(combo), model);

  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(combo), title);
  g_object_set_data(G_OBJECT(combo), "pref_name", (gchar *) pref_name);
  g_signal_connect(combo,
                   "notify::selected",
                   G_CALLBACK(on_selected_change),
                   data);

  adw_preferences_group_add(group, combo);
}

static void
numeric_input_changed(GtkEditable *input, GObject *app)
{
  const gchar *txt;
  gint64 num;
  gchar *end = NULL;
  gint max;
  gchar *tmp;
  const gchar *name;

  txt = gtk_editable_get_text(input);
  name = g_object_get_data(G_OBJECT(input), "pref_name");
  g_print("CHANGED: %s\n", txt);

  if (strlen(txt) == 0) {
    g_object_set_data(app, name, GINT_TO_POINTER(-1));
    return;
  }

  num = g_ascii_strtoll(txt, &end, 10);

  if (num < 0) {
    g_warning("Invalid input: %lu", num);
    gtk_editable_set_text(input, "");
    return;
  }

  max = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(input), "max_input"));
  if (num > max) {
    g_warning("Input too big: %lu", num);

    tmp = g_strdup_printf("%d", max);

    gtk_editable_set_text(input, tmp);

    g_free(tmp);

    return;
  }

  g_object_set_data(app, name, GINT_TO_POINTER(num));

  tmp = g_strdup_printf("%d", (gint) num);
  if (g_strcmp0(tmp, txt) != 0) {
    gtk_editable_set_text(input, tmp);
  }
  g_free(tmp);
}

static void
add_pref_number(AdwPreferencesGroup *group,
                const gchar *title,
                const gchar *pref_name,
                gint max,
                gpointer data)
{
#if ADW_MINOR_VERSION >= 2 && ADW_MAJOR_VERSION >= 1
  GtkWidget *input;
  input = adw_entry_row_new();

  adw_entry_row_set_input_purpose(ADW_ENTRY_ROW(input),
                                  GTK_INPUT_PURPOSE_DIGITS);
  adw_entry_row_set_show_apply_button(ADW_ENTRY_ROW(input), FALSE);

  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(input), title);

  g_object_set_data(G_OBJECT(input), "pref_name", (gchar *) pref_name);
  g_object_set_data(G_OBJECT(input), "max_input", GINT_TO_POINTER(max));

  g_signal_connect(input, "changed", G_CALLBACK(numeric_input_changed), data);

  adw_preferences_group_add(group, input);
#else
  GtkWidget *row;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *input;

  row = gtk_list_box_row_new();
  input = gtk_entry_new();
  label = gtk_label_new(title);
  box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

  gtk_box_append(GTK_BOX(box), label);
  gtk_box_append(GTK_BOX(box), input);
  gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);

  g_object_set_data(G_OBJECT(input), "pref_name", (gchar *) pref_name);
  g_object_set_data(G_OBJECT(input), "max_input", GINT_TO_POINTER(max));

  g_signal_connect(input, "changed", G_CALLBACK(numeric_input_changed), data);
  adw_preferences_group_add(group, row);
#endif
}

static void
pref_menu_cb(G_GNUC_UNUSED GSimpleAction *simple_action,
             G_GNUC_UNUSED GVariant *parameter,
             gpointer *data)
{
  GtkWidget *pref_window;
  GtkWidget *pref_page;
  GtkWidget *audio_pref_group;
  GtkWidget *video_pref_group;
  GtkStringList *codec_options;
  GtkStringList *boolean_options;

  boolean_options = gtk_string_list_new(boolean_options_list);

  /* Setup audio preferences */

  audio_pref_group = adw_preferences_group_new();

  codec_options = gtk_string_list_new(audio_codec_list);

  add_pref_dropdown(ADW_PREFERENCES_GROUP(audio_pref_group),
                    G_LIST_MODEL(codec_options),
                    "Codec",
                    "codec",
                    data);

  adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(audio_pref_group),
                                  "Audio settings");

  /* Setup video preferences */

  video_pref_group = adw_preferences_group_new();

  add_pref_dropdown(ADW_PREFERENCES_GROUP(video_pref_group),
                    G_LIST_MODEL(boolean_options),
                    "Adaptive Bitrate",
                    "adaptive",
                    data);

  add_pref_number(ADW_PREFERENCES_GROUP(video_pref_group),
                  "Max Bitrate (Kbps)",
                  "maxBitrateInKbps",
                  20000000,
                  data);

  add_pref_number(ADW_PREFERENCES_GROUP(video_pref_group),
                  "Compression (0-100)",
                  "compression",
                  100,
                  data);

  add_pref_number(ADW_PREFERENCES_GROUP(video_pref_group),
                  "Key frame interval (GOP)",
                  "keyframeInterval",
                  600,
                  data);

  adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(video_pref_group),
                                  "Video settings");

  /* Show preference page */
  pref_page = adw_preferences_page_new();
  pref_window = adw_preferences_window_new();

  adw_preferences_page_add(ADW_PREFERENCES_PAGE(pref_page),
                           ADW_PREFERENCES_GROUP(audio_pref_group));
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(pref_page),
                           ADW_PREFERENCES_GROUP(video_pref_group));

  adw_preferences_window_add(ADW_PREFERENCES_WINDOW(pref_window),
                             ADW_PREFERENCES_PAGE(pref_page));
  gtk_window_present(GTK_WINDOW(pref_window));
}

static void
build_app_menu(GtkMenuButton *menu_button, GtkApplication *app)
{
  GMenu *menubar = g_menu_new();
  GMenuItem *menu_item_menu;
  GSimpleAction *act;

  menu_item_menu = g_menu_item_new("Preferences", "app.pref");
  g_menu_append_item(menubar, menu_item_menu);
  g_object_unref(menu_item_menu);

  act = g_simple_action_new("pref", NULL);
  g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act));
  g_signal_connect(act, "activate", G_CALLBACK(pref_menu_cb), app);

  gtk_menu_button_set_menu_model((menu_button), G_MENU_MODEL(menubar));
}

GtkWidget *
get_window(const gchar *title_str, GtkApplication *app, GtkWidget *content)
{
  GtkWidget *title;
  GtkWidget *pref_menu;
  GtkWidget *header;
  GtkWidget *window;
  GtkWidget *box;

  window = adw_application_window_new(app);
  box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

  title = adw_window_title_new(title_str, NULL);
  pref_menu = gtk_menu_button_new();
  header = adw_header_bar_new();

  build_app_menu(GTK_MENU_BUTTON(pref_menu), app);

  gtk_menu_button_set_direction(GTK_MENU_BUTTON(pref_menu), GTK_ARROW_NONE);
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(header), title);
  adw_header_bar_pack_end(ADW_HEADER_BAR(header), pref_menu);

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