#include <glib.h>
#include <json-glib/json-glib.h>

#include "messages.h"

#define EVENT_TOPIC         "tns1:WebRTC/tnsaxis:Signaling/CloudEvent"
#define TYPE_PEER_CONNECTED "com.axis.webrtc.peer.connected"
#define TYPE_STREAM_STARTED "com.axis.bodyworn.stream.started"

struct parse_url_ctx {
  GStrvBuilder *list;
  gchar *user;
  gchar *pass;
};

static gchar *
get_string_from_json_object(JsonObject *object)
{
  JsonNode *root;
  JsonGenerator *generator;
  gchar *text;

  /* Make it the root node */
  root = json_node_init_object(json_node_alloc(), object);
  generator = json_generator_new();
  json_generator_set_root(generator, root);
  text = json_generator_to_data(generator, NULL);

  /* Release everything */
  g_object_unref(generator);
  json_node_free(root);

  return text;
}

static message_t *
parse_ice_candidate_msg(JsonObject *data)
{
  JsonObject *params;
  message_t *res = NULL;

  /* see test/json/ice_candidate.json */

  g_assert(data);

  params = json_object_get_object_member(data, "params");

  if (params == NULL) {
    g_warning("Parsing ice candidate: No params");
    return NULL;
  }

  if (!json_object_has_member(data, "sessionId")) {
    g_warning("Parsing ice candidate: No session id");
    return NULL;
  }
  if (!json_object_has_member(params, "candidate")) {
    g_warning("Parsing ice candidate: No candidate");
    return NULL;
  }

  if (!json_object_has_member(params, "sdpMLineIndex")) {
    g_warning("Parsing ice candidate: No sdp line index");
    return NULL;
  }

  g_message("Parse ice candidate");

  res = g_malloc0(sizeof(*res));
  res->type = MSG_TYPE_ICE_CANDIDATE;
  res->session_id = g_strdup(json_object_get_string_member(data, "sessionId"));

  res->data.ice_candidate.candidate =
          g_strdup(json_object_get_string_member(params, "candidate"));

  res->data.ice_candidate.index =
          (guint) json_object_get_int_member(params, "sdpMLineIndex");

  return res;
}

static message_t *
parse_response_msg(JsonObject *data)
{
  message_t *res;

  res = g_malloc0(sizeof(*res));
  res->type = MSG_TYPE_RESPONSE;

  res->session_id = g_strdup(json_object_get_string_member(data, "sessionId"));

  return res;
}

static message_t *
parse_sdp_msg(JsonObject *data)
{
  JsonObject *params;
  message_t *res;
  /* see test/json/sdp_offer.json */

  g_assert(data);

  if (!json_object_has_member(data, "params")) {
    g_warning("Parse sdp offer: No params");
    return NULL;
  }

  params = json_object_get_object_member(data, "params");

  if (!json_object_has_member(data, "sessionId")) {
    g_warning("Parse sdp offer: No session id");
    return NULL;
  }

  if (!json_object_has_member(params, "sdp")) {
    g_warning("Parse sdp offer: sdp");
    return NULL;
  }

  res = g_malloc0(sizeof(*res));
  res->type = MSG_TYPE_SDP_OFFER;

  res->session_id = g_strdup(json_object_get_string_member(data, "sessionId"));
  res->data.sdp_offer.sdp =
          g_strdup(json_object_get_string_member(params, "sdp"));
  return res;
}

static void
parse_url_list(G_GNUC_UNUSED JsonArray *array,
               G_GNUC_UNUSED guint index_,
               JsonNode *element_node,
               gpointer user_data)
{
  struct parse_url_ctx *ctx = (struct parse_url_ctx *) user_data;
  const gchar *str;

  g_assert(ctx);
  g_assert(element_node);

  str = json_node_get_string(element_node);

  if (str == NULL) {
    return;
  }

  if (ctx->user != NULL && ctx->pass != NULL) {
    GString *uri;
    gchar *usr_str;
    gchar *res;

    uri = g_string_new(str);

    usr_str = g_strdup_printf("://%s:%s@", ctx->user, ctx->pass);

    g_string_replace(uri, "://", usr_str, 1);

    res = g_string_free(uri, FALSE);
    g_strv_builder_add(ctx->list, res);

    g_free(usr_str);
    g_free(res);
    return;
  }

  g_strv_builder_add(ctx->list, str);
}

