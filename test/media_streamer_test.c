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

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gprintf.h>

#include <media_streamer.h>

typedef enum {
	MENU_STATE_UNKNOWN = 0,
	MENU_STATE_MAIN_MENU,
	MENU_STATE_BROADCAST_MENU,
	MENU_STATE_VOIP_MENU,
	MENU_STATE_PLAYING_MENU,
	MENU_STATE_PRESET_MENU
} menu_state_e;

typedef enum {
	SUBMENU_STATE_UNKNOWN,
	SUBMENU_STATE_GETTING_IP,
	SUBMENU_STATE_GETTING_SEEK_POS,
	SUBMENU_STATE_GETTING_VIDEOFILE_URI,
	SUBMENU_STATE_GETTING_SUBFILE_URI,
	SUBMENU_STATE_AUTOPLUG,
	SUBMENU_STATE_SCENARIO,
	SUBMENU_STATE_PLAYING_SCENARIO,
	SUBMENU_STATE_FORMAT
} submenu_state_e;

#define SECOND_VOIP_MASK 0x8
#define DOUBLE_STREAMER_MASK 0x10

typedef enum {
	PRESET_UNKNOWN = 0,
	PRESET_RTP_STREAMER = 0x01,
	PRESET_RTP_CLIENT = 0x02,
	PRESET_VOIP = PRESET_RTP_STREAMER | PRESET_RTP_CLIENT,
	PRESET_VOIP_2 = PRESET_VOIP | SECOND_VOIP_MASK,
	PRESET_DOUBLE_VOIP_SERVER = PRESET_RTP_STREAMER | DOUBLE_STREAMER_MASK,
	PRESET_DOUBLE_VOIP_SERVER_2 = PRESET_RTP_STREAMER | DOUBLE_STREAMER_MASK | SECOND_VOIP_MASK,
	PRESET_DOUBLE_VOIP_CLIENT = PRESET_RTP_CLIENT | DOUBLE_STREAMER_MASK,
	PRESET_DOUBLE_VOIP_CLIENT_2 = PRESET_RTP_CLIENT | DOUBLE_STREAMER_MASK | SECOND_VOIP_MASK
} preset_type_e;

typedef enum {
	SCENARIO_MODE_UNKNOWN,
	SCENARIO_MODE_CAMERA_SCREEN,
	SCENARIO_MODE_MICROPHONE_PHONE,
	SCENARIO_MODE_FULL_VIDEO_AUDIO,
	SCENARIO_MODE_VIDEOTEST_SCREEN,
	SCENARIO_MODE_AUDIOTEST_PHONE,
	SCENARIO_MODE_TEST_VIDEO_AUDIO,
	SCENARIO_MODE_FILE_STREAM_VIDEO_AUDIO,
	SCENARIO_MODE_FILE_PLAY_VIDEO_AUDIO,
	SCENARIO_MODE_FILE_SUBTITLE_VIDEO_AUDIO,
	SCENARIO_MODE_HTTP_VIDEO_AUDIO,
	SCENARIO_MODE_APPSRC_APPSINK
} scenario_mode_e;

#define PACKAGE "media_streamer_test"

#define MAX_STRING_LEN    2048
#define DEFAULT_IP_ADDR   "127.0.0.1"
#define DEFAULT_SEEK_POS   0
#define MSEC_MULTIPLIER   1000
#define MAX_NODES_COUNT   50

#define VIDEO_PORT 5000
#define AUDIO_PORT 6000

#define VIDEO_WIDTH       352
#define VIDEO_HIGHT       288
#define VIDEO_AVG_BPS     1
#define VIDEO_MAX_BPS     3
#define AUDIO_CHANNEL     1
#define AUDIO_SAMPLERATE  8000

/*---------------------------------------------------------------------------
|    GLOBAL VARIABLE DEFINITIONS:                     |
---------------------------------------------------------------------------*/

static media_streamer_h g_media_streamer;
static media_streamer_h g_media_streamer_2;
static media_streamer_h current_media_streamer = &g_media_streamer;

media_streamer_node_h g_nodes[MAX_NODES_COUNT] = { 0, };

int g_node_counter = 0;
#define APPEND_NODE(x) {g_nodes[g_node_counter++] = x; }

GMainLoop *g_loop;

gchar *g_broadcast_address = NULL;
int g_seek_pos = 0;
int g_time = 0;
gchar *g_uri = NULL;
gchar *g_sub_uri = NULL;

menu_state_e g_menu_state = MENU_STATE_MAIN_MENU;
submenu_state_e g_sub_menu_state = SUBMENU_STATE_UNKNOWN;
preset_type_e g_menu_preset = PRESET_UNKNOWN;
scenario_mode_e g_scenario_mode = SCENARIO_MODE_UNKNOWN;

gboolean g_autoplug_mode = FALSE;
gboolean g_video_is_on = FALSE;
gboolean g_audio_is_on = FALSE;

media_format_h vfmt_raw = NULL;
media_format_h vfmt_encoded = NULL;
media_format_h afmt_raw = NULL;
media_format_h afmt_encoded = NULL;

static void streamer_error_cb(media_streamer_h streamer, media_streamer_error_e error, void *user_data)
{
	g_print("Media Streamer posted error [%d] \n", error);
}

static void streamer_seek_cb(void *user_data)
{
	g_print("Media Streamer seeked to required position \n");

	int current_time = 0;

	media_streamer_get_play_position(current_media_streamer, &current_time);

	g_print("Current play position [%02d:%02d:%03d] \n", current_time / (1000 * 60), (current_time / 1000) % 60, current_time % 1000);
}

static void _create(media_streamer_h *streamer)
{
	g_print("== create \n");
	int ret = MEDIA_STREAMER_ERROR_NONE;

	if (*streamer != NULL)
		return;

	ret = media_streamer_create(streamer);

	if (ret != MEDIA_STREAMER_ERROR_NONE) {
		g_print("Fail to create media streamer");
		return;
	}

	media_streamer_set_error_cb(*streamer, streamer_error_cb, NULL);
}

static void _prepare(void)
{
	g_print("== prepare \n");
	int ret = MEDIA_STREAMER_ERROR_NONE;

	ret = media_streamer_prepare(current_media_streamer);
	if (ret != MEDIA_STREAMER_ERROR_NONE) {
		g_print("Fail to prepare media streamer");
		return;
	}
	g_print("== success prepare \n");
}

static void _unprepare(void)
{
	g_print("== unprepare \n");
	int ret = MEDIA_STREAMER_ERROR_NONE;

	ret = media_streamer_unprepare(current_media_streamer);
	if (ret != MEDIA_STREAMER_ERROR_NONE) {
		g_print("Fail to unprepare media streamer");
		return;
	}
	g_print("== success unprepare \n");
}

static void _play(void)
{
	g_print("== play \n");
	int ret = MEDIA_STREAMER_ERROR_NONE;

	ret = media_streamer_play(current_media_streamer);
	if (ret != MEDIA_STREAMER_ERROR_NONE) {
		g_print("Fail to play media streamer");
		return;
	}
	g_print("== success play \n");
}

