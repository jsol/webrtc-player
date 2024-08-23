#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>
#include <gst/pbutils/missing-plugins.h>

#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>

#include "webrtc_session.h"

#define RTP_PAYLOAD_TYPE "96"

struct _WebrtcSession {
  GObject parent;

  WebrtcClient *protocol;
  gchar *id;
  gchar *target;
  GPtrArray *video;
  GPtrArray *audio;
  GPtrArray *mux;
  gboolean use_video;
  gboolean use_audio;
  gboolean use_mux;
  gboolean mux_added;
  GstElement *video_sink;
  GstElement *audio_sink;
  GstElement *webrtc_bin;
  GstElement *pipeline;
};

G_DEFINE_TYPE(WebrtcSession, webrtc_session, G_TYPE_OBJECT)

typedef enum {
  PROP_PROTOCOL = 1,
  PROP_ID,
  PROP_TARGET,
  N_PROPERTIES
} WebrtcSessionProperty;
static GParamSpec *obj_properties[N_PROPERTIES] = {
  NULL,
};

static void
on_new_server_list(G_GNUC_UNUSED WebrtcClient *source,
                   const gchar *session_id,
                   GStrv stun,
                   GStrv turn,
                   gpointer user_data)
{
  WebrtcSession *self = WEBRTC_SESSION(user_data);

  if (g_strcmp0(self->id, session_id) != 0) {
    return;
  }
  g_message("Session %s: Setting server lists", session_id);

  if (self->webrtc_bin == NULL) {
    g_warning("Session %s: No gst bin", session_id);
    return;
  }

  if (g_strv_length(stun) > 0) {
    g_object_set(G_OBJECT(self->webrtc_bin), "stun-server", stun[0], NULL);
  }

  for (guint i = 0; turn[i] != NULL; i++) {
    gboolean ret;
    g_signal_emit_by_name(self->webrtc_bin, "add-turn-server", turn[i], &ret);
    if (ret == FALSE) {
      g_warning("Session %s: Failed to set turn server %s",
                session_id,
                turn[i]);
    }
  }
}

static void
on_answer_created(GstPromise *promise, gpointer user_data)
{
  WebrtcSession *self = WEBRTC_SESSION(user_data);
  GstPromiseResult res;
  const GstStructure *reply;
  gchar *sdp_text = NULL;
  GstWebRTCSessionDescription *answer = NULL;

  g_message("Answer created");

  res = gst_promise_wait(promise);
  if (res != GST_PROMISE_RESULT_REPLIED) {
    switch (res) {
    case GST_PROMISE_RESULT_INTERRUPTED:
      g_warning("Session %s: SDP request was interrupted", self->id);
      break;
    case GST_PROMISE_RESULT_EXPIRED:
      g_warning("Session %s: SDP request expired", self->id);
      break;
    default:
      g_error("Session %s: Unknown error while receiving SDP answer", self->id);
      break;
    }

    return;
  }

  reply = gst_promise_get_reply(promise);

  gchar *str = gst_structure_to_string(reply);
  g_message("Answer struct created: %s", str);
  g_free(str);

  gst_structure_get(reply,
                    "answer",
                    GST_TYPE_WEBRTC_SESSION_DESCRIPTION,
                    &answer,
                    NULL);

  if (answer == NULL) {
    g_warning("DID NOT GET A PROPER ANSWER");
    return;
  }
  g_message("Setting answer as local description?");

  g_signal_emit_by_name(self->webrtc_bin,
                        "set-local-description",
                        answer,
                        NULL);

  sdp_text = gst_sdp_message_as_text(answer->sdp);
  g_message("SDP TEXT: %s", sdp_text);

  webrtc_client_send_sdp_answer(self->protocol,
                                self->target,
                                self->id,
                                sdp_text);
  g_free(sdp_text);

  /* TODO: Cleanup? Add ICE candidates here? */
}

