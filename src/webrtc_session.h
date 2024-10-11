#pragma once

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

#include "webrtc_client.h"

G_BEGIN_DECLS

enum webrtc_session_elem_type {
  WEBRTC_SESSION_ELEM_VIDEO = 0,
  WEBRTC_SESSION_ELEM_AUDIO,
  WEBRTC_SESSION_ELEM_MUX
};

enum webrtc_session_audio_codec {
  WEBRTC_SESSION_AUDIO_CODEC_AAC = 0,
  WEBRTC_SESSION_AUDIO_CODEC_OPUS
};

/*
 * Type declaration.
 */

#define WEBRTC_TYPE_SESSION webrtc_session_get_type()
G_DECLARE_FINAL_TYPE(WebrtcSession, webrtc_session, WEBRTC, SESSION, GObject)

/*
 * Method definitions.
 */
gboolean webrtc_session_check_plugins(void);

WebrtcSession *webrtc_session_new(WebrtcClient *protocol,
                                  const gchar *id,
                                  const gchar *target);

void webrtc_session_add_element(WebrtcSession *self,
                                enum webrtc_session_elem_type type,
                                GstElement *el);

void webrtc_session_start(WebrtcSession *self, gboolean stat_file);
void webrtc_session_stop(WebrtcSession *self);

void webrtc_session_set_audio_codec(WebrtcSession *self,
                                    enum webrtc_session_audio_codec codec);
void webrtc_session_set_adaptive_bitrate(WebrtcSession *self, gboolean enabled);
void webrtc_session_set_max_bitrate(WebrtcSession *self, gint val);
void webrtc_session_set_compression(WebrtcSession *self, gint val);
void webrtc_session_set_gop(WebrtcSession *self, gint val);

const gchar * webrtc_session_get_id(WebrtcSession *self);
G_END_DECLS
