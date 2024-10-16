
#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>
#include <gtk/gtk.h>

#include "webrtc_gui.h"
#include "webrtc_client.h"
#include "webrtc_settings.h"
#include "sidebar.h"

struct _WebrtcGui {
  GObject parent;

  GtkWidget *video;

  GtkWidget *video_grid;
  GstElement *sink;

  GtkWidget *button_list;

  WebrtcClient *protocol;
  GHashTable *videos;
  GObject *app;
  GHashTable *buttons;
  WebrtcSettings *settings;
};

G_DEFINE_TYPE(WebrtcGui, webrtc_gui, G_TYPE_OBJECT)

typedef enum {
  PROP_PROTOCOL = 1,
  PROP_SETTINGS,
  N_PROPERTIES
} WebrtcGuiProperty;
static GParamSpec *obj_properties[N_PROPERTIES] = {
  NULL,
};

enum gui_signals {
  SIG_NEW_STREAM = 0,
  SIG_CLOSE_STREAM,
  SIG_LAST,
};
static guint gui_signal_defs[SIG_LAST] = { 0 };

static void
on_activate(GObject *button, WebrtcGui *self)
{
  const gchar *target;
  const gchar *session_id;

  target = g_object_get_data(button, "target");
  session_id = g_object_get_data(button, "session_id");

  g_signal_emit(self, gui_signal_defs[SIG_NEW_STREAM], 0, target, session_id);
}

static void
on_deactivate(GObject *button, WebrtcGui *self)
{
  const gchar *target;
  const gchar *session_id;

  target = g_object_get_data(button, "target");
  session_id = g_object_get_data(button, "session_id");

  g_signal_emit(self, gui_signal_defs[SIG_CLOSE_STREAM], 0, target, session_id);
}

static void
on_new_stream(G_GNUC_UNUSED WebrtcClient *source,
              struct stream_started *info,
              WebrtcGui *self)
{
  GtkWidget *button;

  g_assert(self);
  g_assert(info);

  button = gtk_button_new_with_label(info->bearer_name);
  button = get_button(info->bearer_name,
                      info->subject,
                      info->session_id,
                      G_CALLBACK(on_activate),
                      G_CALLBACK(on_deactivate),
                      self);

  gtk_list_box_append(GTK_LIST_BOX(self->button_list), button);
  g_hash_table_insert(self->buttons,
                      g_strdup(info->session_id),
                      g_object_ref(button));
}

static void
on_stream_end(G_GNUC_UNUSED WebrtcClient *source,
              struct stream_started *info,
              WebrtcGui *self)
{
  GtkWidget *button;

  g_assert(self);
  g_assert(info);

  button = g_hash_table_lookup(self->buttons, info->session_id);

  if (button == NULL) {
    return;
  }

  gtk_list_box_remove(GTK_LIST_BOX(self->button_list), button);
  g_hash_table_remove(self->buttons, info->session_id);
}

static void
webrtc_gui_dispose(GObject *obj)
{
  WebrtcGui *self = WEBRTC_GUI(obj);

  g_assert(self);

  /* Do unrefs of objects and such. The object might be used after dispose,
   * and dispose might be called several times on the same object
   */

  g_clear_object(&self->videos);
  g_clear_object(&self->buttons);

  /* Always chain up to the parent dispose function to complete object
   * destruction. */
  G_OBJECT_CLASS(webrtc_gui_parent_class)->dispose(obj);
}

static void
webrtc_gui_finalize(GObject *obj)
{
  WebrtcGui *self = WEBRTC_GUI(obj);

  g_assert(self);

  /* free stuff */

  g_clear_object(&self->protocol);

  /* Always chain up to the parent finalize function to complete object
   * destruction. */
  G_OBJECT_CLASS(webrtc_gui_parent_class)->finalize(obj);
}

static void
get_property(GObject *object,
             guint property_id,
             GValue *value,
             GParamSpec *pspec)
{
  WebrtcGui *self = WEBRTC_GUI(object);

  switch ((WebrtcGuiProperty) property_id) {
  case PROP_PROTOCOL:
    g_value_set_object(value, self->protocol);
    break;

  case PROP_SETTINGS:
    g_value_set_object(value, self->settings);
    break;

  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    break;
  }
}

static void
set_property(GObject *object,
             guint property_id,
             const GValue *value,
             GParamSpec *pspec)
{
  WebrtcGui *self = WEBRTC_GUI(object);

  switch ((WebrtcGuiProperty) property_id) {
  case PROP_PROTOCOL:
    g_clear_object(&self->protocol);
    self->protocol = g_value_dup_object(value);
    g_signal_connect(self->protocol,
                     "new-stream",
                     G_CALLBACK(on_new_stream),
                     self);
    g_signal_connect(self->protocol,
                     "remove-stream",
                     G_CALLBACK(on_stream_end),
                     self);
    break;
  case PROP_SETTINGS:
    g_clear_object(&self->settings);
    self->settings = g_value_dup_object(value);
    break;

  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    break;
  }
}

