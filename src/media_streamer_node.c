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

static int __ms_node_set_property(media_streamer_node_s *ms_node,
                                  const gchar *param_key,
                                  const gchar *param_value)
{
	ms_retvm_if(!ms_node && !ms_node->gst_element, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error: empty node");
	ms_retvm_if(!param_key && !param_value, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Error: invalid property parameter");

	__ms_element_set_property(ms_node->gst_element, param_key, param_value);

	return MEDIA_STREAMER_ERROR_NONE;
}

static int __ms_rtp_node_set_property(media_streamer_node_s *ms_node,
                                      const char *param_key,
                                      const char *param_value)
{
	ms_retvm_if(!ms_node && !ms_node->gst_element, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error: empty node");

	gchar **tokens = NULL;
	gchar *elem_name = NULL;
	guint i = 0;
	GstElement *rtp_elem = NULL;
	GstElement *rtcp_elem = NULL;

	tokens = g_strsplit(param_key, ",", 3);
	ms_retvm_if(tokens == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Invalid rtp parameter line.");
	elem_name = tokens[0];

	if (FALSE == __ms_get_rtp_elements(ms_node, &rtp_elem, &rtcp_elem, elem_name)) {
		ms_error("Error: invalid parameter [%s]", param_key);
		g_strfreev(tokens);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	for (i = 1; (tokens && tokens[i]); i++) {
		ms_retvm_if(!rtp_elem || !rtcp_elem,
		            MEDIA_STREAMER_ERROR_INVALID_OPERATION,
		            "Error: [%s] did not found or created into streamer", tokens[i]);

		if (!g_strcmp0(tokens[i], "port")) {
			__ms_element_set_property(rtp_elem, tokens[i], param_value);
			gchar *next_port = g_strdup(param_value);
			next_port[strlen(next_port) - 1] += 1;
			__ms_element_set_property(rtcp_elem, tokens[i], next_port);
			MS_SAFE_GFREE(next_port);
		} else if (!g_strcmp0(tokens[i], "host") && MS_ELEMENT_IS_SINK(elem_name)) {
			__ms_element_set_property(rtp_elem, tokens[i], param_value);
			__ms_element_set_property(rtcp_elem, tokens[i], param_value);
		} else if (!g_strcmp0(tokens[i], "format") && MS_ELEMENT_IS_SOURCE(elem_name)) {
			__ms_element_set_property(rtp_elem, "caps", param_value);
		}

	}
	g_strfreev(tokens);

	return MEDIA_STREAMER_ERROR_NONE;
}

int __ms_node_create(media_streamer_node_s *node,
                     media_format_h in_fmt,
                     media_format_h out_fmt)
{
	ms_retvm_if(node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	dictionary *dict = NULL;
	char *plugin_name = NULL;
	media_format_mimetype_e mime;

	if (MEDIA_FORMAT_ERROR_NONE != media_format_get_video_info(out_fmt, &mime, NULL, NULL, NULL, NULL)) {
		media_format_get_audio_info(out_fmt, &mime, NULL, NULL, NULL, NULL);
	}
	char *format_prefix = NULL;

	__ms_load_ini_dictionary(&dict);

	node->set_param = (ms_node_set_param)__ms_node_set_property;

	switch (node->type) {
		case MEDIA_STREAMER_NODE_TYPE_VIDEO_ENCODER:
			format_prefix = g_strdup_printf("%s:encoder", __ms_convert_mime_to_string(mime));
			plugin_name = __ms_ini_get_string(dict,
			                                  format_prefix, DEFAULT_VIDEO_ENCODER);
			node->gst_element = __ms_video_encoder_element_create(dict, mime);
			break;
		case MEDIA_STREAMER_NODE_TYPE_VIDEO_DECODER:
			format_prefix = g_strdup_printf("%s:decoder", __ms_convert_mime_to_string(mime));
			plugin_name = __ms_ini_get_string(dict,
			                                  format_prefix, DEFAULT_VIDEO_DECODER);
			node->gst_element = __ms_video_decoder_element_create(dict, mime);
			break;
		case MEDIA_STREAMER_NODE_TYPE_PARSER:
			format_prefix = g_strdup_printf("%s:parser", __ms_convert_mime_to_string(mime));
			plugin_name = __ms_ini_get_string(dict,
			                                  format_prefix, DEFAULT_VIDEO_PARSER);
			node->gst_element = __ms_element_create(plugin_name, NULL);
			break;
		case MEDIA_STREAMER_NODE_TYPE_FILTER:
			node->gst_element = __ms_element_create("capsfilter", NULL);
			break;
		case MEDIA_STREAMER_NODE_TYPE_VIDEO_PAY:
			format_prefix = g_strdup_printf("%s:rtppay", __ms_convert_mime_to_string(mime));
			plugin_name = __ms_ini_get_string(dict,
			                                  format_prefix, DEFAULT_VIDEO_RTPPAY);
			node->gst_element = __ms_element_create(plugin_name, NULL);
			break;
		case MEDIA_STREAMER_NODE_TYPE_AUDIO_PAY:
			plugin_name = __ms_ini_get_string(dict,
			                                  "audio-raw:rtppay", DEFAULT_AUDIO_RTPPAY);
			node->gst_element = __ms_element_create(plugin_name, NULL);
			break;
		case MEDIA_STREAMER_NODE_TYPE_VIDEO_DEPAY:
			format_prefix = g_strdup_printf("%s:rtpdepay", __ms_convert_mime_to_string(mime));
			plugin_name = __ms_ini_get_string(dict,
			                                  format_prefix, DEFAULT_VIDEO_RTPDEPAY);
			node->gst_element = __ms_element_create(plugin_name, NULL);
			break;
		case MEDIA_STREAMER_NODE_TYPE_AUDIO_DEPAY:
			plugin_name = __ms_ini_get_string(dict,
			                                  "audio-raw:rtpdepay", DEFAULT_AUDIO_RTPPAY);
			node->gst_element = __ms_element_create(plugin_name, NULL);
			break;
		case MEDIA_STREAMER_NODE_TYPE_RTP:
			node->gst_element = __ms_rtp_element_create(node);
			node->set_param = (ms_node_set_param)__ms_rtp_node_set_property;
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

	ms_retvm_if(node->gst_element == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error creating GstElement");

	node->name = gst_element_get_name(node->gst_element);

	MS_SAFE_FREE(plugin_name);
	MS_SAFE_FREE(format_prefix);
	__ms_destroy_ini_dictionary(dict);

	return MEDIA_STREAMER_ERROR_NONE;
}

/* This signal callback is called when appsrc needs data, we add an idle handler
 * to the mainloop to start pushing data into the appsrc */
static void __ms_src_start_feed_cb(GstElement *pipeline, guint size, gpointer data)
{
	media_streamer_node_s *ms_src = (media_streamer_node_s *)data;
	ms_retm_if(ms_src == NULL, "Handle is NULL");

	if (ms_src->callbacks_structure != NULL) {
		media_streamer_callback_s *src_callback = (media_streamer_callback_s *)ms_src->callbacks_structure;
		media_streamer_custom_buffer_status_cb buffer_status_cb =
				(media_streamer_custom_buffer_status_cb) src_callback->callback;
		buffer_status_cb((media_streamer_node_h)ms_src, MEDIA_STREAMER_CUSTOM_BUFFER_UNDERRUN, src_callback->user_data);
	}
}

/* This callback is called when appsrc has enough data and we can stop sending.
 * We remove the idle handler from the mainloop */
static void __ms_src_stop_feed_cb(GstElement *pipeline, gpointer data)
{
	media_streamer_node_s *ms_src = (media_streamer_node_s *)data;
	ms_retm_if(ms_src == NULL, "Handle is NULL");

	if (ms_src->callbacks_structure != NULL) {
		media_streamer_callback_s *src_callback = (media_streamer_callback_s *)ms_src->callbacks_structure;
		media_streamer_custom_buffer_status_cb buffer_status_cb =
				(media_streamer_custom_buffer_status_cb) src_callback->callback;
		buffer_status_cb((media_streamer_node_h)ms_src, MEDIA_STREAMER_CUSTOM_BUFFER_OVERFLOW, src_callback->user_data);
	}
}

int __ms_src_node_create(media_streamer_node_s *node)
{
	ms_retvm_if(node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	dictionary *dict = NULL;
	char *plugin_name = NULL;

	__ms_load_ini_dictionary(&dict);

	node->set_param = (ms_node_set_param)__ms_node_set_property;

	switch (node->subtype) {
		case MEDIA_STREAMER_NODE_SRC_TYPE_FILE:
			ms_error("Error: not implemented yet");
			break;
		case MEDIA_STREAMER_NODE_SRC_TYPE_RTSP:
			node->gst_element = __ms_element_create(DEFAULT_UDP_SOURCE, NULL);
			break;
		case MEDIA_STREAMER_NODE_SRC_TYPE_HTTP:
			ms_error("Error: not implemented yet");
			break;
		case MEDIA_STREAMER_NODE_SRC_TYPE_CAMERA:
			plugin_name = __ms_ini_get_string(dict,
			                                  "sources:camera_source", DEFAULT_CAMERA_SOURCE);
			node->gst_element = __ms_camera_element_create(plugin_name);

			break;
		case MEDIA_STREAMER_NODE_SRC_TYPE_AUDIO_CAPTURE:
			plugin_name = __ms_ini_get_string(dict,
			                                  "sources:audio_source", DEFAULT_AUDIO_SOURCE);
			node->gst_element = __ms_element_create(plugin_name, NULL);
			break;
		case MEDIA_STREAMER_NODE_SRC_TYPE_VIDEO_CAPTURE:
			plugin_name = __ms_ini_get_string(dict,
			                                  "sources:video_source", DEFAULT_VIDEO_SOURCE);
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
			g_signal_connect(node->gst_element, "need-data", G_CALLBACK(__ms_src_start_feed_cb), (gpointer)node);
			g_signal_connect(node->gst_element, "enough-data", G_CALLBACK(__ms_src_stop_feed_cb), (gpointer)node);
			break;
		default:
			ms_error("Error: invalid Src node Type [%d]", node->subtype);
			break;
	}

	ms_retvm_if(node->gst_element == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error creating GstElement");
	node->name = gst_element_get_name(node->gst_element);

	MS_SAFE_FREE(plugin_name);
	__ms_destroy_ini_dictionary(dict);

	return MEDIA_STREAMER_ERROR_NONE;
}

/* The appsink has received a buffer */
static void __ms_sink_new_buffer_cb(GstElement *sink, gpointer *data)
{
	media_streamer_node_s *ms_sink = (media_streamer_node_s *)data;
	ms_retm_if(ms_sink == NULL, "Handle is NULL");

	if (ms_sink->callbacks_structure != NULL) {
		media_streamer_sink_callbacks_s *sink_callbacks =
				(media_streamer_sink_callbacks_s *)ms_sink->callbacks_structure;
		media_streamer_sink_data_ready_cb data_ready_cb =
				(media_streamer_sink_data_ready_cb) sink_callbacks->data_ready_cb.callback;

		data_ready_cb((media_streamer_node_h)ms_sink, sink_callbacks->data_ready_cb.user_data);
	}
}

/* The appsink has got eos */
static void sink_eos(GstElement *sink, gpointer *data)
{
	media_streamer_node_s *ms_sink = (media_streamer_node_s *)data;
	ms_retm_if(ms_sink == NULL, "Handle is NULL");

	if (ms_sink->callbacks_structure != NULL) {
		media_streamer_sink_callbacks_s *sink_callbacks =
				(media_streamer_sink_callbacks_s *)ms_sink->callbacks_structure;
		media_streamer_sink_eos_cb eos_cb =
				(media_streamer_sink_eos_cb) sink_callbacks->eos_cb.callback;

		eos_cb((media_streamer_node_h)ms_sink, sink_callbacks->eos_cb.user_data);
	}
}

int __ms_sink_node_create(media_streamer_node_s *node)
{
	ms_retvm_if(node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	dictionary *dict = NULL;
	char *plugin_name = NULL;

	__ms_load_ini_dictionary(&dict);

	node->set_param = (ms_node_set_param)__ms_node_set_property;

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
			plugin_name = __ms_ini_get_string(dict,
			                                  "sinks:audio_sink", DEFAULT_AUDIO_SINK);
			node->gst_element = __ms_element_create(plugin_name, NULL);
			break;
		case MEDIA_STREAMER_NODE_SINK_TYPE_SCREEN:
			plugin_name = __ms_ini_get_string(dict,
			                                  "sinks:video_sink", DEFAULT_VIDEO_SINK);
			node->gst_element = __ms_element_create(plugin_name, NULL);
			break;
		case MEDIA_STREAMER_NODE_SINK_TYPE_FAKE:
			node->gst_element = __ms_element_create(DEFAULT_FAKE_SINK, NULL);
			break;
		case MEDIA_STREAMER_NODE_SINK_TYPE_CUSTOM:
			node->gst_element = __ms_element_create(DEFAULT_APP_SINK, NULL);
			g_object_set(G_OBJECT(node->gst_element), "emit-signals", TRUE, NULL);
			g_signal_connect(node->gst_element, "new-sample", G_CALLBACK(__ms_sink_new_buffer_cb), (gpointer)node);
			g_signal_connect(node->gst_element, "eos", G_CALLBACK(sink_eos), (gpointer)node);
			break;
		default:
			ms_error("Error: invalid Sink node Type [%d]", node->subtype);
			break;
	}

	ms_retvm_if(node->gst_element == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error creating GstElement");
	node->name = gst_element_get_name(node->gst_element);

	MS_SAFE_FREE(plugin_name);
	__ms_destroy_ini_dictionary(dict);

	return MEDIA_STREAMER_ERROR_NONE;
}

void __ms_node_destroy(void *data)
{
	char *node_name = NULL;
	media_streamer_node_s *node = (media_streamer_node_s *)data;
	ms_retm_if(node == NULL, "Empty value while deleting element from table");

	__ms_element_unlink(node->gst_element);

	node_name = g_strdup(node->name);
	MS_SAFE_UNREF(node->gst_element);
	MS_SAFE_FREE(node->name);
	MS_SAFE_FREE(node->callbacks_structure);

	ms_info("Node [%s] destroyed", node_name);
	MS_SAFE_FREE(node_name);
}

void __ms_node_insert_into_table(GHashTable *nodes_table,
                                 media_streamer_node_s *ms_node)
{
	if (g_hash_table_contains(nodes_table, ms_node->name)) {
		ms_debug("Current Node [%s] already added", ms_node->name);
		return;
	}
	g_hash_table_insert(nodes_table, (gpointer)ms_node->name, (gpointer)ms_node);
	ms_info("Node [%s] added into streamer, node type/subtype [%d/%d]",
	        ms_node->name, ms_node->type, ms_node->subtype);
}

int __ms_node_remove_from_table(GHashTable *nodes_table,
                                media_streamer_node_s *ms_node)
{
	ms_retvm_if(nodes_table == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Handle is NULL");
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Handle is NULL");

	gboolean g_ret = g_hash_table_remove(nodes_table, ms_node->name);
	ms_retvm_if(g_ret != TRUE, MEDIA_STREAMER_ERROR_INVALID_OPERATION,
	            "Error while removing element from table");

	return MEDIA_STREAMER_ERROR_NONE;
}

int __ms_autoplug_prepare(media_streamer_s *ms_streamer)
{
	GstElement *unlinked_element = NULL;
	GstPad *unlinked_pad = NULL;
	gchar *unlinked_element_name = NULL;

	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER,
	            "Handle is NULL");

	/*Find unlinked element into src_bin */
	unlinked_pad = gst_bin_find_unlinked_pad(GST_BIN(ms_streamer->src_bin), GST_PAD_SRC);
	while (unlinked_pad)
	{
		GstCaps *pad_caps = gst_pad_query_caps(unlinked_pad, NULL);
		GstStructure *src_pad_struct = gst_caps_get_structure(pad_caps, 0);
		const gchar *field = gst_structure_get_name(src_pad_struct);

		unlinked_element = gst_pad_get_parent_element(unlinked_pad);
		unlinked_element_name = gst_element_get_name(unlinked_element);
		ms_debug("Autoplug: found unlinked element [%s] in src_bin with pad type [%s].",
				unlinked_element_name, field);

		/*find elements into topology */
		GstPad *unlinked_topo_pad = gst_bin_find_unlinked_pad(GST_BIN(ms_streamer->topology_bin), GST_PAD_SINK);
		GstElement *unlinked_topo_element = gst_pad_get_parent_element(unlinked_topo_pad);
		gchar *unlinked_topo_pad_name = gst_pad_get_name(unlinked_topo_pad);

		GstElement *encoder;
		GstElement *pay;
		if (MS_ELEMENT_IS_VIDEO(field)) {
			dictionary *dict = NULL;
			__ms_load_ini_dictionary(&dict);

			encoder = __ms_video_encoder_element_create(dict, MEDIA_FORMAT_H263);
			pay = __ms_element_create(DEFAULT_VIDEO_RTPPAY, NULL);
			gst_bin_add_many(GST_BIN(ms_streamer->topology_bin), encoder, pay, NULL);

			__ms_destroy_ini_dictionary(dict);

			gst_element_link_many(unlinked_element, encoder, pay, NULL);
			gst_element_link_pads(pay, "src", unlinked_topo_element, unlinked_topo_pad_name);
		} else if (MS_ELEMENT_IS_AUDIO(field)) {
			encoder = __ms_audio_encoder_element_create();
			pay = __ms_element_create(DEFAULT_AUDIO_RTPPAY, NULL);
			gst_bin_add_many(GST_BIN(ms_streamer->topology_bin), encoder, pay, NULL);

			gst_element_link_many(unlinked_element, encoder, pay, NULL);
			gst_element_link_pads(pay, "src", unlinked_topo_element, unlinked_topo_pad_name);
		} else {
			ms_debug("Autoplug mode doesn't support [%s] type nodes.", field);
		}

		MS_SAFE_GFREE(unlinked_topo_pad_name);
		MS_SAFE_GFREE(unlinked_element_name);
		MS_SAFE_UNREF(unlinked_pad);
		gst_caps_unref(pad_caps);

		unlinked_pad = gst_bin_find_unlinked_pad(GST_BIN(ms_streamer->src_bin), GST_PAD_SRC);
	}

	return MEDIA_STREAMER_ERROR_NONE;
}

static void __params_foreach_cb(const char *key,
                                const int type,
                                const bundle_keyval_t *kv,
                                void *user_data)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;
	media_streamer_node_s *ms_node = (media_streamer_node_s *)user_data;
	ms_retm_if(ms_node == NULL, "Handle is NULL");

	void *basic_val = NULL;
	size_t basic_size = 0;

	ms_info("Try to add parameter [%s] with type [%d] to the node [%s].", key, type, ms_node->name);

	if (!bundle_keyval_type_is_array((bundle_keyval_t *)kv)) {
		bundle_keyval_get_basic_val((bundle_keyval_t *)kv, &basic_val, &basic_size);
		ms_info("Read param value[%s] with size [%d].", (gchar *)basic_val, basic_size);

		if (ms_node->set_param != NULL) {
			ret = ms_node->set_param(ms_node, (gchar *)key, (gchar *)basic_val);
		} else {
			ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
		}

	} else {
		ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	ms_retm_if(ret != MEDIA_STREAMER_ERROR_NONE,
	           "Error while adding param [%s,%s] to the node [%s]",
	           key, type, ms_node->name);
}

int __ms_node_read_params_from_bundle(media_streamer_node_s *node,
                                      bundle *param_list)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;
	bundle_foreach(param_list, __params_foreach_cb, (void *)node);
	return ret;
}

int __ms_node_write_params_into_bundle(media_streamer_node_s *node,
                                       bundle *param_list)
{
	GParamSpec **property_specs;
	guint num_properties, i;
	char *string_val = NULL;

	property_specs = g_object_class_list_properties
	                 (G_OBJECT_GET_CLASS(node->gst_element), &num_properties);

	if (num_properties <= 0) {
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	ms_info("Getting parameter of the Node [%s]", node->name);

	for (i = 0; i < num_properties; i++) {
		GValue value = { 0, };
		GParamSpec *param = property_specs[i];

		g_value_init(&value, param->value_type);
		if (param->flags & G_PARAM_READWRITE) {
			g_object_get_property(G_OBJECT(node->gst_element), param->name, &value);
		} else {
			/* Skip properties which user can not change. */
			continue;
		}

		ms_info("%-20s: %s\n", g_param_spec_get_name(param), g_param_spec_get_blurb(param));

		switch (G_VALUE_TYPE(&value)) {
			case G_TYPE_STRING:
				bundle_add_str(param_list, g_param_spec_get_name(param), g_value_get_string(&value));
				ms_info("Got string value: [%s]", g_value_get_string(&value));
				break;

			case G_TYPE_BOOLEAN:
				string_val = g_strdup_printf("%s", g_value_get_boolean(&value) ? "true" : "false");
				bundle_add_str(param_list, g_param_spec_get_name(param), string_val);
				ms_info("Got boolean value: [%s]", string_val);
				break;

			case G_TYPE_ULONG: {
					GParamSpecULong *pulong = G_PARAM_SPEC_ULONG(param);
					string_val = g_strdup_printf("%lu", g_value_get_ulong(&value));
					bundle_add_str(param_list, g_param_spec_get_name(param), string_val);
					ms_info("Got ulong value: [%s], range: %lu - %lu (default %s)",
					        string_val, pulong->minimum, pulong->maximum);
					break;
				}

			case G_TYPE_LONG: {
					GParamSpecLong *plong = G_PARAM_SPEC_LONG(param);
					string_val = g_strdup_printf("%ld", g_value_get_long(&value));
					bundle_add_str(param_list, g_param_spec_get_name(param), string_val);
					ms_info("Got long value: [%s], range: %ld - %ld (default %s)",
					        string_val, plong->minimum, plong->maximum);
					break;
				}

			case G_TYPE_UINT: {
					GParamSpecUInt *puint = G_PARAM_SPEC_UINT(param);
					string_val = g_strdup_printf("%u", g_value_get_uint(&value));
					bundle_add_str(param_list, g_param_spec_get_name(param), string_val);
					ms_info("Got uint value: [%s], range: %u - %u",
					        string_val, puint->minimum, puint->maximum);
					break;
				}

			case G_TYPE_INT: {
					GParamSpecInt *pint = G_PARAM_SPEC_INT(param);
					string_val = g_strdup_printf("%d", g_value_get_int(&value));
					bundle_add_str(param_list, g_param_spec_get_name(param), string_val);
					ms_info("Got int value: [%s], range: %d - %d",
					        string_val, pint->minimum, pint->maximum);
					break;
				}

			case G_TYPE_UINT64: {
					GParamSpecUInt64 *puint64 = G_PARAM_SPEC_UINT64(param);
					string_val = g_strdup_printf("%" G_GUINT64_FORMAT, g_value_get_uint64(&value));
					bundle_add_str(param_list, g_param_spec_get_name(param), string_val);
					ms_info("Got uint64 value: [%s], range: %" G_GUINT64_FORMAT "- %" G_GUINT64_FORMAT,
					        string_val, puint64->minimum, puint64->maximum);
					break;
				}

			case G_TYPE_INT64: {
					GParamSpecInt64 *pint64 = G_PARAM_SPEC_INT64(param);
					string_val = g_strdup_printf("%" G_GINT64_FORMAT, g_value_get_int64(&value));
					bundle_add_str(param_list, g_param_spec_get_name(param), string_val);
					ms_info("Got uint64 value: [%s], range: %" G_GINT64_FORMAT "- %" G_GINT64_FORMAT,
					        string_val, pint64->minimum, pint64->maximum);
					break;
				}

			case G_TYPE_FLOAT: {
					GParamSpecFloat *pfloat = G_PARAM_SPEC_FLOAT(param);
					string_val = g_strdup_printf("%15.7g", g_value_get_float(&value));
					bundle_add_str(param_list, g_param_spec_get_name(param), string_val);
					ms_info("Got float value: [%s], range:%15.7g -%15.7g",
					        string_val, pfloat->minimum, pfloat->maximum);
					break;
				}

			case G_TYPE_DOUBLE: {
					GParamSpecDouble *pdouble = G_PARAM_SPEC_DOUBLE(param);
					string_val = g_strdup_printf("%15.7g", g_value_get_double(&value));
					bundle_add_str(param_list, g_param_spec_get_name(param), string_val);
					ms_info("Got double value: [%s], range:%15.7g -%15.7g",
					        string_val, pdouble->minimum, pdouble->maximum);
					break;
				}

			default:
				ms_info("Got unknown type with param->value_type [%d]", param->value_type);
				break;

				MS_SAFE_FREE(string_val);
		}
	}
	return MEDIA_STREAMER_ERROR_NONE;
}
