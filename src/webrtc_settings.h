#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define WEBRTC_SETTINGS_AUDIO_CODEC    g_quark_from_static_string("codec")
#define WEBRTC_SETTINGS_VIDEO_ADAPTIVE g_quark_from_static_string("adaptive")
#define WEBRTC_SETTINGS_VIDEO_MAX_BITRATE                                      \
  g_quark_from_static_string("maxBitrateInKbps")
#define WEBRTC_SETTINGS_VIDEO_COMPRESSION                                      \
  g_quark_from_static_string("compression")
#define WEBRTC_SETTINGS_VIDEO_GOP  g_quark_from_static_string("keyframeInterval")
#define WEBRTC_SETTINGS_FORCE_TURN g_quark_from_static_string("force_turn")

#define AUDIO_CODEC_LIST { "No audio", "opus", "aac", NULL }
#define BOOLEAN_LIST     { "disabled", "enabled", NULL }

/** matching the settings */
enum webrtc_settings_audio_codec {
  WEBRTC_SETTINGS_AUDIO_CODEC_NONE = 0,
  WEBRTC_SETTINGS_AUDIO_CODEC_OPUS,
  WEBRTC_SETTINGS_AUDIO_CODEC_AAC,
  WEBRTC_SETTINGS_AUDIO_CODEC_LAST,
};

#define WEBRTC_TYPE_SETTINGS webrtc_settings_get_type()
G_DECLARE_FINAL_TYPE(WebrtcSettings, webrtc_settings, WEBRTC, SETTINGS, GObject)

WebrtcSettings *webrtc_settings_new(void);

enum webrtc_settings_audio_codec
webrtc_settings_audio_codec(WebrtcSettings *self);

gboolean webrtc_settings_video_adaptive(WebrtcSettings *self);
gint webrtc_settings_video_compression(WebrtcSettings *self);
gint64 webrtc_settings_video_max_bitrate(WebrtcSettings *self);
gint webrtc_settings_video_gop(WebrtcSettings *self);

gboolean webrtc_settings_ice_force_turn(WebrtcSettings *self);

void webrtc_settings_set_string(WebrtcSettings *self,
                                GQuark setting,
                                const gchar *val);

void
webrtc_settings_set_number(WebrtcSettings *self, GQuark setting, gint64 val);

guint webrtc_settings_selected(WebrtcSettings *self, GQuark setting);

void webrtc_settings_bind(WebrtcSettings *self);

gint webrtc_settings_parse_opts(WebrtcSettings *self, int argc, char **argv);

const gchar *webrtc_settings_get_target(WebrtcSettings *self);
const gchar *webrtc_settings_get_output(WebrtcSettings *self);
G_END_DECLS
