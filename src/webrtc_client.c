#include <glib.h>
#include <glib-object.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>
#include <gio/gio.h>

#include "webrtc_client.h"
#include "messages.h"

#define URL_PATH_AUTH    "local/BodyWornLiveStandalone/auth.cgi"
#define URL_PATH_WSS     "local/BodyWornLiveStandalone/client"
#define URL_PATH_TARGETS "local/BodyWornLiveStandalone/status.cgi"
#define URL_PATH_STREAM  "vapix/ws-data-stream?sources=events"

#define POST_DATA_KEY     "post-data"
#define SEND_FUNC_KEY     "send-func"
#define INCOMING_FUNC_KEY "inc-func"

#define AUTH_REQ_BODY                                                          \
  "{\"apiVersion\":\"1.0\","                                                   \
  "\"method\":\"getSignalingClientToken\","                                    \
  "\"params\":{}}"

#define TARGETS_REQ_BODY                                                       \
  "{\"apiVersion\":\"1.0\",\"method\":\"getTargets\",\"params\":{}}"

#define CONCAT(A, B, C) A##B##C
#define JSON_GET_(KIND) CONCAT(json_object_get_, KIND, _member)
#define JSON_SET_(KIND) CONCAT(json_object_set_, KIND, _member)
#define JSON_COPY_OBJ(from, to, member, KIND)                                  \
  JSON_SET_(KIND)(to, member, JSON_GET_(KIND)(from, member));

struct _WebrtcClient {
  GObject parent;

  SoupSession *session;
  SoupWebsocketConnection *client;
  SoupWebsocketConnection *data_stream;
  GQueue *client_queue;
  GCancellable *cancel;
  gchar *server;
  gchar *user;
  gchar *pass;
  gchar *token;
};

G_DEFINE_TYPE(WebrtcClient, webrtc_client, G_TYPE_OBJECT)

enum client_signals {
  SIG_SDP = 0,
  SIG_NEW_PEER,
  SIG_NEW_STREAM,
  SIG_SERVER_LIST,
  SIG_NEW_CANDIDATE,
  SIG_LAST
};
static guint client_signal_defs[SIG_LAST] = { 0 };

typedef enum {
  PROP_SERVER = 1,
  PROP_USER,
  PROP_PASS,
  PROP_TOKEN,
  N_PROPERTIES
} WebrtcClientProperty;

static GParamSpec *obj_properties[N_PROPERTIES] = {
  NULL,
};

static gboolean
accept_certificate_callback(G_GNUC_UNUSED SoupMessage *msg,
                            G_GNUC_UNUSED GTlsCertificate *certificate,
                            G_GNUC_UNUSED GTlsCertificateFlags tls_errors,
                            G_GNUC_UNUSED gpointer user_data);

static gboolean authenticate_callback(G_GNUC_UNUSED SoupMessage *msg,
                                      SoupAuth *auth,
                                      gboolean retrying,
                                      gpointer user_data);

static void restarted_callback(SoupMessage *msg, gpointer user_data);

