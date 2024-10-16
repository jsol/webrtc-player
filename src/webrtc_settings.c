#include <glib.h>
#include <gio/gio.h>
#include <glib-object.h>
#include "webrtc_settings.h"

struct _WebrtcSettings {
  GObject parent;

  gint gop;
  enum webrtc_settings_audio_codec audio_codec;
  gint compression;
  gint64 max_bitrate;
  gboolean adaptive;
  GSettings *settings;
  gchar *target;
  gchar *output;
  gboolean force_turn;
};

G_DEFINE_TYPE(WebrtcSettings, webrtc_settings, G_TYPE_OBJECT)

typedef enum {
  PROP_GOP = 1,
  PROP_AUDIO_CODEC,
  PROP_COMPRESSION,
  PROP_MAX_BITRATE,
  PROP_ADAPTIVE,
  PROP_FORCE_TURN,
  N_PROPERTIES
} WebrtcSettingsProperty;
static GParamSpec *obj_properties[N_PROPERTIES] = {
  NULL,
};

static void
webrtc_settings_dispose(GObject *obj)
{
  WebrtcSettings *self = WEBRTC_SETTINGS(obj);

  g_assert(self);

  /* Do unrefs of objects and such. The object might be used after dispose,
   * and dispose might be called several times on the same object
   */

  /* Always chain up to the parent dispose function to complete object
   * destruction. */
  G_OBJECT_CLASS(webrtc_settings_parent_class)->dispose(obj);
}

static void
webrtc_settings_finalize(GObject *obj)
{
  WebrtcSettings *self = WEBRTC_SETTINGS(obj);

  g_assert(self);

  g_free(self->target);
  g_free(self->output);

  /* Always chain up to the parent finalize function to complete object
   * destruction. */
  G_OBJECT_CLASS(webrtc_settings_parent_class)->finalize(obj);
}