static void _pause(void)
{
	g_print("== pause \n");
	int ret = MEDIA_STREAMER_ERROR_NONE;

	ret = media_streamer_pause(current_media_streamer);
	if (ret != MEDIA_STREAMER_ERROR_NONE) {
		g_print("Fail to pause media streamer");
		return;
	}
	g_print("== success pause \n");
}

static void _seek(void)
{
	g_print("== seek \n");
	int ret = MEDIA_STREAMER_ERROR_NONE;

	ret = media_streamer_set_play_position(current_media_streamer, g_time, TRUE, streamer_seek_cb, current_media_streamer);

	if (ret != MEDIA_STREAMER_ERROR_NONE) {
		g_print("Fail to seek media streamer");
		return;
	}
	g_print("== success seek \n");
}

static void _stop(void)
{
	g_print("== stop \n");
	int ret = MEDIA_STREAMER_ERROR_NONE;

	ret = media_streamer_stop(current_media_streamer);
	if (ret != MEDIA_STREAMER_ERROR_NONE) {
		g_print("Fail to stop media streamer");
		return;
	}
	g_print("== success stop \n");
}

static void _destroy(media_streamer_h streamer)
{
	g_print("== destroy \n");
	int ret = MEDIA_STREAMER_ERROR_NONE;

	if (streamer == NULL) {
		g_print("media streamer already destroyed");
		return;
	}

	ret = media_streamer_destroy(streamer);
	if (ret != MEDIA_STREAMER_ERROR_NONE) {
		g_print("Fail to destroy media streamer");
		return;
	}

	if (current_media_streamer == g_media_streamer)
		g_media_streamer = NULL;
	else
		g_media_streamer_2 = NULL;

	current_media_streamer = NULL;

	g_print("== success destroy \n");
}

static void create_formats(void)
{
	if (!vfmt_raw || !vfmt_encoded || !afmt_raw)
		g_print("Formats already created!");

	/* Define video raw format */
	media_format_create(&vfmt_raw);
	if (media_format_set_video_mime(vfmt_raw, MEDIA_FORMAT_I420) != MEDIA_FORMAT_ERROR_NONE)
		g_print("media_format_set_video_mime failed!");

	media_format_set_video_width(vfmt_raw, VIDEO_WIDTH);
	media_format_set_video_height(vfmt_raw, VIDEO_HIGHT);
	media_format_set_video_avg_bps(vfmt_raw, VIDEO_AVG_BPS);
	media_format_set_video_max_bps(vfmt_raw, VIDEO_MAX_BPS);

	/* Define encoded video format */
	media_format_create(&vfmt_encoded);
	if (media_format_set_video_mime(vfmt_encoded, MEDIA_FORMAT_H263) != MEDIA_FORMAT_ERROR_NONE)
		g_print("media_format_set_video_mime failed!");

	media_format_set_video_width(vfmt_encoded, VIDEO_WIDTH);
	media_format_set_video_height(vfmt_encoded, VIDEO_HIGHT);
	media_format_set_video_avg_bps(vfmt_encoded, VIDEO_AVG_BPS);
	media_format_set_video_max_bps(vfmt_encoded, VIDEO_MAX_BPS);

	/* Define audio raw format */
	media_format_create(&afmt_raw);
	if (media_format_set_audio_mime(afmt_raw, MEDIA_FORMAT_PCM) != MEDIA_FORMAT_ERROR_NONE)
		g_print("media_format_set_audio_mime failed!");

	media_format_set_audio_channel(afmt_raw, AUDIO_CHANNEL);
	media_format_set_audio_samplerate(afmt_raw, AUDIO_SAMPLERATE);

	/* Define audio encoded format */
	media_format_create(&afmt_encoded);
	if (media_format_set_audio_mime(afmt_encoded, MEDIA_FORMAT_AMR_NB) != MEDIA_FORMAT_ERROR_NONE)
		g_print("media_format_set_audio_mime failed!");

	media_format_set_audio_channel(afmt_encoded, AUDIO_CHANNEL);
	media_format_set_audio_samplerate(afmt_encoded, AUDIO_SAMPLERATE);
}

static void set_rtp_params(media_streamer_node_h rtp_node, const char *ip, int video_port, int audio_port, gboolean port_reverse)
{
	bundle *params = bundle_create();

	if (g_audio_is_on) {
		gchar *audio_src_port = g_strdup_printf("%d", port_reverse ? (audio_port + 5) : audio_port);
		gchar *audio_sink_port = g_strdup_printf("%d", port_reverse ? audio_port : (audio_port + 5));

		if (g_menu_preset & PRESET_RTP_STREAMER)
			bundle_add_str(params, MEDIA_STREAMER_PARAM_AUDIO_OUT_PORT, audio_sink_port);

		if (g_menu_preset & PRESET_RTP_CLIENT)
			bundle_add_str(params, MEDIA_STREAMER_PARAM_AUDIO_IN_PORT, audio_src_port);

		g_free(audio_src_port);
		g_free(audio_sink_port);
	}

	if (g_video_is_on) {
		char *video_src_port = g_strdup_printf("%d", port_reverse ? (video_port + 5) : video_port);
		char *video_sink_port = g_strdup_printf("%d", port_reverse ? video_port : (video_port + 5));

		if (g_menu_preset & PRESET_RTP_STREAMER)
			bundle_add_str(params, MEDIA_STREAMER_PARAM_VIDEO_OUT_PORT, video_sink_port);

		if (g_menu_preset & PRESET_RTP_CLIENT)
			bundle_add_str(params, MEDIA_STREAMER_PARAM_VIDEO_IN_PORT, video_src_port);

		g_free(video_src_port);
		g_free(video_sink_port);
	}

	bundle_add_str(params, MEDIA_STREAMER_PARAM_HOST, ip);

	media_streamer_node_set_params(rtp_node, params);
	media_streamer_node_set_pad_format(rtp_node, "video_in", vfmt_encoded);
	media_streamer_node_set_pad_format(rtp_node, "audio_in", afmt_encoded);

	bundle_free(params);
}

static void _create_file_playing()
{
	media_streamer_node_h file_src = NULL;
	media_streamer_node_create_src(MEDIA_STREAMER_NODE_SRC_TYPE_FILE, &file_src);
	media_streamer_node_set_param(file_src, MEDIA_STREAMER_PARAM_URI, g_uri);
	media_streamer_node_add(current_media_streamer, file_src);
	APPEND_NODE(file_src);

	/*********************** videosink *********************************** */
	media_streamer_node_h video_sink = NULL;
	media_streamer_node_create_sink(MEDIA_STREAMER_NODE_SINK_TYPE_OVERLAY, &video_sink);
	media_streamer_node_add(current_media_streamer, video_sink);
	APPEND_NODE(video_sink);

	/*********************** audiosink *********************************** */
	media_streamer_node_h audio_sink = NULL;
	media_streamer_node_create_sink(MEDIA_STREAMER_NODE_SINK_TYPE_AUDIO, &audio_sink);
	media_streamer_node_add(current_media_streamer, audio_sink);
	APPEND_NODE(audio_sink);
}