static void
on_text_message(SoupWebsocketConnection *ws,
                G_GNUC_UNUSED SoupWebsocketDataType datatype,
                GBytes *message,
                gpointer user_data)
{
  WebrtcClient *self = WEBRTC_CLIENT(user_data);
  message_t *msg;
  gchar *reply;
  GError *lerr = NULL;

  msg = message_parse(message, &lerr);

  if (msg == NULL) {
    g_warning("Error parsing message: %s",
              lerr ? lerr->message : "No error message");
    g_clear_error(&lerr);
    return;
  }

  switch (msg->type) {
  case MSG_TYPE_RESPONSE:
    g_message("Response received");
    break;
  case MSG_TYPE_HELLO: {
    gchar *msg;

    while ((msg = g_queue_pop_head(self->client_queue)) != NULL) {
      soup_websocket_connection_send_text(self->client, msg);
      g_free(msg);
    }

    g_message("Hello received");
  }

  break;
  case MSG_TYPE_PEER_CONNECTED:
    g_signal_emit(self,
                  client_signal_defs[SIG_NEW_PEER],
                  0,
                  msg->data.connected.subject,
                  msg->data.connected.subject);
    break;
  case MSG_TYPE_SDP_OFFER:
    g_signal_emit(self,
                  client_signal_defs[SIG_SDP],
                  0,
                  msg->session_id,
                  msg->data.sdp_offer.sdp);
    break;
  case MSG_TYPE_STREAM_STARTED: {
    struct stream_started info = { 0 };
    info.bearer_id = msg->data.new_stream.bearer_id;
    info.bearer_name = msg->data.new_stream.bearer_name;
    info.recording_id = msg->data.new_stream.recording_id;
    info.session_id = msg->data.new_stream.session_id;
    info.source = msg->data.new_stream.source;
    info.subject = msg->data.new_stream.subject;
    info.system_id = msg->data.new_stream.system_id;
    info.time = msg->data.new_stream.time;
    info.trigger_type = msg->data.new_stream.trigger_type;
    g_signal_emit(self, client_signal_defs[SIG_NEW_STREAM], 0, &info);
    break;
  }

  case MSG_TYPE_ICE_CANDIDATE:
    g_signal_emit(self,
                  client_signal_defs[SIG_NEW_CANDIDATE],
                  0,
                  msg->session_id,
                  msg->data.ice_candidate.candidate,
                  msg->data.ice_candidate.index);
    break;

  case MSG_TYPE_INIT_SESSION:
    g_signal_emit(self,
                  client_signal_defs[SIG_SERVER_LIST],
                  0,
                  msg->session_id,
                  msg->data.init_session.stun_servers,
                  msg->data.init_session.turn_servers);
    break;
  }

  reply = message_create_reply(msg, self->token);

  /* use ws instead of self so that the reply goes back on the channel it came
   * in */
  if (reply != NULL) {
    g_message("Sending reply %s", reply);
    soup_websocket_connection_send_text(ws, reply);
  }

  message_free(msg);
  g_free(reply);

  // send_client_reply(self, obj);
}

static void
send_hello(WebrtcClient *self)
{
  gchar *msg;

  g_assert(self);

  msg = message_create_hello(self->token);

  g_message("Sending msg %s", msg);
  soup_websocket_connection_send_text(self->client, msg);
  g_free(msg);
}

static void
send_data_stream_filter(WebrtcClient *self)
{
  gchar *msg;

  g_assert(self);

  msg = message_create_stream_filter();
  g_message("Sending msg %s", msg);
  soup_websocket_connection_send_text(self->data_stream, msg);
  g_free(msg);
}

static void
client_connection_cb(GObject *source, GAsyncResult *res, gpointer user_data)
{
  SoupSession *session = SOUP_SESSION(source);
  WebrtcClient *self = WEBRTC_CLIENT(user_data);
  GError *err = NULL;

  self->client = soup_session_websocket_connect_finish(session, res, &err);

  if (self->client == NULL) {
    g_error("Error connecting: %s",
            err != NULL ? err->message : "No error message");
    g_clear_error(&err);
    return;
  }
  g_message("Connection to client received");

  g_signal_connect(self->client, "message", G_CALLBACK(on_text_message), self);
  send_hello(self);

  /* TODO: add disconnect / error handler*/
}

static void
data_stream_connection_cb(GObject *source,
                          GAsyncResult *res,
                          gpointer user_data)
{
  SoupSession *session = SOUP_SESSION(source);
  WebrtcClient *self = WEBRTC_CLIENT(user_data);
  GError *err = NULL;

  self->data_stream = soup_session_websocket_connect_finish(session, res, &err);

  if (self->data_stream == NULL) {
    g_error("Error connecting: %s",
            err != NULL ? err->message : "No error message");
    g_clear_error(&err);
    return;
  }
  g_message("Connection to data stream received");

  g_signal_connect(self->data_stream,
                   "message",
                   G_CALLBACK(on_text_message),
                   self);
  send_data_stream_filter(self);

  /* TODO: add disconnect / error handler*/
}

