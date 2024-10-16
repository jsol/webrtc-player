#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>
#include <gdk/gdk.h>

#include "webrtc_client.h"
#include "webrtc_gui.h"
#include "webrtc_session.h"
#include "webrtc_settings.h"
#include "adw_wrapper.h"

struct app_ctx {
  WebrtcGui *gui;
  GtkApplication *app;
  GHashTable *sessions;
  WebrtcSettings *settings;
};
static void
new_peer(G_GNUC_UNUSED GObject *source,
         const gchar *s,
         const gchar *subject,
         G_GNUC_UNUSED gpointer *user_data)
{
  g_message("New client connected: %s -> %s", s, subject);
}

static GstElement *
new_video_sink(struct app_ctx *ctx, const gchar *id)
{
  GdkPaintable *paintable = NULL;
  GstElement *video_sink;

  g_print("Getting paintable sink\n");

  g_print("Creating sink\n");
  video_sink = gst_element_factory_make("gtk4paintablesink", "video-output");

  if (!video_sink) {
    g_printerr("One element could not be created. Exiting.\n");
    return NULL;
  }

  g_object_get(G_OBJECT(video_sink), "paintable", &paintable, NULL);

  g_print("Setting paintable \n");
  webrtc_gui_add_paintable(ctx->gui, id, paintable);

  return video_sink;
}

static void
on_new_stream(G_GNUC_UNUSED GObject *source,
              WebrtcClient *c,
              const gchar *target,
              const gchar *session_id,
              struct app_ctx *ctx)
{
  GstElement *video_sink;
  GstElement *audio_sink;
  WebrtcSession *sess;

  sess = webrtc_session_new(c, ctx->settings, session_id, target);
  video_sink = new_video_sink(ctx, session_id);
  audio_sink = gst_element_factory_make("autoaudiosink", "audio-output");

  webrtc_session_add_element(sess, WEBRTC_SESSION_ELEM_VIDEO, video_sink);
  webrtc_session_add_element(sess, WEBRTC_SESSION_ELEM_AUDIO, audio_sink);

  webrtc_session_start(sess, FALSE);

  g_hash_table_insert(ctx->sessions, g_strdup(session_id), sess);
}

static void
on_close_stream(G_GNUC_UNUSED GObject *source,
                const gchar *target,
                const gchar *session_id,
                struct app_ctx *ctx)
{
  WebrtcSession *sess;

  sess = g_hash_table_lookup(ctx->sessions, session_id);

  if (sess == NULL) {
    g_warning("Tried to close a session that did not exists: %s (%s)",
              session_id,
              target);
    return;
  }

  webrtc_gui_remove_paintable(ctx->gui, session_id);
  webrtc_session_stop(sess);

  g_hash_table_remove(ctx->sessions, session_id);
}

static void
on_remove_stream(G_GNUC_UNUSED GObject *source,
                 struct stream_started *info,
                 struct app_ctx *ctx)
{
  WebrtcSession *sess;

  sess = g_hash_table_lookup(ctx->sessions, info->session_id);

  if (sess == NULL) {
    return;
  }

  g_message("Stopping session id: %s", info->session_id);

  webrtc_session_stop(sess);
  g_hash_table_remove(ctx->sessions, info->session_id);
  webrtc_gui_remove_paintable(ctx->gui, info->session_id);
}

static void
on_connect_client(G_GNUC_UNUSED GObject *src,
                  const gchar *server,
                  const gchar *user,
                  const gchar *password,
                  struct app_ctx *ctx)
{
  WebrtcClient *c;

  c = webrtc_client_new(server, user, password);

  g_signal_connect(c, "new-peer", G_CALLBACK(new_peer), NULL);
  g_signal_connect(c, "remove-stream", G_CALLBACK(on_remove_stream), ctx);

  webrtc_gui_add_client(ctx->gui, c);

  webrtc_client_connect_async(c);
}

int
main(int argc, char **argv)
{
  struct app_ctx ctx = { 0 };
  gint code;
  gst_init(&argc, &argv);

  if (!webrtc_session_check_plugins()) {
    return 1;
  }

  ctx.settings = webrtc_settings_new();

  webrtc_settings_bind(ctx.settings);

  code = webrtc_settings_parse_opts(ctx.settings, argc, argv);
  if (code >= 0) {
    goto out;
  }

  ctx.sessions = g_hash_table_new_full(g_str_hash,
                                       g_str_equal,
                                       g_free,
                                       g_object_unref);

  ctx.gui = webrtc_gui_new(ctx.settings);

  g_signal_connect(ctx.gui, "new-stream", G_CALLBACK(on_new_stream), &ctx);
  g_signal_connect(ctx.gui, "close-stream", G_CALLBACK(on_close_stream), &ctx);
  g_signal_connect(ctx.gui,
                   "connect-client",
                   G_CALLBACK(on_connect_client),
                   &ctx);

  ctx.app = get_application();

  if (strlen(g_getenv("WEBRTC_HOST")) > 0) {
    on_connect_client(NULL,
                      g_getenv("WEBRTC_HOST"),
                      g_getenv("WEBRTC_USER"),
                      g_getenv("WEBRTC_PASS"),
                      &ctx);
  }

  g_signal_connect(ctx.app,
                   "activate",
                   G_CALLBACK(webrtc_gui_activate),
                   ctx.gui);
  g_application_run(G_APPLICATION(ctx.app), argc, argv);

out:
  return code;
}