static void
on_desc_set(GstPromise *promise, gpointer user_data)
{
  WebrtcSession *self = WEBRTC_SESSION(user_data);
  GstPromiseResult res;
  GstPromise *promise_answer;

  g_message("Description set");

  res = gst_promise_wait(promise);
  if (res != GST_PROMISE_RESULT_REPLIED) {
    switch (res) {
    case GST_PROMISE_RESULT_INTERRUPTED:
      g_warning("Session %s: SDP set was interrupted", self->id);
      break;
    case GST_PROMISE_RESULT_EXPIRED:
      g_warning("Session %s: SDP set expired", self->id);
      break;
    default:
      g_error("Session %s: Unknown error while receiving setting description",
              self->id);
      break;
    }

    return;
  }

  /* Initiate returning an SDP answer */
  g_message("Creating answer");
  promise_answer =
          gst_promise_new_with_change_func(on_answer_created, self, NULL);
  g_signal_emit_by_name(self->webrtc_bin,
                        "create-answer",
                        NULL,
                        promise_answer);
}

static void
on_sdp(G_GNUC_UNUSED WebrtcClient *source,
       const gchar *session_id,
       const gchar *sdp,
       gpointer user_data)
{
  WebrtcSession *self = WEBRTC_SESSION(user_data);
  GstWebRTCSessionDescription *desc;
  GstSDPMessage *msg;
  GstSDPResult res;
  GstPromise *promise;

  if (g_strcmp0(self->id, session_id) != 0) {
    return;
  }

  g_message("Session %s: Setting sdp", session_id);

  if (self->webrtc_bin == NULL) {
    g_warning("Session %s: No gst bin", session_id);
    return;
  }

  res = gst_sdp_message_new_from_text(sdp, &msg);
  if (res != GST_SDP_OK) {
    g_warning("Session %s: Invalid SDP", session_id);
    return;
  }

  promise = gst_promise_new_with_change_func(on_desc_set, self, NULL);
  desc = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_OFFER, msg);
  g_signal_emit_by_name(self->webrtc_bin,
                        "set-remote-description",
                        desc,
                        promise);

  /* TODO: cleanup */
  gst_webrtc_session_description_free(desc);
}

static void
on_new_candidate(G_GNUC_UNUSED WebrtcClient *source,
                 const gchar *session_id,
                 const gchar *candidate,
                 gint mline_index,
                 gpointer user_data)
{
  WebrtcSession *self = WEBRTC_SESSION(user_data);

  if (g_strcmp0(self->id, session_id) != 0) {
    return;
  }
  g_message("Session %s: Adding ice candidate", session_id);

  if (self->webrtc_bin == NULL) {
    g_warning("Session %s: No gst bin", session_id);
    return;
  }

  g_signal_emit_by_name(self->webrtc_bin,
                        "add-ice-candidate",
                        (guint) mline_index,
                        candidate);
  /* TODO: Figure out when to add NULL */
}

static gboolean
bus_call(G_GNUC_UNUSED GstBus *bus, GstMessage *msg, gpointer user_data)
{
  WebrtcSession *self = WEBRTC_SESSION(user_data);

  switch (GST_MESSAGE_TYPE(msg)) {
  case GST_MESSAGE_EOS:
    g_print("End of stream\n");

    break;

  case GST_MESSAGE_LATENCY: {
    g_info("Recalculating latency");
    gst_bin_recalculate_latency(GST_BIN(self->pipeline));

    break;
  }
  case GST_MESSAGE_STATE_CHANGED: {
    GstState old_state, new_state;

    gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);
    g_print("Element %s changed state from %s to %s.\n",
            GST_OBJECT_NAME(msg->src),
            gst_element_state_get_name(old_state),
            gst_element_state_get_name(new_state));
    break;
  }
  case GST_MESSAGE_ERROR: {
    gchar *debug_str = NULL;
    GError *error = NULL;

    gst_message_parse_error(msg, &error, &debug_str);
    g_print("DEBUG: %s\n", debug_str);
    g_free(debug_str);

    g_warning("Error: %s", error->message);
    g_error_free(error);

    break;
  }
  default:
    g_print("GOT MESSAGE %u \n", GST_MESSAGE_TYPE(msg));
    break;
  }

  if (gst_is_missing_plugin_message(msg)) {
    g_print("Missing plugin: %s\t%s\n",
            gst_missing_plugin_message_get_installer_detail(msg),
            gst_missing_plugin_message_get_description(msg));
  }

  return TRUE;
}