static void
init_socket_connection(WebrtcClient *self,
                       const gchar *uri,
                       GAsyncReadyCallback on_connection)
{
  g_assert(self);

  SoupMessage *msg = soup_message_new(SOUP_METHOD_GET, uri);

  g_signal_connect(msg,
                   "accept-certificate",
                   G_CALLBACK(accept_certificate_callback),
                   NULL);

  g_signal_connect(msg,
                   "authenticate",
                   G_CALLBACK(authenticate_callback),
                   self);

  g_signal_connect(msg, "restarted", G_CALLBACK(restarted_callback), self);

  soup_session_websocket_connect_async(self->session,
                                       msg,
                                       NULL,
                                       NULL,
                                       G_PRIORITY_DEFAULT,
                                       self->cancel,
                                       on_connection,
                                       self);
}

static void
restarted_callback(SoupMessage *msg, gpointer user_data)
{
  WebrtcClient *self = WEBRTC_CLIENT(user_data);
  GBytes *body;

  g_assert(self); /** Only if it is a post auth request... */

  body = (GBytes *) g_object_get_data(G_OBJECT(msg), POST_DATA_KEY);

  if (body != NULL) {
    soup_message_set_request_body_from_bytes(msg, "application/json", body);
  }
}

static gboolean
authenticate_callback(G_GNUC_UNUSED SoupMessage *msg,
                      SoupAuth *auth,
                      gboolean retrying,
                      gpointer user_data)
{
  WebrtcClient *self = WEBRTC_CLIENT(user_data);

  g_assert(self);

  if (retrying) {
    g_message("Is retrying");
    // Maybe don't try again if our password failed
    return FALSE;
  }

  soup_auth_authenticate(auth, self->user, self->pass);
  g_message("Is NOT retrying");

  // Returning TRUE means we have or *will* handle it.
  // soup_auth_authenticate() or soup_auth_cancel() can be called later
  // for example after showing a prompt to the user or loading the password
  // from a keyring.
  return TRUE;
}

static gboolean
accept_certificate_callback(G_GNUC_UNUSED SoupMessage *msg,
                            G_GNUC_UNUSED GTlsCertificate *certificate,
                            G_GNUC_UNUSED GTlsCertificateFlags tls_errors,
                            G_GNUC_UNUSED gpointer user_data)
{
  return TRUE;
}

static void
parse_target(G_GNUC_UNUSED JsonArray *array,
             G_GNUC_UNUSED guint index_,
             JsonNode *element_node,
             gpointer user_data)
{
  WebrtcClient *self = user_data;
  JsonObject *obj;
  struct stream_started info = { 0 };

  obj = json_node_get_object(element_node);

  if (!json_object_has_member(obj, "sessionId")) {
    return;
  }

  if (json_object_has_member(obj, "stopped") ||
      json_object_has_member(obj, "disconnected")) {
    return;
  }

  info.bearer_id = json_object_get_string_member(obj, "bearerId");
  info.bearer_name = json_object_get_string_member(obj, "bearerName");
  info.session_id = json_object_get_string_member(obj, "sessionId");
  info.subject = json_object_get_string_member(obj, "id");
  info.time = json_object_get_string_member(obj, "started");

  g_signal_emit(self, client_signal_defs[SIG_NEW_STREAM], 0, &info);
}