static void _create_file_sub_playing()
{
	media_streamer_node_h file_sub_src = NULL;
	media_streamer_node_create_src(MEDIA_STREAMER_NODE_SRC_TYPE_FILE, &file_sub_src);
	media_streamer_node_set_param(file_sub_src, MEDIA_STREAMER_PARAM_URI, g_uri);
	media_streamer_node_add(current_media_streamer, file_sub_src);
	APPEND_NODE(file_sub_src);

	media_streamer_node_h txt_src = NULL;
	media_streamer_node_create_src(MEDIA_STREAMER_NODE_SRC_TYPE_FILE, &txt_src);
	media_streamer_node_set_param(txt_src, MEDIA_STREAMER_PARAM_URI, g_sub_uri);
	media_streamer_node_add(current_media_streamer, txt_src);
	APPEND_NODE(txt_src);

	/*********************** videosink *********************************** */
	media_streamer_node_h video_sink = NULL;
	media_streamer_node_create_sink(MEDIA_STREAMER_NODE_SINK_TYPE_OVERLAY, &video_sink);
	media_streamer_node_add(current_media_streamer, video_sink);
	APPEND_NODE(video_sink);

	/*********************** audiosink *********************************** */
	media_streamer_node_h audio_sink = NULL;
	media_streamer_node_create_sink(MEDIA_STREAMER_NODE_SINK_TYPE_AUDIO, &audio_sink);
	media_streamer_node_add(current_media_streamer, audio_sink);
	APPEND_NODE(audio_sink);
}

static void _create_file_streaming()
{
	g_print("\n _create_file_playing \n");
	media_streamer_node_h file_src = NULL;
	media_streamer_node_create_src(MEDIA_STREAMER_NODE_SRC_TYPE_FILE, &file_src);
	media_streamer_node_set_param(file_src, MEDIA_STREAMER_PARAM_URI, g_uri);
	media_streamer_node_add(current_media_streamer, file_src);
	APPEND_NODE(file_src);
}

static void _create_http_playing()
{
	media_streamer_node_h http_src = NULL;
	media_streamer_node_create_src(MEDIA_STREAMER_NODE_SRC_TYPE_HTTP, &http_src);
	media_streamer_node_set_param(http_src, MEDIA_STREAMER_PARAM_URI, g_uri);
	media_streamer_node_add(current_media_streamer, http_src);
	APPEND_NODE(http_src);

	/*********************** videosink *********************************** */
	media_streamer_node_h video_sink = NULL;
	media_streamer_node_create_sink(MEDIA_STREAMER_NODE_SINK_TYPE_OVERLAY, &video_sink);
	media_streamer_node_add(current_media_streamer, video_sink);
	APPEND_NODE(video_sink);

	/*********************** audiosink *********************************** */
	media_streamer_node_h audio_sink = NULL;
	media_streamer_node_create_sink(MEDIA_STREAMER_NODE_SINK_TYPE_AUDIO, &audio_sink);
	media_streamer_node_add(current_media_streamer, audio_sink);
	APPEND_NODE(audio_sink);
}

static void _create_rtp_streamer(media_streamer_node_h rtp_bin)
{
	g_print("== _create_rtp_streamer \n");

	if (g_video_is_on) {
		/*********************** video source *********************************** */
		media_streamer_node_h video_src = NULL;

		if (g_scenario_mode == SCENARIO_MODE_CAMERA_SCREEN || g_scenario_mode == SCENARIO_MODE_FULL_VIDEO_AUDIO)
			media_streamer_node_create_src(MEDIA_STREAMER_NODE_SRC_TYPE_CAMERA, &video_src);
		else if (g_scenario_mode == SCENARIO_MODE_VIDEOTEST_SCREEN || g_scenario_mode == SCENARIO_MODE_TEST_VIDEO_AUDIO)
			media_streamer_node_create_src(MEDIA_STREAMER_NODE_SRC_TYPE_VIDEO_TEST, &video_src);

		media_streamer_node_add(current_media_streamer, video_src);
		APPEND_NODE(video_src);

		/*********************** encoder **************************************** */
		media_streamer_node_h video_enc = NULL;
		media_streamer_node_create(MEDIA_STREAMER_NODE_TYPE_VIDEO_ENCODER, vfmt_raw, vfmt_encoded, &video_enc);
		media_streamer_node_add(current_media_streamer, video_enc);
		APPEND_NODE(video_enc);

		/*********************** videopay *************************************** */
		media_streamer_node_h video_pay = NULL;
		media_streamer_node_create(MEDIA_STREAMER_NODE_TYPE_VIDEO_PAY, vfmt_encoded, NULL, &video_pay);
		media_streamer_node_add(current_media_streamer, video_pay);
		APPEND_NODE(video_pay);

		/*====================Linking Video Streamer=========================== */
		media_streamer_node_link(video_src, "src", video_enc, "sink");
		media_streamer_node_link(video_enc, "src", video_pay, "sink");
		media_streamer_node_link(video_pay, "src", rtp_bin, "video_in");
		/*====================================================================== */

		g_print("== success streamer video part \n");
	}

	if (g_audio_is_on) {
		/*********************** audiosrc *********************************** */
		media_streamer_node_h audio_src = NULL;

		if (g_scenario_mode == SCENARIO_MODE_MICROPHONE_PHONE || g_scenario_mode == SCENARIO_MODE_FULL_VIDEO_AUDIO)
			media_streamer_node_create_src(MEDIA_STREAMER_NODE_SRC_TYPE_AUDIO_CAPTURE, &audio_src);
		else if (g_scenario_mode == SCENARIO_MODE_AUDIOTEST_PHONE || g_scenario_mode == SCENARIO_MODE_TEST_VIDEO_AUDIO)
			media_streamer_node_create_src(MEDIA_STREAMER_NODE_SRC_TYPE_AUDIO_TEST, &audio_src);

		media_streamer_node_add(current_media_streamer, audio_src);
		APPEND_NODE(audio_src);

		/*********************** audioencoder *********************************** */
		media_streamer_node_h audio_enc = NULL;
		media_streamer_node_create(MEDIA_STREAMER_NODE_TYPE_AUDIO_ENCODER, afmt_raw, afmt_encoded, &audio_enc);
		media_streamer_node_add(current_media_streamer, audio_enc);
		APPEND_NODE(audio_enc);

		/*********************** rtpamrpay *********************************** */
		media_streamer_node_h audio_pay = NULL;
		media_streamer_node_create(MEDIA_STREAMER_NODE_TYPE_AUDIO_PAY, afmt_encoded, NULL, &audio_pay);
		media_streamer_node_add(current_media_streamer, audio_pay);
		APPEND_NODE(audio_pay);

		/*====================Linking Audio Streamer========================== */
		media_streamer_node_link(audio_src, "src", audio_enc, "sink");
		media_streamer_node_link(audio_enc, "src", audio_pay, "sink");
		media_streamer_node_link(audio_pay, "src", rtp_bin, "audio_in");
		/*====================================================================== */

		g_print("== success streamer audio part \n");
	}
}

