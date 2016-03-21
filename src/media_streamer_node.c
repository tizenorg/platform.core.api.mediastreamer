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

static gboolean __ms_rtp_node_has_property(media_streamer_node_s *ms_node, const gchar *param_name)
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
		g_value_take_string(val, g_strdup(param_value));
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
	dictionary *dict = NULL;
	char *plugin_name = NULL;
	media_format_mimetype_e mime;

	if (MEDIA_FORMAT_ERROR_NONE != media_format_get_video_info(out_fmt, &mime, NULL, NULL, NULL, NULL))
		media_format_get_audio_info(out_fmt, &mime, NULL, NULL, NULL, NULL);
	char *format_prefix = NULL;

	__ms_load_ini_dictionary(&dict);

	switch (node->type) {
	case MEDIA_STREAMER_NODE_TYPE_VIDEO_ENCODER:
		format_prefix = g_strdup_printf("%s:encoder", __ms_convert_mime_to_string(mime));
		plugin_name = __ms_ini_get_string(dict, format_prefix, DEFAULT_VIDEO_ENCODER);
		node->gst_element = __ms_video_encoder_element_create(dict, mime);
		break;
	case MEDIA_STREAMER_NODE_TYPE_VIDEO_DECODER:
		format_prefix = g_strdup_printf("%s:decoder", __ms_convert_mime_to_string(mime));
		plugin_name = __ms_ini_get_string(dict, format_prefix, DEFAULT_VIDEO_DECODER);
		node->gst_element = __ms_video_decoder_element_create(dict, mime);
		break;
	case MEDIA_STREAMER_NODE_TYPE_PARSER:
		format_prefix = g_strdup_printf("%s:parser", __ms_convert_mime_to_string(mime));
		plugin_name = __ms_ini_get_string(dict, format_prefix, DEFAULT_VIDEO_PARSER);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_TYPE_FILTER:
		node->gst_element = __ms_element_create("capsfilter", NULL);
		break;
	case MEDIA_STREAMER_NODE_TYPE_VIDEO_PAY:
		format_prefix = g_strdup_printf("%s:rtppay", __ms_convert_mime_to_string(mime));
		plugin_name = __ms_ini_get_string(dict, format_prefix, DEFAULT_VIDEO_RTPPAY);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_TYPE_AUDIO_PAY:
		plugin_name = __ms_ini_get_string(dict, "audio-raw:rtppay", DEFAULT_AUDIO_RTPPAY);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_TYPE_VIDEO_DEPAY:
		format_prefix = g_strdup_printf("%s:rtpdepay", __ms_convert_mime_to_string(mime));
		plugin_name = __ms_ini_get_string(dict, format_prefix, DEFAULT_VIDEO_RTPDEPAY);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_TYPE_AUDIO_DEPAY:
		plugin_name = __ms_ini_get_string(dict, "audio-raw:rtpdepay", DEFAULT_AUDIO_RTPDEPAY);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_TYPE_RTP:
		node->gst_element = __ms_rtp_element_create(node);
		break;
	case MEDIA_STREAMER_NODE_TYPE_QUEUE:
		node->gst_element = __ms_element_create(DEFAULT_QUEUE, NULL);
		break;
	case MEDIA_STREAMER_NODE_TYPE_AUDIO_ENCODER:
		node->gst_element = __ms_audio_encoder_element_create();
		break;
	case MEDIA_STREAMER_NODE_TYPE_VIDEO_CONVERTER:
		node->gst_element = __ms_element_create("videoconvert", NULL);
		break;
	case MEDIA_STREAMER_NODE_TYPE_AUDIO_CONVERTER:
		node->gst_element = __ms_element_create("audioconvert", NULL);
		break;
	case MEDIA_STREAMER_NODE_TYPE_AUDIO_RESAMPLE:
		node->gst_element = __ms_element_create("audioresample", NULL);
		break;
	default:
		ms_error("Error: invalid node Type [%d]", node->type);
		break;
	}

	MS_SAFE_FREE(plugin_name);
	MS_SAFE_FREE(format_prefix);
	__ms_destroy_ini_dictionary(dict);

	if (node->gst_element == NULL)
		ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	else
		node->name = gst_element_get_name(node->gst_element);

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