static void
parse_server_list(G_GNUC_UNUSED JsonArray *array,
                  G_GNUC_UNUSED guint index_,
                  JsonNode *element_node,
                  gpointer user_data)
{
  JsonObject *obj;
  struct parse_url_ctx ctx = { 0 };

  g_assert(element_node);
  g_assert(user_data);

  if (!JSON_NODE_HOLDS_OBJECT(element_node)) {
    return;
  }

  obj = json_node_get_object(element_node);

  if (!json_object_has_member(obj, "urls")) {
    return;
  }

  ctx.list = (GStrvBuilder *) user_data;

  if (json_object_has_member(obj, "username")) {
    ctx.user =
            g_uri_escape_string(json_object_get_string_member(obj, "username"),
                                NULL,
                                FALSE);
  }
  if (json_object_has_member(obj, "password")) {
    ctx.pass =
            g_uri_escape_string(json_object_get_string_member(obj, "password"),
                                NULL,
                                FALSE);
  }

  json_array_foreach_element(json_object_get_array_member(obj, "urls"),
                             parse_url_list,
                             &ctx);

  g_free(ctx.user);
  g_free(ctx.pass);
}

static message_t *
parse_init_session_msg(JsonObject *obj)
{
  JsonArray *list;
  GStrvBuilder *turn_servers_builder;
  GStrvBuilder *stun_servers_builder;
  JsonObject *data;
  message_t *res;

  if (!json_object_has_member(obj, "turnServers") ||
      !json_object_has_member(obj, "stunServers")) {
    return NULL;
  }

  res = g_malloc(sizeof(*res));

  res->type = MSG_TYPE_INIT_SESSION;

  data = json_object_get_object_member(obj, "data");
  res->session_id = g_strdup(json_object_get_string_member(data, "sessionId"));

  turn_servers_builder = g_strv_builder_new();
  list = json_object_get_array_member(obj, "turnServers");
  json_array_foreach_element(list, parse_server_list, turn_servers_builder);
  res->data.init_session.turn_servers =
          g_strv_builder_end(turn_servers_builder);

  stun_servers_builder = g_strv_builder_new();
  list = json_object_get_array_member(obj, "stunServers");
  json_array_foreach_element(list, parse_server_list, stun_servers_builder);
  res->data.init_session.stun_servers =
          g_strv_builder_end(stun_servers_builder);

  res->target = g_strdup(json_object_get_string_member(obj, "targetId"));
  res->correlation_id =
          g_strdup(json_object_get_string_member(obj, "correlationId"));
  return res;
}