static void _create_rtp_streamer_autoplug(media_streamer_node_h rtp_bin)
{
	g_print("== _create_rtp_streamer_autoplug \n");

	if (g_video_is_on) {
		/*********************** video source *********************************** */
		media_streamer_node_h video_src = NULL;

		if (g_scenario_mode == SCENARIO_MODE_CAMERA_SCREEN || g_scenario_mode == SCENARIO_MODE_FULL_VIDEO_AUDIO)
			media_streamer_node_create_src(MEDIA_STREAMER_NODE_SRC_TYPE_CAMERA, &video_src);
		else if (g_scenario_mode == SCENARIO_MODE_VIDEOTEST_SCREEN || g_scenario_mode == SCENARIO_MODE_TEST_VIDEO_AUDIO)
			media_streamer_node_create_src(MEDIA_STREAMER_NODE_SRC_TYPE_VIDEO_TEST, &video_src);

		media_streamer_node_add(current_media_streamer, video_src);
		APPEND_NODE(video_src);

		g_print("== success streamer_autoplug video part \n");
	}

	if (g_audio_is_on) {
		/*********************** audiosrc *********************************** */
		media_streamer_node_h audio_src = NULL;

		if (g_scenario_mode == SCENARIO_MODE_MICROPHONE_PHONE || g_scenario_mode == SCENARIO_MODE_FULL_VIDEO_AUDIO)
			media_streamer_node_create_src(MEDIA_STREAMER_NODE_SRC_TYPE_AUDIO_CAPTURE, &audio_src);
		else if (g_scenario_mode == SCENARIO_MODE_AUDIOTEST_PHONE || g_scenario_mode == SCENARIO_MODE_TEST_VIDEO_AUDIO)
			media_streamer_node_create_src(MEDIA_STREAMER_NODE_SRC_TYPE_AUDIO_TEST, &audio_src);

		media_streamer_node_add(current_media_streamer, audio_src);
		APPEND_NODE(audio_src);

		g_print("== success streamer_autoplug audio part \n");
	}
}

static void _create_rtp_client(media_streamer_node_h rtp_bin)
{
	g_print("== _create_rtp_client \n");

	if (g_video_is_on) {

		/* ********************** video_depay ********************************** */
		media_streamer_node_h video_depay = NULL;
		media_streamer_node_create(MEDIA_STREAMER_NODE_TYPE_VIDEO_DEPAY, NULL, vfmt_encoded, &video_depay);
		media_streamer_node_add(current_media_streamer, video_depay);
		APPEND_NODE(video_depay);

		/* ********************** videodec ************************************ */
		media_streamer_node_h video_dec = NULL;
		media_streamer_node_create(MEDIA_STREAMER_NODE_TYPE_VIDEO_DECODER, vfmt_encoded, vfmt_raw, &video_dec);
		media_streamer_node_add(current_media_streamer, video_dec);
		APPEND_NODE(video_dec);

		/* ********************** videoqueue ************************************ */
		media_streamer_node_h video_queue = NULL;
		media_streamer_node_create(MEDIA_STREAMER_NODE_TYPE_QUEUE, NULL, NULL, &video_queue);
		media_streamer_node_add(current_media_streamer, video_queue);
		APPEND_NODE(video_queue);

		/* ********************** videosink *********************************** */
		media_streamer_node_h video_sink = NULL;
		media_streamer_node_create_sink(MEDIA_STREAMER_NODE_SINK_TYPE_OVERLAY, &video_sink);
		media_streamer_node_add(current_media_streamer, video_sink);
		APPEND_NODE(video_sink);

		/* ====================Linking Video Client=========================== */
		media_streamer_node_link(video_depay, "src", video_dec, "sink");
		media_streamer_node_link(video_dec, "src", video_queue, "sink");
		media_streamer_node_link(video_queue, "src", video_sink, "sink");

		g_print("== success client video part \n");
	}

	if (g_audio_is_on) {

		/* ********************** audiodepay ********************************** */
		media_streamer_node_h audio_depay = NULL;
		media_streamer_node_create(MEDIA_STREAMER_NODE_TYPE_AUDIO_DEPAY, NULL, NULL, &audio_depay);
		media_streamer_node_add(current_media_streamer, audio_depay);
		APPEND_NODE(audio_depay);

		/* ********************** audiodecoder ****************************** */
		media_streamer_node_h audio_dec = NULL;
		media_streamer_node_create(MEDIA_STREAMER_NODE_TYPE_AUDIO_DECODER, NULL, NULL, &audio_dec);
		media_streamer_node_add(current_media_streamer, audio_dec);
		APPEND_NODE(audio_dec);

		/* ********************** audioqueue ********************************** */
		media_streamer_node_h audio_queue = NULL;
		media_streamer_node_create(MEDIA_STREAMER_NODE_TYPE_QUEUE, NULL, NULL, &audio_queue);
		media_streamer_node_add(current_media_streamer, audio_queue);
		APPEND_NODE(audio_queue);

		/* ********************** audiosink *********************************** */
		media_streamer_node_h audio_sink = NULL;
		media_streamer_node_create_sink(MEDIA_STREAMER_NODE_SINK_TYPE_AUDIO, &audio_sink);
		media_streamer_node_add(current_media_streamer, audio_sink);
		APPEND_NODE(audio_sink);

		/* ====================Linking Audio Client=========================== */
		media_streamer_node_link(audio_depay, "src", audio_dec, "sink");
		media_streamer_node_link(audio_dec, "src", audio_queue, "sink");
		media_streamer_node_link(audio_queue, "src", audio_sink, "sink");
		/* =================================================================== */

		g_print("== success client audio part \n");
	}
}

static void _create_rtp_client_autoplug(media_streamer_node_h rtp_bin)
{
	g_print("== _create_rtp_client_autoplug \n");

	if (g_video_is_on) {

		/* ********************** videosink *********************************** */
		media_streamer_node_h video_sink = NULL;
		media_streamer_node_create_sink(MEDIA_STREAMER_NODE_SINK_TYPE_OVERLAY, &video_sink);
		media_streamer_node_add(current_media_streamer, video_sink);
		APPEND_NODE(video_sink);

		g_print("== success client_autoplug video part \n");
	}

	if (g_audio_is_on) {

		/* ********************** audiosink *********************************** */
		media_streamer_node_h audio_sink = NULL;
		media_streamer_node_create_sink(MEDIA_STREAMER_NODE_SINK_TYPE_AUDIO, &audio_sink);
		media_streamer_node_add(current_media_streamer, audio_sink);
		APPEND_NODE(audio_sink);

		g_print("== success client_autoplug audio part \n");
	}
}

static media_streamer_node_h _create_rtp(int video_port, int audio_port, gboolean second_client)
{
	g_print("== create rtp node for current preset \n");

	/* ********************** rtpbin *********************************** */
	media_streamer_node_h rtp_bin = NULL;
	media_streamer_node_create(MEDIA_STREAMER_NODE_TYPE_RTP, NULL, NULL, &rtp_bin);
	set_rtp_params(rtp_bin, g_broadcast_address, video_port, audio_port, second_client);
	media_streamer_node_add(current_media_streamer, rtp_bin);
	APPEND_NODE(rtp_bin);
	return rtp_bin;
}