static void
on_targets_callback(GObject *source, GAsyncResult *result, gpointer user_data)
{
  WebrtcClient *self = user_data;
  GError *lerr = NULL;
  GBytes *bytes;
  JsonParser *parser = NULL;
  JsonNode *root;
  JsonObject *obj;
  JsonObject *data;

  g_assert(self);

  g_message("Receiving targets list");

  bytes = soup_session_send_and_read_finish(SOUP_SESSION(source),
                                            result,
                                            &lerr);

  if (bytes == NULL) {
    g_warning("Error retrieving targets: %s",
              lerr ? lerr->message : "No error message");
    g_clear_error(&lerr);
    goto out;
  }

  parser = json_parser_new();

  if (!json_parser_load_from_data(parser,
                                  g_bytes_get_data(bytes, NULL),
                                  g_bytes_get_size(bytes),
                                  &lerr)) {
    gchar *msg;
    msg = g_strndup(g_bytes_get_data(bytes, NULL), g_bytes_get_size(bytes));
    g_warning("Could not parse incoming message %s: %s",
              msg,
              lerr != NULL ? lerr->message : "No error message");
    g_clear_error(&lerr);
    g_free(msg);
    goto out;
  }

  root = json_parser_get_root(parser);
  obj = json_node_get_object(root);

  if (!json_object_has_member(obj, "data")) {
    gchar *msg;
    msg = g_strndup(g_bytes_get_data(bytes, NULL), g_bytes_get_size(bytes));

    g_warning("Message does not contain type data: %s", msg);
    g_free(msg);
    goto out;
  }

  data = json_object_get_object_member(obj, "data");

  if (!json_object_has_member(data, "targets")) {
    g_warning("No target array");
    goto out;
  }

  json_array_foreach_element(json_object_get_array_member(data, "targets"),
                             parse_target,
                             self);

  /* Fall through */
out:
  /* parser owns all the JSON objects and nodes */

  g_clear_object(&parser);
  g_clear_pointer(&bytes, g_bytes_unref);
  g_object_unref(self);
}

static void
get_online_streams(WebrtcClient *self)
{
  g_assert(self);
  gchar *url;
  GBytes *body;

  g_message("Fetching online streams");

  url = g_strdup_printf("https://%s/%s", self->server, URL_PATH_TARGETS);
  body = g_bytes_new(TARGETS_REQ_BODY, strlen(TARGETS_REQ_BODY));
  SoupMessage *msg = soup_message_new(SOUP_METHOD_POST, url);

  g_object_set_data_full(G_OBJECT(msg),
                         POST_DATA_KEY,
                         body,
                         (GDestroyNotify) g_bytes_unref);
  soup_message_set_request_body_from_bytes(msg, "application/json", body);
  g_signal_connect(msg,
                   "accept-certificate",
                   G_CALLBACK(accept_certificate_callback),
                   NULL);

  g_signal_connect(msg,
                   "authenticate",
                   G_CALLBACK(authenticate_callback),
                   self);

  g_signal_connect(msg, "restarted", G_CALLBACK(restarted_callback), self);

  soup_session_send_and_read_async(self->session,
                                   msg,
                                   G_PRIORITY_DEFAULT,
                                   self->cancel,
                                   on_targets_callback,
                                   g_object_ref(self));
}

