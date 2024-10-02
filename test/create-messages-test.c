#include <glib.h>
#include <json-glib/json-glib.h>

#include "messages.h"

static gchar *
load_json_file(const gchar *name)
{
  gchar *path;
  gchar *content = NULL;
  GError *lerr = NULL;

  path = g_strdup_printf("%s/send/%s.json", g_getenv("G_TEST_SRCDIR"), name);

  if (!g_file_get_contents(path, &content, NULL, &lerr)) {
    g_warning("Failed to load json file at %s: %s",
              path,
              lerr ? lerr->message : "No error message");
    g_clear_error(&lerr);
    return NULL;
  }

  return content;
}

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

void
replace_member(const gchar *path, const gchar *with, gchar **json)
{
  JsonParser *parser = NULL;
  JsonNode *root;
  JsonObject *obj;
  gchar **names = NULL;
  const gchar *member;

  parser = json_parser_new();

  if (!json_parser_load_from_data(parser, *json, strlen(*json), NULL)) {
    return;
  }

  root = json_parser_get_root(parser);

  if (root == NULL) {
    g_warning("Failed to get a root node from string %s", *json);
    goto out;
  }

  if (!JSON_NODE_HOLDS_OBJECT(root)) {
    g_warning("Bad JSON message received");
    goto out;
  }

  obj = json_node_get_object(root);

  names = g_strsplit(path, ".", -1);
  member = names[g_strv_length(names) - 1];

  for (guint i = 0; names[i] != member; i++) {
    obj = json_object_get_object_member(obj, names[i]);
  }

  json_object_set_string_member(obj, member, with);

  g_free(*json);
  *json = get_string_from_json_object(json_node_get_object(root));

out:
  g_clear_object(&parser);
  g_strfreev(names);
}

void
mask_json(const gchar *path, gchar **json1, gchar **json2)
{
  gchar *mask;

  mask = g_uuid_string_random();

  replace_member(path, mask, json1);
  replace_member(path, mask, json2);
}

void
test_create_hello(void)
{
  gchar *json;
  gchar *msg;

  json = load_json_file("hello");
  g_assert_true(json != NULL);

  msg = message_create_hello(
          "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjE3MjQzMjgwODgsIm5iZiI6MTcyNDMyNzc4MywiaWF0IjoxNzI0MzI3Nzg4fQ.u7VZO8zOWCDpeVEx52XfTZPYgE7idGEeny-mEDpPdnQ");

  mask_json("correlationId", &msg, &json);
  g_assert_true(msg != NULL);
  g_assert_cmpstr(json, ==, msg);

  g_free(json);
  g_free(msg);
}

void
test_create_stream_filter(void)
{
  gchar *json;
  gchar *msg;

  json = load_json_file("stream_filter");
  g_assert_true(json != NULL);

  msg = message_create_stream_filter();
  mask_json("apiVersion",
            &msg,
            &json); /* TODO: just so that formatting is same */

  g_assert_true(msg != NULL);
  g_assert_cmpstr(json, ==, msg);

  g_free(json);
  g_free(msg);
}

void
test_create_init_session(void)
{
  gchar *json;
  gchar *msg;
  GHashTable *video_settings;
  GHashTable *audio_settings;

  json = load_json_file("init_session");
  g_assert_true(json != NULL);

  video_settings = g_hash_table_new(g_str_hash, g_str_equal);
  audio_settings = g_hash_table_new(g_str_hash, g_str_equal);

  msg = message_create_init_session(
          "B8A44FB69350",
          "971eb7ba-7a4c-458f-96d7-d6d019c096cf",
          video_settings,
          audio_settings,
          "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjE3MjQzMzEyMTAsIm5iZiI6MTcyNDMzMDkwNSwiaWF0IjoxNzI0MzMwOTEwfQ.VQrl3HwiSIc8YIfqc9MO1HV5gmfVFmSZ3EO57a-GyrA");

  mask_json("correlationId", &msg, &json);
  mask_json("data.context", &msg, &json);
  g_assert_true(msg != NULL);
  g_assert_cmpstr(json, ==, msg);

  g_free(json);
  g_free(msg);
  g_hash_table_unref(audio_settings);
  g_hash_table_unref(video_settings);
}