/* Application source callback */
static void buffer_status_cb(media_streamer_node_h node, media_streamer_custom_buffer_status_e status, void *user_data)
{

	static int count = 0;

	/* Try send only 10 packets */
	if (status == MEDIA_STREAMER_CUSTOM_BUFFER_UNDERRUN && count < 10) {
		g_print("Buffer status cb got underflow\n");

		char *test = g_strdup_printf("[%d]This is buffer_status_cb test!", count);
		guint64 size = strlen(test);

		media_packet_h packet;
		media_packet_create_from_external_memory(vfmt_encoded, (void *)test, size, NULL, NULL, &packet);
		media_streamer_node_push_packet(node, packet);
		count++;

		media_packet_destroy(packet);
		g_free(test);
	} else {
		media_streamer_node_push_packet(node, NULL);
		g_print("Buffer status cb got overflow\n");
	}
}

/* Application sink callbacks */
static void new_buffer_cb(media_streamer_node_h node, void *user_data)
{
	char *received_data = NULL;
	media_packet_h packet;

	media_streamer_node_pull_packet(node, &packet);
	media_packet_get_buffer_data_ptr(packet, (void **)&received_data);
	g_print("Received new packet from appsink with data [%s]\n", received_data);

	media_packet_destroy(packet);
}

static void eos_cb(media_streamer_node_h node, void *user_data)
{
	g_print("Got EOS cb from appsink\n");
}

static void _create_app_test()
{
	g_print("== _create_appsrc \n");

	/* ********************** app_src *********************************** */
	media_streamer_node_h app_src = NULL;
	media_streamer_node_create_src(MEDIA_STREAMER_NODE_SRC_TYPE_CUSTOM, &app_src);
	media_streamer_node_add(current_media_streamer, app_src);
	APPEND_NODE(app_src);

	/* ********************** app_sink *********************************** */
	media_streamer_node_h app_sink = NULL;
	media_streamer_node_create_sink(MEDIA_STREAMER_NODE_SINK_TYPE_CUSTOM, &app_sink);
	media_streamer_node_set_pad_format(app_sink, "sink", vfmt_raw);
	media_streamer_node_add(current_media_streamer, app_sink);
	APPEND_NODE(app_sink);

	/*====================Linking ======================================== */
	media_streamer_node_link(app_src, "src", app_sink, "sink");
	/*====================================================================== */

	media_streamer_src_set_buffer_status_cb(app_src, buffer_status_cb, NULL);
	media_streamer_sink_set_data_ready_cb(app_sink, new_buffer_cb, NULL);
	media_streamer_sink_set_eos_cb(app_sink, eos_cb, NULL);

	g_print("== success appsrc part \n");
}

/***************************************************************/
/**  Testsuite */
/***************************************************************/

/*
 * Function resets menu state to the main menu state
 * and cleans media streamer handle.
 */
void reset_current_menu_state(void)
{
	g_menu_state = MENU_STATE_MAIN_MENU;
	g_autoplug_mode = FALSE;
	current_media_streamer = NULL;

	if (g_media_streamer) {
		_destroy(g_media_streamer);
		g_media_streamer = NULL;
	}

	if (g_media_streamer_2) {
		_destroy(g_media_streamer_2);
		g_media_streamer_2 = NULL;
	}

	if (g_broadcast_address != NULL) {
		g_free(g_broadcast_address);
		g_broadcast_address = NULL;
	}

	if (g_uri != NULL) {
		g_free(g_uri);
		g_uri = NULL;
	}

	if (g_sub_uri != NULL) {
		g_free(g_sub_uri);
		g_sub_uri = NULL;
	}

	/* Clean Up Nodes */
	int i = g_node_counter - 1;
	for (; i >= 0; --i) {
		media_streamer_node_destroy(g_nodes[i]);
		g_nodes[i] = NULL;
	}
	g_node_counter = 0;
}

void quit()
{
	reset_current_menu_state();
	g_main_loop_quit(g_loop);
}

static void display_getting_ip_menu(void)
{
	g_print("\n");
	g_print("====================================================\n");
	g_print("Please input IP address for broadcasting\n");
	g_print("By default will be used [%s]\n", DEFAULT_IP_ADDR);
	g_print("----------------------------------------------------\n");
}

static void display_getting_uri_menu(void)
{
	g_print("\n");
	g_print("====================================================\n");
	g_print("Please input video URI for playing\n");
	g_print("----------------------------------------------------\n");
}

static void display_getting_sub_uri_menu(void)
{
	g_print("\n");
	g_print("====================================================\n");
	g_print("Please input Subtitle path for playing\n");
	g_print("----------------------------------------------------\n");
}

static void display_getting_seek_position_menu(void)
{
	g_print("\n");
	g_print("====================================================\n");
	g_print("Please enter desired position\n");
	g_print("By default will be seeked to [%d]\n", DEFAULT_SEEK_POS);
	g_print("----------------------------------------------------\n");
}

static void display_autoplug_select_menu(void)
{
	g_print("\n");
	g_print("Please select Media Streamer pluging mode\n");
	g_print("By default will be used [%s] mode\n", g_autoplug_mode == TRUE ? "autoplug" : "manual");
	g_print("1. Manual mode \n");
	g_print("2. Autoplug mode \n");
}

static void display_scenario_select_menu(void)
{
	g_print("\n");
	g_print("Please select Scenario mode\n");
	g_print("By default will be used [%d] mode\n", g_scenario_mode);
	g_print("1. Camera -> Screen \n");
	g_print("2. Microphone -> Phones\n");
	g_print("3. Camera + Microphone -> Screen + Phones\n");
	g_print("4. Video test -> Screen\n");
	g_print("5. Audio test -> Phones\n");
	g_print("6. Video test + Audio test -> Screen + Phones\n");
	g_print("7. File -> Screen + Phones\n");

}

static void display_playing_scenario_select_menu(void)
{
	g_print("\n");
	g_print("====================================================\n");
	g_print("   media streamer test: Playing menu v0.3\n");
	g_print("----------------------------------------------------\n");
	g_print("\n");
	g_print("Please select Playing Scenario mode\n");
	g_print("By default will be used [%d] mode\n", g_scenario_mode);
	g_print("1. VideoFile playing \n");
	g_print("2. VideoFile + SubtitleFile playing \n");
	g_print("3. HTTP Source playing \n");
	g_print("4. Appsrc -> Appsink \n");
	g_print("b. back \n");
	g_print("----------------------------------------------------\n");
	g_print("====================================================\n");
}

static void display_preset_menu(void)
{
	g_print("\n");
	g_print("====================================================\n");
	g_print("   media streamer test: Preset menu v0.3\n");
	g_print("----------------------------------------------------\n");
	g_print("1. create media streamer \n");
	g_print("2. set up current preset \n");
	g_print("3. prepare \n");
	g_print("4. unprepare \n");
	g_print("5. play \n");
	g_print("6. pause \n");
	g_print("7. seek \n");
	g_print("8. stop \n");
	g_print("9. destroy media streamer \n\n");
	g_print("b. back \n");
	g_print("----------------------------------------------------\n");
	g_print("====================================================\n");
}