static message_t *
parse_stream_started(const gchar *msg, GError **err)
{
  JsonParser *parser = NULL;
  JsonNode *root;
  JsonObject *obj;
  JsonObject *data;
  message_t *res;

  parser = json_parser_new();

  if (!json_parser_load_from_data(parser, msg, strlen(msg), err)) {
    return NULL;
  }

  root = json_parser_get_root(parser);
  if (root == NULL) {
    g_warning("Failed to get a root node from string %s", msg);
    goto out;
  }

  if (!JSON_NODE_HOLDS_OBJECT(root)) {
    g_warning("Bad JSON message received");
    goto out;
  }

  obj = json_node_get_object(root);
  data = json_object_get_object_member(obj, "data");

  res = g_malloc0(sizeof(*res));
  res->type = MSG_TYPE_STREAM_STARTED;

  res->session_id = g_strdup(json_object_get_string_member(data, "sessionId"));
  res->target = g_strdup(json_object_get_string_member(obj, "subject"));

  res->data.new_stream.source =
          g_strdup(json_object_get_string_member(obj, "source"));
  res->data.new_stream.subject =
          g_strdup(json_object_get_string_member(obj, "subject"));
  res->data.new_stream.time =
          g_strdup(json_object_get_string_member(obj, "time"));

  res->data.new_stream.trigger_type =
          g_strdup(json_object_get_string_member(data, "triggerType"));
  res->data.new_stream.bearer_id =
          g_strdup(json_object_get_string_member(data, "bearerId"));
  res->data.new_stream.bearer_name =
          g_strdup(json_object_get_string_member(data, "bearerName"));
  res->data.new_stream.system_id =
          g_strdup(json_object_get_string_member(data, "systemId"));
  res->data.new_stream.session_id =
          g_strdup(json_object_get_string_member(data, "sessionId"));
  res->data.new_stream.recording_id =
          g_strdup(json_object_get_string_member(data, "recordingId"));

out:
  g_clear_object(&parser);

  return res;
}

static message_t *
parse_peer_connected(const gchar *msg, GError **err)
{
  JsonParser *parser = NULL;
  JsonNode *root;
  JsonObject *obj;
  message_t *res = NULL;

  parser = json_parser_new();

  if (!json_parser_load_from_data(parser, msg, strlen(msg), err)) {
    return NULL;
  }

  root = json_parser_get_root(parser);
  if (root == NULL) {
    g_warning("Failed to get a root node from string %s", msg);
    goto out;
  }

  if (!JSON_NODE_HOLDS_OBJECT(root)) {
    g_warning("Bad JSON message received");
    goto out;
  }

  obj = json_node_get_object(root);

  res = g_malloc0(sizeof(*res));
  res->type = MSG_TYPE_PEER_CONNECTED;

  res->data.connected.source =
          g_strdup(json_object_get_string_member(obj, "source"));
  res->data.connected.subject =
          g_strdup(json_object_get_string_member(obj, "subject"));

out:
  g_clear_object(&parser);

  return res;
}

static message_t *
parse_events_notify_msg(JsonObject *obj, GError **err)
{
  JsonObject *notification;
  JsonObject *message;
  JsonObject *data;
  const gchar *topic;
  const gchar *event;
  const gchar *event_type;

  g_assert(obj);

  notification = json_object_get_object_member(obj, "notification");
  topic = json_object_get_string_member(notification, "topic");

  if (g_strcmp0(topic, EVENT_TOPIC) == 0) {
    message = json_object_get_object_member(notification, "message");
    data = json_object_get_object_member(message, "data");
    event = json_object_get_string_member(data, "event");
    event_type = json_object_get_string_member(data, "eventType");

    if (g_strcmp0(event_type, TYPE_PEER_CONNECTED) == 0) {
      return parse_peer_connected(event, err);
    } else if (g_strcmp0(event_type, TYPE_STREAM_STARTED) == 0) {
      return parse_stream_started(event, err);
    }
  }

  return NULL;
}

static message_t *
parse_hello_msg(JsonObject *obj)
{
  message_t *res = NULL;

  res = g_malloc0(sizeof(*res));

  res->type = MSG_TYPE_HELLO;
  res->correlation_id =
          g_strdup(json_object_get_string_member(obj, "correlationId"));

  return res;
}

