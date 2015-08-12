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

#define ms_debug(fmt, arg...) do { \
		LOGD(FONT_COLOR_RESET""fmt"", ##arg);     \
	} while(0)

#define ms_info(fmt, arg...) do { \
		LOGI(FONT_COLOR_GREEN""fmt"", ##arg);     \
	} while(0)

#define ms_error(fmt, arg...) do { \
		LOGE(FONT_COLOR_RED""fmt"", ##arg);     \
	} while(0)

#define ms_debug_fenter() do { \
		LOGD(FONT_COLOR_YELLOW"<Enter>");     \
	} while(0)

#define ms_debug_fleave() do { \
		LOGD(FONT_COLOR_PURPLE"<Leave>");     \
	} while(0)

#define ms_retm_if(expr, fmt, arg...) do { \
		if(expr) { \
			LOGE(FONT_COLOR_RED""fmt"", ##arg);     \
			return; \
		} \
	} while(0)

#define ms_retvm_if(expr, val, fmt, arg...) do { \
		if(expr) { \
			LOGE(FONT_COLOR_RED""fmt"", ##arg);     \
			return(val); \
		} \
	} while(0)


#define MS_SAFE_FREE(src)           {if(src) {free(src); src = NULL;}}
#define MS_SAFE_GFREE(src)          {if(src) {g_free(src); src = NULL;}}
#define MS_SAFE_UNREF(src)          {if(src) {gst_object_unref(GST_OBJECT(src)); src = NULL;}}
#define MS_TABLE_SAFE_UNREF(src)    {if(src) {g_hash_table_unref(src); src = NULL;}}

/* Ini Utils */
#define MEDIA_STREAMER_INI_DEFAULT_PATH	"/usr/etc/media_streamer.ini"
#define MEDIA_STREAMER_INI_MAX_STRLEN	100

/**
 * @brief Media Streamer ini settings structure.
 *
 * @since_tizen 3.0
 */
typedef struct __media_streamer_ini {
	/* general */
	gboolean generate_dot;

} media_streamer_ini_t;

/*Test elements*/
#define DEFAULT_VIDEO_TEST_SOURCE           "videotestsrc"
#define DEFAULT_AUDIO_TEST_SOURCE           "audiotestsrc"
#define DEFAULT_FAKE_SINK                   "fakesink"
#define DEFAULT_QUEUE                       "queue"

/* setting default values if each value is not specified in .ini file */
/* general */
#define DEFAULT_GENERATE_DOT                FALSE
#define DEFAULT_AUDIO_SOURCE                "alsasrc"
#define DEFAULT_CAMERA_SOURCE               "v4l2src"
#define DEFAULT_VIDEO_SOURCE                "ximagesrc"
#define DEFAULT_APP_SOURCE                  "appsrc"
#define DEFAULT_AUDIO_SINK                  "pulsesink"
#define DEFAULT_VIDEO_SINK                  "autovideosink"
#define DEFAULT_VIDEO_CONVERT               "videoconvert"
#define DEFAULT_AUDIO_CONVERT               "audioconvert"
#define DEFAULT_AUDIO_RESAMPLE              "audioresample"
#define DEFAULT_APP_SINK                    "appsink"

/* udp streaming */
#define DEFAULT_UDP_SOURCE                  "udpsrc"
#define DEFAULT_UDP_SINK                    "udpsink"
#define DEFAULT_RTP_BIN                     "rtpbin"

/* video format defaults */
#define DEFAULT_VIDEO_ENCODER               "omxh264enc"
#define DEFAULT_VIDEO_DECODER               "omxh264dec"
#define DEFAULT_VIDEO_PARSER                "h263parse"
#define DEFAULT_VIDEO_RTPPAY                "rtph263pay"
#define DEFAULT_VIDEO_RTPDEPAY              "rtph263depay"

/* audio format defaults */
#define DEFAULT_AUDIO_RTPPAY                "rtpL16pay"
#define DEFAULT_AUDIO_RTPDEPAY              "rtpL16depay"

#define MEDIA_STREAMER_DEFAULT_CAMERA_FORMAT "video/x-raw,width=1280,height=720"
#define MEDIA_STREAMER_DEFAULT_AUDIO_FORMAT "audio/x-raw,channels=1,rate=44100,format=S16BE"
#define MEDIA_STREAMER_DEFAULT_ENCODER_FORMAT "video/x-h263,stream-format=byte-stream,profile=high"

#define MS_ELEMENT_IS_OUTPUT(el) g_strrstr(el, "out")
#define MS_ELEMENT_IS_INPUT(el) g_strrstr(el, "in")
#define MS_ELEMENT_IS_AUDIO(el) g_strrstr(el, "audio")
#define MS_ELEMENT_IS_VIDEO(el) g_strrstr(el, "video")

#define MEDIA_STREAMER_DEFAULT_DOT_DIR "/tmp"
#define MEDIA_STREAMER_DEFAULT_INI \
"\
[general] \n\
; generating dot file representing pipeline state \n\
generate dot = no \n\
dot dir = /tmp \n\
\n\
\n\
[sources] \n\
\n\
audio_source = pulsesrc \n\
camera_source = v4l2src \n\
video_source = ximagesrc \n\
udp_source = udpsrc \n\
\n\
\n\
[sinks] \n\
\n\
audio_sink = pulsesink \n\
video_sink = waylandsink \n\
udp_sink = udpsink \n\
\n\
\n\
[h263] \n\
\n\
encoder = avenc_h263 \n\
decoder = avdec_h263 \n\
rtppay = rtph263pay \n\
rtpdepay = rtph263depay \n\
parser = h263parse \n\
\n\
\n\
[h264] \n\
\n\
encoder = omxh264enc \n\
decoder = avdec_h264 \n\
rtppay = rtph264pay \n\
rtpdepay = rtph264depay \n\
parser = h264parse \n\
\n\
\n\
[audio-raw] \n\
\n\
encoder = \n\
decoder = \n\
rtppay = rtpL16pay \n\
rtpdepay = rtpL16depay \n\
\n\
"

/**
 * @brief Load media streamer settings from ini file.
 *
 * @since_tizen 3.0
 */
int __ms_load_ini_settings(media_streamer_ini_t *ini);

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
gchar *__ms_ini_get_string(dictionary *dict, const char *ini_path,
                           char *default_str);

/**
 * @brief Converts Media Format mime type into Caps media format string.
 *
 * @since_tizen 3.0
 */
const gchar *__ms_convert_mime_to_string(media_format_mimetype_e mime);

/**
 * @brief Converts Caps stream format into Media Format mime type.
 *
 * @since_tizen 3.0
 */
media_format_mimetype_e __ms_convert_string_format_to_mime(const char *format_type);

#ifdef __cplusplus
}
#endif

#endif