#include <glib.h>

#include "messages.h"

static GBytes *
load_json_file(const gchar *name)
{
  gchar *path;
  gchar *content = NULL;
  gsize size;
  GBytes *res = NULL;
  GError *lerr = NULL;

  path = g_strdup_printf("%s/incoming/%s.json",
                         g_getenv("G_TEST_SRCDIR"),
                         name);

  if (!g_file_get_contents(path, &content, &size, &lerr)) {
    g_warning("Failed to load json file at %s: %s",
              path,
              lerr ? lerr->message : "No error message");
    g_clear_error(&lerr);
    return NULL;
  }

  res = g_bytes_new_take((gpointer) content, size);

  return res;
}

void
test_parse_ice_candidate(void)
{
  GBytes *json;
  message_t *msg;

  json = load_json_file("ice_candidate");
  g_assert_true(json != NULL);

  msg = message_parse(json, NULL);
  g_assert_true(msg != NULL);
  g_assert_cmpuint(MSG_TYPE_ICE_CANDIDATE, ==, msg->type);

  g_assert_cmpstr("candidate:11 UDP 2015363327 100.64.0.127 49129 typ host",
                  ==,
                  msg->data.ice_candidate.candidate);
  g_assert_cmpuint(1, ==, msg->data.ice_candidate.index);

  message_free(msg);
  g_bytes_unref(json);
}

void
test_parse_sdp_offer(void)
{
  GBytes *json;
  message_t *msg;

  json = load_json_file("sdp_offer");
  g_assert_true(json != NULL);

  msg = message_parse(json, NULL);
  g_assert_true(msg != NULL);
  g_assert_cmpuint(MSG_TYPE_SDP_OFFER, ==, msg->type);

  g_assert_cmpstr("sdp-content", ==, msg->data.sdp_offer.sdp);

  message_free(msg);
  g_bytes_unref(json);
}

void
test_parse_peer_connection(void)
{
  GBytes *json;
  message_t *msg;

  json = load_json_file("peer_connected");
  g_assert_true(json != NULL);

  msg = message_parse(json, NULL);
  g_assert_true(msg != NULL);
  g_assert_cmpuint(MSG_TYPE_PEER_CONNECTED, ==, msg->type);

  g_assert_cmpstr("client", ==, msg->data.connected.source);
  g_assert_cmpstr(
          "BGaynd4dvgJimxBqf49AmgIdHhFN39Gimq6zTNFMlI5W8Bl2zScEqHHjEXgr6Pjv::auto_assigned",
          ==,
          msg->data.connected.subject);
  message_free(msg);
  g_bytes_unref(json);
}

void
test_parse_stream_started(void)
{
  GBytes *json;
  message_t *msg;

  json = load_json_file("stream_started");
  g_assert_true(json != NULL);

  msg = message_parse(json, NULL);
  g_assert_true(msg != NULL);
  g_assert_cmpuint(MSG_TYPE_STREAM_STARTED, ==, msg->type);

  g_assert_cmpstr("971eb7ba-7a4c-458f-96d7-d6d019c096cf", ==, msg->session_id);
  g_assert_cmpstr("B8A44FB69350", ==, msg->target);
  g_assert_cmpstr("button", ==, msg->data.new_stream.trigger_type);
  g_assert_cmpstr("229dc0d9-5b6a-4456-9e2c-e405424c40cf",
                  ==,
                  msg->data.new_stream.bearer_id);
  g_assert_cmpstr("jenson-cellphone", ==, msg->data.new_stream.bearer_name);
  g_assert_cmpstr("3798d7d1-be89-48b2-8b8c-9f28461c737b",
                  ==,
                  msg->data.new_stream.system_id);
  g_assert_cmpstr("971eb7ba-7a4c-458f-96d7-d6d019c096cf",
                  ==,
                  msg->data.new_stream.session_id);
  g_assert_cmpstr("20240822_135948_E5D8_B8A44FB69350",
                  ==,
                  msg->data.new_stream.recording_id);

  message_free(msg);
  g_bytes_unref(json);
}