static message_t *
parse_signaling_msg(JsonObject *obj)
{
  JsonObject *data;
  const gchar *type;
  const gchar *method;
  message_t *res = NULL;
  /*
  Response for init session:
  {"type":"signaling","targetId":"B8A44F5682A8","orgId":null,"data":{"apiVersion":"1.0","method":"initSession","type":"response","sessionId":"4c7089a8-d5ca-47bd-9eb2-749417c27d80","context":"059d30fe-c6a2-4c04-a45f-9745c5a47e13","id":"467","data":{}},"correlationId":"f2841edd-1537-4426-9761-dec492a4fa7f"}



  Send sdp offer response back to server:
  {"type":"signaling","targetId":"B8A44F5682A8","orgId":null,"correlationId":"","data":{"apiVersion":"1.0","type":"response","sessionId":"4c7089a8-d5ca-47bd-9eb2-749417c27d80","method":"setSdpOffer","params":{}},"accessToken":"eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjE3MjM2NjIyNDMsIm5iZiI6MTcyMzY2MTkzOCwiaWF0IjoxNzIzNjYxOTQzfQ.SoEo70cuc8EDpaBskcWla1ZiR21cXACb0SYtcj21hyo"}

  Add Ice candidate message:


  Respond with:
  {"type":"signaling","targetId":"B8A44F5682A8","orgId":null,"correlationId":"","data":{"apiVersion":"1.0","type":"response","sessionId":"4c7089a8-d5ca-47bd-9eb2-749417c27d80","method":"addIceCandidate","context":"1492972058","data":{}},"accessToken":"eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjE3MjM2NjIyNDMsIm5iZiI6MTcyMzY2MTkzOCwiaWF0IjoxNzIzNjYxOTQzfQ.SoEo70cuc8EDpaBskcWla1ZiR21cXACb0SYtcj21hyo"}
  */

  data = json_object_get_object_member(obj, "data");
  type = json_object_get_string_member(data, "type");
  method = json_object_get_string_member(data, "method");

  if (g_strcmp0(type, "response") == 0) {
    res = parse_response_msg(data);
  }

  if (g_strcmp0(type, "request") == 0) {
    if (g_strcmp0(method, "setSdpOffer") == 0) {
      res = parse_sdp_msg(data);
    }

    if (g_strcmp0(method, "addIceCandidate") == 0) {
      res = parse_ice_candidate_msg(data);
    }
  }

  if (res) {
    res->target = g_strdup(json_object_get_string_member(obj, "targetId"));
    res->correlation_id =
            g_strdup(json_object_get_string_member(obj, "correlationId"));
  }

  return res;
}

message_t *
message_parse(GBytes *src, GError **err)
{
  gchar *msg = NULL;
  JsonParser *parser = NULL;
  JsonNode *root;
  JsonObject *obj;
  const gchar *type;
  message_t *res = NULL;

  msg = g_strndup(g_bytes_get_data(src, NULL), g_bytes_get_size(src));
  g_message("Got message on websocket: %s", msg);

  parser = json_parser_new();

  if (!json_parser_load_from_data(parser,
                                  g_bytes_get_data(src, NULL),
                                  g_bytes_get_size(src),
                                  err)) {
    goto out;
  }

  root = json_parser_get_root(parser);
  if (root == NULL) {
    g_warning("Failed to get a root node from string %s", msg);
    goto out;
  }

  if (!JSON_NODE_HOLDS_OBJECT(root)) {
    g_warning("Bad JSON message received");
    goto out;
  }

  obj = json_node_get_object(root);

  /* Here the actual message content is parsed, if it is of interest: */
  if (json_object_has_member(obj, "method")) {
    type = json_object_get_string_member(obj, "method");
    if (g_strcmp0(type, "events:notify") == 0) {
      res = parse_events_notify_msg(json_object_get_object_member(obj,
                                                                  "params"),
                                    err);
    }
    goto out;
  }

  if (json_object_has_member(obj, "type")) {
    type = json_object_get_string_member(obj, "type");
    if (g_strcmp0(type, "hello") == 0) {
      res = parse_hello_msg(obj);
    } else if (g_strcmp0(type, "initSession") == 0) {
      res = parse_init_session_msg(obj);
    } else if (g_strcmp0(type, "signaling") == 0) {
      res = parse_signaling_msg(obj);
    }
    goto out;
  }

out:
  g_clear_object(&parser);
  g_free(msg);
  return res;
}