static void display_voip_menu(void)
{
	g_print("\n");
	g_print("====================================================\n");
	g_print("   media streamer test: VoIP menu v0.3\n");
	g_print("----------------------------------------------------\n");
	g_print("1. one streamer VoIP 1 \n");
	g_print("2. one streamer VoIP 2 \n");
	g_print("3. two streamers VoIP server 1 \n");
	g_print("4. two streamers VoIP client 1 \n");
	g_print("5. two streamers VoIP server 2 \n");
	g_print("6. two streamers VoIP client 2 \n");
	g_print("b. back \n");
	g_print("----------------------------------------------------\n");
	g_print("====================================================\n");
}

static void display_broadcast_menu(void)
{
	g_print("\n");
	g_print("====================================================\n");
	g_print("   media streamer test: Broadcast menu v0.3\n");
	g_print("----------------------------------------------------\n");
	g_print("1. Broadcast client \n");
	g_print("2. Broadcast streamer \n");
	g_print("b. back \n");
	g_print("----------------------------------------------------\n");
	g_print("====================================================\n");
}

static void display_main_menu(void)
{
	g_print("\n");
	g_print("====================================================\n");
	g_print("   media streamer test: Main menu v0.3\n");
	g_print("----------------------------------------------------\n");
	g_print("1. Broadcast \n");
	g_print("2. VOIP \n");
	g_print("3. Local Playing \n");
	g_print("q. quit \n");
	g_print("----------------------------------------------------\n");
	g_print("====================================================\n");
}

static void display_menu(void)
{
	if (g_sub_menu_state == SUBMENU_STATE_UNKNOWN) {
		switch (g_menu_state) {
		case MENU_STATE_MAIN_MENU:
			display_main_menu();
			break;
		case MENU_STATE_BROADCAST_MENU:
			display_broadcast_menu();
			break;
		case MENU_STATE_VOIP_MENU:
			display_voip_menu();
			break;
		case MENU_STATE_PLAYING_MENU:
			display_preset_menu();
			break;
		case MENU_STATE_PRESET_MENU:
			display_preset_menu();
			break;
		default:
			g_print("*** Unknown status.\n");
			break;
		}
	} else {
		switch (g_sub_menu_state) {
		case SUBMENU_STATE_GETTING_IP:
			display_getting_ip_menu();
			break;
		case SUBMENU_STATE_GETTING_SEEK_POS:
			display_getting_seek_position_menu();
			break;
		case SUBMENU_STATE_GETTING_VIDEOFILE_URI:
			display_getting_uri_menu();
			break;
		case SUBMENU_STATE_GETTING_SUBFILE_URI:
			display_getting_sub_uri_menu();
			break;
		case SUBMENU_STATE_AUTOPLUG:
			display_autoplug_select_menu();
			break;
		case SUBMENU_STATE_SCENARIO:
			display_scenario_select_menu();
			break;
		case SUBMENU_STATE_PLAYING_SCENARIO:
			display_playing_scenario_select_menu();
			break;
		default:
			g_print("*** Unknown Submenu state.\n");
			break;
		}
	}
}

static void run_preset(void)
{
	media_streamer_node_h rtp_bin = NULL;
	create_formats();

	switch (g_menu_preset) {
	case PRESET_RTP_STREAMER:
		rtp_bin = _create_rtp(VIDEO_PORT, AUDIO_PORT, FALSE);
		if (g_autoplug_mode)
			_create_rtp_streamer_autoplug(rtp_bin);
		else
			_create_rtp_streamer(rtp_bin);
		break;
	case PRESET_RTP_CLIENT:
		rtp_bin = _create_rtp(VIDEO_PORT, AUDIO_PORT, TRUE);
		if (g_autoplug_mode)
			_create_rtp_client_autoplug(rtp_bin);
		else
			_create_rtp_client(rtp_bin);
		break;
	case PRESET_VOIP:
		rtp_bin = _create_rtp(VIDEO_PORT, AUDIO_PORT, FALSE);
		if (g_autoplug_mode) {
			_create_rtp_streamer_autoplug(rtp_bin);
			_create_rtp_client_autoplug(rtp_bin);
		} else {
			_create_rtp_streamer(rtp_bin);
			_create_rtp_client(rtp_bin);
		}
		break;
	case PRESET_VOIP_2:
		rtp_bin = _create_rtp(VIDEO_PORT, AUDIO_PORT, TRUE);
		if (g_autoplug_mode) {
			_create_rtp_streamer_autoplug(rtp_bin);
			_create_rtp_client_autoplug(rtp_bin);
		} else {
			_create_rtp_streamer(rtp_bin);
			_create_rtp_client(rtp_bin);
		}
		break;
	case PRESET_DOUBLE_VOIP_SERVER:
		rtp_bin = _create_rtp(VIDEO_PORT, AUDIO_PORT, FALSE);
		_create_rtp_streamer(rtp_bin);
		break;
	case PRESET_DOUBLE_VOIP_SERVER_2:
		rtp_bin = _create_rtp(VIDEO_PORT + 10, AUDIO_PORT + 10, TRUE);
		_create_rtp_streamer(rtp_bin);
		break;
	case PRESET_DOUBLE_VOIP_CLIENT:
		rtp_bin = _create_rtp(VIDEO_PORT + 10, AUDIO_PORT + 10, FALSE);
		_create_rtp_client(rtp_bin);
		break;
	case PRESET_DOUBLE_VOIP_CLIENT_2:
		rtp_bin = _create_rtp(VIDEO_PORT, AUDIO_PORT, TRUE);
		_create_rtp_client(rtp_bin);
		break;
	default:
		g_print("Invalid menu preset was selected!");
		break;
	}
}

void run_playing_preset(void)
{
	create_formats();

	if (g_scenario_mode == SCENARIO_MODE_FILE_PLAY_VIDEO_AUDIO)
		_create_file_playing();
	else if (g_scenario_mode == SCENARIO_MODE_FILE_SUBTITLE_VIDEO_AUDIO)
		_create_file_sub_playing();
	else if (g_scenario_mode == SCENARIO_MODE_HTTP_VIDEO_AUDIO)
		_create_http_playing();
	else if ((g_scenario_mode == SCENARIO_MODE_APPSRC_APPSINK))
		_create_app_test();
	else
		g_print("Invalid playing menu preset was selected!");
}

void _interpret_main_menu(char *cmd)
{
	int len = strlen(cmd);

	if (len == 1) {
		if (!strncmp(cmd, "1", len))
			g_menu_state = MENU_STATE_BROADCAST_MENU;
		else if (!strncmp(cmd, "2", len))
			g_menu_state = MENU_STATE_VOIP_MENU;
		else if (!strncmp(cmd, "3", len))
			g_menu_state = MENU_STATE_PLAYING_MENU;
		else if (!strncmp(cmd, "q", len))
			quit();
	} else {
		g_print("wrong command\n");
	}
}

void _interpret_broadcast_menu(char *cmd)
{
	int len = strlen(cmd);

	if (len == 1) {
		if (!strncmp(cmd, "1", len)) {
			g_menu_preset = PRESET_RTP_CLIENT;
			g_menu_state = MENU_STATE_PRESET_MENU;
		} else if (!strncmp(cmd, "2", len)) {
			g_menu_preset = PRESET_RTP_STREAMER;
			g_menu_state = MENU_STATE_PRESET_MENU;
		} else if (!strncmp(cmd, "b", len)) {
			reset_current_menu_state();
			display_menu();
		}
	} else {
		g_print("wrong command\n");
	}
}