static void
on_ice_candidate_callback(G_GNUC_UNUSED GstElement *object,
                          guint mline_index,
                          gchar *candidate,
                          gpointer user_data)
{
  WebrtcSession *self = WEBRTC_SESSION(user_data);

  webrtc_client_send_ice_candidate(self->protocol,
                                   self->target,
                                   self->id,
                                   candidate,
                                   mline_index);
}

static void
on_pad_decodebin_added(G_GNUC_UNUSED GstElement *element,
                       GstPad *pad,
                       gpointer user_data)
{
  GstPad *qpad;
  GstPadLinkReturn ret;
  GstCaps *caps;
  GPtrArray *elems = NULL;
  const gchar *name;
  WebrtcSession *self = WEBRTC_SESSION(user_data);

  if (GST_PAD_DIRECTION(pad) != GST_PAD_SRC) {
    g_message("Pad not a source pad, returning");
  }

  if (!gst_pad_has_current_caps(pad)) {
    g_warning("Pad '%s' has no caps, can't do anything, ignoring\n",
              GST_PAD_NAME(pad));
    return;
  }

  g_message("Linking decode bin with sinks");

  caps = gst_pad_get_current_caps(pad);
  name = gst_structure_get_name(gst_caps_get_structure(caps, 0));

  if (g_str_has_prefix(name, "video")) {
    g_message("Adding video pad");

    if (self->use_video) {
      elems = self->video;
    }
  } else if (g_str_has_prefix(name, "audio")) {
    g_message("Adding audio pad");

    if (self->use_audio) {
      elems = self->audio;
    }
  }

  if (elems != NULL) {
    for (guint i = 0; i < elems->len; i++) {
      gst_bin_add(GST_BIN(self->pipeline), GST_ELEMENT(elems->pdata[i]));
      gst_element_sync_state_with_parent(GST_ELEMENT(elems->pdata[i]));

      if (i > 0) {
        gst_element_link(GST_ELEMENT(elems->pdata[i - 1]),
                         GST_ELEMENT(elems->pdata[i]));
      }
    }

    qpad = gst_element_get_static_pad(GST_ELEMENT(elems->pdata[0]), "sink");

    ret = gst_pad_link(pad, qpad);
    g_assert_cmphex(ret, ==, GST_PAD_LINK_OK);
  }

  if (self->use_mux) {
    elems = self->mux;
    if (!self->mux_added) {
      for (guint i = 0; i < elems->len; i++) {
        gst_bin_add(GST_BIN(self->pipeline), GST_ELEMENT(elems->pdata[i]));
        gst_element_sync_state_with_parent(GST_ELEMENT(elems->pdata[i]));
        if (i > 0) {
          gst_element_link(GST_ELEMENT(elems->pdata[i - 1]),
                           GST_ELEMENT(elems->pdata[i]));
        }
      }
      self->mux_added = TRUE;
    }

    if (g_str_has_prefix(name, "video")) {
      qpad = gst_element_request_pad_simple(GST_ELEMENT(elems->pdata[0]),
                                            "video_%u");
      ret = gst_pad_link(pad, qpad);
      g_assert_cmphex(ret, ==, GST_PAD_LINK_OK);
    } else if (g_str_has_prefix(name, "audio")) {
      qpad = gst_element_request_pad_simple(GST_ELEMENT(elems->pdata[0]),
                                            "audio_%u");
      ret = gst_pad_link(pad, qpad);
      g_assert_cmphex(ret, ==, GST_PAD_LINK_OK);
    }
  }

  g_object_set(self->webrtc_bin, "latency", 200, NULL);
  gst_bin_recalculate_latency(GST_BIN(self->pipeline));


}

