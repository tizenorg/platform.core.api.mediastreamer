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

char *param_table[PROPERTY_COUNT][2] = {
	{MEDIA_STREAMER_PARAM_CAMERA_ID, "camera-id"},
	{MEDIA_STREAMER_PARAM_CAMERA_ID, "camera"},
	{MEDIA_STREAMER_PARAM_CAMERA_ID, "device-name"},
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
	{MEDIA_STREAMER_PARAM_VISIBLE, "visible"},
	{MEDIA_STREAMER_PARAM_HOST, "host"}
};

int __ms_node_set_property(media_streamer_node_s * ms_node, const gchar * param_key, const gchar * param_value)
{
	ms_retvm_if(!ms_node && !ms_node->gst_element, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error: empty node");
	ms_retvm_if(!param_key && !param_value, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Error: invalid property parameter");

	__ms_element_set_property(ms_node->gst_element, param_key, param_value);

	return MEDIA_STREAMER_ERROR_NONE;
}

static int __ms_rtp_node_set_property(media_streamer_node_s * ms_node, const char *param_key, const char *param_value)
{
	ms_retvm_if(!ms_node && !ms_node->gst_element, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error: empty node");

	gchar **tokens = NULL;
	GstElement *rtp_elem = NULL;
	GstElement *rtcp_elem = NULL;

	gchar *elem_name, *direction, *param;

	tokens = g_strsplit(param_key, "_", 3);
	if (tokens && tokens[0] && !tokens[1]) {
		/*In this case can be param names without direction description
		 * parameter 'host' will be set to all udpsink
		 * parameter 'format' will be set all udpsrc
		 */

		param = tokens[0];
		if (!g_strcmp0(param, MEDIA_STREAMER_PARAM_HOST)) {
			__ms_get_rtp_elements(ms_node, &rtp_elem, &rtcp_elem, "audio", "out", FALSE);
			if (rtp_elem && rtcp_elem) {
				__ms_element_set_property(rtp_elem, param, param_value);
				__ms_element_set_property(rtcp_elem, param, param_value);
				MS_SAFE_UNREF(rtp_elem);
				MS_SAFE_UNREF(rtcp_elem);
			}

			__ms_get_rtp_elements(ms_node, &rtp_elem, &rtcp_elem, "video", "out", FALSE);
			if (rtp_elem && rtcp_elem) {
				__ms_element_set_property(rtp_elem, param, param_value);
				__ms_element_set_property(rtcp_elem, param, param_value);
				MS_SAFE_UNREF(rtp_elem);
				MS_SAFE_UNREF(rtcp_elem);
			}
		} else {
			ms_error("Error: Unsupported parameter [%s] for rtp node.", param);
		}

		g_strfreev(tokens);
		return MEDIA_STREAMER_ERROR_NONE;
	} else if (tokens && tokens[0] && tokens[1] && tokens[2]) {

		/*
		 * Rtp node parameter name consist of three fields separated with symbol '_':
		 * <video/audio>_<in/out>_<param_key>
		 */

		elem_name = tokens[0];
		direction = tokens[1];
		param = tokens[2];
		if (FALSE == __ms_get_rtp_elements(ms_node, &rtp_elem, &rtcp_elem, elem_name, direction, TRUE)) {
			ms_error("Error: invalid parameter [%s]", param_key);
			g_strfreev(tokens);
			return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
		}

		if (!g_strcmp0(param, MEDIA_STREAMER_PARAM_PORT)) {
			__ms_element_set_property(rtp_elem, param, param_value);
			gchar *next_port = g_strdup(param_value);
			next_port[strlen(next_port) - 1] += 1;
			__ms_element_set_property(rtcp_elem, param, next_port);
			MS_SAFE_GFREE(next_port);
		}

		g_strfreev(tokens);
		MS_SAFE_UNREF(rtp_elem);
		MS_SAFE_UNREF(rtcp_elem);
		return MEDIA_STREAMER_ERROR_NONE;
	} else {
		ms_error("Invalid rtp parameter name.");
	}

	g_strfreev(tokens);

	return MEDIA_STREAMER_ERROR_INVALID_PARAMETER;
}

int __ms_node_create(media_streamer_node_s * node, media_format_h in_fmt, media_format_h out_fmt)
{
	ms_retvm_if(node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	int ret = MEDIA_STREAMER_ERROR_NONE;
	dictionary *dict = NULL;
	char *plugin_name = NULL;
	media_format_mimetype_e mime;
	gboolean dec_use;

	if (MEDIA_FORMAT_ERROR_NONE != media_format_get_video_info(out_fmt, &mime, NULL, NULL, NULL, NULL))
		media_format_get_audio_info(out_fmt, &mime, NULL, NULL, NULL, NULL);

	char *format_prefix = NULL;

	__ms_load_ini_dictionary(&dict);

	node->set_param = (ms_node_set_param) __ms_node_set_property;

	switch (node->type) {
	case MEDIA_STREAMER_NODE_TYPE_VIDEO_ENCODER:
		format_prefix = g_strdup_printf("%s:encoder", __ms_convert_mime_to_string(mime));
		plugin_name = __ms_ini_get_string(dict, format_prefix, DEFAULT_VIDEO_ENCODER);
		node->gst_element = __ms_video_encoder_element_create(dict, mime);
		break;
	case MEDIA_STREAMER_NODE_TYPE_VIDEO_DECODER:
		dec_use = iniparser_getboolean(dict, "general:use decodebin", DEFAULT_USE_DECODEBIN);
		if (dec_use) {
			node->gst_element = __ms_element_create("decodebin", NULL);
			__ms_signal_create(&node->sig_list, node->gst_element, "pad-added", G_CALLBACK(__decodebin_newpad_client_cb), node);
			__ms_signal_create(&node->sig_list, node->gst_element, "autoplug-select", G_CALLBACK(__ms_decodebin_autoplug_select), NULL);
		} else {
			format_prefix = g_strdup_printf("%s:decoder", __ms_convert_mime_to_string(mime));
			plugin_name = __ms_ini_get_string(dict, format_prefix, DEFAULT_VIDEO_DECODER);
			node->gst_element = __ms_video_decoder_element_create(dict, mime);
		}
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
		node->set_param = (ms_node_set_param) __ms_rtp_node_set_property;
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
static void __ms_src_start_feed_cb(GstElement * pipeline, guint size, gpointer data)
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
static void __ms_src_stop_feed_cb(GstElement * pipeline, gpointer data)
{
	media_streamer_node_s *ms_src = (media_streamer_node_s *) data;
	ms_retm_if(ms_src == NULL, "Handle is NULL");

	if (ms_src->callbacks_structure != NULL) {
		media_streamer_callback_s *src_callback = (media_streamer_callback_s *) ms_src->callbacks_structure;
		media_streamer_custom_buffer_status_cb buffer_status_cb = (media_streamer_custom_buffer_status_cb) src_callback->callback;
		buffer_status_cb((media_streamer_node_h) ms_src, MEDIA_STREAMER_CUSTOM_BUFFER_OVERFLOW, src_callback->user_data);
	}
}

int __ms_src_node_create(media_streamer_node_s * node)
{
	ms_retvm_if(node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	int ret = MEDIA_STREAMER_ERROR_NONE;
	dictionary *dict = NULL;
	char *plugin_name = NULL;

	__ms_load_ini_dictionary(&dict);

	node->set_param = (ms_node_set_param) __ms_node_set_property;

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
		node->gst_element = __ms_camera_element_create(plugin_name);

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
static void __ms_sink_new_buffer_cb(GstElement * sink, gpointer * data)
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
static void sink_eos(GstElement * sink, gpointer * data)
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

int __ms_sink_node_create(media_streamer_node_s * node)
{
	ms_retvm_if(node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	int ret = MEDIA_STREAMER_ERROR_NONE;
	dictionary *dict = NULL;
	char *plugin_name = NULL;

	__ms_load_ini_dictionary(&dict);

	node->set_param = (ms_node_set_param) __ms_node_set_property;

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
	case MEDIA_STREAMER_NODE_SINK_TYPE_SCREEN:
		plugin_name = __ms_ini_get_string(dict, "sinks:video_sink", DEFAULT_VIDEO_SINK);
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

void __ms_node_destroy(media_streamer_node_s * node)
{
	gchar *node_name = g_strdup(node->name);

	/* Disconnects and clean all node signals */
	g_list_free_full(node->sig_list, __ms_signal_destroy);

	MS_SAFE_UNREF(node->gst_element);
	MS_SAFE_FREE(node->name);
	MS_SAFE_FREE(node->callbacks_structure);
	MS_SAFE_FREE(node);

	ms_info("Node [%s] has been destroyed", node_name);
	MS_SAFE_FREE(node_name);
}

int __ms_node_insert_into_table(GHashTable * nodes_table, media_streamer_node_s * ms_node)
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

static gboolean __ms_src_need_typefind(GstElement * src)
{
	gboolean ret = FALSE;
	g_assert(src);

	GstPad *src_pad = gst_element_get_static_pad(src, "src");
	if (!src_pad || gst_pad_is_linked(src_pad)) {
		MS_SAFE_UNREF(src_pad);
		return FALSE;
	}

	GstCaps *src_caps = gst_pad_query_caps(src_pad, NULL);
	if (gst_caps_is_any(src_caps))
		ret = TRUE;

	gst_caps_unref(src_caps);
	MS_SAFE_UNREF(src_pad);
	return ret;
}

int __ms_pipeline_prepare(media_streamer_s * ms_streamer)
{
	GstElement *found_element;

	GstCaps *new_pad_caps = NULL;
	GstStructure *new_pad_struct = NULL;
	const gchar *new_pad_type = NULL;

	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	/* Find unlinked element in src_bin */
	GstPad *unlinked_pad = gst_bin_find_unlinked_pad(GST_BIN(ms_streamer->src_bin), GST_PAD_SRC);
	GstElement *unlinked_element = NULL;

	while (unlinked_pad) {
		unlinked_element = gst_pad_get_parent_element(unlinked_pad);
		ms_debug("Autoplug: found unlinked element [%s]", GST_ELEMENT_NAME(unlinked_element));

		/* If element in src bin is filesrc */
		if (__ms_src_need_typefind(unlinked_element)) {
			found_element = __ms_element_create("decodebin", NULL);
			__ms_bin_add_element(ms_streamer->topology_bin, found_element, FALSE);
			gst_element_sync_state_with_parent(found_element);

			if (__ms_bin_find_element_by_klass(ms_streamer->topology_bin, found_element, MEDIA_STREAMER_BIN_KLASS, "rtp_container")) {
				__ms_signal_create(&ms_streamer->autoplug_sig_list, found_element, "pad-added", G_CALLBACK(__decodebin_newpad_streamer_cb), ms_streamer);
				g_signal_connect(found_element, "pad-added", G_CALLBACK(__decodebin_newpad_streamer_cb), ms_streamer);
			} else {
				__ms_signal_create(&ms_streamer->autoplug_sig_list, found_element, "pad-added", G_CALLBACK(__decodebin_newpad_cb), ms_streamer);
				g_signal_connect(found_element, "pad-added", G_CALLBACK(__decodebin_newpad_cb), ms_streamer);
			}
			__ms_signal_create(&ms_streamer->autoplug_sig_list, found_element, "autoplug-select", G_CALLBACK(__ms_decodebin_autoplug_select), NULL);

			found_element = __ms_link_with_new_element(unlinked_element, found_element, NULL);
			__ms_generate_dots(ms_streamer->pipeline, GST_ELEMENT_NAME(found_element));

		} else {

			found_element = __ms_element_create(DEFAULT_QUEUE, NULL);
			__ms_bin_add_element(ms_streamer->topology_bin, found_element, FALSE);
			gst_element_sync_state_with_parent(found_element);

			found_element = __ms_link_with_new_element(unlinked_element, found_element, NULL);

			/* Check the new pad's type */
			new_pad_caps = gst_pad_query_caps(unlinked_pad, 0);
			new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
			new_pad_type = gst_structure_get_name(new_pad_struct);

			if (MS_ELEMENT_IS_VIDEO(new_pad_type)) {
				found_element = __ms_combine_next_element(found_element, MEDIA_STREAMER_BIN_KLASS, "video_encoder", NULL);
				found_element = __ms_combine_next_element(found_element, MEDIA_STREAMER_PAYLOADER_KLASS, NULL, NULL);
				found_element = __ms_combine_next_element(found_element, MEDIA_STREAMER_BIN_KLASS, "rtp_container", NULL);
			}

			if (MS_ELEMENT_IS_AUDIO(new_pad_type)) {
				found_element = __ms_combine_next_element(found_element, MEDIA_STREAMER_BIN_KLASS, "audio_encoder", NULL);
				found_element = __ms_combine_next_element(found_element, MEDIA_STREAMER_PAYLOADER_KLASS, NULL, NULL);
				found_element = __ms_combine_next_element(found_element, MEDIA_STREAMER_BIN_KLASS, "rtp_container", NULL);
			}
			__ms_generate_dots(ms_streamer->pipeline, GST_ELEMENT_NAME(found_element));
			gst_caps_unref(new_pad_caps);
		}
		MS_SAFE_UNREF(unlinked_pad);
		MS_SAFE_UNREF(unlinked_element);
		unlinked_pad = gst_bin_find_unlinked_pad(GST_BIN(ms_streamer->src_bin), GST_PAD_SRC);
	}

	return MEDIA_STREAMER_ERROR_NONE;
}

static gboolean __ms_bin_remove_elements(media_streamer_s * ms_streamer, GstElement * bin)
{
	GValue element = G_VALUE_INIT;
	GstIterator *bin_iterator = gst_bin_iterate_elements(GST_BIN(bin));

	gboolean ret = TRUE;		/* If Bin doesn't have any elements function returns TRUE */
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

int __ms_pipeline_unprepare(media_streamer_s * ms_streamer)
{
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	int ret = MEDIA_STREAMER_ERROR_NONE;

#define MS_BIN_UNPREPARE(bin) \
	if (!__ms_bin_remove_elements(ms_streamer, bin)) {\
		ms_debug("Got a few errors during unprepare [%s] bin.", GST_ELEMENT_NAME(bin));\
		ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;\
	}
	/* Disconnects and clean all autoplug signals */
	g_list_free_full(ms_streamer->autoplug_sig_list, __ms_signal_destroy);
	ms_streamer->autoplug_sig_list = NULL;

	MS_BIN_UNPREPARE(ms_streamer->src_bin);
	MS_BIN_UNPREPARE(ms_streamer->topology_bin);
	MS_BIN_UNPREPARE(ms_streamer->sink_video_bin);
	MS_BIN_UNPREPARE(ms_streamer->sink_audio_bin);

	__ms_bin_remove_element(ms_streamer->sink_video_bin);

	__ms_bin_remove_element(ms_streamer->sink_audio_bin);

#undef MS_BIN_UNPREPARE
	return ret;
}

static void __params_foreach_cb(const char *key, const int type, const bundle_keyval_t * kv, void *user_data)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;
	media_streamer_node_s *ms_node = (media_streamer_node_s *) user_data;
	ms_retm_if(ms_node == NULL, "Handle is NULL");

	void *basic_val = NULL;
	size_t basic_size = 0;

	ms_info("Try to add parameter [%s] with type [%d] to the node [%s].", key, type, ms_node->name);

	if (!bundle_keyval_type_is_array((bundle_keyval_t *) kv)) {
		bundle_keyval_get_basic_val((bundle_keyval_t *) kv, &basic_val, &basic_size);
		ms_info("Read param value[%s] with size [%lu].", (gchar *) basic_val, (unsigned long)basic_size);

		if (ms_node->set_param != NULL)
			ret = ms_node->set_param((struct media_streamer_node_s *)ms_node, (char *)key, (char *)basic_val);
		else
			ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;

	} else {
		ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	ms_retm_if(ret != MEDIA_STREAMER_ERROR_NONE, "Error while adding param [%s,%d] to the node [%s]", key, type, ms_node->name);
}

int __ms_node_read_params_from_bundle(media_streamer_node_s * node, bundle * param_list)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;
	bundle_foreach(param_list, __params_foreach_cb, (void *)node);
	return ret;
}

static void __ms_node_get_param_value(GParamSpec * param, GValue value, char **string_value)
{
	char *string_val = NULL;
	GParamSpecInt *pint = NULL;

	if (!g_strcmp0(param->name, MEDIA_STREAMER_PARAM_CAMERA_ID)) {
		pint = G_PARAM_SPEC_INT(param);
		string_val = g_strdup_printf("%d", g_value_get_int(&value));
		ms_info("Got int value: [%s], range: %d - %d", string_val, pint->minimum, pint->maximum);
	} else if (!g_strcmp0(param->name, MEDIA_STREAMER_PARAM_CAPTURE_WIDTH)) {
		pint = G_PARAM_SPEC_INT(param);
		string_val = g_strdup_printf("%d", g_value_get_int(&value));
		ms_info("Got int value: [%s], range: %d - %d", string_val, pint->minimum, pint->maximum);
	} else if (!g_strcmp0(param->name, MEDIA_STREAMER_PARAM_CAPTURE_HEIGHT)) {
		pint = G_PARAM_SPEC_INT(param);
		string_val = g_strdup_printf("%d", g_value_get_int(&value));
		ms_info("Got int value: [%s], range: %d - %d", string_val, pint->minimum, pint->maximum);
	} else if (!g_strcmp0(param->name, MEDIA_STREAMER_PARAM_IS_LIVE_STREAM)) {
		string_val = g_strdup_printf("%s", g_value_get_boolean(&value) ? "true" : "false");
		ms_info("Got boolean value: [%s]", string_val);
	} else if (!g_strcmp0(param->name, MEDIA_STREAMER_PARAM_URI)) {
		string_val = g_strdup_printf("%s", g_value_get_string(&value));
		ms_info("Got string value: [%s]", g_value_get_string(&value));
	} else if (!g_strcmp0(param->name, MEDIA_STREAMER_PARAM_USER_AGENT)) {
		string_val = g_strdup_printf("%s", g_value_get_string(&value));
		ms_info("Got string value: [%s]", g_value_get_string(&value));
	} else if (!g_strcmp0(param->name, MEDIA_STREAMER_PARAM_STREAM_TYPE)) {
		string_val = g_strdup_printf("%d", g_value_get_enum(&value));
		ms_info("Got int value: [%s] ", string_val);
	} else if (!g_strcmp0(param->name, MEDIA_STREAMER_PARAM_PORT)) {
		pint = G_PARAM_SPEC_INT(param);
		string_val = g_strdup_printf("%d", g_value_get_int(&value));
		ms_info("Got int value: [%s], range: %d - %d", string_val, pint->minimum, pint->maximum);
	} else if (!g_strcmp0(param->name, MEDIA_STREAMER_PARAM_VIDEO_IN_PORT)) {
		pint = G_PARAM_SPEC_INT(param);
		string_val = g_strdup_printf("%d", g_value_get_int(&value));
		ms_info("Got int value: [%s], range: %d - %d", string_val, pint->minimum, pint->maximum);
	} else if (!g_strcmp0(param->name, MEDIA_STREAMER_PARAM_AUDIO_IN_PORT)) {
		pint = G_PARAM_SPEC_INT(param);
		string_val = g_strdup_printf("%d", g_value_get_int(&value));
		ms_info("Got int value: [%s], range: %d - %d", string_val, pint->minimum, pint->maximum);
	} else if (!g_strcmp0(param->name, MEDIA_STREAMER_PARAM_VIDEO_OUT_PORT)) {
		pint = G_PARAM_SPEC_INT(param);
		string_val = g_strdup_printf("%d", g_value_get_int(&value));
		ms_info("Got int value: [%s], range: %d - %d", string_val, pint->minimum, pint->maximum);
	} else if (!g_strcmp0(param->name, MEDIA_STREAMER_PARAM_AUDIO_OUT_PORT)) {
		pint = G_PARAM_SPEC_INT(param);
		string_val = g_strdup_printf("%d", g_value_get_int(&value));
		ms_info("Got int value: [%s], range: %d - %d", string_val, pint->minimum, pint->maximum);
	} else if (!g_strcmp0(param->name, MEDIA_STREAMER_PARAM_IP_ADDRESS)) {
		string_val = g_strdup_printf("%s", g_value_get_string(&value));
		ms_info("Got string value: [%s]", g_value_get_string(&value));
	} else if (!g_strcmp0(param->name, MEDIA_STREAMER_PARAM_AUDIO_DEVICE)) {
		string_val = g_strdup_printf("%s", g_value_get_string(&value));
		ms_info("Got string value: [%s]", g_value_get_string(&value));
	} else if (!g_strcmp0(param->name, MEDIA_STREAMER_PARAM_CLOCK_SYNCHRONIZED)) {
		string_val = g_strdup_printf("%s", g_value_get_boolean(&value) ? "true" : "false");
		ms_info("Got boolean value: [%s]", string_val);
	} else if (!g_strcmp0(param->name, MEDIA_STREAMER_PARAM_ROTATE)) {
		string_val = g_strdup_printf("%d", g_value_get_enum(&value));
		ms_info("Got enum value: [%s] ", string_val);
	} else if (!g_strcmp0(param->name, MEDIA_STREAMER_PARAM_FLIP)) {
		string_val = g_strdup_printf("%d", g_value_get_enum(&value));
		ms_info("Got enum value: [%s] ", string_val);
	} else if (!g_strcmp0(param->name, MEDIA_STREAMER_PARAM_DISPLAY_GEOMETRY_METHOD)) {
		string_val = g_strdup_printf("%d", g_value_get_enum(&value));
		ms_info("Got enum value: [%s] ", string_val);
	} else if (!g_strcmp0(param->name, MEDIA_STREAMER_PARAM_VISIBLE)) {
		string_val = g_strdup_printf("%s", g_value_get_boolean(&value) ? "true" : "false");
		ms_info("Got boolean value: [%s]", string_val);
	} else if (!g_strcmp0(param->name, MEDIA_STREAMER_PARAM_HOST)) {
		string_val = g_strdup_printf("%s", g_value_get_string(&value));
		ms_info("Got string value: [%s]", g_value_get_string(&value));
	}

	*string_value = string_val;
}

void __ms_node_check_param_name(GstElement * element, gboolean name_is_known, const char *param_name, char **init_param_name)
{
	char *set_param_name = NULL;
	char *orig_param_name = NULL;
	int it_param = 0;

	GParamSpec *param;

	for (it_param = 0; it_param < PROPERTY_COUNT; it_param++) {
		set_param_name = param_table[it_param][0];
		orig_param_name = param_table[it_param][1];
		param = g_object_class_find_property(G_OBJECT_GET_CLASS(element), orig_param_name);

		if (name_is_known) {
			if (!g_strcmp0(param_name, set_param_name)) {
				if (param) {
					*init_param_name = orig_param_name;
					break;
				}
			}
		} else {
			if (!g_strcmp0(param_name, orig_param_name)) {
				if (param) {
					*init_param_name = set_param_name;
					break;
				}
			}
		}
	}
}

int __ms_node_write_params_into_bundle(media_streamer_node_s * node, bundle * param_list)
{
	GParamSpec **property_specs;
	guint num_properties, i;
	char *string_val = NULL;
	char *param_init_name = NULL;

	property_specs = g_object_class_list_properties(G_OBJECT_GET_CLASS(node->gst_element), &num_properties);

	if (num_properties <= 0)
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;

	ms_info("Getting parameter of the Node [%s]", node->name);

	for (i = 0; i < num_properties; i++) {
		GValue value = { 0, };
		GParamSpec *param = property_specs[i];

		__ms_node_check_param_name(node->gst_element, FALSE, param->name, &param_init_name);

		if (param_init_name) {
			g_value_init(&value, param->value_type);

			if (param->flags & G_PARAM_READWRITE) {
				g_object_get_property(G_OBJECT(node->gst_element), param->name, &value);
			} else {

				/* Skip properties which user can not change. */
				continue;
			}

			ms_info("%-20s: %s\n", param_init_name, g_param_spec_get_blurb(param));
			__ms_node_get_param_value(param, value, &string_val);

			bundle_add_str(param_list, param_init_name, string_val);
			param_init_name = NULL;
		}
	}

	return MEDIA_STREAMER_ERROR_NONE;
}

int __ms_node_write_param_into_value(media_streamer_node_s * node, const char *param_name, char **param_value)
{
	char *string_val = NULL;
	char *param_init_name = NULL;

	GValue value = { 0, };
	GParamSpec *param;

	__ms_node_check_param_name(node->gst_element, TRUE, param_name, &param_init_name);

	if (param_init_name) {
		param = g_object_class_find_property(G_OBJECT_GET_CLASS(node->gst_element), param_init_name);

		ms_info("Getting parameter of the Node [%s]", node->name);

		g_value_init(&value, param->value_type);

		if (param->flags & G_PARAM_READWRITE)
			g_object_get_property(G_OBJECT(node->gst_element), param_init_name, &value);

		ms_info("%-20s: %s\n", param_name, g_param_spec_get_blurb(param));
		__ms_node_get_param_value(param, value, &string_val);
	}

	if (!string_val)
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;

	*param_value = string_val;

	return MEDIA_STREAMER_ERROR_NONE;
}
