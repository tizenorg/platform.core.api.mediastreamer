/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __MEDIA_STREAMER_UTIL_H__
#define __MEDIA_STREAMER_UTIL_H__

#include <stdlib.h>
#include <glib.h>
#include <gst/gst.h>
#include <dlog.h>
#include <iniparser.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DLog Utils*/
#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "CAPI_MEDIASTREAMER"

#define FONT_COLOR_RESET    "\033[0m"
#define FONT_COLOR_RED      "\033[31m"
#define FONT_COLOR_GREEN    "\033[32m"
#define FONT_COLOR_YELLOW   "\033[33m"
#define FONT_COLOR_BLUE     "\033[34m"
#define FONT_COLOR_PURPLE   "\033[35m"
#define FONT_COLOR_CYAN     "\033[36m"
#define FONT_COLOR_GRAY     "\033[37m"

#define ms_debug(fmt, arg...)                         \
	do {                                              \
		LOGD(FONT_COLOR_RESET""fmt"", ##arg);         \
	} while (0)

#define ms_info(fmt, arg...)                          \
	do {                                              \
		LOGI(FONT_COLOR_GREEN""fmt"", ##arg);         \
	} while (0)

#define ms_error(fmt, arg...)                         \
	do {                                              \
		LOGE(FONT_COLOR_RED""fmt"", ##arg);           \
	} while (0)

#define ms_debug_fenter()                             \
	do {                                              \
		LOGD(FONT_COLOR_YELLOW"<Enter>");             \
	} while (0)

#define ms_debug_fleave()                             \
	do {                                              \
		LOGD(FONT_COLOR_PURPLE"<Leave>");             \
	} while (0)

#define ms_retm_if(expr, fmt, arg...)                 \
	do {                                              \
		if (expr) {                                   \
			LOGE(FONT_COLOR_RED""fmt"", ##arg);       \
			return;                                   \
		}                                             \
	} while (0)

#define ms_retvm_if(expr, val, fmt, arg...)           \
	do {                                              \
		if (expr) {                                   \
			LOGE(FONT_COLOR_RED""fmt"", ##arg);       \
			return(val);                              \
		}                                             \
	} while (0)

#define MS_SAFE_FREE(src)           {if (src) { free(src); src = NULL; } }
#define MS_SAFE_GFREE(src)          {if (src) { g_free(src); src = NULL; } }
#define MS_SAFE_UNREF(src)          {if (src) { gst_object_unref(GST_OBJECT(src)); src = NULL; } }
#define MS_TABLE_SAFE_UNREF(src)    {if (src) { g_hash_table_unref(src); src = NULL; } }

#define MS_TIME_NONE ((int)-1)

/* Ini Utils */
#ifndef MEDIA_STREAMER_INI_PATH
	#define MEDIA_STREAMER_INI_PATH	"/etc/media_streamer.ini"
#endif

#define MEDIA_STREAMER_INI_MAX_STRLEN	100
#define RTP_STREAM_DISABLED (0)

/**
 * @brief Media Streamer ini settings structure.
 *
 * @since_tizen 3.0
 */
typedef struct __media_streamer_ini {
	/* general */
	gboolean generate_dot;
	gboolean use_decodebin;

} media_streamer_ini_t;

/**
 * @brief Media Streamer signal structure.
 *
 * @since_tizen 3.0
 */
typedef struct {
	GObject *obj;
	gulong signal_id;
} media_streamer_signal_s;

/*Test elements*/
#define DEFAULT_VIDEO_TEST_SOURCE           "videotestsrc"
#define DEFAULT_AUDIO_TEST_SOURCE           "audiotestsrc"
#define DEFAULT_FAKE_SINK                   "fakesink"

/* setting default values if each value is not specified in .ini file */
/* general */
#define DEFAULT_GENERATE_DOT                FALSE
#define DEFAULT_USE_DECODEBIN               FALSE
#define DEFAULT_QUEUE                       "queue"
#define DEFAULT_TYPEFIND                    "typefind"
#define DEFAULT_DECODEBIN                   "decodebin"
#define DEFAULT_AUDIO_SOURCE                "alsasrc"
#define DEFAULT_CAMERA_SOURCE               "v4l2src"
#define DEFAULT_VIDEO_SOURCE                "ximagesrc"
#define DEFAULT_APP_SOURCE                  "appsrc"
#define DEFAULT_AUDIO_SINK                  "pulsesink"
#define DEFAULT_VIDEO_SINK                  "waylandsink"
#define DEFAULT_EVAS_SINK                   "evaspixmapsink"
#define DEFAULT_VIDEO_SCALE                 "videoscale"
#define DEFAULT_VIDEO_CONVERT               "videoconvert"
#define DEFAULT_TEXT_OVERLAY                "textoverlay"
#define DEFAULT_AUDIO_CONVERT               "audioconvert"
#define DEFAULT_AUDIO_RESAMPLE              "audioresample"
#define DEFAULT_APP_SINK                    "appsink"

/* udp streaming */
#define DEFAULT_UDP_SOURCE                  "udpsrc"
#define DEFAULT_FILE_SOURCE                 "filesrc"
#define DEFAULT_HTTP_SOURCE                 "souphttpsrc"
#define DEFAULT_UDP_SINK                    "udpsink"
#define DEFAULT_RTP_BIN                     "rtpbin"

/* video format defaults */
#define DEFAULT_VIDEO_ENCODER               "avenc_h263"
#define DEFAULT_VIDEO_DECODER               "avdec_h263"
#define DEFAULT_VIDEO_PARSER                "h263parse"
#define DEFAULT_VIDEO_RTPPAY                "rtph263pay"
#define DEFAULT_VIDEO_RTPDEPAY              "rtph263depay"

/* audio format defaults */
#define DEFAULT_AUDIO_PARSER                "aacparse"
#define DEFAULT_AUDIO_RTPPAY                "rtpL16pay"
#define DEFAULT_AUDIO_RTPDEPAY              "rtpL16depay"

#define DEFAULT_AUDIO "S16LE"
#define MEDIA_STREAMER_DEFAULT_CAMERA_FORMAT "video/x-raw,width=1280,height=720"
#define MEDIA_STREAMER_DEFAULT_AUDIO_FORMAT "audio/x-raw,channels=1,rate=44100,format="DEFAULT_AUDIO
#define MEDIA_STREAMER_DEFAULT_ENCODER_FORMAT(format) "video/x-"format",stream-format=byte-stream"

#define MS_ELEMENT_IS_OUTPUT(el) g_strrstr(el, "out")
#define MS_ELEMENT_IS_INPUT(el) g_strrstr(el, "in")
#define MS_ELEMENT_IS_AUDIO(el) g_strrstr(el, "audio")
#define MS_ELEMENT_IS_VIDEO(el) g_strrstr(el, "video")
#define MS_ELEMENT_IS_IMAGE(el) g_strrstr(el, "image")

#define MS_RTP_PAD_VIDEO_IN "video_in"
#define MS_RTP_PAD_AUDIO_IN "audio_in"
#define MS_RTP_PAD_VIDEO_OUT "video_out"
#define MS_RTP_PAD_AUDIO_OUT "audio_out"
#define MEDIA_STREAMER_PARAM_VIDEO_IN_FORMAT MS_RTP_PAD_VIDEO_IN"_format"
#define MEDIA_STREAMER_PARAM_AUDIO_IN_FORMAT MS_RTP_PAD_AUDIO_IN"_format"

#define MS_ELEMENT_IS_RTP(el) g_strrstr(el, "rtp_container")
#define MS_ELEMENT_IS_TEXT(el) g_strrstr(el, "text")
#define MS_ELEMENT_IS_ENCODER(el) g_strrstr(el, "encoder")
#define MS_ELEMENT_IS_DECODER(el) g_strrstr(el, "decoder")

#define MEDIA_STREAMER_DEFAULT_DOT_DIR "/tmp"

#define MS_BIN_FOREACH_ELEMENTS(bin, fn, streamer) \
	do { \
		GstIterator *iter = gst_bin_iterate_elements(GST_BIN(bin)); \
		if (gst_iterator_foreach(iter, fn, streamer) != GST_ITERATOR_DONE) \
			ms_error("Error while iterating elements in bin [%s]!", GST_ELEMENT_NAME(bin)); \
		gst_iterator_free(iter); \
	} while (0)

#define MS_BIN_UNPREPARE(bin) \
	if (!__ms_bin_remove_elements(ms_streamer, bin)) {\
		ms_debug("Got a few errors during unprepare [%s] bin.", GST_ELEMENT_NAME(bin));\
		ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;\
	}

#define MS_SET_INT_RTP_PARAM(obj, key, value) \
	do { \
		GValue *val = g_malloc0(sizeof(GValue)); \
		g_value_init(val, G_TYPE_INT); \
		g_value_set_int(val, value); \
		g_object_set_data_full(G_OBJECT(obj), key, (gpointer)val, __ms_rtp_param_value_destroy); \
	} while (0)

#define MS_SET_INT_STATIC_STRING_PARAM(obj, key, value) \
	do { \
		GValue *val = g_malloc0(sizeof(GValue)); \
		g_value_init(val, G_TYPE_STRING); \
		g_value_set_static_string(val, value); \
		g_object_set_data_full(G_OBJECT(obj), key, (gpointer)val, __ms_rtp_param_value_destroy); \
	} while (0)

#define MS_SET_INT_CAPS_PARAM(obj, key, value) \
	do { \
		GValue *val = g_malloc0(sizeof(GValue)); \
		g_value_init(val, GST_TYPE_CAPS); \
		gst_value_set_caps(val, value); \
		g_object_set_data_full(G_OBJECT(obj), key, (gpointer)val, __ms_rtp_param_value_destroy); \
	} while (0)

/**
 * @brief Loads media streamer settings from ini file.
 *        The default values will be used if error has occurred.
 *
 * @since_tizen 3.0
 */
void __ms_load_ini_settings(media_streamer_ini_t *ini);

/**
 * @brief Load settings from ini file into dictionary object.
 *
 * @since_tizen 3.0
 */
gboolean __ms_load_ini_dictionary(dictionary **dict);

/**
 * @brief Destroys ini dictionary object and frees all resources.
 *
 * @since_tizen 3.0
 */
gboolean __ms_destroy_ini_dictionary(dictionary *dict);

/**
 * @brief Read and copy string reading from ini file.
 *
 * @since_tizen 3.0
 */
gchar *__ms_ini_get_string(dictionary *dict, const char *ini_path, char *default_str);

/**
 * @brief Converts Media Format mime type into Caps media format string.
 *
 * @since_tizen 3.0
 */
const gchar *__ms_convert_mime_to_string(media_format_mimetype_e mime);

/**
 * @brief Converts Media Format mime type into rtp media type.
 *
 * @since_tizen 3.0
 */
const gchar *__ms_convert_mime_to_rtp_format(media_format_mimetype_e mime);

/**
 * @brief Converts Caps stream format into Media Format mime type.
 *
 * @since_tizen 3.0
 */
media_format_mimetype_e __ms_convert_string_format_to_mime(const char *format_type);

/**
 * @brief Creates Media streamer signal structure,
 *        connects it to object and appends it to signal list.
 *
 * @since_tizen 3.0
 */
void __ms_signal_create(GList **sig_list, GstElement *obj, const char *sig_name, GCallback cb, gpointer user_data);

/**
 * @brief Disconnects signal from object and
 * destroys Media streamer signal object.
 *
 * @since_tizen 3.0
 */
void __ms_signal_destroy(void *data);

/**
 * @brief Destroys the which set as rtp node parameter.
 *
 * @since_tizen 3.0
 */
void __ms_rtp_param_value_destroy(gpointer data);

#ifdef __cplusplus
}
#endif

#endif
