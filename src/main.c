#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>
#include <gdk/gdk.h>

#include "webrtc_client.h"
#include "webrtc_gui.h"
#include "webrtc_session.h"
#include "sidebar.h"

struct app_ctx {
  WebrtcClient *c;
  WebrtcGui *gui;
  GtkApplication *app;
  GHashTable *sessions;
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
new_video_sink(struct app_ctx *ctx)
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
  webrtc_gui_add_paintable(ctx->gui, paintable);

  return video_sink;
}

static void
on_new_stream(G_GNUC_UNUSED GObject *source,
              const gchar *target,
              const gchar *session_id,
              struct app_ctx *ctx)
{
  GstElement *video_sink;
  GstElement *audio_sink;
  WebrtcSession *sess;

  sess = webrtc_session_new(ctx->c, session_id, target);
  video_sink = new_video_sink(ctx);
  audio_sink = gst_element_factory_make("autoaudiosink", "audio-output");

  webrtc_session_add_element(sess, WEBRTC_SESSION_ELEM_VIDEO, video_sink);
  webrtc_session_add_element(sess, WEBRTC_SESSION_ELEM_AUDIO, audio_sink);

  webrtc_session_start(sess);
}

int
main(int argc, char **argv)
{
  struct app_ctx ctx = { 0 };
  gst_init(&argc, &argv);

  if (!webrtc_session_check_plugins()) {
    return 1;
  }

  ctx.c = webrtc_client_new(g_getenv("WEBRTC_HOST"),
                            g_getenv("WEBRTC_USER"),
                            g_getenv("WEBRTC_PASS"));

  g_signal_connect(ctx.c, "new-peer", G_CALLBACK(new_peer), NULL);

  ctx.gui = webrtc_gui_new(ctx.c);

  webrtc_client_connect_async(ctx.c);

  g_signal_connect(ctx.gui, "new-stream", G_CALLBACK(on_new_stream), &ctx);

  ctx.app = get_application();

  g_signal_connect(ctx.app,
                   "activate",
                   G_CALLBACK(webrtc_gui_activate),
                   ctx.gui);
  g_application_run(G_APPLICATION(ctx.app), argc, argv);
}