gchar *
message_create_hello(const gchar *token)
{
  JsonObject *j = json_object_new();
  gchar *corr_id;
  gchar *msg;

  corr_id = g_uuid_string_random();

  json_object_set_string_member(j, "type", "hello");
  json_object_set_string_member(j, "id", "noid");
  json_object_set_string_member(j, "correlationId", corr_id);
  json_object_set_string_member(j, "accessToken", token);

  msg = get_string_from_json_object(j);

  g_free(corr_id);
  g_clear_pointer(&j, json_object_unref);

  return msg;
}

gchar *
message_create_stream_filter(void)
{
  JsonObject *j = json_object_new();
  JsonObject *params = json_object_new();
  JsonArray *event_filter_list = json_array_new();
  JsonObject *event_filter = json_object_new();
  gchar *msg;

  json_object_set_string_member(
          event_filter,
          "topicFilter",
          "tns1:WebRTC/tnsaxis:Signaling/tnsaxis:CloudEvent");

  json_array_add_object_element(event_filter_list, event_filter);
  json_object_set_array_member(params, "eventFilterList", event_filter_list);

  json_object_set_string_member(j, "apiVersion", "1.0");
  json_object_set_string_member(j, "context", "0");
  json_object_set_string_member(j, "method", "events:configure");
  json_object_set_object_member(j, "params", params);

  msg = get_string_from_json_object(j);

  g_clear_pointer(&j, json_object_unref);

  return msg;
}

static void
apply_settings(JsonObject *dst,
               const gchar *json_name,
               const GHashTable *conf,
               const gchar *setting_name)
{
  GVariant *tmp;

  g_assert(dst);
  g_assert(json_name);
  g_assert(conf);
  g_assert(setting_name);

  tmp = g_hash_table_lookup((GHashTable *) conf, setting_name);

  if (tmp == NULL) {
    return;
  }

  switch (g_variant_classify(tmp)) {
  case G_VARIANT_CLASS_STRING:
    json_object_set_string_member(dst,
                                  json_name,
                                  g_variant_get_string(tmp, NULL));
    break;

  case G_VARIANT_CLASS_BOOLEAN:
    json_object_set_boolean_member(dst, json_name, g_variant_get_boolean(tmp));
    break;

  case G_VARIANT_CLASS_INT32:
    json_object_set_int_member(dst, json_name, g_variant_get_int32(tmp));
    break;

  case G_VARIANT_CLASS_INT64:
    json_object_set_int_member(dst, json_name, g_variant_get_int64(tmp));
    break;
  default:
    g_warning("Unhandled audio / video settings format!");
  }
}

gchar *
message_create_init_session(const gchar *target,
                            const gchar *session_id,
                            const GHashTable *video_settings,
                            const GHashTable *audio_settings,
                            const gchar *token)
{
  JsonObject *root;
  JsonObject *data;
  JsonObject *params;
  JsonObject *video;
  JsonObject *audio;
  gchar *context;
  gchar *correlation;
  gchar *msg;

  /*
    {
     "type":"initSession",
     "targetId":"B8A44FB69350",
     "correlationId":"cd3711e0-763d-42c9-a519-657aa12504ce",
     "data":{
        "apiVersion":"1.0",
        "type":"request",
        "method":"initSession",
        "sessionId":"dcf82619-0667-4358-b09e-f34aabad1f91",
        "context":"188d90f8-a4bc-4f18-936d-0765b65171bc",
        "params":{
           "type":"live",
           "videoReceive":{},
           "audioReceive":{}
        }
     },
     "accessToken":"eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjE3MjM3MzI0MTAsIm5iZiI6MTcyMzczMjEwNSwiaWF0IjoxNzIzNzMyMTEwfQ.o5GE8CPQOV8tSbX95KJL92GAHLQYcBI8NKFY7t07FFA"
  }
  */

  context = g_uuid_string_random();
  correlation = g_uuid_string_random();
  root = json_object_new();
  data = json_object_new();
  params = json_object_new();
  video = json_object_new();
  audio = json_object_new();

  apply_settings(audio, "codec", audio_settings, "codec");
  apply_settings(video, "adaptive", video_settings, "adaptive");

  json_object_set_string_member(params, "type", "live");
  json_object_set_object_member(params, "videoReceive", video);
  json_object_set_object_member(params, "audioReceive", audio);

  json_object_set_string_member(data, "apiVersion", "1.0");
  json_object_set_string_member(data, "type", "request");
  json_object_set_string_member(data, "method", "initSession");
  json_object_set_string_member(data, "sessionId", session_id);
  json_object_set_string_member(data, "context", context);
  json_object_set_object_member(data, "params", params);

  json_object_set_string_member(root, "type", "initSession");
  json_object_set_string_member(root, "targetId", target);
  json_object_set_string_member(root, "correlationId", correlation);
  json_object_set_object_member(root, "data", data);
  json_object_set_string_member(root, "accessToken", token);

  msg = get_string_from_json_object(root);

  g_free(context);
  g_free(correlation);

  return msg;
}