void _interpret_voip_menu(char *cmd)
{
	int len = strlen(cmd);

	if (len == 1) {
		if (!strncmp(cmd, "1", len)) {
			g_menu_preset = PRESET_VOIP;
			g_menu_state = MENU_STATE_PRESET_MENU;
		} else if (!strncmp(cmd, "2", len)) {
			g_menu_preset = PRESET_VOIP_2;
			g_menu_state = MENU_STATE_PRESET_MENU;
		} else if (!strncmp(cmd, "3", len)) {
			/*double Server 1 */
			g_menu_preset = PRESET_DOUBLE_VOIP_SERVER;
			g_menu_state = MENU_STATE_PRESET_MENU;
		} else if (!strncmp(cmd, "4", len)) {
			/*double Client 1 */
			g_menu_preset = PRESET_DOUBLE_VOIP_CLIENT;
			g_menu_state = MENU_STATE_PRESET_MENU;
		} else if (!strncmp(cmd, "5", len)) {
			/*double Server 2 */
			g_menu_preset = PRESET_DOUBLE_VOIP_SERVER_2;
			g_menu_state = MENU_STATE_PRESET_MENU;
		} else if (!strncmp(cmd, "6", len)) {
			/*double Client 2 */
			g_menu_preset = PRESET_DOUBLE_VOIP_CLIENT_2;
			g_menu_state = MENU_STATE_PRESET_MENU;
		} else if (!strncmp(cmd, "b", len)) {
			reset_current_menu_state();
			/*display_menu(); */
		}

	} else {
		g_print("wrong command\n");
	}
}

void _interpret_playing_scenario_menu(char *cmd)
{
	int len = strlen(cmd);

	if (len == 1) {
		if (!strncmp(cmd, "1", len)) {
			g_scenario_mode = SCENARIO_MODE_FILE_PLAY_VIDEO_AUDIO;
			g_sub_menu_state = SUBMENU_STATE_GETTING_VIDEOFILE_URI;
			return;
		} else if (!strncmp(cmd, "2", len)) {
			g_scenario_mode = SCENARIO_MODE_FILE_SUBTITLE_VIDEO_AUDIO;
			g_sub_menu_state = SUBMENU_STATE_GETTING_VIDEOFILE_URI;
			return;
		} else if (!strncmp(cmd, "3", len)) {
			g_scenario_mode = SCENARIO_MODE_HTTP_VIDEO_AUDIO;
			g_sub_menu_state = SUBMENU_STATE_GETTING_VIDEOFILE_URI;
			return;
		} else if (!strncmp(cmd, "4", len)) {
			g_scenario_mode = SCENARIO_MODE_APPSRC_APPSINK;
		}
	}
	g_sub_menu_state = SUBMENU_STATE_UNKNOWN;
}

void _interpret_scenario_menu(char *cmd)
{
	int len = strlen(cmd);

	if (len == 1) {
		if (!strncmp(cmd, "1", len)) {
			g_scenario_mode = SCENARIO_MODE_CAMERA_SCREEN;
			g_video_is_on = TRUE;
			g_audio_is_on = FALSE;
			g_sub_menu_state = SUBMENU_STATE_UNKNOWN;
			run_preset();
		} else if (!strncmp(cmd, "2", len)) {
			g_scenario_mode = SCENARIO_MODE_MICROPHONE_PHONE;
			g_video_is_on = FALSE;
			g_audio_is_on = TRUE;
			g_sub_menu_state = SUBMENU_STATE_UNKNOWN;
			run_preset();
		} else if (!strncmp(cmd, "3", len)) {
			g_scenario_mode = SCENARIO_MODE_FULL_VIDEO_AUDIO;
			g_video_is_on = TRUE;
			g_audio_is_on = TRUE;
			g_sub_menu_state = SUBMENU_STATE_UNKNOWN;
			run_preset();
		} else if (!strncmp(cmd, "4", len)) {
			g_scenario_mode = SCENARIO_MODE_VIDEOTEST_SCREEN;
			g_video_is_on = TRUE;
			g_audio_is_on = FALSE;
			g_sub_menu_state = SUBMENU_STATE_UNKNOWN;
			run_preset();
		} else if (!strncmp(cmd, "5", len)) {
			g_scenario_mode = SCENARIO_MODE_AUDIOTEST_PHONE;
			g_video_is_on = FALSE;
			g_audio_is_on = TRUE;
			g_sub_menu_state = SUBMENU_STATE_UNKNOWN;
			run_preset();
		} else if (!strncmp(cmd, "6", len)) {
			g_scenario_mode = SCENARIO_MODE_TEST_VIDEO_AUDIO;
			g_video_is_on = TRUE;
			g_audio_is_on = TRUE;
			g_sub_menu_state = SUBMENU_STATE_UNKNOWN;
			run_preset();
		} else if (!strncmp(cmd, "7", len)) {
			g_scenario_mode = SCENARIO_MODE_FILE_STREAM_VIDEO_AUDIO;
			g_video_is_on = TRUE;
			g_audio_is_on = TRUE;
			if (g_menu_preset == PRESET_RTP_STREAMER) {
				g_sub_menu_state = SUBMENU_STATE_GETTING_VIDEOFILE_URI;
			} else if (g_menu_preset == PRESET_RTP_CLIENT) {
				create_formats();
				media_streamer_node_h rtp_bin = _create_rtp(VIDEO_PORT, AUDIO_PORT, TRUE);
				if (g_autoplug_mode)
					_create_rtp_client_autoplug(rtp_bin);
				else
					_create_rtp_client(rtp_bin);
				g_sub_menu_state = SUBMENU_STATE_UNKNOWN;
			}
		}
	}
}

void _interpret_getting_ip_menu(char *cmd)
{
	int min_len = strlen("0.0.0.0");
	int cmd_len = strlen(cmd);
	if (g_broadcast_address != NULL)
		g_free(g_broadcast_address);

	if (cmd_len > min_len) {
		g_broadcast_address = g_strdup(cmd);
		g_print("== IP address set to [%s]\n", g_broadcast_address);
	} else {
		g_broadcast_address = g_strdup(DEFAULT_IP_ADDR);
		g_print("Invalid IP. Default address will be used [%s]\n", DEFAULT_IP_ADDR);
	}

	g_sub_menu_state = SUBMENU_STATE_SCENARIO;
}

void _interpret_getting_seek_position_menu(char *cmd)
{
	if (cmd > DEFAULT_SEEK_POS) {
		g_time = strtol(cmd, NULL, 10) * MSEC_MULTIPLIER;
		g_print("== Seek position set to [%d] second\n", g_time);
	} else {
		g_time = DEFAULT_SEEK_POS;
		g_print("Invalid seek pos. Default seek position will be used [%d]\n", DEFAULT_SEEK_POS);
		return;
	}
	_seek();

	g_sub_menu_state = SUBMENU_STATE_UNKNOWN;
}

