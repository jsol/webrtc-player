#include <glib.h>

enum message_type {
  MSG_TYPE_HELLO = 0,
  MSG_TYPE_SDP_OFFER,
  MSG_TYPE_STREAM_STARTED,
  MSG_TYPE_PEER_CONNECTED,
  MSG_TYPE_INIT_SESSION,
  MSG_TYPE_ICE_CANDIDATE,
  MSG_TYPE_RESPONSE
};

typedef struct message {
  enum message_type type;
  gchar *session_id;
  gchar *target;
  gchar *correlation_id;

  union {
    /* hello has no data atm */
    struct {
      gchar *sdp;
    } sdp_offer;

    struct {
      gchar *source;
      gchar *subject;
    } connected;

    struct {
      gchar *source;
      gchar *subject;
      gchar *time;
      gchar *trigger_type;
      gchar *bearer_id;
      gchar *bearer_name;
      gchar *system_id;
      gchar *session_id;
      gchar *recording_id;
    } new_stream;

    struct {
      GStrv turn_servers;
      GStrv stun_servers;
    } init_session;

    struct {
      gchar *candidate;
      guint index;
    } ice_candidate;

    /* response has no data atm */
  } data;
} message_t;

message_t *message_parse(GBytes *src, GError **err);

gchar *message_create_hello(const gchar *token);
gchar *message_create_stream_filter(void);
gchar *message_create_init_session(const gchar *target,
                                   const gchar *session_id,
                                   const GHashTable *video_settings,
                                   const GHashTable *audio_settings,
                                   const gchar *token);

gchar *message_create_sdp_answer(const gchar *target,
                                 const gchar *session_id,
                                 const gchar *sdp,
                                 const gchar *token);

gchar *message_create_ice_candidate(const gchar *target,
                                    const gchar *session_id,
                                    const gchar *ice,
                                    guint line_index,
                                    const gchar *token);

gchar *message_create_reply(message_t *msg, const gchar *token);

void message_free(message_t *msg);