static void
data_channel_on_error(G_GNUC_UNUSED GObject *dc,
                      G_GNUC_UNUSED gpointer user_data)
{
  g_warning("Data channel error");
}

static void
data_channel_on_open(GObject *dc, G_GNUC_UNUSED gpointer user_data)
{
  GBytes *bytes = g_bytes_new("data", strlen("data"));
  g_message("data channel opened\n");
  g_signal_emit_by_name(dc, "send-string", "Hi! from GStreamer");
  g_signal_emit_by_name(dc, "send-data", bytes);
  g_bytes_unref(bytes);
}

static void
data_channel_on_close(G_GNUC_UNUSED GObject *dc,
                      G_GNUC_UNUSED gpointer user_data)
{
  g_message("Data channel closed");
}

static void
data_channel_on_message_string(G_GNUC_UNUSED GObject *dc,
                               gchar *str,
                               G_GNUC_UNUSED gpointer user_data)
{
  gst_print("Received data channel message: %s\n", str);
}

static void
on_data_channel(G_GNUC_UNUSED GstElement *webrtc,
                GObject *data_channel,
                G_GNUC_UNUSED gpointer user_data)
{
  g_signal_connect(data_channel,
                   "on-error",
                   G_CALLBACK(data_channel_on_error),
                   NULL);
  g_signal_connect(data_channel,
                   "on-open",
                   G_CALLBACK(data_channel_on_open),
                   NULL);
  g_signal_connect(data_channel,
                   "on-close",
                   G_CALLBACK(data_channel_on_close),
                   NULL);
  g_signal_connect(data_channel,
                   "on-message-string",
                   G_CALLBACK(data_channel_on_message_string),
                   NULL);
}

static void
on_pad_added(G_GNUC_UNUSED GstElement *element, GstPad *pad, gpointer user_data)
{
  GstElement *decodebin;
  GstPad *sinkpad;
  WebrtcSession *self = WEBRTC_SESSION(user_data);

  if (GST_PAD_DIRECTION(pad) != GST_PAD_SRC) {
    g_message("Pad not a source pad, returning");
  }

  g_message("Linking decode bin");

  decodebin = gst_element_factory_make("decodebin", NULL);
  g_signal_connect(decodebin,
                   "pad-added",
                   G_CALLBACK(on_pad_decodebin_added),
                   self);
  gst_bin_add(GST_BIN(self->pipeline), decodebin);
  gst_element_sync_state_with_parent(decodebin);

  sinkpad = gst_element_get_static_pad(decodebin, "sink");
  gst_pad_link(pad, sinkpad);
  gst_object_unref(sinkpad);
}

static void
webrtc_session_dispose(GObject *obj)
{
  WebrtcSession *self = WEBRTC_SESSION(obj);

  g_assert(self);

  /* Do unrefs of objects and such. The object might be used after dispose,
   * and dispose might be called several times on the same object
   */

  /* Always chain up to the parent dispose function to complete object
   * destruction. */
  G_OBJECT_CLASS(webrtc_session_parent_class)->dispose(obj);
}

static void
webrtc_session_finalize(GObject *obj)
{
  WebrtcSession *self = WEBRTC_SESSION(obj);

  g_assert(self);

  /* free stuff */

  g_free(self->protocol);

  g_free(self->id);

  /* Always chain up to the parent finalize function to complete object
   * destruction. */
  G_OBJECT_CLASS(webrtc_session_parent_class)->finalize(obj);
}