void
test_parse_hello(void)
{
  GBytes *json;
  message_t *msg;

  json = load_json_file("hello");
  g_assert_true(json != NULL);

  msg = message_parse(json, NULL);
  g_assert_true(msg != NULL);
  g_assert_cmpuint(MSG_TYPE_HELLO, ==, msg->type);

  g_assert_cmpstr("bc50e768-3545-43b9-91b5-d93b5d71ea4b",
                  ==,
                  msg->correlation_id);

  message_free(msg);
  g_bytes_unref(json);
}

void
test_parse_init_session(void)
{
  GBytes *json;
  message_t *msg;

  json = load_json_file("init_session");
  g_assert_true(json != NULL);

  msg = message_parse(json, NULL);
  g_assert_true(msg != NULL);
  g_assert_cmpuint(MSG_TYPE_INIT_SESSION, ==, msg->type);

  g_assert_cmpstr("f2841edd-1537-4426-9761-dec492a4fa7f",
                  ==,
                  msg->correlation_id);

  g_assert_cmpstr("4c7089a8-d5ca-47bd-9eb2-749417c27d80", ==, msg->session_id);
  g_assert_cmpstr("B8A44F5682A8", ==, msg->target);

  g_assert_cmpuint(1, ==, g_strv_length(msg->data.init_session.stun_servers));
  g_assert_cmpuint(2, ==, g_strv_length(msg->data.init_session.turn_servers));

  g_assert_cmpstr("stun://bwscloud4g.ddns.bodyworn-dev.axis.cloud:3002",
                  ==,
                  msg->data.init_session.stun_servers[0]);

  g_assert_cmpstr(
          "turn://1723748347%3Aaxis:FNDlmSAZitmNUuLEFv8XU8AZ1iY%3D@bwscloud4g.ddns.bodyworn-dev.axis.cloud:3002?transport=tcp",
          ==,
          msg->data.init_session.turn_servers[0]);
  g_assert_cmpstr(
          "turn://1723748347%3Aaxis:FNDlmSAZitmNUuLEFv8XU8AZ1iY%3D@bwscloud4g.ddns.bodyworn-dev.axis.cloud:3002?transport=udp",
          ==,
          msg->data.init_session.turn_servers[1]);

  message_free(msg);
  g_bytes_unref(json);
}


void
test_parse_response(void)
{
  GBytes *json;
  message_t *msg;

  json = load_json_file("response");
  g_assert_true(json != NULL);

  msg = message_parse(json, NULL);
  g_assert_true(msg != NULL);
  g_assert_cmpuint(MSG_TYPE_RESPONSE, ==, msg->type);

  g_assert_cmpstr("c9b6e773-e94f-4539-a767-c252ed93ea48",
                  ==,
                  msg->correlation_id);

  g_assert_cmpstr("971eb7ba-7a4c-458f-96d7-d6d019c096cf", ==, msg->session_id);
  g_assert_cmpstr("B8A44FB69350", ==, msg->target);

  message_free(msg);
  g_bytes_unref(json);
}

int
main(int argc, char *argv[])
{
  g_test_init(&argc, &argv, NULL);

  // Define the tests.
  g_test_add_func("/message/parse/signaling/ice_candidate",
                  test_parse_ice_candidate);
  g_test_add_func("/message/parse/signaling/sdp_offer", test_parse_sdp_offer);
  g_test_add_func("/message/parse/event/peer_connected",
                  test_parse_peer_connection);
  g_test_add_func("/message/parse/event/stream_started",
                  test_parse_stream_started);
  g_test_add_func("/message/parse/client/hello", test_parse_hello);
  g_test_add_func("/message/parse/signaling/init_session",
                  test_parse_init_session);
  g_test_add_func("/message/parse/signaling/response", test_parse_response);

  return g_test_run();
}
