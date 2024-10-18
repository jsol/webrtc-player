#include <adwaita.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "adw_wrapper.h"

#include "webrtc_settings.h"

#define APP_ID "com.github.jsol.webrtc_player"

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
           GObject *client,
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

  g_object_set_data(G_OBJECT(button), "client", client);
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
           GObject *client,
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

  g_object_set_data(G_OBJECT(onoff), "client", client);
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
get_button_list(const gchar *title, const gchar *subtitle)
{
  GtkWidget *row;

  row = adw_expander_row_new();

  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);
  adw_expander_row_set_subtitle(ADW_EXPANDER_ROW(row), subtitle);

  adw_expander_row_set_expanded(ADW_EXPANDER_ROW(row), TRUE);

  return row;
}

void
add_button_to_list(GtkWidget *list, GtkWidget *button)
{
  adw_expander_row_add_row(ADW_EXPANDER_ROW(list), button);
}

static void
on_selected_change(GObject *self,
                   G_GNUC_UNUSED GParamSpec *pspec,
                   gpointer user_data)
{
  WebrtcSettings *settings = WEBRTC_SETTINGS(user_data);
  const gchar *name;
  GtkStringObject *sel;
  const gchar *txt;

  name = g_object_get_data(self, "pref_name");
  sel = adw_combo_row_get_selected_item(ADW_COMBO_ROW(self));

  txt = gtk_string_object_get_string(sel);

  webrtc_settings_set_string(settings, GPOINTER_TO_INT(name), txt);

  g_clear_object(&sel);
}

static void
add_pref_dropdown(AdwPreferencesGroup *group,
                  GListModel *model,
                  const gchar *title,
                  GQuark pref,
                  guint selected,
                  gpointer data)
{
  GtkWidget *combo;

  combo = adw_combo_row_new();
  adw_combo_row_set_model(ADW_COMBO_ROW(combo), model);

  adw_combo_row_set_selected(ADW_COMBO_ROW(combo), selected);

  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(combo), title);
  g_object_set_data(G_OBJECT(combo), "pref_name", GINT_TO_POINTER(pref));
  g_signal_connect(combo,
                   "notify::selected",
                   G_CALLBACK(on_selected_change),
                   data);

  adw_preferences_group_add(group, combo);
}

static void
numeric_input_changed(GtkEditable *input, WebrtcSettings *settings)
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
    webrtc_settings_set_number(settings, GPOINTER_TO_INT(name), -1);
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

  webrtc_settings_set_number(settings, GPOINTER_TO_INT(name), num);

  tmp = g_strdup_printf("%d", (gint) num);
  if (g_strcmp0(tmp, txt) != 0) {
    gtk_editable_set_text(input, tmp);
  }
  g_free(tmp);
}

static void
add_pref_number(AdwPreferencesGroup *group,
                const gchar *title,
                GQuark pref_name,
                gint64 current,
                gint max,
                gpointer data)
{
  gchar *tmp = NULL;
  GtkWidget *input;
#if ADW_MINOR_VERSION >= 2 && ADW_MAJOR_VERSION >= 1

  input = adw_entry_row_new();

  adw_entry_row_set_input_purpose(ADW_ENTRY_ROW(input),
                                  GTK_INPUT_PURPOSE_DIGITS);
  adw_entry_row_set_show_apply_button(ADW_ENTRY_ROW(input), FALSE);

  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(input), title);

  g_object_set_data(G_OBJECT(input), "pref_name", GINT_TO_POINTER(pref_name));
  g_object_set_data(G_OBJECT(input), "max_input", GINT_TO_POINTER(max));

  g_signal_connect(input, "changed", G_CALLBACK(numeric_input_changed), data);

  adw_preferences_group_add(group, input);
#else
  GtkWidget *row;
  GtkWidget *box;
  GtkWidget *label;

  row = gtk_list_box_row_new();
  input = gtk_entry_new();
  label = gtk_label_new(title);
  box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

  gtk_box_append(GTK_BOX(box), label);
  gtk_box_append(GTK_BOX(box), input);
  gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);

  g_object_set_data(G_OBJECT(input), "pref_name", (GINT_TO_POINTER(pref_name));
  g_object_set_data(G_OBJECT(input), "max_input", GINT_TO_POINTER(max));

  g_signal_connect(input, "changed", G_CALLBACK(numeric_input_changed), data);
  adw_preferences_group_add(group, row);
#endif

  tmp = g_strdup_printf("%ld", current);
  gtk_editable_set_text(GTK_EDITABLE(input), tmp);
  g_free(tmp);
}

