#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>
#include <glib-unix.h>

#include "webrtc_client.h"
#include "webrtc_session.h"

struct app_ctx {
  WebrtcClient *c;
  GHashTable *sessions;
  GMainLoop *loop;
  struct {
    gchar *output;
    gchar *target;
    gboolean aac;
  } config;
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
  if (ctx->config.target != NULL &&
      g_ascii_strncasecmp(info->subject,
                          ctx->config.target,
                          strlen(ctx->config.target)) != 0) {
    g_message("Device %s connected, waiting for %s",
              info->subject,
              ctx->config.target);
    return;
  }

  sess = webrtc_session_new(ctx->c, info->session_id, info->subject);
  g_hash_table_insert(ctx->sessions, g_strdup(info->session_id), sess);

  webrtc_session_set_adaptive_bitrate(sess, FALSE);
  if (ctx->config.aac) {
    webrtc_session_set_audio_codec(sess, WEBRTC_SESSION_AUDIO_CODEC_AAC);
  } else {
    webrtc_session_set_audio_codec(sess, WEBRTC_SESSION_AUDIO_CODEC_OPUS);
  }

  mux = gst_element_factory_make("matroskamux", "mux");
  filesink = gst_element_factory_make("filesink", "filesink");

  g_object_set(G_OBJECT(mux), "streamable", TRUE, NULL);

  if (ctx->config.output == NULL) {
    gchar *tmp;
    tmp = g_strdup_printf("%s-%s.mkv", info->subject, info->session_id);
    g_object_set(G_OBJECT(filesink), "location", tmp, NULL);
    g_free(tmp);
  } else {
    g_object_set(G_OBJECT(filesink), "location", ctx->config.output, NULL);
  }

  webrtc_session_add_element(sess, WEBRTC_SESSION_ELEM_MUX, mux);
  webrtc_session_add_element(sess, WEBRTC_SESSION_ELEM_MUX, filesink);

  webrtc_session_start(sess, TRUE);
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
}

static gboolean
handle_term_signals(struct app_ctx *ctx)
{
  g_assert(ctx);

  g_message("Caught termination signal, shutting down");
  g_main_loop_quit(ctx->loop);

  return FALSE;
}

static void
stop_sessions(G_GNUC_UNUSED gpointer key,
              gpointer value,
              G_GNUC_UNUSED gpointer user_data)
{
  WebrtcSession *sess = WEBRTC_SESSION(value);

  webrtc_session_stop(sess);
}

int
main(int argc, char **argv)
{
  struct app_ctx ctx = { 0 };
  gst_init(&argc, &argv);
  GError *error = NULL;
  GOptionContext *context;

  /* clang-format off */
  GOptionEntry entries[] = {
    { "output", 'o', 0, G_OPTION_ARG_STRING, &ctx.config.output, "Output file", "FILE" },
    { "target", 't', 0, G_OPTION_ARG_STRING, &ctx.config.target, "Target device", "TARGET" },
    { "aac", 'a', 0, G_OPTION_ARG_NONE, &ctx.config.aac, "Use AAC audio encoding", "AAC" },
    G_OPTION_ENTRY_NULL
  };

  /* clang-format on */

  context = g_option_context_new("-run the WebRTC client");
  g_option_context_add_main_entries(context, entries, NULL);
  /** g_option_context_add_group(context, gtk_get_option_group(TRUE));*/
  if (!g_option_context_parse(context, &argc, &argv, &error)) {
    g_print("option parsing failed: %s\n", error->message);
    exit(1);
  }

  ctx.sessions = g_hash_table_new_full(g_str_hash,
                                       g_str_equal,
                                       g_free,
                                       g_object_unref);

  ctx.c = webrtc_client_new(g_getenv("WEBRTC_HOST"),
                            g_getenv("WEBRTC_USER"),
                            g_getenv("WEBRTC_PASS"));

  if (ctx.config.target == NULL) {
    ctx.config.target = g_strdup(g_getenv("WEBRTC_TARGET"));
  }

  if (ctx.config.output == NULL) {
    ctx.config.output = g_strdup(g_getenv("WEBRTC_OUTPUT"));
  }

  if (ctx.config.target == NULL) {
    g_print("Target must be set\n");
    exit(1);
  }

  if (g_ascii_strncasecmp("any", ctx.config.target, 3) != 0) {
    g_clear_pointer(&ctx.config.target, g_free);
    g_message("Waiting for any device to connect");
  }

  g_signal_connect(ctx.c, "new-peer", G_CALLBACK(new_peer), NULL);
  g_signal_connect(ctx.c, "new-stream", G_CALLBACK(on_new_stream), &ctx);
  g_signal_connect(ctx.c, "remove-stream", G_CALLBACK(on_remove_stream), &ctx);

  webrtc_client_connect_async(ctx.c);
  ctx.loop = g_main_loop_new(NULL, FALSE);

  g_unix_signal_add(SIGTERM, G_SOURCE_FUNC(handle_term_signals), &ctx);
  g_unix_signal_add(SIGINT, G_SOURCE_FUNC(handle_term_signals), &ctx);

  g_main_loop_run(ctx.loop);

  g_hash_table_foreach(ctx.sessions, stop_sessions, NULL);

  g_clear_object(&ctx.c);
  g_clear_pointer(&ctx.sessions, g_hash_table_unref);
}