gchar *
message_create_sdp_answer(const gchar *target,
                          const gchar *session_id,
                          const gchar *sdp,
                          const gchar *token)
{
  JsonObject *root;
  JsonObject *data;
  JsonObject *params;
  gchar *context;
  gchar *correlation;
  gchar *msg;

  g_return_val_if_fail(target != NULL, NULL);
  g_return_val_if_fail(session_id != NULL, NULL);
  g_return_val_if_fail(sdp != NULL, NULL);
  g_return_val_if_fail(token != NULL, NULL);

  context = g_strdup_printf("%u", g_str_hash(sdp));
  correlation = g_uuid_string_random();
  root = json_object_new();
  data = json_object_new();
  params = json_object_new();

  json_object_set_string_member(params, "type", "answer");
  json_object_set_string_member(params, "sdp", sdp);

  json_object_set_string_member(data, "apiVersion", "1.0");
  json_object_set_string_member(data, "type", "request");
  json_object_set_string_member(data, "method", "setSdpAnswer");
  json_object_set_string_member(data, "sessionId", session_id);
  json_object_set_object_member(data, "params", params);
  json_object_set_string_member(data, "context", context);

  json_object_set_string_member(root, "type", "signaling");
  json_object_set_string_member(root, "targetId", target);
  json_object_set_string_member(root, "correlationId", correlation);
  json_object_set_object_member(root, "data", data);
  json_object_set_string_member(root, "accessToken", token);

  msg = get_string_from_json_object(root);

  g_free(context);
  g_free(correlation);
  return msg;
}

gchar *
message_create_ice_candidate(const gchar *target,
                             const gchar *session_id,
                             const gchar *ice,
                             guint line_index,
                             const gchar *token)
{
  JsonObject *root;
  JsonObject *data;
  JsonObject *params;
  gchar *context;
  gchar *correlation;
  gchar *msg;

  g_return_val_if_fail(target != NULL, NULL);
  g_return_val_if_fail(session_id != NULL, NULL);
  g_return_val_if_fail(ice != NULL, NULL);
  g_return_val_if_fail(token != NULL, NULL);

  context = g_strdup_printf("%u", g_str_hash(ice));
  correlation = g_uuid_string_random();
  root = json_object_new();
  data = json_object_new();
  params = json_object_new();

  json_object_set_string_member(params, "candidate", ice);
  // json_object_set_string_member(params, "sdpMid", "video0"); // TODO,
  // should not be needed with line_index
  json_object_set_int_member(params, "sdpMLineIndex", (gint) line_index);
  // json_object_set_string_member(params, "usernameFragment", "live"); //
  // TODO, should not be needed with line_index

  json_object_set_string_member(data, "apiVersion", "1.0");
  json_object_set_string_member(data, "type", "request");
  json_object_set_string_member(data, "method", "addIceCandidate");
  json_object_set_string_member(data, "sessionId", session_id);
  json_object_set_object_member(data, "params", params);
  json_object_set_string_member(data, "context", context);

  json_object_set_string_member(root, "type", "signaling");
  json_object_set_string_member(root, "targetId", target);
  json_object_set_string_member(root, "correlationId", correlation);
  json_object_set_object_member(root, "data", data);
  json_object_set_string_member(root, "accessToken", token);

  msg = get_string_from_json_object(root);

  g_free(context);
  g_free(correlation);

  return msg;
}