void _interpret_getting_sub_uri_menu(char *cmd)
{
	if (cmd) {
		if (g_sub_uri != NULL)
			g_free(g_sub_uri);
		g_sub_uri = g_strdup(cmd);
		g_print("== URI set to [%s]\n", g_sub_uri);
	} else {
		g_print("Empty URI!\n");
		return;
	}

	if (g_menu_state == MENU_STATE_PLAYING_MENU) {
		run_playing_preset();
		g_sub_menu_state = SUBMENU_STATE_UNKNOWN;
	} else {
		g_sub_menu_state = SUBMENU_STATE_UNKNOWN;
		run_preset();
	}
}

void _interpret_getting_uri_menu(char *cmd)
{
	if (cmd) {
		if (g_uri != NULL)
			g_free(g_uri);
		g_uri = g_strdup(cmd);
		g_print("== URI set to [%s]\n", g_uri);
	} else {
		g_print("Empty URI!\n");
		return;
	}

	if (g_menu_state == MENU_STATE_PLAYING_MENU) {
		if (g_scenario_mode == SCENARIO_MODE_FILE_SUBTITLE_VIDEO_AUDIO) {
			g_sub_menu_state = SUBMENU_STATE_GETTING_SUBFILE_URI;
		} else {
			run_playing_preset();
			g_sub_menu_state = SUBMENU_STATE_UNKNOWN;
		}
	} else {
		if (g_scenario_mode == SCENARIO_MODE_FILE_STREAM_VIDEO_AUDIO) {
			create_formats();
			_create_file_streaming();
			_create_rtp(VIDEO_PORT, AUDIO_PORT, FALSE);
		} else {
			run_preset();
		}
		g_sub_menu_state = SUBMENU_STATE_UNKNOWN;
	}
}

void _interpret_autoplug_menu(char *cmd)
{
	if (strlen(cmd) == 1) {
		if (cmd[0] == '2')
			g_autoplug_mode = true;
		else
			g_autoplug_mode = false;
	} else {
		g_print("Invalid input. Default autoplug mode will be used\n");
	}

	g_print("Selected pluging mode is [%s]\n", g_autoplug_mode == TRUE ? "autoplug" : "manual");

	if (g_menu_preset & PRESET_RTP_STREAMER)
		g_sub_menu_state = SUBMENU_STATE_GETTING_IP;
	else
		g_sub_menu_state = SUBMENU_STATE_SCENARIO;
}

void _interpret_preset_menu(char *cmd)
{
	int len = strlen(cmd);

	if (len == 1 || len == 2) {
		if (!strncmp(cmd, "1", len)) {
			if ((g_menu_preset & DOUBLE_STREAMER_MASK) && (g_menu_preset & PRESET_RTP_CLIENT)) {
				_create(&g_media_streamer_2);
				current_media_streamer = g_media_streamer_2;
				g_print("== success create media streamer 2\n");
			} else {
				_create(&g_media_streamer);
				current_media_streamer = g_media_streamer;
				g_print("== success create media streamer\n");
			}

		} else if (!strncmp(cmd, "2", len)) {
			/* call the run_preset function after autoplug mode was selected; */
			if (g_menu_state == MENU_STATE_PLAYING_MENU)
				g_sub_menu_state = SUBMENU_STATE_PLAYING_SCENARIO;
			else
				g_sub_menu_state = SUBMENU_STATE_AUTOPLUG;
		} else if (!strncmp(cmd, "3", len)) {
			_prepare();
		} else if (!strncmp(cmd, "4", len)) {
			_unprepare();
		} else if (!strncmp(cmd, "5", len)) {
			_play();
		} else if (!strncmp(cmd, "6", len)) {
			_pause();
		} else if (!strncmp(cmd, "7", len)) {
			g_sub_menu_state = SUBMENU_STATE_GETTING_SEEK_POS;
		} else if (!strncmp(cmd, "8", len)) {
			_stop();
		} else if (!strncmp(cmd, "9", len)) {
			_destroy(current_media_streamer);
		} else if (!strncmp(cmd, "b", len)) {
			if (g_menu_preset & DOUBLE_STREAMER_MASK) {
				g_menu_state = MENU_STATE_VOIP_MENU;
				g_autoplug_mode = FALSE;
				current_media_streamer = NULL;
			} else {
				reset_current_menu_state();
			}
			g_sub_menu_state = SUBMENU_STATE_UNKNOWN;
			display_menu();
		}

	} else {
		g_print("wrong command\n");
	}
}

static void interpret_cmd(char *cmd)
{
	if (g_sub_menu_state == SUBMENU_STATE_UNKNOWN) {
		switch (g_menu_state) {
		case MENU_STATE_MAIN_MENU:
			_interpret_main_menu(cmd);
			break;
		case MENU_STATE_BROADCAST_MENU:
			_interpret_broadcast_menu(cmd);
			break;
		case MENU_STATE_VOIP_MENU:
			_interpret_voip_menu(cmd);
			break;
		case MENU_STATE_PLAYING_MENU:
			_interpret_preset_menu(cmd);
			break;
		case MENU_STATE_PRESET_MENU:
			_interpret_preset_menu(cmd);
			break;
		default:
			g_print("Invalid command\n");
			return;
			break;
		}
	} else {
		switch (g_sub_menu_state) {
		case SUBMENU_STATE_GETTING_IP:
			_interpret_getting_ip_menu(cmd);
			break;
		case SUBMENU_STATE_GETTING_SEEK_POS:
			_interpret_getting_seek_position_menu(cmd);
			break;
		case SUBMENU_STATE_GETTING_VIDEOFILE_URI:
			_interpret_getting_uri_menu(cmd);
			break;
		case SUBMENU_STATE_GETTING_SUBFILE_URI:
			_interpret_getting_sub_uri_menu(cmd);
			break;
		case SUBMENU_STATE_AUTOPLUG:
			_interpret_autoplug_menu(cmd);
			break;
		case SUBMENU_STATE_SCENARIO:
			_interpret_scenario_menu(cmd);
			break;
		case SUBMENU_STATE_PLAYING_SCENARIO:
			_interpret_playing_scenario_menu(cmd);
			break;
		default:
			g_print("*** Unknown Submenu state.\n");
			break;
		}
	}

	display_menu();
}

gboolean input(GIOChannel *channel)
{
	gchar buf[MAX_STRING_LEN];
	gsize read;
	GError *error = NULL;

	g_io_channel_read_chars(channel, buf, MAX_STRING_LEN, &read, &error);

	buf[read] = '\0';
	g_strstrip(buf);
	interpret_cmd(buf);

	return TRUE;
}

int main(int argc, char **argv)
{
	GIOChannel *stdin_channel;
	stdin_channel = g_io_channel_unix_new(0);
	g_io_channel_set_flags(stdin_channel, G_IO_FLAG_NONBLOCK, NULL);
	g_io_add_watch(stdin_channel, G_IO_IN, (GIOFunc) input, NULL);

	display_menu();

	g_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(g_loop);

	g_main_loop_unref(g_loop);
	g_io_channel_unref(stdin_channel);
	return 0;
}