static void
on_auth_callback(GObject *source, GAsyncResult *result, gpointer user_data)
{
  WebrtcClient *self = user_data;
  GError *lerr = NULL;
  GBytes *bytes;
  JsonParser *parser = NULL;
  JsonNode *root;
  JsonObject *obj;
  JsonObject *data;
  gchar *uri;

  g_assert(self);

  bytes = soup_session_send_and_read_finish(SOUP_SESSION(source),
                                            result,
                                            &lerr);

  if (bytes == NULL) {
    g_warning("Error retrieving token: %s",
              lerr ? lerr->message : "No error message");
    g_clear_error(&lerr);
    goto out;
  }

  parser = json_parser_new();

  if (!json_parser_load_from_data(parser,
                                  g_bytes_get_data(bytes, NULL),
                                  g_bytes_get_size(bytes),
                                  &lerr)) {
    gchar *msg;
    msg = g_strndup(g_bytes_get_data(bytes, NULL), g_bytes_get_size(bytes));
    g_warning("Could not parse incoming message %s: %s",
              msg,
              lerr != NULL ? lerr->message : "No error message");
    g_clear_error(&lerr);
    g_free(msg);
    goto out;
  }

  root = json_parser_get_root(parser);
  obj = json_node_get_object(root);

  if (!json_object_has_member(obj, "data")) {
    gchar *msg;
    msg = g_strndup(g_bytes_get_data(bytes, NULL), g_bytes_get_size(bytes));

    g_warning("Message does not contain type data: %s", msg);
    g_free(msg);
    goto out;
  }

  data = json_object_get_object_member(obj, "data");

  g_object_set(self,
               "token",
               json_object_get_string_member(data, "token"),
               NULL);
  g_message("Got auth token %s", self->token);

  g_message("Expires at %s", json_object_get_string_member(data, "expiresAt"));

  uri = g_strdup_printf("wss://%s/%s?authorization=%s",
                        self->server,
                        URL_PATH_WSS,
                        self->token); /* TODO: Url-secure the token */
  init_socket_connection(self, uri, client_connection_cb);

  get_online_streams(self);

  /* Fall through */
out:
  /* parser owns all the JSON objects and nodes */
  g_free(uri);
  g_clear_object(&parser);
  g_clear_pointer(&bytes, g_bytes_unref);
  g_object_unref(self);
}

static void
get_auth(WebrtcClient *self)
{
  g_assert(self);
  gchar *url;
  GBytes *body;

  url = g_strdup_printf("https://%s/%s", self->server, URL_PATH_AUTH);
  body = g_bytes_new(AUTH_REQ_BODY, strlen(AUTH_REQ_BODY));
  SoupMessage *msg = soup_message_new(SOUP_METHOD_POST, url);

  g_object_set_data_full(G_OBJECT(msg),
                         POST_DATA_KEY,
                         body,
                         (GDestroyNotify) g_bytes_unref);
  soup_message_set_request_body_from_bytes(msg, "application/json", body);
  g_signal_connect(msg,
                   "accept-certificate",
                   G_CALLBACK(accept_certificate_callback),
                   NULL);

  g_signal_connect(msg,
                   "authenticate",
                   G_CALLBACK(authenticate_callback),
                   self);

  g_signal_connect(msg, "restarted", G_CALLBACK(restarted_callback), self);

  soup_session_send_and_read_async(self->session,
                                   msg,
                                   G_PRIORITY_DEFAULT,
                                   self->cancel,
                                   on_auth_callback,
                                   g_object_ref(self));
}

static void
webrtc_client_dispose(GObject *obj)
{
  WebrtcClient *self = WEBRTC_CLIENT(obj);

  g_assert(self);

  /* Do unrefs of objects and such. The object might be used after dispose,
   * and dispose might be called several times on the same object
   */

  /* Always chain up to the parent dispose function to complete object
   * destruction. */
  G_OBJECT_CLASS(webrtc_client_parent_class)->dispose(obj);
}

static void
webrtc_client_finalize(GObject *obj)
{
  WebrtcClient *self = WEBRTC_CLIENT(obj);

  g_assert(self);

  g_free(self->token);
  g_free(self->user);
  g_free(self->pass);
  g_free(self->server);
  g_queue_free_full(self->client_queue, g_free);

  /* free stuff */

  /* Always chain up to the parent finalize function to complete object
   * destruction. */
  G_OBJECT_CLASS(webrtc_client_parent_class)->finalize(obj);
}