gchar *
message_create_reply(message_t *msg, const gchar *token)
{
  JsonObject *root;
  JsonObject *data;
  gchar *reply;

  switch (msg->type) {
  case MSG_TYPE_HELLO:
  case MSG_TYPE_RESPONSE:
  case MSG_TYPE_STREAM_STARTED:
  case MSG_TYPE_PEER_CONNECTED:
  case MSG_TYPE_INIT_SESSION:
    return NULL;
  default:
    g_message("composing reply message");
  }

  root = json_object_new();
  data = json_object_new();

  json_object_set_string_member(root, "targetId", msg->target);
  json_object_set_string_member(root, "correlationId", msg->correlation_id);
  json_object_set_string_member(root, "accessToken", token);

  json_object_set_string_member(data, "apiVersion", "1.0");
  json_object_set_string_member(data, "type", "response");
  json_object_set_string_member(data, "sessionId", msg->session_id);
  json_object_set_string_member(data, "context", "");
  json_object_set_object_member(data, "data", json_object_new());

  switch (msg->type) {
  case MSG_TYPE_SDP_OFFER:
    json_object_set_string_member(root, "type", "signaling");
    json_object_set_string_member(data, "method", "setSdpOffer");

    break;

  case MSG_TYPE_INIT_SESSION:
    json_object_set_string_member(root, "type", "signaling");
    json_object_set_string_member(data, "method", "initSession");
    break;

  case MSG_TYPE_ICE_CANDIDATE:
    json_object_set_string_member(root, "type", "signaling");
    json_object_set_string_member(data, "method", "addIceCandidate");
    break;

  default:
    g_warning("Unhandled type : %u", msg->type);
  }

  json_object_set_object_member(root, "data", data);

  /* maybe add access token? */
  reply = get_string_from_json_object(root);

  g_clear_pointer(&root, json_object_unref);

  return reply;
}

void
message_free(message_t *msg)
{
  if (msg == NULL) {
    return;
  }
  switch (msg->type) {
  case MSG_TYPE_RESPONSE:
  case MSG_TYPE_HELLO:
    /* no data */
    break;

  case MSG_TYPE_SDP_OFFER:
    g_free(msg->data.sdp_offer.sdp);
    break;

  case MSG_TYPE_STREAM_STARTED:
    g_free(msg->data.new_stream.source);
    g_free(msg->data.new_stream.subject);
    g_free(msg->data.new_stream.time);
    g_free(msg->data.new_stream.trigger_type);
    g_free(msg->data.new_stream.bearer_id);
    g_free(msg->data.new_stream.bearer_name);
    g_free(msg->data.new_stream.system_id);
    g_free(msg->data.new_stream.session_id);
    g_free(msg->data.new_stream.recording_id);
    break;

  case MSG_TYPE_PEER_CONNECTED:
    g_free(msg->data.connected.source);
    g_free(msg->data.connected.subject);
    break;

  case MSG_TYPE_INIT_SESSION:
    g_strfreev(msg->data.init_session.turn_servers);
    g_strfreev(msg->data.init_session.stun_servers);
    break;

  case MSG_TYPE_ICE_CANDIDATE:
    g_free(msg->data.ice_candidate.candidate);
    break;
  }

  g_free(msg->session_id);
  g_free(msg->target);
  g_free(msg->correlation_id);

  g_free(msg);
}