static void
pref_menu_cb(G_GNUC_UNUSED GSimpleAction *simple_action,
             G_GNUC_UNUSED GVariant *parameter,
             gpointer *data)
{
  WebrtcSettings *settings = (WebrtcSettings *) data;
  GtkWidget *pref_window;
  GtkWidget *pref_page;
  GtkWidget *audio_pref_group;
  GtkWidget *video_pref_group;
  GtkWidget *webrtc_pref_group;
  GtkStringList *codec_options;
  GtkStringList *boolean_options;
  const gchar *audio_codec_list[] = AUDIO_CODEC_LIST;
  const gchar *boolean_options_list[] = BOOLEAN_LIST;

  boolean_options = gtk_string_list_new(boolean_options_list);

  /* Setup audio preferences */

  audio_pref_group = adw_preferences_group_new();

  codec_options = gtk_string_list_new(audio_codec_list);

  add_pref_dropdown(ADW_PREFERENCES_GROUP(audio_pref_group),
                    G_LIST_MODEL(codec_options),
                    "Codec",
                    WEBRTC_SETTINGS_AUDIO_CODEC,
                    webrtc_settings_selected(settings,
                                             WEBRTC_SETTINGS_AUDIO_CODEC),
                    data);

  adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(audio_pref_group),
                                  "Audio settings");

  /* Setup video preferences */

  video_pref_group = adw_preferences_group_new();

  add_pref_dropdown(ADW_PREFERENCES_GROUP(video_pref_group),
                    G_LIST_MODEL(boolean_options),
                    "Adaptive Bitrate",
                    WEBRTC_SETTINGS_VIDEO_ADAPTIVE,
                    webrtc_settings_selected(settings,
                                             WEBRTC_SETTINGS_VIDEO_ADAPTIVE),
                    data);

  add_pref_number(ADW_PREFERENCES_GROUP(video_pref_group),
                  "Max Bitrate (Kbps)",
                  WEBRTC_SETTINGS_VIDEO_MAX_BITRATE,
                  webrtc_settings_video_max_bitrate(settings),
                  20000000,
                  data);

  add_pref_number(ADW_PREFERENCES_GROUP(video_pref_group),
                  "Compression (0-100)",
                  WEBRTC_SETTINGS_VIDEO_COMPRESSION,
                  webrtc_settings_video_compression(settings),
                  100,
                  data);

  add_pref_number(ADW_PREFERENCES_GROUP(video_pref_group),
                  "Key frame interval (GOP)",
                  WEBRTC_SETTINGS_VIDEO_GOP,
                  webrtc_settings_video_gop(settings),
                  600,
                  data);

  adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(video_pref_group),
                                  "Video settings");

  webrtc_pref_group = adw_preferences_group_new();

  add_pref_dropdown(ADW_PREFERENCES_GROUP(webrtc_pref_group),
                    G_LIST_MODEL(boolean_options),
                    "Force TURN relay",
                    WEBRTC_SETTINGS_FORCE_TURN,
                    webrtc_settings_selected(settings,
                                             WEBRTC_SETTINGS_FORCE_TURN),
                    data);

  adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(webrtc_pref_group),
                                  "WebRTC settings");

  /* Show preference page */
  pref_page = adw_preferences_page_new();
  pref_window = adw_preferences_window_new();

  adw_preferences_page_add(ADW_PREFERENCES_PAGE(pref_page),
                           ADW_PREFERENCES_GROUP(audio_pref_group));
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(pref_page),
                           ADW_PREFERENCES_GROUP(video_pref_group));

  adw_preferences_page_add(ADW_PREFERENCES_PAGE(pref_page),
                           ADW_PREFERENCES_GROUP(webrtc_pref_group));

  adw_preferences_window_add(ADW_PREFERENCES_WINDOW(pref_window),
                             ADW_PREFERENCES_PAGE(pref_page));
  gtk_window_present(GTK_WINDOW(pref_window));
}

static GtkEditable *
add_connect_string(AdwPreferencesGroup *group,
                   const gchar *title,
                   gboolean apply)
{
  GtkWidget *input;
#if ADW_MINOR_VERSION >= 2 && ADW_MAJOR_VERSION >= 1

  input = adw_entry_row_new();

  adw_entry_row_set_show_apply_button(ADW_ENTRY_ROW(input), apply);

  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(input), title);

  adw_preferences_group_add(group, input);
#else
  GtkWidget *row;
  GtkWidget *box;
  GtkWidget *label;

  row = gtk_list_box_row_new();
  input = gtk_entry_new();
  label = gtk_label_new(title);
  box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

  gtk_box_append(GTK_BOX(box), label);
  gtk_box_append(GTK_BOX(box), input);
  gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);

  g_object_set_data(G_OBJECT(input), "pref_name", (GINT_TO_POINTER(pref_name));
  g_object_set_data(G_OBJECT(input), "max_input", GINT_TO_POINTER(max));

  g_signal_connect(input, "changed", G_CALLBACK(numeric_input_changed), data);
  adw_preferences_group_add(group, row);

#endif
  return GTK_EDITABLE(input);
}

