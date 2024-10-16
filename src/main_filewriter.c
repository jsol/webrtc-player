#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>
#include <glib-unix.h>

#include "webrtc_client.h"
#include "webrtc_session.h"
#include "webrtc_settings.h"

struct app_ctx {
  WebrtcClient *c;
  GHashTable *sessions;
  GMainLoop *loop;
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

static void
on_new_stream(G_GNUC_UNUSED GObject *source,
              struct stream_started *info,
              struct app_ctx *ctx)
{
  GstElement *mux;
  GstElement *filesink;
  WebrtcSession *sess;
  const gchar *target;
  const gchar *output;

  g_print("New stream: %p, %p\n", info, ctx);

  target = webrtc_settings_get_target(ctx->settings);
  output = webrtc_settings_get_output(ctx->settings);
  if (target != NULL &&
      g_ascii_strncasecmp(info->subject, target, strlen(target)) != 0) {
    g_message("Device %s connected, waiting for %s", info->subject, target);
    return;
  }

  sess = webrtc_session_new(ctx->c,
                            ctx->settings,
                            info->session_id,
                            info->subject);
  g_hash_table_insert(ctx->sessions, g_strdup(info->session_id), sess);

  mux = gst_element_factory_make("matroskamux", "mux");
  filesink = gst_element_factory_make("filesink", "filesink");

  g_object_set(G_OBJECT(mux), "streamable", TRUE, NULL);

  if (output == NULL) {
    gchar *tmp;
    tmp = g_strdup_printf("%s-%s.mkv", info->subject, info->session_id);
    g_object_set(G_OBJECT(filesink), "location", tmp, NULL);
    g_free(tmp);
  } else {
    g_object_set(G_OBJECT(filesink), "location", output, NULL);
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
  gint code;

  ctx.settings = webrtc_settings_new();

  code = webrtc_settings_parse_opts(ctx.settings, argc, argv);
  if (code >= 0) {
    goto out;
  }

  ctx.sessions = g_hash_table_new_full(g_str_hash,
                                       g_str_equal,
                                       g_free,
                                       g_object_unref);

  ctx.c = webrtc_client_new(g_getenv("WEBRTC_HOST"),
                            g_getenv("WEBRTC_USER"),
                            g_getenv("WEBRTC_PASS"));

  g_signal_connect(ctx.c, "new-peer", G_CALLBACK(new_peer), NULL);
  g_signal_connect(ctx.c, "new-stream", G_CALLBACK(on_new_stream), &ctx);
  g_signal_connect(ctx.c, "remove-stream", G_CALLBACK(on_remove_stream), &ctx);

  webrtc_client_connect_async(ctx.c);
  ctx.loop = g_main_loop_new(NULL, FALSE);

  g_unix_signal_add(SIGTERM, G_SOURCE_FUNC(handle_term_signals), &ctx);
  g_unix_signal_add(SIGINT, G_SOURCE_FUNC(handle_term_signals), &ctx);

  g_main_loop_run(ctx.loop);

  g_hash_table_foreach(ctx.sessions, stop_sessions, NULL);

out:
  g_clear_object(&ctx.c);
  g_clear_pointer(&ctx.sessions, g_hash_table_unref);

  return code;
}