int __ms_src_node_create(media_streamer_node_s *node)
{
	ms_retvm_if(node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	int ret = MEDIA_STREAMER_ERROR_NONE;
	dictionary *dict = NULL;
	char *plugin_name = NULL;

	__ms_load_ini_dictionary(&dict);

	switch (node->subtype) {
	case MEDIA_STREAMER_NODE_SRC_TYPE_FILE:
		node->gst_element = __ms_element_create(DEFAULT_FILE_SOURCE, NULL);
		break;
	case MEDIA_STREAMER_NODE_SRC_TYPE_RTSP:
		node->gst_element = __ms_element_create(DEFAULT_UDP_SOURCE, NULL);
		break;
	case MEDIA_STREAMER_NODE_SRC_TYPE_HTTP:
		node->gst_element = __ms_element_create(DEFAULT_HTTP_SOURCE, NULL);
		break;
	case MEDIA_STREAMER_NODE_SRC_TYPE_CAMERA:
		plugin_name = __ms_ini_get_string(dict, "sources:camera_source", DEFAULT_CAMERA_SOURCE);
		node->gst_element = __ms_element_create(plugin_name, NULL);

		break;
	case MEDIA_STREAMER_NODE_SRC_TYPE_AUDIO_CAPTURE:
		plugin_name = __ms_ini_get_string(dict, "sources:audio_source", DEFAULT_AUDIO_SOURCE);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_SRC_TYPE_VIDEO_CAPTURE:
		plugin_name = __ms_ini_get_string(dict, "sources:video_source", DEFAULT_VIDEO_SOURCE);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_SRC_TYPE_VIDEO_TEST:
		node->gst_element = __ms_element_create(DEFAULT_VIDEO_TEST_SOURCE, NULL);
		break;
	case MEDIA_STREAMER_NODE_SRC_TYPE_AUDIO_TEST:
		node->gst_element = __ms_element_create(DEFAULT_AUDIO_TEST_SOURCE, NULL);
		break;
	case MEDIA_STREAMER_NODE_SRC_TYPE_CUSTOM:
		node->gst_element = __ms_element_create(DEFAULT_APP_SOURCE, NULL);
		__ms_signal_create(&node->sig_list, node->gst_element, "need-data", G_CALLBACK(__ms_src_start_feed_cb), node);
		__ms_signal_create(&node->sig_list, node->gst_element, "enough-data", G_CALLBACK(__ms_src_stop_feed_cb), node);
		break;
	default:
		ms_error("Error: invalid Src node Type [%d]", node->subtype);
		break;
	}

	MS_SAFE_FREE(plugin_name);
	__ms_destroy_ini_dictionary(dict);

	if (node->gst_element == NULL)
		ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	else
		node->name = gst_element_get_name(node->gst_element);

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
	dictionary *dict = NULL;
	char *plugin_name = NULL;

	__ms_load_ini_dictionary(&dict);

	switch (node->subtype) {
	case MEDIA_STREAMER_NODE_SINK_TYPE_FILE:
		ms_error("Error: not implemented yet");
		break;
	case MEDIA_STREAMER_NODE_SINK_TYPE_RTSP:
		node->gst_element = __ms_element_create(DEFAULT_UDP_SINK, NULL);
		break;
	case MEDIA_STREAMER_NODE_SINK_TYPE_HTTP:
		ms_error("Error: not implemented yet");
		break;
	case MEDIA_STREAMER_NODE_SINK_TYPE_AUDIO:
		plugin_name = __ms_ini_get_string(dict, "sinks:audio_sink", DEFAULT_AUDIO_SINK);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_SINK_TYPE_OVERLAY:
		plugin_name = __ms_ini_get_string(dict, "sinks:video_sink", DEFAULT_VIDEO_SINK);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_SINK_TYPE_EVAS:
		plugin_name = __ms_ini_get_string(dict, "sinks:evas_sink", DEFAULT_EVAS_SINK);
		node->gst_element = __ms_element_create(plugin_name, NULL);
		break;
	case MEDIA_STREAMER_NODE_SINK_TYPE_FAKE:
		node->gst_element = __ms_element_create(DEFAULT_FAKE_SINK, NULL);
		break;
	case MEDIA_STREAMER_NODE_SINK_TYPE_CUSTOM:
		node->gst_element = __ms_element_create(DEFAULT_APP_SINK, NULL);
		g_object_set(G_OBJECT(node->gst_element), "emit-signals", TRUE, NULL);
		__ms_signal_create(&node->sig_list, node->gst_element, "new-sample", G_CALLBACK(__ms_sink_new_buffer_cb), node);
		__ms_signal_create(&node->sig_list, node->gst_element, "eos", G_CALLBACK(sink_eos), node);
		break;
	default:
		ms_error("Error: invalid Sink node Type [%d]", node->subtype);
		break;
	}

	MS_SAFE_FREE(plugin_name);
	__ms_destroy_ini_dictionary(dict);

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

static void _sink_node_lock_state(const GValue *item, gpointer user_data)
{
	GstElement *sink_element = GST_ELEMENT(g_value_get_object(item));
	ms_retm_if(!sink_element, "Handle is NULL");

	if (!gst_element_is_locked_state(sink_element)) {
		GstState current_state = GST_STATE_VOID_PENDING;
		gst_element_set_locked_state(sink_element, TRUE);

		gst_element_get_state(sink_element, &current_state, NULL, GST_MSECOND);

		ms_debug("Locked sink element [%s] into state [%s]", GST_ELEMENT_NAME(sink_element), gst_element_state_get_name(current_state));
	} else {
		gst_element_set_locked_state(sink_element, FALSE);
		gst_element_sync_state_with_parent(sink_element);
	}
}

static void _src_node_prepare(const GValue *item, gpointer user_data)
{
	media_streamer_s *ms_streamer = (media_streamer_s *) user_data;
	GstElement *src_element = GST_ELEMENT(g_value_get_object(item));

	ms_retm_if(!ms_streamer || !src_element, "Handle is NULL");
	ms_debug("Autoplug: found src element [%s]", GST_ELEMENT_NAME(src_element));

	GstPad *src_pad = gst_element_get_static_pad(src_element, "src");
	GstElement *found_element = NULL;

	if (__ms_src_need_typefind(src_pad)) {
		found_element = __ms_decodebin_create(ms_streamer);
		found_element = __ms_link_with_new_element(src_element, src_pad, found_element);
	} else {
		/* Check the source element`s pad type */
		const gchar *new_pad_type = __ms_get_pad_type(src_pad);
		if (gst_pad_is_linked(src_pad)) {
			MS_BIN_FOREACH_ELEMENTS(ms_streamer->sink_bin, _sink_node_lock_state, NULL);
			MS_SAFE_UNREF(src_pad);
			return;
		}

		if (MS_ELEMENT_IS_VIDEO(new_pad_type) || MS_ELEMENT_IS_IMAGE(new_pad_type)) {
			found_element = __ms_combine_next_element(src_element, src_pad, ms_streamer->topology_bin, MEDIA_STREAMER_QUEUE_KLASS, NULL, DEFAULT_QUEUE);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_BIN_KLASS, "video_encoder", NULL);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_PAYLOADER_KLASS, NULL, NULL);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_BIN_KLASS, "rtp_container", NULL);
		}

		if (MS_ELEMENT_IS_AUDIO(new_pad_type)) {
			found_element = __ms_combine_next_element(src_element, src_pad, ms_streamer->topology_bin, MEDIA_STREAMER_QUEUE_KLASS, NULL, DEFAULT_QUEUE);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_BIN_KLASS, "audio_encoder", NULL);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_PAYLOADER_KLASS, NULL, DEFAULT_AUDIO_RTPPAY);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_BIN_KLASS, "rtp_container", NULL);
		}
		__ms_generate_dots(ms_streamer->pipeline, "after_connecting_rtp");
	}
	MS_SAFE_UNREF(src_pad);
}

