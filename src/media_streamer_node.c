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

#include <media_streamer_node.h>
#include <media_streamer_util.h>
#include <media_streamer_gst.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cynara-client.h>

#define SMACK_LABEL_LEN 255

param_s param_table[] = {
	{MEDIA_STREAMER_PARAM_CAMERA_ID, "camera-id"},
	{MEDIA_STREAMER_PARAM_CAMERA_ID, "device"},
	{MEDIA_STREAMER_PARAM_CAPTURE_WIDTH, "capture-width"},
	{MEDIA_STREAMER_PARAM_CAPTURE_HEIGHT, "capture-height"},
	{MEDIA_STREAMER_PARAM_IS_LIVE_STREAM, "is-live"},
	{MEDIA_STREAMER_PARAM_IS_LIVE_STREAM, "living"},
	{MEDIA_STREAMER_PARAM_URI, "uri"},
	{MEDIA_STREAMER_PARAM_URI, "location"},
	{MEDIA_STREAMER_PARAM_USER_AGENT, "user-agent"},
	{MEDIA_STREAMER_PARAM_STREAM_TYPE, "stream-type"},
	{MEDIA_STREAMER_PARAM_PORT, "port"},
	{MEDIA_STREAMER_PARAM_VIDEO_IN_PORT, "video_in_port"},
	{MEDIA_STREAMER_PARAM_AUDIO_IN_PORT, "audio_in_port"},
	{MEDIA_STREAMER_PARAM_VIDEO_OUT_PORT, "video_out_port"},
	{MEDIA_STREAMER_PARAM_AUDIO_OUT_PORT, "audio_out_port"},
	{MEDIA_STREAMER_PARAM_IP_ADDRESS, "address"},
	{MEDIA_STREAMER_PARAM_AUDIO_DEVICE, "audio_device"},
	{MEDIA_STREAMER_PARAM_CLOCK_SYNCHRONIZED, "sync"},
	{MEDIA_STREAMER_PARAM_ROTATE, "rotate"},
	{MEDIA_STREAMER_PARAM_FLIP, "flip"},
	{MEDIA_STREAMER_PARAM_DISPLAY_GEOMETRY_METHOD, "display-geometry-method"},
	{MEDIA_STREAMER_PARAM_EVAS_OBJECT, "evas-object"},
	{MEDIA_STREAMER_PARAM_VISIBLE, "visible"},
	{MEDIA_STREAMER_PARAM_HOST, "host"},
	{NULL, NULL}
};

node_info_s nodes_info[] = {
	{"Generic", "none"},                               /* MEDIA_STREAMER_NODE_TYPE_NONE */
	{"Source", "fakesrc"},                             /* MEDIA_STREAMER_NODE_TYPE_SRC */
	{"Sink", "fakesink"},                              /* MEDIA_STREAMER_NODE_TYPE_SINK */
	{"Codec/Encoder/Video", "video_encoder"},          /* MEDIA_STREAMER_NODE_TYPE_VIDEO_ENCODER */
	{"Codec/Decoder/Video", "video_decoder"},          /* MEDIA_STREAMER_NODE_TYPE_VIDEO_DECODER */
	{"Codec/Encoder/Audio", "audio_encoder"},          /* MEDIA_STREAMER_NODE_TYPE_AUDIO_ENCODER */
	{"Codec/Decoder/Audio", "audio_decoder"},          /* MEDIA_STREAMER_NODE_TYPE_AUDIO_DECODER */
	{"Filter/Converter/Video", "videoconvert"},        /* MEDIA_STREAMER_NODE_TYPE_VIDEO_CONVERTER */
	{"Filter/Converter/Audio", "audioconvert"},        /* MEDIA_STREAMER_NODE_TYPE_AUDIO_CONVERTER */
	{MEDIA_STREAMER_STRICT, "audioresample"},          /* MEDIA_STREAMER_NODE_TYPE_AUDIO_RESAMPLE */
	{"Codec/Payloader/Network/RTP", "rtph263pay"},     /* MEDIA_STREAMER_NODE_TYPE_VIDEO_PAY */
	{"Codec/Payloader/Network/RTP", "rtpamrpay"},      /* MEDIA_STREAMER_NODE_TYPE_AUDIO_PAY */
	{"Codec/Depayloader/Network/RTP", "rtph263depay"}, /* MEDIA_STREAMER_NODE_TYPE_VIDEO_DEPAY */
	{"Codec/Depayloader/Network/RTP", "rtpamrdepay"},  /* MEDIA_STREAMER_NODE_TYPE_AUDIO_DEPAY */
	{"Filter/Effect/Video", "videorate"},              /* MEDIA_STREAMER_NODE_TYPE_VIDEO_RATE */
	{"Filter/Converter/Video/Scaler", "videoscale"},   /* MEDIA_STREAMER_NODE_TYPE_VIDEO_SCALE */
	{MEDIA_STREAMER_STRICT, "textoverlay"},            /* MEDIA_STREAMER_NODE_TYPE_TEXT_OVERLAY */
	{"Codec/Parser/Converter/Video", "h263parse"},     /* MEDIA_STREAMER_NODE_TYPE_PARSER */
	{MEDIA_STREAMER_STRICT, "capsfilter"},             /* MEDIA_STREAMER_NODE_TYPE_FILTER */
	{MEDIA_STREAMER_STRICT, "tee"},                    /* MEDIA_STREAMER_NODE_TYPE_TEE */
	{MEDIA_STREAMER_STRICT, "queue"},                  /* MEDIA_STREAMER_NODE_TYPE_QUEUE */
	{MEDIA_STREAMER_STRICT, "multiqueue"},             /* MEDIA_STREAMER_NODE_TYPE_MQUEUE */
	{"Codec/Muxer", "qtmux"},                          /* MEDIA_STREAMER_NODE_TYPE_MUXER */
	{"Codec/Demuxer", "qtdemux"},                      /* MEDIA_STREAMER_NODE_TYPE_DEMUXER */
	{"Generic/Bin", "rtpbin"},                         /* MEDIA_STREAMER_NODE_TYPE_RTP */
	{MEDIA_STREAMER_STRICT, "input-selector"},         /* MEDIA_STREAMER_NODE_TYPE_INPUT_SELECTOR */
	{MEDIA_STREAMER_STRICT, "output-selector"},        /* MEDIA_STREAMER_NODE_TYPE_OUTPUT_SELECTOR */
	{MEDIA_STREAMER_STRICT, "interleave"},             /* MEDIA_STREAMER_NODE_TYPE_INTERLEAVE */
	{MEDIA_STREAMER_STRICT, "deinterleave"},           /* MEDIA_STREAMER_NODE_TYPE_DEINTERLEAVE */
	{NULL, NULL}
};