static void
get_property(GObject *object,
             guint property_id,
             GValue *value,
             GParamSpec *pspec)
{
  WebrtcSession *self = WEBRTC_SESSION(object);

  switch ((WebrtcSessionProperty) property_id) {
  case PROP_PROTOCOL:
    g_value_set_object(value, self->protocol);
    break;

  case PROP_ID:
    g_value_set_string(value, self->id);
    break;

  case PROP_TARGET:
    g_value_set_string(value, self->target);
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
  WebrtcSession *self = WEBRTC_SESSION(object);

  switch ((WebrtcSessionProperty) property_id) {
  case PROP_PROTOCOL:
    g_clear_object(&self->protocol);
    self->protocol = g_value_dup_object(value);

    g_signal_connect(self->protocol,
                     "new-candidate",
                     G_CALLBACK(on_new_candidate),
                     self);

    g_signal_connect(self->protocol, "sdp", G_CALLBACK(on_sdp), self);

    g_signal_connect(self->protocol,
                     "new-server-lists",
                     G_CALLBACK(on_new_server_list),
                     self);
    break;
  case PROP_ID:
    g_free(self->id);
    self->id = g_value_dup_string(value);
    break;

  case PROP_TARGET:
    g_free(self->target);
    self->target = g_value_dup_string(value);
    break;

  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    break;
  }
}

static void
webrtc_session_class_init(WebrtcSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = webrtc_session_dispose;
  object_class->finalize = webrtc_session_finalize;
  object_class->set_property = set_property;
  object_class->get_property = get_property;

  obj_properties[PROP_PROTOCOL] =
          g_param_spec_object("protocol",
                              "Protocol",
                              "Placeholder description.",
                              WEBRTC_TYPE_CLIENT, /* default */
                              G_PARAM_READWRITE);

  obj_properties[PROP_ID] = g_param_spec_string("id",
                                                "Id",
                                                "Placeholder description.",
                                                NULL, /* default */
                                                G_PARAM_READWRITE);

  obj_properties[PROP_TARGET] = g_param_spec_string("target",
                                                    "Target",
                                                    "Placeholder description.",
                                                    NULL, /* default */
                                                    G_PARAM_READWRITE);

  g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

static void
webrtc_session_init(G_GNUC_UNUSED WebrtcSession *self)
{
  /* initialize all public and private members to reasonable default values.
   * They are all automatically initialized to 0 to begin with. */

  self->audio = g_ptr_array_new_full(0, g_object_unref);
  self->video = g_ptr_array_new_full(0, g_object_unref);
  self->mux = g_ptr_array_new_full(0, g_object_unref);

  g_ptr_array_add(self->video, gst_element_factory_make("queue", NULL));
  g_ptr_array_add(self->video, gst_element_factory_make("videoconvert", NULL));

  g_ptr_array_add(self->audio, gst_element_factory_make("queue", NULL));
  g_ptr_array_add(self->audio, gst_element_factory_make("audioconvert", NULL));
  g_ptr_array_add(self->audio, gst_element_factory_make("audioresample", NULL));
}

WebrtcSession *
webrtc_session_new(WebrtcClient *protocol, const gchar *id, const gchar *target)
{
  return g_object_new(WEBRTC_TYPE_SESSION,
                      "protocol",
                      protocol,
                      "id",
                      id,
                      "target",
                      target,
                      NULL);
}

void
webrtc_session_add_element(WebrtcSession *self,
                           enum webrtc_session_elem_type type,
                           GstElement *el)
{
  switch (type) {
  case WEBRTC_SESSION_ELEM_VIDEO:
    g_ptr_array_add(self->video, el);
    self->use_video = TRUE;
    break;
  case WEBRTC_SESSION_ELEM_AUDIO:
    g_ptr_array_add(self->audio, el);
    self->use_audio = TRUE;
    break;
  case WEBRTC_SESSION_ELEM_MUX:
    g_ptr_array_add(self->mux, el);
    self->use_mux = TRUE;
    break;
  default:
    g_warning("Invalid element type!");
  }
}

static void
set_transceiver(WebrtcSession *self)
{
  GstCaps *video_caps;
  GstCaps *audio_caps;
  GstWebRTCRTPTransceiver *trans = NULL;

  g_assert(self);

  video_caps = gst_caps_from_string(
          "application/x-rtp,media=video,encoding-name=H264,payload=" RTP_PAYLOAD_TYPE);
  g_signal_emit_by_name(self->webrtc_bin,
                        "add-transceiver",
                        GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY,
                        video_caps,
                        &trans);
  gst_caps_unref(video_caps);
  g_object_set(trans, "do-nack", TRUE, NULL);

  gst_object_unref(trans);

  audio_caps = gst_caps_from_string(
          "x-rtp,media=audio,encoding-name=OPUS,"
          "clock-rate=48000,payload=97,encoding-params=(string)2");
  g_signal_emit_by_name(self->webrtc_bin,
                        "add-transceiver",
                        GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY,
                        audio_caps,
                        &trans);
  gst_caps_unref(audio_caps);
  g_object_set(trans, "do-nack", TRUE, NULL);
  gst_object_unref(trans);
}

void
webrtc_session_start(WebrtcSession *self)
{
  GstElement *pipeline;
  GstBus *bus;

  // GstElement *audio_demuxer, *audio_decoder, *audio_conv, *audio_sink;
  // GstCaps *videoscalecaps;

  // guint bus_watch_id;

  /* Create gstreamer elements */
  pipeline = gst_pipeline_new("video-player");
  self->pipeline = pipeline;
  self->webrtc_bin = gst_element_factory_make("webrtcbin", "video-source");

  g_object_set(self->webrtc_bin,
               "bundle-policy",
               GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE,
               "latency",
               10000,
               NULL);


  // conv = gst_element_factory_make("videoconvert", "converter");
  // sink = gst_element_factory_make("gtk4paintablesink", "video-output");

  /*
  videoscale = gst_element_factory_make("capsfilter", "videoscale");

  videoscalecaps = gst_caps_new_simple("video/x-raw",
                                       "width",
                                       G_TYPE_INT,
                                       1200, // GetWidth(),
                                       "height",
                                       G_TYPE_INT,
                                       720, // GetHeight(),
                                       "format",
                                       G_TYPE_STRING,
                                       "RGB",
                                       NULL);
   g_object_set(G_OBJECT(videoscale), "caps", videoscalecaps, NULL);
*/

  if (!pipeline || !self->webrtc_bin) {
    g_printerr("One element could not be created. Exiting.\n");

    return;
  }

  g_signal_connect(self->webrtc_bin,
                   "on-ice-candidate",
                   G_CALLBACK(on_ice_candidate_callback),
                   self);

  bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
  gst_bus_add_watch(bus, bus_call, self);
  gst_object_unref(bus);

  set_transceiver(self);

  gst_bin_add(GST_BIN(pipeline), self->webrtc_bin);

  /* Iterate */
  g_print("Running...\n");

  gst_element_set_state(self->pipeline, GST_STATE_READY);
  g_signal_connect(self->webrtc_bin,
                   "on-data-channel",
                   G_CALLBACK(on_data_channel),
                   NULL);

  g_signal_connect(self->webrtc_bin,
                   "pad-added",
                   G_CALLBACK(on_pad_added),
                   self);

  webrtc_client_init_session(self->protocol, self->target, self->id);
  gst_element_set_state(self->pipeline, GST_STATE_PLAYING);
}

gboolean
webrtc_session_check_plugins(void)
{
  gboolean ret = TRUE;
  GstPlugin *plugin;
  GstRegistry *registry;

  const gchar *needed[] = { "opus", "vpx",  "nice",       "webrtc", "dtls",
                            "srtp", "sctp", "rtpmanager", NULL };

  registry = gst_registry_get();
  for (guint i = 0; i < g_strv_length((gchar **) needed); i++) {
    g_message("Checking %s", needed[i]);
    plugin = gst_registry_find_plugin(registry, needed[i]);
    if (!plugin) {
      gst_print("Required gstreamer plugin '%s' not found\n", needed[i]);
      ret = FALSE;
      continue;
    }
    gst_object_unref(plugin);
  }

  return ret;
}
