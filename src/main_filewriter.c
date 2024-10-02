#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

#include "webrtc_client.h"
#include "webrtc_session.h"

struct app_ctx {
  WebrtcClient *c;
  gchar *target;
  GHashTable *sessions;
  GMainLoop *loop;
};
static void
new_peer(G_GNUC_UNUSED GObject *source,
         const gchar *s,
         const gchar *subject,
         G_GNUC_UNUSED gpointer *user_data)
{
  g_message("New client connected: %s -> %s", s, subject);
}

static void
on_new_stream(G_GNUC_UNUSED GObject *source,
              struct stream_started *info,
              struct app_ctx *ctx)
{
  GstElement *mux;
  GstElement *filesink;
  WebrtcSession *sess;

  g_print("New stream: %p, %p\n", info, ctx);
  if (g_strcmp0(info->subject, ctx->target) != 0) {
    g_message("Device %s connected, waiting for %s",
              info->subject,
              ctx->target);
    return;
  }

  sess = webrtc_session_new(ctx->c, info->session_id, info->subject);

  webrtc_session_set_adaptive_bitrate(sess, FALSE);
  webrtc_session_set_audio_codec(sess, WEBRTC_SESSION_AUDIO_CODEC_OPUS);

  mux = gst_element_factory_make("matroskamux", "mux");
  filesink = gst_element_factory_make("filesink", "filesink");

  g_object_set(G_OBJECT(mux), "streamable", TRUE, NULL);
  g_object_set(G_OBJECT(filesink), "location", "file.mkv", NULL);

  webrtc_session_add_element(sess, WEBRTC_SESSION_ELEM_MUX, mux);
  webrtc_session_add_element(sess, WEBRTC_SESSION_ELEM_MUX, filesink);

  webrtc_session_start(sess);
}

int
main(int argc, char **argv)
{
  struct app_ctx ctx = { 0 };
  gst_init(&argc, &argv);

  ctx.c = webrtc_client_new(g_getenv("WEBRTC_HOST"),
                            g_getenv("WEBRTC_USER"),
                            g_getenv("WEBRTC_PASS"));
  ctx.target = g_strdup(g_getenv("WEBRTC_TARGET"));

  g_signal_connect(ctx.c, "new-peer", G_CALLBACK(new_peer), NULL);
  g_signal_connect(ctx.c, "new-stream", G_CALLBACK(on_new_stream), &ctx);

  webrtc_client_connect_async(ctx.c);
  ctx.loop = g_main_loop_new(NULL, FALSE);

  g_main_loop_run(ctx.loop);
}