void __ms_get_state(media_streamer_s *ms_streamer)
{
	GstState state_old, state_new;
	GstStateChangeReturn ret_state = gst_element_get_state(ms_streamer->pipeline, &state_old, &state_new, GST_CLOCK_TIME_NONE);
	if (ret_state == GST_STATE_CHANGE_SUCCESS)
		ms_info("Got state for [%s]: old [%s], new [%s]", GST_ELEMENT_NAME(ms_streamer->pipeline), gst_element_state_get_name(state_old), gst_element_state_get_name(state_new));
	else
		ms_error("Couldn`t get state for [%s]", GST_ELEMENT_NAME(ms_streamer->pipeline));
}

static gboolean __ms_rtp_node_has_property(media_streamer_node_s *ms_node, const char *param_name)
{
	ms_retvm_if(!ms_node || !ms_node->gst_element, FALSE, "Error: empty node");
	ms_retvm_if(!param_name, FALSE, "Error: invalid property parameter");

	if (ms_node->type != MEDIA_STREAMER_NODE_TYPE_RTP)
		return FALSE;

	GValue *val = (GValue *)g_object_get_data(G_OBJECT(ms_node->gst_element), param_name);
	return val ? TRUE : FALSE;
}

static int __ms_rtp_node_get_property(media_streamer_node_s *ms_node, param_s *param, GValue *value)
{
	ms_retvm_if(!ms_node || !ms_node->gst_element, FALSE, "Error: empty node");
	ms_retvm_if(ms_node->type != MEDIA_STREAMER_NODE_TYPE_RTP, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Invalid node type");
	ms_retvm_if(!param, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Error: invalid property parameter");

	int ret = MEDIA_STREAMER_ERROR_NONE;

	GValue *val = (GValue *)g_object_get_data(G_OBJECT(ms_node->gst_element), param->param_name);
	if (!strcmp(param->param_name, MEDIA_STREAMER_PARAM_VIDEO_IN_PORT) ||
		!strcmp(param->param_name, MEDIA_STREAMER_PARAM_AUDIO_IN_PORT) ||
		!strcmp(param->param_name, MEDIA_STREAMER_PARAM_VIDEO_OUT_PORT) ||
		!strcmp(param->param_name, MEDIA_STREAMER_PARAM_AUDIO_OUT_PORT)) {
		g_value_init(value, G_TYPE_INT);
	} else if (!strcmp(param->param_name, MEDIA_STREAMER_PARAM_HOST)) {
		g_value_init(value, G_TYPE_STRING);
	} else
		ret = MEDIA_STREAMER_ERROR_INVALID_PARAMETER;

	g_value_copy(val, value);
	return ret;
}

static int __ms_rtp_node_set_property(media_streamer_node_s *ms_node, param_s *param, const char *param_value)
{
	ms_retvm_if(!ms_node || !ms_node->gst_element, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error: empty node");
	ms_retvm_if(ms_node->type != MEDIA_STREAMER_NODE_TYPE_RTP, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Invalid node type");
	ms_retvm_if(!param, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Error: invalid property parameter");

	int ret = MEDIA_STREAMER_ERROR_NONE;

	GValue *val = (GValue *)g_object_get_data(G_OBJECT(ms_node->gst_element), param->param_name);
	if (!val)
		ret = MEDIA_STREAMER_ERROR_INVALID_PARAMETER;

	if (!strcmp(param->param_name, MEDIA_STREAMER_PARAM_VIDEO_IN_PORT) ||
		!strcmp(param->param_name, MEDIA_STREAMER_PARAM_AUDIO_IN_PORT) ||
		!strcmp(param->param_name, MEDIA_STREAMER_PARAM_VIDEO_OUT_PORT) ||
		!strcmp(param->param_name, MEDIA_STREAMER_PARAM_AUDIO_OUT_PORT)) {
		g_value_unset(val);
		g_value_init(val, G_TYPE_INT);
		g_value_set_int(val, (int)strtol(param_value, NULL, 10));
	} else if (!strcmp(param->param_name, MEDIA_STREAMER_PARAM_HOST)) {
		g_value_unset(val);
		g_value_init(val, G_TYPE_STRING);
		g_value_set_string(val, param_value);
	} else if (!strcmp(param->param_name, MEDIA_STREAMER_PARAM_VIDEO_IN_FORMAT) ||
			   !strcmp(param->param_name, MEDIA_STREAMER_PARAM_AUDIO_IN_FORMAT)) {
		GstCaps *caps = gst_caps_from_string(param_value);
		if (caps) {
			g_value_unset(val);
			g_value_init(val, GST_TYPE_CAPS);
			gst_value_set_caps(val, caps);
		} else
			ret = MEDIA_STREAMER_ERROR_INVALID_PARAMETER;
	} else
		ret = MEDIA_STREAMER_ERROR_INVALID_PARAMETER;

	return ret;
}

int __ms_node_create(media_streamer_node_s *node, media_format_h in_fmt, media_format_h out_fmt)
{
	ms_retvm_if(node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	int ret = MEDIA_STREAMER_ERROR_NONE;

	GstCaps *sink_caps = in_fmt ? __ms_create_caps_from_fmt(in_fmt) : NULL;
	GstCaps *src_caps = out_fmt ? __ms_create_caps_from_fmt(out_fmt) : NULL;

	node_plug_s plug_info = {&(nodes_info[node->type]), src_caps, sink_caps, NULL};
	ms_info("Creating node with info: klass_name[%s]; default[%s]",
			plug_info.info->klass_name, plug_info.info->default_name);

	node->gst_element = __ms_node_element_create(&plug_info, node->type);
	if (node->gst_element)
		node->name = gst_element_get_name(node->gst_element);
	else
		ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;

	if (src_caps)
		gst_caps_unref(src_caps);

	if (sink_caps)
		gst_caps_unref(sink_caps);

	return ret;
}

/* This signal callback is called when appsrc needs data, we add an idle handler
 * to the mainloop to start pushing data into the appsrc */
static void __ms_src_start_feed_cb(GstElement *pipeline, guint size, gpointer data)
{
	media_streamer_node_s *ms_src = (media_streamer_node_s *) data;
	ms_retm_if(ms_src == NULL, "Handle is NULL");

	if (ms_src->callbacks_structure != NULL) {
		media_streamer_callback_s *src_callback = (media_streamer_callback_s *) ms_src->callbacks_structure;
		media_streamer_custom_buffer_status_cb buffer_status_cb = (media_streamer_custom_buffer_status_cb) src_callback->callback;
		buffer_status_cb((media_streamer_node_h) ms_src, MEDIA_STREAMER_CUSTOM_BUFFER_UNDERRUN, src_callback->user_data);
	}
}

/* This callback is called when appsrc has enough data and we can stop sending.
 * We remove the idle handler from the mainloop */
static void __ms_src_stop_feed_cb(GstElement *pipeline, gpointer data)
{
	media_streamer_node_s *ms_src = (media_streamer_node_s *) data;
	ms_retm_if(ms_src == NULL, "Handle is NULL");

	if (ms_src->callbacks_structure != NULL) {
		media_streamer_callback_s *src_callback = (media_streamer_callback_s *) ms_src->callbacks_structure;
		media_streamer_custom_buffer_status_cb buffer_status_cb = (media_streamer_custom_buffer_status_cb) src_callback->callback;
		buffer_status_cb((media_streamer_node_h) ms_src, MEDIA_STREAMER_CUSTOM_BUFFER_OVERFLOW, src_callback->user_data);
	}
}

static int __ms_node_check_priveleges(media_streamer_node_s *node)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;
	int ret_val = 0;
	char *privilege = NULL;

	if (node->type == MEDIA_STREAMER_NODE_TYPE_SRC) {
		switch (node->subtype) {
		case MEDIA_STREAMER_NODE_SRC_TYPE_HTTP:
			privilege = "http://tizen.org/privilege/internet";
			break;
		case MEDIA_STREAMER_NODE_SRC_TYPE_CAMERA:
			privilege = "http://tizen.org/privilege/camera";
			break;
		case MEDIA_STREAMER_NODE_SRC_TYPE_AUDIO_CAPTURE:
			privilege = "http://tizen.org/privilege/recorder";
			break;
		case MEDIA_STREAMER_NODE_SRC_TYPE_VIDEO_CAPTURE:
			privilege = "http://tizen.org/privilege/camera";
			break;
		default:
			ms_info("For current Src Node [%s] subtype [%d] privileges are not needed", node->name, node->subtype);
			break;
		}
	}

	if (node->type == MEDIA_STREAMER_NODE_TYPE_SINK) {
		switch (node->subtype) {
		case MEDIA_STREAMER_NODE_SINK_TYPE_HTTP:
			privilege = "http://tizen.org/privilege/internet";
			break;
		default:
			ms_info("For current Sink Node [%s] subtype [%d] privileges are not needed", node->name, node->subtype);
			break;
		}
	}

	/* Skip checking for privilige permission in case of other types of Nodes */
	if (privilege == NULL)
		return ret;

	FILE* opened_file;

	char smackLabel[SMACK_LABEL_LEN + 1];
	char uid[10];
	cynara *cynara_h;

	if (CYNARA_API_SUCCESS != cynara_initialize(&cynara_h, NULL)) {
		ms_error("Failed to initialize cynara structure\n");
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	bzero(smackLabel, SMACK_LABEL_LEN + 1);

	/* Getting smack label */
	opened_file = fopen("/proc/self/attr/current", "r");
	if (opened_file == NULL) {
		ms_error("Failed to open /proc/self/attr/current\n");
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}
	ret_val = fread(smackLabel, sizeof(smackLabel), 1, opened_file);
	fclose(opened_file);
	if (ret_val < 0) {
		ms_error("Failed to read /proc/self/attr/current\n");
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	/* Getting uid */
	snprintf(uid, sizeof(uid), "%d", getuid());
	ms_info("%s %s %s\n", smackLabel, uid, privilege);

	/* Checking with cynara for current session */
	ret_val = cynara_check(cynara_h, smackLabel, "", uid, privilege);
	ms_info("Cynara_check [%d] ", ret_val);

	switch (ret_val) {
	case CYNARA_API_ACCESS_ALLOWED:
		ms_info("Access to Node [%s] subtype [%d] is allowed", node->name, node->subtype);
		break;
	case CYNARA_API_ACCESS_DENIED:
	default:
		ms_error("Access to Node [%s] subtype [%d] is denied", node->name, node->subtype);
		ret = MEDIA_STREAMER_ERROR_PERMISSION_DENIED;
		break;
	}

	cynara_finish(cynara_h);
	return ret;
}

int __ms_src_node_create(media_streamer_node_s *node)
{
	ms_retvm_if(node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	int ret = MEDIA_STREAMER_ERROR_NONE;
	char *plugin_name = NULL;

	ret = __ms_node_check_priveleges(node);
	if (ret != MEDIA_STREAMER_ERROR_NONE) {
		ms_error("Error getting privileges for Src Node");
		return ret;
	}

	switch (node->subtype) {
	case MEDIA_STREAMER_NODE_SRC_TYPE_FILE:
		plugin_name = __ms_ini_get_string("node type 1:file", DEFAULT_FILE_SOURCE);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_SRC_TYPE_RTSP:
		plugin_name = __ms_ini_get_string("node type 1:rtsp", DEFAULT_UDP_SOURCE);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_SRC_TYPE_HTTP:
		plugin_name = __ms_ini_get_string("node type 1:http", DEFAULT_HTTP_SOURCE);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_SRC_TYPE_CAMERA:
		plugin_name = __ms_ini_get_string("node type 1:camera", DEFAULT_CAMERA_SOURCE);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_SRC_TYPE_AUDIO_CAPTURE:
		plugin_name = __ms_ini_get_string("node type 1:audio capture", DEFAULT_AUDIO_SOURCE);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_SRC_TYPE_VIDEO_CAPTURE:
		plugin_name = __ms_ini_get_string("node type 1:video capture", DEFAULT_VIDEO_SOURCE);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_SRC_TYPE_VIDEO_TEST:
		plugin_name = __ms_ini_get_string("node type 1:video test", DEFAULT_VIDEO_TEST_SOURCE);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_SRC_TYPE_AUDIO_TEST:
		plugin_name = __ms_ini_get_string("node type 1:audio test", DEFAULT_AUDIO_TEST_SOURCE);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_SRC_TYPE_CUSTOM:
		plugin_name = __ms_ini_get_string("node type 1:custom", DEFAULT_APP_SOURCE);
		node->gst_element = __ms_element_create(DEFAULT_APP_SOURCE, NULL);
		__ms_signal_create(&node->sig_list, node->gst_element, "need-data", G_CALLBACK(__ms_src_start_feed_cb), node);
		__ms_signal_create(&node->sig_list, node->gst_element, "enough-data", G_CALLBACK(__ms_src_stop_feed_cb), node);
		break;
	default:
		ms_error("Error: invalid Src node Type [%d]", node->subtype);
		break;
	}

	MS_SAFE_FREE(plugin_name);

	if (ret == MEDIA_STREAMER_ERROR_NONE) {
		if (node->gst_element == NULL)
			ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
		else
			node->name = gst_element_get_name(node->gst_element);
	}

	return ret;
}

/* The appsink has received a buffer */
static void __ms_sink_new_buffer_cb(GstElement *sink, gpointer *data)
{
	media_streamer_node_s *ms_sink = (media_streamer_node_s *) data;
	ms_retm_if(ms_sink == NULL, "Handle is NULL");

	if (ms_sink->callbacks_structure != NULL) {
		media_streamer_sink_callbacks_s *sink_callbacks = (media_streamer_sink_callbacks_s *) ms_sink->callbacks_structure;
		media_streamer_sink_data_ready_cb data_ready_cb = (media_streamer_sink_data_ready_cb) sink_callbacks->data_ready_cb.callback;

		if (data_ready_cb)
			data_ready_cb((media_streamer_node_h) ms_sink, sink_callbacks->data_ready_cb.user_data);
	}
}

/* The appsink has got eos */
static void sink_eos(GstElement *sink, gpointer *data)
{
	media_streamer_node_s *ms_sink = (media_streamer_node_s *) data;
	ms_retm_if(ms_sink == NULL, "Handle is NULL");

	if (ms_sink->callbacks_structure != NULL) {
		media_streamer_sink_callbacks_s *sink_callbacks = (media_streamer_sink_callbacks_s *) ms_sink->callbacks_structure;
		media_streamer_sink_eos_cb eos_cb = (media_streamer_sink_eos_cb) sink_callbacks->eos_cb.callback;

		if (eos_cb)
			eos_cb((media_streamer_node_h) ms_sink, sink_callbacks->eos_cb.user_data);
	}
}

int __ms_sink_node_create(media_streamer_node_s *node)
{
	ms_retvm_if(node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	int ret = MEDIA_STREAMER_ERROR_NONE;
	char *plugin_name = NULL;

	ret = __ms_node_check_priveleges(node);
	if (ret != MEDIA_STREAMER_ERROR_NONE) {
		ms_error("Error getting privileges for Sink Node");
		return ret;
	}

	switch (node->subtype) {
	case MEDIA_STREAMER_NODE_SINK_TYPE_FILE:
		plugin_name = __ms_ini_get_string("node type 2:file", DEFAULT_FILE_SINK);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_SINK_TYPE_RTSP:
		plugin_name = __ms_ini_get_string("node type 2:rtsp", DEFAULT_UDP_SINK);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_SINK_TYPE_HTTP:
		ms_error("Error: not implemented yet");
		break;
	case MEDIA_STREAMER_NODE_SINK_TYPE_AUDIO:
		plugin_name = __ms_ini_get_string("node type 2:audio", DEFAULT_AUDIO_SINK);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_SINK_TYPE_OVERLAY:
		plugin_name = __ms_ini_get_string("node type 2:overlay", DEFAULT_VIDEO_SINK);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_SINK_TYPE_EVAS:
		plugin_name = __ms_ini_get_string("node type 2:evas", DEFAULT_EVAS_SINK);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_SINK_TYPE_FAKE:
		plugin_name = __ms_ini_get_string("node type 2:fake", DEFAULT_FAKE_SINK);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_SINK_TYPE_CUSTOM:
		plugin_name = __ms_ini_get_string("node type 2:custom", DEFAULT_APP_SINK);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		if (node->gst_element) {
			g_object_set(G_OBJECT(node->gst_element), "emit-signals", TRUE, NULL);
			__ms_signal_create(&node->sig_list, node->gst_element, "new-sample", G_CALLBACK(__ms_sink_new_buffer_cb), node);
			__ms_signal_create(&node->sig_list, node->gst_element, "eos", G_CALLBACK(sink_eos), node);
		}
		break;
	default:
		ms_error("Error: invalid Sink node Type [%d]", node->subtype);
		break;
	}

	MS_SAFE_FREE(plugin_name);

	if (node->gst_element == NULL)
		ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	else
		node->name = gst_element_get_name(node->gst_element);

	return ret;
}

void __ms_node_destroy(media_streamer_node_s *node)
{
	gchar *node_name = g_strdup(node->name);

	/* Disconnects and clean all node signals */
	g_list_free_full(node->sig_list, __ms_signal_destroy);

	MS_SAFE_UNREF(node->gst_element);
	MS_SAFE_FREE(node->name);
	MS_SAFE_FREE(node->callbacks_structure);
	MS_SAFE_FREE(node);

	ms_info("Node [%s] has been destroyed", node_name);
	MS_SAFE_GFREE(node_name);
}

int __ms_node_insert_into_table(GHashTable *nodes_table, media_streamer_node_s *ms_node)
{
	if (g_hash_table_contains(nodes_table, ms_node->name)) {
		ms_debug("Current Node [%s] already added into Media Streamer", ms_node->name);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}
	if (!g_hash_table_insert(nodes_table, (gpointer) ms_node->name, (gpointer) ms_node)) {
		ms_debug("Error: Failed to add node [%s] into Media Streamer", ms_node->name);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	ms_info("Node [%s] added into streamer, node type/subtype [%d/%d]", ms_node->name, ms_node->type, ms_node->subtype);
	return MEDIA_STREAMER_ERROR_NONE;
}

void __ms_node_remove_from_table(void *data)
{
	media_streamer_node_s *ms_node = (media_streamer_node_s *) data;
	ms_retm_if(ms_node == NULL, "Handle is NULL");

	if (__ms_element_unlink(ms_node->gst_element)) {
		ms_node->linked_by_user = FALSE;
		ms_node->parent_streamer = NULL;
		__ms_bin_remove_element(ms_node->gst_element);
		ms_info("Node [%s] removed from Media Streamer", ms_node->name);
	} else {
		ms_error("Error: Node [%s] remove failed", ms_node->name);
	}
}

static gboolean __ms_src_need_typefind(GstPad *src_pad)
{
	gboolean ret = FALSE;

	if (!src_pad || gst_pad_is_linked(src_pad))
		return FALSE;

	GstCaps *src_caps = gst_pad_query_caps(src_pad, NULL);
	if (gst_caps_is_any(src_caps))
		ret = TRUE;

	gst_caps_unref(src_caps);
	return ret;
}

node_info_s * __ms_node_get_klass_by_its_type(media_streamer_node_type_e element_type)
{
	int it_klass;
	for (it_klass = 0; nodes_info[it_klass].klass_name != NULL; it_klass++) {
		if (it_klass == element_type) {
			ms_info("Next node`s type klass is [%s]", nodes_info[it_klass].klass_name);
			break;
		}
	}

	return &nodes_info[it_klass];
}

static void _src_node_prepare(const GValue *item, gpointer user_data)
{
	media_streamer_s *ms_streamer = (media_streamer_s *) user_data;
	GstElement *src_element = GST_ELEMENT(g_value_get_object(item));
	g_object_ref(src_element);

	media_streamer_node_s *found_node = (media_streamer_node_s *) g_hash_table_lookup(ms_streamer->nodes_table, GST_ELEMENT_NAME(src_element));
	if (!found_node)
		return;

	ms_retm_if(!ms_streamer || !src_element, "Handle is NULL");
	ms_debug("Autoplug: found src element [%s]", GST_ELEMENT_NAME(src_element));

	GstPad *src_pad = gst_element_get_static_pad(src_element, "src");
	GstElement *found_element = NULL;

	if (__ms_src_need_typefind(src_pad)) {
		found_element = __ms_decodebin_create(ms_streamer);
		__ms_link_two_elements(src_element, src_pad, found_element);
		MS_SAFE_UNREF(src_element);
	} else {
		/* Check the source element`s pad type */
		const gchar *new_pad_type = __ms_get_pad_type(src_pad);
		/* If SRC Element linked by user, don`t consider the following nodes` managing */
		if (gst_pad_is_linked(src_pad)) {
			MS_SAFE_UNREF(src_pad);
			MS_SAFE_UNREF(src_element);
			return;
		}
		/* It is media streamer Server part */
		if (MS_ELEMENT_IS_VIDEO(new_pad_type) || MS_ELEMENT_IS_IMAGE(new_pad_type)) {
			found_element = __ms_combine_next_element(src_element, src_pad, ms_streamer->topology_bin, MEDIA_STREAMER_NODE_TYPE_FILTER);
			GstCaps *videoCaps = gst_caps_from_string(MEDIA_STREAMER_DEFAULT_CAMERA_FORMAT);
			g_object_set(G_OBJECT(found_element), "caps", videoCaps, NULL);
			gst_caps_unref(videoCaps);

			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_NODE_TYPE_VIDEO_ENCODER);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_NODE_TYPE_VIDEO_PAY);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_NODE_TYPE_RTP);
		}
		if (MS_ELEMENT_IS_AUDIO(new_pad_type)) {
			found_element = __ms_combine_next_element(src_element, src_pad, ms_streamer->topology_bin, MEDIA_STREAMER_NODE_TYPE_AUDIO_ENCODER);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_NODE_TYPE_AUDIO_PAY);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_NODE_TYPE_RTP);
		}
		__ms_generate_dots(ms_streamer->pipeline, "after_connecting_rtp");
		MS_SAFE_UNREF(found_element);
	}
	MS_SAFE_UNREF(src_pad);
}

int __ms_pipeline_prepare(media_streamer_s *ms_streamer)
{
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	int ret = MEDIA_STREAMER_ERROR_NONE;
	media_streamer_node_s *rtp_node = (media_streamer_node_s *)g_hash_table_lookup(ms_streamer->nodes_table, "rtp_container");
	if (rtp_node) {
		ret = __ms_rtp_element_prepare(rtp_node) ? MEDIA_STREAMER_ERROR_NONE : MEDIA_STREAMER_ERROR_INVALID_PARAMETER;
	} else {
		GstBin *nodes_bin = GST_BIN(ms_streamer->src_bin);
		if (nodes_bin->numchildren == 0) {
			ms_debug(" No any node is added to [%s]", GST_ELEMENT_NAME(ms_streamer->src_bin));
			return MEDIA_STREAMER_ERROR_INVALID_PARAMETER;
		}
		nodes_bin = GST_BIN(ms_streamer->sink_bin);
		if (nodes_bin->numchildren == 0) {
			ms_debug(" No any node is added to [%s]", GST_ELEMENT_NAME(ms_streamer->sink_bin));
			return MEDIA_STREAMER_ERROR_INVALID_PARAMETER;
		}
	}

	MS_BIN_FOREACH_ELEMENTS(ms_streamer->sink_bin, __ms_element_lock_state, ms_streamer);
	MS_BIN_FOREACH_ELEMENTS(ms_streamer->src_bin, _src_node_prepare, ms_streamer);

	ret = __ms_state_change(ms_streamer, MEDIA_STREAMER_STATE_READY);
	if (ret != MEDIA_STREAMER_ERROR_NONE)
		__ms_pipeline_unprepare(ms_streamer);

	return ret;
}

static gboolean __ms_bin_remove_elements(media_streamer_s *ms_streamer, GstElement *bin)
{
	GValue element = G_VALUE_INIT;
	GstIterator *bin_iterator = gst_bin_iterate_elements(GST_BIN(bin));

	/* If Bin doesn't have any elements function returns TRUE */
	gboolean ret = TRUE;

	GstElement *found_element = NULL;
	GstIteratorResult it_res = gst_iterator_next(bin_iterator, &element);
	while (GST_ITERATOR_OK == it_res) {
		found_element = (GstElement *) g_value_get_object(&element);

		/* Get node of this element if it appears as node */
		media_streamer_node_s *found_node = (media_streamer_node_s *) g_hash_table_lookup(ms_streamer->nodes_table, GST_ELEMENT_NAME(found_element));
		if (found_node) {
			if (!found_node->linked_by_user)
				ret = ret && __ms_element_unlink(found_element);
			else
				ms_info("Unprepare skipped user-linked node [%s]", found_node->name);
			__ms_generate_dots(ms_streamer->pipeline, GST_ELEMENT_NAME(found_element));
		} else {
			ret = ret && __ms_bin_remove_element(found_element);
		}

		g_value_reset(&element);

		it_res = gst_iterator_next(bin_iterator, &element);
		if (GST_ITERATOR_RESYNC == it_res) {
			gst_iterator_resync(bin_iterator);
			it_res = gst_iterator_next(bin_iterator, &element);
		}
	}
	g_value_unset(&element);
	gst_iterator_free(bin_iterator);

	return ret;
}

static void __ms_pending_pads_remove(void *data)
{
	GstPad *pad = GST_PAD(data);
	MS_SAFE_UNREF(pad);
}

int __ms_pipeline_unprepare(media_streamer_s *ms_streamer)
{
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	int ret = MEDIA_STREAMER_ERROR_NONE;

	__ms_element_set_state(ms_streamer->pipeline, GST_STATE_NULL);
	ms_streamer->state = MEDIA_STREAMER_STATE_IDLE;
	ms_streamer->pend_state = ms_streamer->state;

	MS_BIN_FOREACH_ELEMENTS(ms_streamer->sink_bin, __ms_element_unlock_state, ms_streamer);

	/* Disconnects and clean all autoplug signals */
	g_list_free_full(ms_streamer->autoplug_sig_list, __ms_signal_destroy);
	ms_streamer->autoplug_sig_list = NULL;

	/* Removes all pending pads according to list */
	g_list_free_full(ms_streamer->pads_types_list, __ms_pending_pads_remove);
	ms_streamer->pads_types_list = NULL;

	media_streamer_node_s *rtp_node = (media_streamer_node_s *)g_hash_table_lookup(ms_streamer->nodes_table, "rtp_container");
	if (rtp_node) {
		g_list_free_full(rtp_node->sig_list, __ms_signal_destroy);
		rtp_node->sig_list = NULL;
		MS_BIN_UNPREPARE(rtp_node->gst_element);
	}

	MS_BIN_UNPREPARE(ms_streamer->src_bin);
	MS_BIN_UNPREPARE(ms_streamer->topology_bin);
	MS_BIN_UNPREPARE(ms_streamer->sink_bin);

	return ret;
}

int __ms_node_set_params_from_bundle(media_streamer_node_s *node, bundle *param_list)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;
	GList *p_list = NULL;
	int written_count = 0;

	ret = __ms_node_get_param_list(node, &p_list);
	if (ret == MEDIA_STREAMER_ERROR_NONE) {
		param_s *param = NULL;
		GList *list_iter = NULL;
		char *string_val = NULL;
		for (list_iter = p_list; list_iter != NULL; list_iter = list_iter->next) {
			param = (param_s *)list_iter->data;
			if (bundle_get_str(param_list, param->param_name, &string_val) != BUNDLE_ERROR_KEY_NOT_AVAILABLE
				&& __ms_node_set_param_value(node, param, string_val) == MEDIA_STREAMER_ERROR_NONE)
				written_count++;
		}
	}
	g_list_free(p_list);

	ms_info("Set [%d] parameters of [%d]", written_count, bundle_get_count(param_list));
	if (written_count == 0)
		ret = MEDIA_STREAMER_ERROR_INVALID_PARAMETER;

	return ret;
}

int __ms_node_write_params_into_bundle(media_streamer_node_s *node, bundle *param_list)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;
	GList *p_list = NULL;

	ret = __ms_node_get_param_list(node, &p_list);
	if (ret == MEDIA_STREAMER_ERROR_NONE) {
		param_s *param = NULL;
		GList *list_iter = NULL;
		char *string_val = NULL;
		for (list_iter = p_list; list_iter != NULL; list_iter = list_iter->next) {
			param = (param_s *)list_iter->data;

			if (__ms_node_get_param_value(node, param, &string_val) == MEDIA_STREAMER_ERROR_NONE) {
				bundle_add_str(param_list, param->param_name, string_val);
				MS_SAFE_FREE(string_val);
			}
		}
	}
	g_list_free(p_list);

	return ret;
}

int __ms_node_get_param(media_streamer_node_s *node, const char *param_name, param_s **param)
{
	GParamSpec *param_spec;
	gboolean found_param = FALSE;
	int it_param;

	for (it_param = 0; param_table[it_param].param_name != NULL; it_param++) {
		if (!g_strcmp0(param_name, param_table[it_param].param_name)) {
			param_spec = g_object_class_find_property(G_OBJECT_GET_CLASS(node->gst_element), param_table[it_param].origin_name);
			if (param_spec || __ms_rtp_node_has_property(node, param_name)) {
				*param = &(param_table[it_param]);
				ms_info("Got parameter [%s] for node [%s]", (*param)->param_name, node->name);
				found_param = TRUE;
				break;
			}
		}
	}
	ms_retvm_if(!found_param, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Node [%s] doesn't have param [%s].", node->name, param_name);
	return MEDIA_STREAMER_ERROR_NONE;
}

int __ms_node_get_param_list(media_streamer_node_s *node, GList **param_list)
{
	GParamSpec *param_spec;
	int it_param;

	for (it_param = 0; param_table[it_param].param_name != NULL; it_param++) {
		param_spec = g_object_class_find_property(G_OBJECT_GET_CLASS(node->gst_element), param_table[it_param].origin_name);
		if (param_spec || __ms_rtp_node_has_property(node, param_table[it_param].param_name)) {
			ms_info("Got parameter [%s] for node [%s]", param_table[it_param].param_name, node->name);
			*param_list = g_list_append(*param_list, &(param_table[it_param]));
		}
	}
	ms_retvm_if(!(*param_list), MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Node [%s] doesn't have any params.", node->name);
	return MEDIA_STREAMER_ERROR_NONE;
}

int __ms_node_get_param_value(media_streamer_node_s *node, param_s *param, char **string_value)
{
	char *string_val = NULL;
	GParamSpec *param_spec = NULL;
	GValue value = G_VALUE_INIT;

	int ret = MEDIA_STREAMER_ERROR_NONE;

	if (node->type == MEDIA_STREAMER_NODE_TYPE_RTP)
		ret = __ms_rtp_node_get_property(node, param, &value);
	else {
		param_spec = g_object_class_find_property(G_OBJECT_GET_CLASS(node->gst_element), param->origin_name);
		if (param_spec) {
			g_value_init(&value, param_spec->value_type);
			g_object_get_property(G_OBJECT(node->gst_element), param->origin_name, &value);

			ms_info("Got parameter [%s] for node [%s] with description [%s]", param->param_name, node->name, g_param_spec_get_blurb(param_spec));
		} else {
			ms_error("There is no parameter [%s] for node [%s]", param->origin_name, node->name);
			return MEDIA_STREAMER_ERROR_INVALID_PARAMETER;
		}
	}

	if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_CAMERA_ID))
		if (G_VALUE_HOLDS_STRING(&value)) {
			/* v4l2src  have string property 'device' with value /dev/video[0-n].
			 * Try to get index from it. */
			const char *str_val = g_value_get_string(&value);
			if (str_val && g_str_has_prefix(str_val, "/dev/video")) {
				string_val = g_strdup(str_val + strlen("/dev/video"));
			} else {
				ms_info("Parameter [%s] was got not for MEDIA_STREAMER_NODE_SRC_TYPE_CAMERA type of nodes", param->param_name);
				ret = MEDIA_STREAMER_ERROR_INVALID_PARAMETER;
			}
		} else {
			string_val = g_strdup_printf("%d", g_value_get_int(&value));
		}
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_CAPTURE_WIDTH))
		string_val = g_strdup_printf("%d", g_value_get_int(&value));
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_CAPTURE_HEIGHT))
		string_val = g_strdup_printf("%d", g_value_get_int(&value));
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_IS_LIVE_STREAM))
		string_val = g_strdup(g_value_get_boolean(&value) ? "true" : "false");
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_URI))
		string_val = g_value_dup_string(&value);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_USER_AGENT))
		string_val = g_value_dup_string(&value);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_STREAM_TYPE))
		string_val = g_strdup_printf("%d", g_value_get_enum(&value));
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_PORT))
		string_val = g_strdup_printf("%d", g_value_get_int(&value));
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_VIDEO_IN_PORT))
		string_val = g_strdup_printf("%d", g_value_get_int(&value));
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_AUDIO_IN_PORT))
		string_val = g_strdup_printf("%d", g_value_get_int(&value));
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_VIDEO_OUT_PORT))
		string_val = g_strdup_printf("%d", g_value_get_int(&value));
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_AUDIO_OUT_PORT))
		string_val = g_strdup_printf("%d", g_value_get_int(&value));
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_IP_ADDRESS))
		string_val = g_value_dup_string(&value);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_AUDIO_DEVICE))
		string_val = g_value_dup_string(&value);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_CLOCK_SYNCHRONIZED))
		string_val = g_strdup(g_value_get_boolean(&value) ? "true" : "false");
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_ROTATE))
		string_val = g_strdup_printf("%d", g_value_get_enum(&value));
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_FLIP))
		string_val = g_strdup_printf("%d", g_value_get_enum(&value));
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_DISPLAY_GEOMETRY_METHOD))
		string_val = g_strdup_printf("%d", g_value_get_enum(&value));
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_VISIBLE))
		string_val = g_strdup(g_value_get_boolean(&value) ? "true" : "false");
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_HOST))
		string_val = g_value_dup_string(&value);

	*string_value = string_val;

	g_value_reset(&value);
	g_value_unset(&value);
	return ret;
}