void
test_create_sdp_answer(void)
{
  gchar *json;
  gchar *msg;

  json = load_json_file("sdp_answer");
  g_assert_true(json != NULL);

  msg = message_create_sdp_answer("B8A44FB69350", "971eb7ba-7a4c-458f-96d7-d6d019c096cf", "v=0\r\no=- 4657433810695400270 2 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\na=group:BUNDLE video0 audio1 application2\r\na=msid-semantic: WMS\r\nm=video 9 UDP/TLS/RTP/SAVPF 96 98\r\nc=IN IP4 0.0.0.0\r\na=rtcp:9 IN IP4 0.0.0.0\r\na=ice-ufrag:wsGr\r\na=ice-pwd:dak8G0jjCdtadtOlmyVfXH3C\r\na=ice-options:trickle\r\na=fingerprint:sha-256 41:BA:99:82:F8:F3:3D:CA:2F:3C:7B:26:C5:CD:E1:2B:27:19:3E:C9:D0:07:2A:D4:AE:6F:C1:6C:2C:40:26:9A\r\na=setup:active\r\na=mid:video0\r\na=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01\r\na=recvonly\r\na=rtcp-mux\r\na=rtcp-rsize\r\na=rtpmap:96 H264/90000\r\na=rtcp-fb:96 transport-cc\r\na=rtcp-fb:96 ccm fir\r\na=rtcp-fb:96 nack\r\na=rtcp-fb:96 nack pli\r\na=fmtp:96 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f\r\na=rtpmap:98 rtx/90000\r\na=fmtp:98 apt=96\r\nm=audio 9 UDP/TLS/RTP/SAVPF 97\r\nc=IN IP4 0.0.0.0\r\na=rtcp:9 IN IP4 0.0.0.0\r\na=ice-ufrag:wsGr\r\na=ice-pwd:dak8G0jjCdtadtOlmyVfXH3C\r\na=ice-options:trickle\r\na=fingerprint:sha-256 41:BA:99:82:F8:F3:3D:CA:2F:3C:7B:26:C5:CD:E1:2B:27:19:3E:C9:D0:07:2A:D4:AE:6F:C1:6C:2C:40:26:9A\r\na=setup:active\r\na=mid:audio1\r\na=recvonly\r\na=rtcp-mux\r\na=rtpmap:97 OPUS/48000/2\r\na=rtcp-fb:97 transport-cc\r\na=fmtp:97 minptime=10;useinbandfec=1\r\nm=application 9 UDP/DTLS/SCTP webrtc-datachannel\r\nc=IN IP4 0.0.0.0\r\na=ice-ufrag:wsGr\r\na=ice-pwd:dak8G0jjCdtadtOlmyVfXH3C\r\na=ice-options:trickle\r\na=fingerprint:sha-256 41:BA:99:82:F8:F3:3D:CA:2F:3C:7B:26:C5:CD:E1:2B:27:19:3E:C9:D0:07:2A:D4:AE:6F:C1:6C:2C:40:26:9A\r\na=setup:active\r\na=mid:application2\r\na=sctp-port:5000\r\n", "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjE3MjQzMzEyMTAsIm5iZiI6MTcyNDMzMDkwNSwiaWF0IjoxNzI0MzMwOTEwfQ.VQrl3HwiSIc8YIfqc9MO1HV5gmfVFmSZ3EO57a-GyrA");

  mask_json("correlationId", &msg, &json);
  mask_json("data.context", &msg, &json);
  g_assert_true(msg != NULL);
  g_assert_cmpstr(json, ==, msg);

  g_free(json);
  g_free(msg);
}

void
test_create_ice_candidate(void)
{
  gchar *json;
  gchar *msg;

  json = load_json_file("add_ice_candidate");
  g_assert_true(json != NULL);

  msg = message_create_ice_candidate(
          "B8A44FB69350",
          "971eb7ba-7a4c-458f-96d7-d6d019c096cf",
          "candidate:108602806 1 udp 2113937151 0aa850c3-d225-46e1-a3f3-57d15446012b.local 35736 typ host generation 0 ufrag wsGr network-cost 999",
          0,
          "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjE3MjQzMzEyMTAsIm5iZiI6MTcyNDMzMDkwNSwiaWF0IjoxNzI0MzMwOTEwfQ.VQrl3HwiSIc8YIfqc9MO1HV5gmfVFmSZ3EO57a-GyrA");

  mask_json("correlationId", &msg, &json);
  mask_json("data.context", &msg, &json);
  g_assert_true(msg != NULL);
  g_assert_cmpstr(json, ==, msg);

  g_free(json);
  g_free(msg);
}

int
main(int argc, char *argv[])
{
  g_test_init(&argc, &argv, NULL);

  // Define the tests.
  g_test_add_func("/message/create/client/hello", test_create_hello);
  g_test_add_func("/message/create/client/stream_filter",
                  test_create_stream_filter);
  g_test_add_func("/message/create/signaling/init_session",
                  test_create_init_session);
  g_test_add_func("/message/create/signaling/sdp_answer",
                  test_create_sdp_answer);
  g_test_add_func("/message/create/signaling/add_ice_candidate",
                  test_create_ice_candidate);

  return g_test_run();
}
