#pragma once

#include <glib.h>
#include <glib-object.h>

#include "webrtc_settings.h"

G_BEGIN_DECLS

/*
 * Type declaration.
 */

#define WEBRTC_TYPE_CLIENT webrtc_client_get_type()
G_DECLARE_FINAL_TYPE(WebrtcClient, webrtc_client, WEBRTC, CLIENT, GObject)

struct stream_started {
  const gchar *source;
  const gchar *subject;
  const gchar *time;
  const gchar *trigger_type;
  const gchar *bearer_id;
  const gchar *bearer_name;
  const gchar *system_id;
  const gchar *session_id;
  const gchar *recording_id;
};

/** Signal: sdp
 * on_sdp(
 *  WebrtcClient *self,
 *  const gchar *example_param,
 *  gpointer user_data
 *);
 *
 * Describe the signal here
 */

/*
 * Method definitions.
 */
WebrtcClient *
webrtc_client_new(const gchar *server, const gchar *user, const gchar *pass);

void webrtc_client_connect_async(WebrtcClient *self);

gboolean webrtc_client_init_session(WebrtcClient *self,
                                    const gchar *target,
                                    WebrtcSettings *settings,
                                    const gchar *session_id);

gboolean webrtc_client_send_sdp_answer(WebrtcClient *self,
                                       const gchar *target,
                                       const gchar *session_id,
                                       const gchar *sdp);

gboolean webrtc_client_send_ice_candidate(WebrtcClient *self,
                                          const gchar *target,
                                          const gchar *session_id,
                                          const gchar *ice,
                                          guint line_index);
const gchar *
webrtc_client_get_name(WebrtcClient *self);

G_END_DECLS