int __ms_node_uri_path_check(const char *file_uri)
{
	struct stat stat_results = {0, };
	int file_open = 0;

	if (!file_uri || !strlen(file_uri))
		return MEDIA_STREAMER_ERROR_INVALID_PARAMETER;

	file_open = open(file_uri, O_RDONLY);
	if (file_open < 0) {
		char mes_error[256];
		strerror_r(errno, mes_error, sizeof(mes_error));
		ms_error("Couldn`t open file according to [%s]. Error N [%d]", mes_error, errno);

		if (EACCES == errno)
			return MEDIA_STREAMER_ERROR_PERMISSION_DENIED;

		return MEDIA_STREAMER_ERROR_INVALID_PARAMETER;
	}

	if (fstat(file_open, &stat_results) < 0) {
		ms_error("Couldn`t get status of the file [%s]", file_uri);
	} else if (stat_results.st_size == 0) {
		ms_error("The size of file is 0");
		close(file_open);
		return MEDIA_STREAMER_ERROR_INVALID_PARAMETER;
	} else {
		ms_debug("Size of file [%lld] bytes", (long long)stat_results.st_size);
	}

	close(file_open);

	return MEDIA_STREAMER_ERROR_NONE;
}

int __ms_node_set_param_value(media_streamer_node_s *ms_node, param_s *param, const char *param_value)
{
	ms_retvm_if(!ms_node || !param || !param_value,  MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	int ret = MEDIA_STREAMER_ERROR_NONE;

	if (ms_node->type == MEDIA_STREAMER_NODE_TYPE_RTP) {
		ret = __ms_rtp_node_set_property(ms_node, param, param_value);
		return ret;
	}

	if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_CAMERA_ID)) {
		int camera_id = (int)strtol(param_value, NULL, 10);
		ms_retvm_if(camera_id == -1, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Invalid %s value", param->param_name);
		if (g_str_has_prefix(ms_node->name, "v4l2src")) {
			/* v4l2src  have string property 'device' with value /dev/video[0-n]. */
			gchar *camera_device_str = g_strdup_printf("/dev/video%d", camera_id);
			g_object_set(ms_node->gst_element, param->origin_name, camera_device_str, NULL);
			MS_SAFE_GFREE(camera_device_str);
		} else
			g_object_set(ms_node->gst_element, param->origin_name, camera_id, NULL);
	} else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_CAPTURE_WIDTH))
		g_object_set(ms_node->gst_element, param->origin_name, (int)strtol(param_value, NULL, 10), NULL);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_CAPTURE_HEIGHT))
		g_object_set(ms_node->gst_element, param->origin_name, (int)strtol(param_value, NULL, 10), NULL);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_IS_LIVE_STREAM))
		g_object_set(ms_node->gst_element, param->origin_name, !g_ascii_strcasecmp(param_value, "true"), NULL);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_URI)) {
		ret = __ms_node_uri_path_check(param_value);
		if (ret == MEDIA_STREAMER_ERROR_NONE)
			g_object_set(ms_node->gst_element, param->origin_name, param_value, NULL);
	} else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_USER_AGENT))
		g_object_set(ms_node->gst_element, param->origin_name, param_value, NULL);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_STREAM_TYPE))
		g_object_set(ms_node->gst_element, param->origin_name, (int)strtol(param_value, NULL, 10), NULL);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_PORT))
		g_object_set(ms_node->gst_element, param->origin_name, (int)strtol(param_value, NULL, 10), NULL);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_IP_ADDRESS))
		g_object_set(ms_node->gst_element, param->origin_name, param_value, NULL);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_AUDIO_DEVICE))
		g_object_set(ms_node->gst_element, param->origin_name, param_value, NULL);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_CLOCK_SYNCHRONIZED))
		g_object_set(ms_node->gst_element, param->origin_name, !g_ascii_strcasecmp(param_value, "true"), NULL);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_ROTATE))
		g_object_set(ms_node->gst_element, param->origin_name, (int)strtol(param_value, NULL, 10), NULL);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_FLIP))
		g_object_set(ms_node->gst_element, param->origin_name, (int)strtol(param_value, NULL, 10), NULL);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_DISPLAY_GEOMETRY_METHOD))
		g_object_set(ms_node->gst_element, param->origin_name, (int)strtol(param_value, NULL, 10), NULL);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_EVAS_OBJECT))
		g_object_set(ms_node->gst_element, param->origin_name, (gpointer)param_value, NULL);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_VISIBLE))
		g_object_set(ms_node->gst_element, param->origin_name, !g_ascii_strcasecmp(param_value, "true"), NULL);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_HOST))
		g_object_set(ms_node->gst_element, param->origin_name, param_value, NULL);
	else {
		ms_info("Can not set parameter [%s] in the node [%s]", param->param_name, ms_node->name);
		ret = MEDIA_STREAMER_ERROR_INVALID_PARAMETER;
	}

	return ret;
}