int __ms_pipeline_prepare(media_streamer_s *ms_streamer)
{
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	media_streamer_node_s *rtp_node = (media_streamer_node_s *)g_hash_table_lookup(ms_streamer->nodes_table, "rtp_container");
	if (rtp_node)
		__ms_rtp_element_prepare(rtp_node);

	MS_BIN_FOREACH_ELEMENTS(ms_streamer->sink_bin, _sink_node_lock_state, NULL);
	MS_BIN_FOREACH_ELEMENTS(ms_streamer->src_bin, _src_node_prepare, ms_streamer);

	return MEDIA_STREAMER_ERROR_NONE;
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

			if (__ms_node_get_param_value(node, param, &string_val) == MEDIA_STREAMER_ERROR_NONE)
				bundle_add_str(param_list, param->param_name, string_val);
		}
	}
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
	GParamSpec *param_spec;
	GValue value = G_VALUE_INIT;

	int ret = MEDIA_STREAMER_ERROR_NONE;

	if (node->type == MEDIA_STREAMER_NODE_TYPE_RTP)
		ret = __ms_rtp_node_get_property(node, param, &value);
	else {
		param_spec = g_object_class_find_property(G_OBJECT_GET_CLASS(node->gst_element), param->origin_name);

		g_value_init(&value, param_spec->value_type);
		g_object_get_property(G_OBJECT(node->gst_element), param->origin_name, &value);

		ms_info("Got parameter [%s] for node [%s] with description [%s]", param->param_name, node->name, g_param_spec_get_blurb(param_spec));
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
	g_value_unset(&value);
	return ret;
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
			MS_SAFE_FREE(camera_device_str);
		} else
			g_object_set(ms_node->gst_element, param->origin_name, camera_id, NULL);
	} else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_CAPTURE_WIDTH))
		g_object_set(ms_node->gst_element, param->origin_name, (int)strtol(param_value, NULL, 10), NULL);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_CAPTURE_HEIGHT))
		g_object_set(ms_node->gst_element, param->origin_name, (int)strtol(param_value, NULL, 10), NULL);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_IS_LIVE_STREAM))
		g_object_set(ms_node->gst_element, param->origin_name, !g_ascii_strcasecmp(param_value, "true"), NULL);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_URI))
		g_object_set(ms_node->gst_element, param->origin_name, param_value, NULL);
	else if (!g_strcmp0(param->param_name, MEDIA_STREAMER_PARAM_USER_AGENT))
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
		if (!strcmp(pad_name, MS_RTP_PAD_VIDEO_IN"_rtp")) {
			ret = media_format_get_video_info(fmt, &mime, NULL, NULL, NULL, NULL);
			if (MEDIA_FORMAT_ERROR_NONE == ret) {
				rtp_caps_str = g_strdup_printf("application/x-rtp,media=video,clock-rate=90000,encoding-name=%s", __ms_convert_mime_to_rtp_format(mime));
				param_s param = {MEDIA_STREAMER_PARAM_VIDEO_IN_FORMAT, MEDIA_STREAMER_PARAM_VIDEO_IN_FORMAT};
				ret = __ms_node_set_param_value(node, &param, rtp_caps_str);
			}
		} else if (!strcmp(pad_name, MS_RTP_PAD_AUDIO_IN"_rtp")) {
			int audio_channels, audio_samplerate;
			ret = media_format_get_audio_info(fmt, &mime, &audio_channels, &audio_samplerate, NULL, NULL);
			if (MEDIA_FORMAT_ERROR_NONE == ret) {
				rtp_caps_str = g_strdup_printf("application/x-rtp,media=audio,clock-rate=%d,encoding-name=%s,channels=%d,payload=96", audio_samplerate, __ms_convert_mime_to_rtp_format(mime), audio_channels);
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