static void
connect_to_server(GObject *button, GObject *emit)
{
  GtkWindow *pref_window;
  GtkEditable *server;
  GtkEditable *user;
  GtkEditable *pass;

  pref_window = g_object_get_data(button, "pref_window");

  server = g_object_get_data(button, "server");
  user = g_object_get_data(button, "user");
  pass = g_object_get_data(button, "password");

  g_signal_emit_by_name(emit,
                        "connect-client",
                        gtk_editable_get_text(server),
                        gtk_editable_get_text(user),
                        gtk_editable_get_text(pass));

  gtk_window_close(pref_window);
}

static void
client_menu_cb(G_GNUC_UNUSED GSimpleAction *simple_action,
               G_GNUC_UNUSED GVariant *parameter,
               gpointer *data)
{
  GtkWidget *pref_window;
  GtkWidget *pref_page;
  GtkWidget *connect_group;
  GtkEditable *server;
  GtkEditable *user;
  GtkEditable *password;

  connect_group = adw_preferences_group_new();

  user = add_connect_string(ADW_PREFERENCES_GROUP(connect_group),
                            "User",
                            FALSE);
  password = add_connect_string(ADW_PREFERENCES_GROUP(connect_group),
                                "Password",
                                FALSE);

  server = add_connect_string(ADW_PREFERENCES_GROUP(connect_group),
                              "Server",
                              TRUE);

  adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(connect_group),
                                  "Connect to new server");

  g_object_set_data(G_OBJECT(server), "server", server);
  g_object_set_data(G_OBJECT(server), "user", user);
  g_object_set_data(G_OBJECT(server), "password", password);

  g_signal_connect(server, "apply", G_CALLBACK(connect_to_server), data);

  /* Show preference page */
  pref_page = adw_preferences_page_new();
  pref_window = adw_preferences_window_new();

  adw_preferences_page_add(ADW_PREFERENCES_PAGE(pref_page),
                           ADW_PREFERENCES_GROUP(connect_group));

  adw_preferences_window_add(ADW_PREFERENCES_WINDOW(pref_window),
                             ADW_PREFERENCES_PAGE(pref_page));

  g_object_set_data(G_OBJECT(server), "pref_window", pref_window);
  gtk_window_present(GTK_WINDOW(pref_window));
}

static void
build_app_menu(GtkMenuButton *menu_button,
               GtkApplication *app,
               WebrtcSettings *settings,
               GObject *emit)
{
  GMenu *menubar = g_menu_new();
  GMenuItem *menu_item_menu;
  GSimpleAction *act;

  menu_item_menu = g_menu_item_new("Preferences", "app.pref");
  g_menu_append_item(menubar, menu_item_menu);
  g_object_unref(menu_item_menu);

  menu_item_menu = g_menu_item_new("Add server", "app.add_client");
  g_menu_append_item(menubar, menu_item_menu);
  g_object_unref(menu_item_menu);

  act = g_simple_action_new("pref", NULL);
  g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act));
  g_signal_connect(act, "activate", G_CALLBACK(pref_menu_cb), settings);

  act = g_simple_action_new("add_client", NULL);
  g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act));
  g_signal_connect(act, "activate", G_CALLBACK(client_menu_cb), emit);

  gtk_menu_button_set_menu_model((menu_button), G_MENU_MODEL(menubar));
}

GtkWidget *
get_window(const gchar *title_str,
           GtkApplication *app,
           GtkWidget *content,
           WebrtcSettings *settings,
           GObject *emit)
{
  GtkWidget *title;
  GtkWidget *pref_menu;
  GtkWidget *header;
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *toast_overlay;

  window = adw_application_window_new(app);
  toast_overlay = adw_toast_overlay_new();
  box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

  title = adw_window_title_new(title_str, NULL);
  pref_menu = gtk_menu_button_new();
  header = adw_header_bar_new();

  build_app_menu(GTK_MENU_BUTTON(pref_menu), app, settings, emit);

  gtk_menu_button_set_direction(GTK_MENU_BUTTON(pref_menu), GTK_ARROW_NONE);
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(header), title);
  adw_header_bar_pack_end(ADW_HEADER_BAR(header), pref_menu);

  gtk_box_append(GTK_BOX(box), header);
  gtk_box_append(GTK_BOX(box), content);

  adw_toast_overlay_set_child(ADW_TOAST_OVERLAY(toast_overlay), box);

  g_object_set_data_full(G_OBJECT(app),
                         "toast_overlay",
                         g_object_ref(toast_overlay),
                         g_object_unref);

  adw_application_window_set_content(ADW_APPLICATION_WINDOW(window),
                                     toast_overlay);

  return window;
}

void
show_toast(GtkApplication *app, const gchar *msg)
{
  AdwToastOverlay *toast_overlay;

  toast_overlay =
          ADW_TOAST_OVERLAY(g_object_get_data(G_OBJECT(app), "toast_overlay"));
  adw_toast_overlay_add_toast(toast_overlay, adw_toast_new(msg));
}

GtkApplication *
get_application(void)
{
  return GTK_APPLICATION(
          adw_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS));
}