int __ms_node_set_pad_format(media_streamer_node_s *node, const char *pad_name, media_format_h fmt)
{
	ms_retvm_if(!node || !pad_name || !fmt,  MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	int ret = MEDIA_STREAMER_ERROR_NONE;
	if (node->type == MEDIA_STREAMER_NODE_TYPE_RTP) {
		media_format_mimetype_e mime;
		gchar *rtp_caps_str = NULL;

		/* It is needed to set 'application/x-rtp' for audio and video udpsrc */
		if (g_strrstr(pad_name, MS_RTP_PAD_VIDEO_IN)) {
			ret = media_format_get_video_info(fmt, &mime, NULL, NULL, NULL, NULL);
			if (MEDIA_FORMAT_ERROR_NONE == ret) {
				rtp_caps_str = g_strdup_printf("application/x-rtp,media=(string)video,clock-rate=(int)90000,encoding-name=%s", __ms_convert_mime_to_rtp_format(mime));
				param_s param = {MEDIA_STREAMER_PARAM_VIDEO_IN_FORMAT, MEDIA_STREAMER_PARAM_VIDEO_IN_FORMAT};
				ret = __ms_node_set_param_value(node, &param, rtp_caps_str);
			}
		} else if (g_strrstr(pad_name, MS_RTP_PAD_AUDIO_IN)) {
			int audio_channels, audio_samplerate;
			ret = media_format_get_audio_info(fmt, &mime, &audio_channels, &audio_samplerate, NULL, NULL);
			if (MEDIA_FORMAT_ERROR_NONE == ret) {
				rtp_caps_str = g_strdup_printf("application/x-rtp,media=(string)audio,clock-rate=(int)8000,encoding-name=(string)AMR,encoding-params=(string)1,octet-align=(string)1");
				param_s param = {MEDIA_STREAMER_PARAM_AUDIO_IN_FORMAT, MEDIA_STREAMER_PARAM_AUDIO_IN_FORMAT};
				ret = __ms_node_set_param_value(node, &param, rtp_caps_str);
			}
		}

		MS_SAFE_GFREE(rtp_caps_str);
	} else {
		ret = __ms_element_set_fmt(node, pad_name, fmt);
	}

	return ret;
}