static void
get_property(GObject *object,
             guint property_id,
             GValue *value,
             GParamSpec *pspec)
{
  WebrtcClient *self = WEBRTC_CLIENT(object);

  switch ((WebrtcClientProperty) property_id) {
  case PROP_SERVER:
    g_value_set_string(value, self->server);
    break;

  case PROP_USER:
    g_value_set_string(value, self->user);
    break;

  case PROP_PASS:
    g_value_set_string(value, self->pass);
    break;

  case PROP_TOKEN:
    g_value_set_string(value, self->token);
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
  WebrtcClient *self = WEBRTC_CLIENT(object);

  switch ((WebrtcClientProperty) property_id) {
  case PROP_SERVER:
    g_free(self->server);
    self->server = g_value_dup_string(value);
    break;
  case PROP_USER:
    g_free(self->user);
    self->user = g_value_dup_string(value);
    break;
  case PROP_PASS:
    g_free(self->pass);
    self->pass = g_value_dup_string(value);
    break;
  case PROP_TOKEN:
    g_free(self->token);
    self->token = g_value_dup_string(value);
    break;
  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    break;
  }
}

static void
webrtc_client_class_init(WebrtcClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = webrtc_client_dispose;
  object_class->finalize = webrtc_client_finalize;
  object_class->set_property = set_property;
  object_class->get_property = get_property;

  GType sdp_types[] = { G_TYPE_STRING, G_TYPE_STRING };
  client_signal_defs[SIG_SDP] =
          g_signal_newv("sdp",
                        G_TYPE_FROM_CLASS(object_class),
                        G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE |
                                G_SIGNAL_NO_HOOKS,
                        NULL /* closure */,
                        NULL /* accumulator */,
                        NULL /* accumulator data */,
                        NULL /* C marshaller */,
                        G_TYPE_NONE /* return_type */,
                        G_N_ELEMENTS(sdp_types) /* n_params */,
                        sdp_types /* param_types, or set to NULL */
          );

  GType new_peer_types[] = { G_TYPE_STRING, G_TYPE_STRING };
  client_signal_defs[SIG_NEW_PEER] =
          g_signal_newv("new-peer",
                        G_TYPE_FROM_CLASS(object_class),
                        G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE |
                                G_SIGNAL_NO_HOOKS,
                        NULL /* closure */,
                        NULL /* accumulator */,
                        NULL /* accumulator data */,
                        NULL /* C marshaller */,
                        G_TYPE_NONE /* return_type */,
                        G_N_ELEMENTS(new_peer_types) /* n_params */,
                        new_peer_types /* param_types, or set to NULL */
          );

  GType new_stream_types[] = { G_TYPE_POINTER };
  client_signal_defs[SIG_NEW_STREAM] =
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

  GType new_server_list_types[] = { G_TYPE_STRING,
                                    G_TYPE_POINTER,
                                    G_TYPE_POINTER };
  client_signal_defs[SIG_SERVER_LIST] =
          g_signal_newv("new-server-lists",
                        G_TYPE_FROM_CLASS(object_class),
                        G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE |
                                G_SIGNAL_NO_HOOKS,
                        NULL /* closure */,
                        NULL /* accumulator */,
                        NULL /* accumulator data */,
                        NULL /* C marshaller */,
                        G_TYPE_NONE /* return_type */,
                        G_N_ELEMENTS(new_server_list_types) /* n_params */,
                        new_server_list_types /* param_types, or set to NULL */
          );

  GType new_candidate_types[] = { G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT };
  client_signal_defs[SIG_NEW_CANDIDATE] =
          g_signal_newv("new-candidate",
                        G_TYPE_FROM_CLASS(object_class),
                        G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE |
                                G_SIGNAL_NO_HOOKS,
                        NULL /* closure */,
                        NULL /* accumulator */,
                        NULL /* accumulator data */,
                        NULL /* C marshaller */,
                        G_TYPE_NONE /* return_type */,
                        G_N_ELEMENTS(new_candidate_types) /* n_params */,
                        new_candidate_types /* param_types, or set to NULL */
          );

  obj_properties[PROP_SERVER] = g_param_spec_string("server",
                                                    "Server",
                                                    "Placeholder description.",
                                                    NULL, /* default */
                                                    G_PARAM_READWRITE);

  obj_properties[PROP_USER] = g_param_spec_string("user",
                                                  "User",
                                                  "Placeholder description.",
                                                  NULL, /* default */
                                                  G_PARAM_READWRITE);

  obj_properties[PROP_PASS] = g_param_spec_string("pass",
                                                  "Pass",
                                                  "Placeholder description.",
                                                  NULL, /* default */
                                                  G_PARAM_READWRITE);

  obj_properties[PROP_TOKEN] = g_param_spec_string("token",
                                                   "Token",
                                                   "Placeholder description.",
                                                   NULL, /* default */
                                                   G_PARAM_READWRITE);

  g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

static void
webrtc_client_init(G_GNUC_UNUSED WebrtcClient *self)
{
  SoupLogger *logger;
  g_assert(self);

  self->client_queue = g_queue_new();
  self->session = soup_session_new();
  logger = soup_logger_new(SOUP_LOGGER_LOG_BODY);
  soup_session_add_feature(self->session, SOUP_SESSION_FEATURE(logger));
}

WebrtcClient *
webrtc_client_new(const gchar *server, const gchar *user, const gchar *pass)
{
  return g_object_new(WEBRTC_TYPE_CLIENT,
                      "server",
                      server,
                      "user",
                      user,
                      "pass",
                      pass,
                      NULL);
}

void
webrtc_client_connect_async(WebrtcClient *self)
{
  gchar *uri = NULL;
  get_auth(self);
  uri = g_strdup_printf("wss://%s/%s", self->server, URL_PATH_STREAM);

  init_socket_connection(self, uri, data_stream_connection_cb);

  g_free(uri);
}

gboolean
webrtc_client_init_session(WebrtcClient *self,
                           const gchar *target,
                           const gchar *session_id)
{
  gchar *msg;

  g_return_val_if_fail(self != NULL, FALSE);
  g_return_val_if_fail(target != NULL, FALSE);
  g_return_val_if_fail(session_id != NULL, FALSE);
  g_return_val_if_fail(self->token != NULL, FALSE);

  msg = message_create_init_session(target, session_id, self->token);

  if (self->client != NULL) {
    g_message("Sending msg %s", msg);
    soup_websocket_connection_send_text(self->client, msg);
    g_free(msg);
  } else {
    g_queue_push_tail(self->client_queue, msg);
  }

  return TRUE;
}

gboolean
webrtc_client_send_sdp_answer(WebrtcClient *self,
                              const gchar *target,
                              const gchar *session_id,
                              const gchar *sdp)
{
  gchar *msg;

  g_return_val_if_fail(self != NULL, FALSE);
  g_return_val_if_fail(target != NULL, FALSE);
  g_return_val_if_fail(session_id != NULL, FALSE);
  g_return_val_if_fail(sdp != NULL, FALSE);
  g_return_val_if_fail(self->token != NULL, FALSE);

  msg = message_create_sdp_answer(target, session_id, sdp, self->token);

  if (self->client != NULL) {
    g_message("Sending msg %s", msg);
    soup_websocket_connection_send_text(self->client, msg);
    g_free(msg);
  } else {
    g_queue_push_tail(self->client_queue, msg);
  }

  return TRUE;
}

gboolean
webrtc_client_send_ice_candidate(WebrtcClient *self,
                                 const gchar *target,
                                 const gchar *session_id,
                                 const gchar *ice,
                                 guint line_index)
{
  gchar *msg;

  g_return_val_if_fail(self != NULL, FALSE);
  g_return_val_if_fail(target != NULL, FALSE);
  g_return_val_if_fail(session_id != NULL, FALSE);
  g_return_val_if_fail(ice != NULL, FALSE);
  g_return_val_if_fail(self->client != NULL, FALSE);
  g_return_val_if_fail(self->token != NULL, FALSE);

  msg = message_create_ice_candidate(target,
                                     session_id,
                                     ice,
                                     line_index,
                                     self->token);

  if (self->client != NULL) {
    g_message("Sending msg %s", msg);
    soup_websocket_connection_send_text(self->client, msg);
    g_free(msg);
  } else {
    g_queue_push_tail(self->client_queue, msg);
  }

  return TRUE;
}