static void
get_property(GObject *object,
             guint property_id,
             GValue *value,
             GParamSpec *pspec)
{
  WebrtcSettings *self = WEBRTC_SETTINGS(object);

  switch ((WebrtcSettingsProperty) property_id) {
  case PROP_GOP:
    g_value_set_int(value, self->gop);
    break;

  case PROP_AUDIO_CODEC:
    g_value_set_int(value, self->audio_codec);
    break;

  case PROP_COMPRESSION:
    g_value_set_int(value, self->compression);
    break;

  case PROP_MAX_BITRATE:
    g_value_set_int64(value, self->max_bitrate);
    break;

  case PROP_ADAPTIVE:
    g_value_set_boolean(value, self->adaptive);
    break;

  case PROP_FORCE_TURN:
    g_value_set_boolean(value, self->force_turn);
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
  WebrtcSettings *self = WEBRTC_SETTINGS(object);

  switch ((WebrtcSettingsProperty) property_id) {
  case PROP_GOP:
    self->gop = g_value_get_int(value);
    break;
  case PROP_AUDIO_CODEC:
    self->audio_codec = g_value_get_int(value);
    break;
  case PROP_COMPRESSION:
    self->compression = g_value_get_int(value);
    break;
  case PROP_MAX_BITRATE:
    self->max_bitrate = g_value_get_int64(value);
    break;
  case PROP_ADAPTIVE:
    self->adaptive = g_value_get_boolean(value);
    break;
  case PROP_FORCE_TURN:
    self->force_turn = g_value_get_boolean(value);
    break;
  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    break;
  }
}

static void
webrtc_settings_class_init(WebrtcSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = webrtc_settings_dispose;
  object_class->finalize = webrtc_settings_finalize;
  object_class->set_property = set_property;
  object_class->get_property = get_property;

  obj_properties[PROP_GOP] = g_param_spec_int("gop",
                                              "Gop",
                                              "Placeholder description.",
                                              25,
                                              500,
                                              120, /* default */
                                              G_PARAM_READWRITE);

  obj_properties[PROP_AUDIO_CODEC] =
          g_param_spec_int("audio_codec",
                           "Audio_codec",
                           "Placeholder description.",
                           0,
                           WEBRTC_SETTINGS_AUDIO_CODEC_LAST - 1,
                           WEBRTC_SETTINGS_AUDIO_CODEC_OPUS, /* default */
                           G_PARAM_READWRITE);

  obj_properties[PROP_COMPRESSION] =
          g_param_spec_int("compression",
                           "Compression",
                           "Placeholder description.",
                           0,
                           100,
                           30,
                           G_PARAM_READWRITE);

  obj_properties[PROP_MAX_BITRATE] =
          g_param_spec_int64("max_bitrate",
                             "Max_bitrate",
                             "Placeholder description.",
                             -1,
                             32000000,
                             -1, /* default */
                             G_PARAM_READWRITE);

  obj_properties[PROP_ADAPTIVE] =
          g_param_spec_boolean("adaptive",
                               "Adaptive",
                               "Placeholder description.",
                               FALSE, /* default */
                               G_PARAM_READWRITE);

  obj_properties[PROP_FORCE_TURN] =
          g_param_spec_boolean("force-turn",
                               "Force TURN relay",
                               "Placeholder description.",
                               FALSE, /* default */
                               G_PARAM_READWRITE);

  g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

static void
webrtc_settings_init(G_GNUC_UNUSED WebrtcSettings *self)
{
  /* initialize all public and private members to reasonable default values.
   * They are all automatically initialized to 0 to begin with. */
}

WebrtcSettings *
webrtc_settings_new(void)
{
  return g_object_new(WEBRTC_TYPE_SETTINGS, NULL);
}

enum webrtc_settings_audio_codec
webrtc_settings_audio_codec(WebrtcSettings *self)
{
  return self->audio_codec;
}

gboolean
webrtc_settings_video_adaptive(WebrtcSettings *self)
{
  return self->adaptive;
}
gint
webrtc_settings_video_compression(WebrtcSettings *self)
{
  return self->compression;
}
gint64
webrtc_settings_video_max_bitrate(WebrtcSettings *self)
{
  return self->max_bitrate;
}
gint
webrtc_settings_video_gop(WebrtcSettings *self)
{
  return self->gop;
}

void
webrtc_settings_set_string(WebrtcSettings *self,
                           GQuark setting,
                           const gchar *val)
{
  const gchar *audio_codec_list[] = AUDIO_CODEC_LIST;
  const gchar *boolean_options_list[] = BOOLEAN_LIST;

  if (setting == WEBRTC_SETTINGS_AUDIO_CODEC) {
    for (guint i = 0; i < G_N_ELEMENTS(audio_codec_list); i++) {
      if (g_strcmp0(audio_codec_list[i], val) == 0) {
        g_object_set(self, "audio_codec", i, NULL);
        g_settings_set_enum(self->settings, "audio-codec", i);
        break;
      }
    }
    return;
  }

  if (setting == WEBRTC_SETTINGS_VIDEO_ADAPTIVE) {
    if (g_strcmp0(boolean_options_list[0], val) == 0) {
      g_object_set(self, "adaptive", FALSE, NULL);
      g_settings_set_boolean(self->settings, "adaptive", FALSE);
    } else {
      g_object_set(self, "adaptive", TRUE, NULL);
      g_settings_set_boolean(self->settings, "adaptive", TRUE);
    }
    return;
  }

  if (setting == WEBRTC_SETTINGS_FORCE_TURN) {
    if (g_strcmp0(boolean_options_list[0], val) == 0) {
      g_object_set(self, "force-turn", FALSE, NULL);
      g_settings_set_boolean(self->settings, "force-turn", FALSE);
    } else {
      g_object_set(self, "force-turn", TRUE, NULL);
      g_settings_set_boolean(self->settings, "force-turn", TRUE);
    }
    return;
  }
}

void
webrtc_settings_set_number(WebrtcSettings *self, GQuark setting, gint64 val)
{
  if (setting == WEBRTC_SETTINGS_VIDEO_MAX_BITRATE) {
    g_object_set(self, "max_bitrate", val, NULL);
    g_settings_set_int64(self->settings, "max-bitrate", val);
  } else if (setting == WEBRTC_SETTINGS_VIDEO_COMPRESSION) {
    g_object_set(self, "compression", val, NULL);
    g_settings_set_int(self->settings, "compression", (gint) val);
  } else if (setting == WEBRTC_SETTINGS_VIDEO_GOP) {
    g_object_set(self, "gop", val, NULL);
    g_settings_set_int(self->settings, "gop", (gint) val);
  }
}

void
webrtc_settings_bind(WebrtcSettings *self)
{
  if (self->settings != NULL) {
    return;
  }

  self->settings = g_settings_new("com.github.jsol.webrtc_player");

  g_object_set(self,
               "audio_codec",
               g_settings_get_enum(self->settings, "audio-codec"),
               NULL);
  g_object_set(self,
               "adaptive",
               g_settings_get_boolean(self->settings, "adaptive"),
               NULL);
  g_object_set(self,
               "max_bitrate",
               g_settings_get_int64(self->settings, "max-bitrate"),
               NULL);
  g_object_set(self,
               "compression",
               g_settings_get_int(self->settings, "compression"),
               NULL);
  g_object_set(self, "gop", g_settings_get_int(self->settings, "gop"), NULL);
}

guint
webrtc_settings_selected(WebrtcSettings *self, GQuark setting)
{
  if (setting == WEBRTC_SETTINGS_AUDIO_CODEC) {
    return self->audio_codec;
  }

  if (setting == WEBRTC_SETTINGS_VIDEO_ADAPTIVE) {
    if (self->adaptive == FALSE) {
      return 0;
    } else {
      return 1;
    }
  }

  if (setting == WEBRTC_SETTINGS_FORCE_TURN) {
    if (self->force_turn == FALSE) {
      return 0;
    } else {
      return 1;
    }
  }

  return 0;
}

gint
webrtc_settings_parse_opts(WebrtcSettings *self, int argc, char **argv)
{
  GError *error = NULL;
  GOptionContext *context;
  gchar *audio;
  gboolean turn;

  /* clang-format off */
  GOptionEntry entries[] = {
    { "output", 'o', 0, G_OPTION_ARG_STRING, &self->output, "Output file", "FILE" },
    { "target", 't', 0, G_OPTION_ARG_STRING, &self->target, "Target device", "TARGET" },
    { "audio", 'a', 0, G_OPTION_ARG_STRING, &audio, "Which audio codec to use (AAC | OPUS | NONE)", "AUDIO" },
    { "force-turn", 'u', 0, G_OPTION_ARG_NONE, &turn, "Forces TURN relay", "TURN" },
    G_OPTION_ENTRY_NULL
  };

  /* clang-format on */

  context = g_option_context_new("-run the WebRTC client");
  g_option_context_add_main_entries(context, entries, NULL);
  /** g_option_context_add_group(context, gtk_get_option_group(TRUE));*/
  if (!g_option_context_parse(context, &argc, &argv, &error)) {
    g_print("option parsing failed: %s\n", error->message);
    return 1;
  }

  if (self->target == NULL) {
    self->target = g_strdup(g_getenv("WEBRTC_TARGET"));
  }

  if (self->output == NULL) {
    self->output = g_strdup(g_getenv("WEBRTC_OUTPUT"));
  }

  if (self->target == NULL) {
    g_print("Target must be set\n");
    return 1;
  }

  if (g_ascii_strncasecmp("any", self->target, 3) == 0) {
    g_clear_pointer(&self->target, g_free);
    g_message("Waiting for any device to connect");
  }

  if (g_ascii_strcasecmp(audio, "aac") == 0) {
    self->audio_codec = WEBRTC_SETTINGS_AUDIO_CODEC_AAC;
  } else if (g_ascii_strcasecmp(audio, "opus") == 0) {
    self->audio_codec = WEBRTC_SETTINGS_AUDIO_CODEC_OPUS;
  } else if (g_ascii_strcasecmp(audio, "none") == 0) {
    self->audio_codec = WEBRTC_SETTINGS_AUDIO_CODEC_NONE;
  } else {
    self->audio_codec = WEBRTC_SETTINGS_AUDIO_CODEC_OPUS;
  }
  self->force_turn = turn;

  return -1;
}

const gchar *
webrtc_settings_get_target(WebrtcSettings *self)
{
  g_return_val_if_fail(self != NULL, NULL);

  return self->target;
}

const gchar *
webrtc_settings_get_output(WebrtcSettings *self)
{
  g_return_val_if_fail(self != NULL, NULL);

  return self->output;
}

gboolean
webrtc_settings_ice_force_turn(WebrtcSettings *self)
{
  g_return_val_if_fail(self != NULL, FALSE);

  return self->force_turn;
}