static void
webrtc_gui_class_init(WebrtcGuiClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = webrtc_gui_dispose;
  object_class->finalize = webrtc_gui_finalize;
  object_class->set_property = set_property;
  object_class->get_property = get_property;

  GType new_stream_types[] = { G_TYPE_STRING, G_TYPE_STRING };
  gui_signal_defs[SIG_NEW_STREAM] =
          g_signal_newv("new-stream",
                        G_TYPE_FROM_CLASS(object_class),
                        G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE |
                                G_SIGNAL_NO_HOOKS,
                        NULL /* closure */,
                        NULL /* accumulator */,
                        NULL /* accumulator data */,
                        NULL /* C marshaller */,
                        G_TYPE_NONE /* return_type */,
                        G_N_ELEMENTS(new_stream_types) /* n_params */,
                        new_stream_types /* param_types, or set to NULL */
          );

  gui_signal_defs[SIG_CLOSE_STREAM] =
          g_signal_newv("close-stream",
                        G_TYPE_FROM_CLASS(object_class),
                        G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE |
                                G_SIGNAL_NO_HOOKS,
                        NULL /* closure */,
                        NULL /* accumulator */,
                        NULL /* accumulator data */,
                        NULL /* C marshaller */,
                        G_TYPE_NONE /* return_type */,
                        G_N_ELEMENTS(new_stream_types) /* n_params */,
                        new_stream_types /* param_types, or set to NULL */
          );

  obj_properties[PROP_PROTOCOL] =
          g_param_spec_object("protocol",
                              "Protocol",
                              "Placeholder description.",
                              WEBRTC_TYPE_CLIENT, /* default */
                              G_PARAM_READWRITE);

  obj_properties[PROP_SETTINGS] =
          g_param_spec_object("settings",
                              "Settings",
                              "Placeholder description.",
                              WEBRTC_TYPE_SETTINGS, /* default */
                              G_PARAM_READWRITE);

  g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

static void
webrtc_gui_init(WebrtcGui *self)
{
  /* initialize all public and private members to reasonable default values.
   * They are all automatically initialized to 0 to begin with. */

  self->videos = g_hash_table_new_full(g_str_hash,
                                       g_str_equal,
                                       g_free,
                                       g_object_unref);
  self->buttons = g_hash_table_new_full(g_str_hash,
                                        g_str_equal,
                                        g_free,
                                        g_object_unref);
}

WebrtcGui *
webrtc_gui_new(WebrtcClient *protocol, WebrtcSettings *settings)
{
  WebrtcGui *self = g_object_new(WEBRTC_TYPE_GUI,
                                 "protocol",
                                 protocol,
                                 "settings",
                                 settings,
                                 NULL);

  return self;
}

void
webrtc_gui_add_paintable(WebrtcGui *self,
                         const gchar *id,
                         GdkPaintable *paintable)
{
  GtkWidget *video;
  GtkWidget *child;

  child = gtk_flow_box_child_new();
  video = gtk_picture_new();
  gtk_widget_set_size_request(video, 640, 360);

  gtk_flow_box_child_set_child(GTK_FLOW_BOX_CHILD(child), video);

  gtk_flow_box_append(GTK_FLOW_BOX(self->video_grid), child);

  gtk_picture_set_paintable(GTK_PICTURE(video), paintable);
  g_hash_table_insert(self->videos, g_strdup(id), g_object_ref(child));
}

void
webrtc_gui_remove_paintable(WebrtcGui *self, const gchar *id)
{
  GtkWidget *child;

  child = g_hash_table_lookup(self->videos, id);

  if (child == NULL) {
    g_warning("Tried to close invalid paintable with id %s", id);
    return;
  }

  g_hash_table_remove(self->videos, id);
  gtk_flow_box_remove(GTK_FLOW_BOX(self->video_grid), child);
}

void
webrtc_gui_activate(GtkApplication *app, G_GNUC_UNUSED gpointer user_data)
{
  GtkWidget *window;

  WebrtcGui *self = WEBRTC_GUI(user_data);

  self->app = G_OBJECT(app);

  g_print("Setting up rest of UI\n");
  /*self->button_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);*/
  self->button_list = gtk_list_box_new();
  self->video_grid = gtk_flow_box_new();
  window = get_window("WebRTC Player",
                      app,
                      get_framed_content(self->button_list, self->video_grid),
                      self->settings);

  /* Todo: Fix video grid */
  g_print("Showing window \n");
  gtk_window_set_default_size(GTK_WINDOW(window), 1200, 720);
  gtk_window_present(GTK_WINDOW(window));
  g_print("Done\n");
}
