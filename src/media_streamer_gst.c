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

#include <bundle.h>
#include <media_streamer_gst.h>

void __ms_generate_dots(GstElement *bin, gchar *name_tag)
{
	gchar *dot_name;
	ms_retm_if(bin == NULL, "Handle is NULL");

	if (!name_tag) {
		dot_name = g_strdup(DOT_FILE_NAME);
	} else {
		dot_name = g_strconcat(DOT_FILE_NAME, ".", name_tag, NULL);
	}

	GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(bin), GST_DEBUG_GRAPH_SHOW_ALL, dot_name);

	MS_SAFE_GFREE(dot_name);
}

const char *_ms_state_to_string(GstState state)
{
	static const char pending[] = "PENDING\0";
	static const char null_s[] = "NULL\0";
	static const char ready[] = "READY\0";
	static const char paused[] = "PAUSED\0";
	static const char playing[] = "PLAYING\0";
	switch (state) {
		case GST_STATE_VOID_PENDING:
			return pending;
			break;
		case GST_STATE_NULL:
			return null_s;
			break;
		case GST_STATE_READY:
			return ready;
			break;
		case GST_STATE_PAUSED:
			return paused;
			break;
		case GST_STATE_PLAYING:
			return playing;
			break;
		default:
			return "\0";
			break;
	};
	return 0;
}

static int __ms_add_ghostpad(GstElement *gst_element,
                             const char *pad_name,
                             GstElement *gst_bin,
                             const char *ghost_pad_name)
{
	ms_retvm_if(!ghost_pad_name || !gst_bin, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	int ret = MEDIA_STREAMER_ERROR_NONE;

	GstPad *ghost_pad = NULL;
	gchar *bin_name = gst_element_get_name(gst_bin);

	if (!gst_element || !pad_name) {
		ghost_pad = gst_ghost_pad_new_no_target(ghost_pad_name, GST_PAD_SRC);
		gst_element_add_pad(GST_ELEMENT(gst_bin), ghost_pad);
		ms_info("Added [%s] empty ghostpad into [%s]", ghost_pad_name, bin_name);
		ret = MEDIA_STREAMER_ERROR_NONE;
	} else {
		gchar *element_name = gst_element_get_name(gst_element);
		GstPad *element_pad = gst_element_get_static_pad(gst_element, pad_name);
		if (!element_pad) {
			/*maybe it is request pad */
			element_pad = gst_element_get_request_pad(gst_element, pad_name);
		}
		if (element_pad != NULL) {
			ghost_pad = gst_ghost_pad_new(ghost_pad_name, element_pad);
			gst_pad_set_active(ghost_pad, TRUE);

			gst_element_add_pad(GST_ELEMENT(gst_bin), ghost_pad);
			ms_info("Added %s ghostpad from [%s] into [%s]", pad_name, element_name, bin_name);
			MS_SAFE_UNREF(element_pad);
			MS_SAFE_GFREE(element_name);

			ret = MEDIA_STREAMER_ERROR_NONE;
		} else {
			ms_error("Error: element [%s] does not have valid [%s] pad for adding into [%s] bin",
			         element_name, pad_name, bin_name);
			ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
		}
	}

	MS_SAFE_GFREE(bin_name);

	return ret;
}

static GObject *__ms_get_property_owner(GstElement *element, const gchar *key, GValue *value)
{
	GParamSpec *param;
	GObject *obj = NULL;

	if (GST_IS_CHILD_PROXY(element)) {
		int i;
		int childs_count = gst_child_proxy_get_children_count(GST_CHILD_PROXY(element));

		param = NULL;
		for (i = 0; (i < childs_count) && (param == NULL); ++i) {
			obj = gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(element), i);
			param = g_object_class_find_property(G_OBJECT_GET_CLASS(obj), key);
		}
		ms_retvm_if(param == NULL || obj == NULL, NULL, "Error: Bin object does not have property [%s].", key);
	} else {
		obj = G_OBJECT(element);
		param = g_object_class_find_property
		        (G_OBJECT_GET_CLASS(obj), key);
	}

	g_value_init(value, param->value_type);

	if (param->flags & G_PARAM_WRITABLE) {
		g_object_get_property(G_OBJECT(obj), key, value);
	} else {
		/* Skip properties which user can not change. */
		ms_error("Error: node param [%s] is not writable!", key);
		return NULL;
	}
	ms_info("%-20s: %s\n", g_param_spec_get_name(param), g_param_spec_get_blurb(param));

	return obj;
}

gboolean __ms_element_set_property(GstElement *element, const gchar *key, const gchar *param_value)
{
	gchar *element_name = gst_element_get_name(element);
	GValue value = G_VALUE_INIT;
	GObject *obj = __ms_get_property_owner(element, key, &value);

	if (obj == NULL) {
		ms_debug("Element [%s] does not have property [%s].", element_name, key);
		MS_SAFE_GFREE(element_name);
		return FALSE;
	}

	switch (G_VALUE_TYPE(&value)) {
		case G_TYPE_STRING:
			g_value_set_string(&value, param_value);
			ms_info("Set string value: [%s]", g_value_get_string(&value));
			break;

		case G_TYPE_BOOLEAN: {
				gboolean bool_val = !g_strcmp0(param_value, "true") ? TRUE : FALSE;
				g_value_set_boolean(&value, bool_val);
				ms_info("Set boolean value: [%d]", g_value_get_boolean(&value));
				break;
			}

		case G_TYPE_ULONG: {
				unsigned long pulong = atol(param_value);
				g_value_set_ulong(&value, pulong);
				ms_info("Set ulong value: [%lu]", g_value_get_ulong(&value));
				break;
			}

		case G_TYPE_LONG: {
				long plong = atol(param_value);
				g_value_set_long(&value, plong);
				ms_info("Set long value: [%ld]", g_value_get_long(&value));
				break;
			}

		case G_TYPE_UINT: {
				unsigned int puint = atoi(param_value);
				g_value_set_uint(&value, puint);
				ms_info("Set uint value: [%u]", g_value_get_uint(&value));
				break;
			}

		case G_TYPE_INT: {
				int pint = atoi(param_value);
				g_value_set_int(&value, pint);
				ms_info("Set int value: [%d]", g_value_get_int(&value));
				break;
			}

		case G_TYPE_UINT64: {
				unsigned long long puint64 = strtoull(param_value, NULL, 10);
				g_value_set_uint64(&value, puint64);
				ms_info("Set long value: [%llu]", g_value_get_uint64(&value));
				break;
			}

		case G_TYPE_INT64: {
				long long pint64 = strtoll(param_value, NULL, 10);
				g_value_set_int64(&value, pint64);
				ms_info("Set long value: [%ll]", g_value_get_int64(&value));
				break;
			}
		case G_TYPE_FLOAT: {
				float pfloat = strtof(param_value, NULL);
				g_value_set_float(&value, pfloat);
				ms_info("Set long value: [%15.7g]", g_value_get_float(&value));
				break;
			}
		case G_TYPE_DOUBLE: {
				double pdouble = strtod(param_value, NULL);
				g_value_set_double(&value, pdouble);
				ms_info("Set long value: [%15.7g]", g_value_get_float(&value));
				break;
			}
		default:
			if (G_VALUE_TYPE(&value) == GST_TYPE_CAPS) {
				GstCaps *caps = gst_caps_from_string(param_value);

				if (!caps) {
					ms_error("Can not create caps from param value.");
					return FALSE;
				} else {
					ms_info("Create Caps from params and set to the object.");
					g_object_set(obj, key, caps, NULL);
					gst_caps_unref(caps);
				}
				return TRUE;
			} else {
				ms_info("Got unknown type with param->value_type [%d]", G_VALUE_TYPE(&value));
				return FALSE;
			}
			break;
	}
	g_object_set_property(obj, key, &value);
	MS_SAFE_GFREE(element_name);
}

gboolean __ms_element_unlink(GstElement *element)
{
	gboolean ret = TRUE;
	GstPad *pad = NULL;
	gchar *pad_name = NULL;

	gchar *peer_pad_name = NULL;
	GstPad *peer_pad = NULL;
	GstElement *parent = NULL;

	GValue elem = G_VALUE_INIT;
	GstIterator *pad_iterator = gst_element_iterate_pads(element);

	while (GST_ITERATOR_OK == gst_iterator_next(pad_iterator, &elem)) {
		pad = (GstPad *)g_value_get_object(&elem);
		pad_name = gst_pad_get_name(pad);

		if (gst_pad_is_linked(pad)) {
			peer_pad = gst_pad_get_peer(pad);
			peer_pad_name = gst_pad_get_name(peer_pad);

			gboolean gst_ret = GST_PAD_IS_SRC(pad) ?
			                   gst_pad_unlink(pad, peer_pad) :
			                   gst_pad_unlink(peer_pad, pad);

			if (!gst_ret) {
				ms_error("Filed to unlink pad [%s] from peer pad \n", pad_name);
				ret = ret && FALSE;
			}

			MS_SAFE_GFREE(peer_pad_name);
			MS_SAFE_UNREF(peer_pad);
		}

		MS_SAFE_GFREE(pad_name);
		g_value_reset(&elem);
	}
	g_value_unset(&elem);
	gst_iterator_free(pad_iterator);

	/*Remove node's element from bin and reference saving*/
	parent = gst_element_get_parent(element);

	if (parent != NULL) {
		gst_object_ref(element);
		ret = ret && gst_bin_remove(GST_BIN(parent), element);
	}

	return ret;
}

#if 0
static void __ms_link_elements_on_pad_added_cb(GstPad *new_pad, GstElement *sink_element)
{
	GstPadLinkReturn ret;
	GstPad *sink_pad;
	gchar *new_pad_name = NULL;
	gchar *sink_element_name = NULL;
	gchar *peer_pad_name = NULL;
	GstPad *peer_pad = NULL;

	sink_pad = gst_element_get_static_pad(sink_element, "sink");
	ms_retm_if(sink_pad == NULL, "Sinkpad is NULL");

	sink_element_name = gst_element_get_name(sink_element);
	new_pad_name = gst_pad_get_name(new_pad);

	if (!gst_pad_is_linked(sink_pad)) {
		ms_info("Pads [rtpbin].[%s] and [%s].[sink] are not linked\n", new_pad_name, sink_element_name);

		ret = gst_pad_link(new_pad, sink_pad);
		if (GST_PAD_LINK_FAILED(ret)) {
			ms_error("Failed to link [rtpbin].[%s] and [%s].[sink]\n", new_pad_name, sink_element_name);
		} else {
			ms_info("Succeeded to link [rtpbin].[%s]->[%s].[sink]\n", new_pad_name, sink_element_name);
		}
	} else {
		peer_pad = gst_pad_get_peer(sink_pad);
		peer_pad_name = gst_pad_get_name(peer_pad);

		ms_debug("Pads [rtpbin].[%s]->[%s].[sink] are previously linked\n", peer_pad_name, sink_element_name);

		ret = gst_pad_unlink(peer_pad, sink_pad);
		if (!ret) {
			ms_error("Filed to unlink pads [rtpbin].[%s] <-and-> [%s].[sink] \n", peer_pad_name, sink_element_name);
		} else {
			ms_info("Pads [rtpbin].[%s] <-and-> [%s].[sink] are unlinked successfully\n", peer_pad_name, sink_element_name);
		}

		ret = gst_pad_link(new_pad, sink_pad);
		if (GST_PAD_LINK_FAILED(ret)) {
			ms_error("Failed to link [rtpbin].[%s] and [%s].[sink]\n", new_pad_name, sink_element_name);
		} else {
			ms_info("Succeeded to link [rtpbin].[%s]->[%s].[sink]\n", new_pad_name, sink_element_name);
		}
	}

	MS_SAFE_GFREE(sink_element_name);
	MS_SAFE_GFREE(new_pad_name);
	MS_SAFE_GFREE(peer_pad_name);
}

static void __ms_got_rtpstream_on_pad_added_cb(media_streamer_node_s *ms_node, GstPad *new_pad, const gchar *compared_type)
{
	GstPad *src_pad;
	GstCaps *src_pad_caps = NULL;
	GstStructure *src_pad_struct = NULL;

	gchar *sink_element_name = NULL;
	GstElement *sink_element;

	GValue elem = G_VALUE_INIT;
	const gchar *depay_klass_name = "Codec/Depayloader/Network/RTP";
	GstIterator *bin_iterator;

	gchar *new_pad_name = gst_pad_get_name(new_pad);
	gchar *source_pad_name = g_strdup_printf("%s_source", compared_type);

	bin_iterator = gst_bin_iterate_elements(GST_BIN(ms_node->parent_streamer->topology_bin));
	while (GST_ITERATOR_OK == gst_iterator_next(bin_iterator, &elem)) {

		sink_element = (GstElement *)g_value_get_object(&elem);
		sink_element_name = gst_element_get_name(sink_element);

		const gchar *klass_name = gst_element_factory_get_klass(gst_element_get_factory(sink_element));

		if (g_strrstr(klass_name, depay_klass_name)) {
			src_pad = gst_element_get_static_pad(sink_element, "src");
			ms_retm_if(src_pad == NULL, "Src pad is NULL");

			src_pad_caps = gst_pad_query_caps(src_pad, NULL);
			src_pad_struct = gst_caps_get_structure(src_pad_caps, 0);
			const gchar *src_pad_type = gst_structure_get_name(src_pad_struct);

			if (g_strrstr(src_pad_type, compared_type)) {
				ms_debug("Element to connect [%s] has type [%s] \n", sink_element_name, src_pad_type);
				GstPad *video_source_pad = gst_element_get_static_pad(ms_node->gst_element, source_pad_name);

				gst_ghost_pad_set_target(GST_GHOST_PAD(video_source_pad), new_pad);
				gst_pad_set_active(video_source_pad, TRUE);
				__ms_generate_dots(ms_node->parent_streamer->pipeline);
			}

			gst_caps_unref(src_pad_caps);

			MS_SAFE_UNREF(src_pad);
		}
		g_value_reset(&elem);
	}
	g_value_unset(&elem);
	gst_iterator_free(bin_iterator);
	MS_SAFE_GFREE(sink_element_name);
	MS_SAFE_GFREE(source_pad_name);
	MS_SAFE_FREE(new_pad_name);
}
#endif

static void __ms_rtpbin_pad_added_cb(GstElement *src, GstPad *new_pad, gpointer user_data)
{
	GstCaps *src_pad_caps = NULL;
	GstPad *target_pad = NULL;

	gchar *new_pad_name = NULL;
	gchar *src_element_name = NULL;

	media_streamer_node_s *ms_node = (media_streamer_node_s *)user_data;
	ms_retm_if(ms_node == NULL, "Handle is NULL");

	new_pad_name = gst_pad_get_name(new_pad);
	src_element_name = gst_element_get_name(src);
	ms_debug("Pad [%s] added on [%s]\n", new_pad_name, src_element_name);

	target_pad = gst_ghost_pad_get_target(GST_GHOST_PAD(new_pad));
	src_pad_caps = gst_pad_query_caps(target_pad, NULL);

	if (ms_node->parent_streamer && !gst_caps_is_any(src_pad_caps)) {
		gchar *source_pad_name = NULL;
		GstElement *sink_bin = NULL;
		GstStructure *src_pad_struct = NULL;
		src_pad_struct = gst_caps_get_structure(src_pad_caps, 0);

		const gchar *src_pad_type = gst_structure_get_string(src_pad_struct, "media");
		ms_debug("type is [%s]", src_pad_type);
		if (MS_ELEMENT_IS_VIDEO(src_pad_type)) {
			source_pad_name = g_strdup_printf("%s_source", "video");
			sink_bin = ms_node->parent_streamer->sink_video_bin;
		} else if (MS_ELEMENT_IS_AUDIO(src_pad_type)) {
			source_pad_name = g_strdup_printf("%s_source", "audio");
			sink_bin = ms_node->parent_streamer->sink_audio_bin;
		}

		if (source_pad_name != NULL) {
			if (gst_object_get_parent(GST_OBJECT(sink_bin)) == NULL) {
				gst_bin_add(GST_BIN(ms_node->parent_streamer->pipeline), sink_bin);
			}
			gst_element_sync_state_with_parent(sink_bin);


			GstPad *source_pad = gst_element_get_static_pad(ms_node->gst_element, source_pad_name);
			gst_ghost_pad_set_target(GST_GHOST_PAD(source_pad), new_pad);
			gst_pad_set_active(source_pad, TRUE);

			GstPad *sink_pad = gst_bin_find_unlinked_pad(GST_BIN(sink_bin), GST_PAD_SINK);
			if (sink_pad != NULL) {
				__ms_add_ghostpad(gst_pad_get_parent(sink_pad), "sink", sink_bin, "sink");
				if (gst_element_link_pads(ms_node->gst_element, source_pad_name, sink_bin, "sink")) {
					__ms_element_set_state(ms_node->gst_element, GST_STATE_PLAYING);
					__ms_generate_dots(ms_node->parent_streamer->pipeline, "playing");
				} else {
					ms_error("Failed to link [rtp_containeer].[%s] and [sink_bin].[sink]\n", source_pad_name);
				}
				MS_SAFE_UNREF(sink_pad);
			}
			MS_SAFE_UNREF(source_pad);
			MS_SAFE_GFREE(source_pad_name);
		}
	} else {
		ms_debug("Node doesn`t have parent streamer or caps media type\n");
	}

	gst_caps_unref(src_pad_caps);
	MS_SAFE_UNREF(target_pad);
	MS_SAFE_GFREE(new_pad_name);
	MS_SAFE_GFREE(src_element_name);
}

int __ms_element_set_state(GstElement *gst_element, GstState gst_state)
{
	ms_retvm_if(gst_element == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	GstStateChangeReturn ret_state;
	gchar *element_name = gst_element_get_name(gst_element);

	ret_state = gst_element_set_state(gst_element, gst_state);
	if (ret_state == GST_STATE_CHANGE_FAILURE) {
		ms_error("Failed to set element [%s] into %s state", element_name, _ms_state_to_string(gst_state));
		MS_SAFE_GFREE(element_name);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	MS_SAFE_GFREE(element_name);
	return MEDIA_STREAMER_ERROR_NONE;
}

GstElement *__ms_element_create(const char *plugin_name, const char *name)
{
	GstElement *plugin_elem = NULL;
	ms_retvm_if(plugin_name == NULL, (GstElement *)NULL, "Error empty plugin name");
	ms_info("Creating [%s] element", plugin_name);
	plugin_elem = gst_element_factory_make(plugin_name, name);
	ms_retvm_if(plugin_elem == NULL, (GstElement *)NULL, "Error creating element [%s]", plugin_name);
	return plugin_elem;
}

GstElement *__ms_camera_element_create(const char *camera_plugin_name)
{
	ms_retvm_if(camera_plugin_name == NULL, (GstElement *)NULL, "Error empty camera plugin name");

	gboolean gst_ret = FALSE;
	GstElement *camera_bin = gst_bin_new("camera_src");
	GstElement *camera_elem = __ms_element_create(camera_plugin_name, NULL);
	GstElement *filter = __ms_element_create("capsfilter", NULL);
	GstElement *scale = __ms_element_create("videoscale", NULL);
	GstElement *videoconvert = __ms_element_create("videoconvert", NULL);
	ms_retvm_if(!filter || !camera_elem || !camera_bin || !scale || !videoconvert , (GstElement *)NULL,
	            "Error: creating elements for camera bin");

	GstCaps *videoCaps = gst_caps_from_string(MEDIA_STREAMER_DEFAULT_CAMERA_FORMAT);
	g_object_set(G_OBJECT(filter), "caps", videoCaps, NULL);
	gst_caps_unref(videoCaps);

	gst_bin_add_many(GST_BIN(camera_bin), camera_elem, filter, scale, videoconvert, NULL);
	gst_ret = gst_element_link_many(camera_elem, filter, scale, videoconvert, NULL);
	if (gst_ret != TRUE) {
		ms_error("Failed to link elements into camerabin");
		MS_SAFE_UNREF(camera_bin);
	}

	__ms_add_ghostpad(videoconvert, "src", camera_bin, "src");

	return camera_bin;
}

GstElement *__ms_video_encoder_element_create(dictionary *dict , media_format_mimetype_e mime)
{
	char *plugin_name = NULL;
	char *format_prefix = NULL;

	format_prefix = g_strdup_printf("%s:encoder", __ms_convert_mime_to_string(mime));
	plugin_name = __ms_ini_get_string(dict, format_prefix, DEFAULT_VIDEO_ENCODER);
	GstElement *encoder_elem = __ms_element_create(plugin_name, NULL);
	MS_SAFE_FREE(format_prefix);
	MS_SAFE_FREE(plugin_name);

	format_prefix = g_strdup_printf("%s:parser", __ms_convert_mime_to_string(mime));
	plugin_name = __ms_ini_get_string(dict, format_prefix, DEFAULT_VIDEO_PARSER);
	GstElement *encoder_parser = __ms_element_create(plugin_name, NULL);
	MS_SAFE_FREE(format_prefix);
	MS_SAFE_FREE(plugin_name);

	gboolean gst_ret = FALSE;
	GstElement *encoder_bin = gst_bin_new("video_encoder");
	GstElement *filter = __ms_element_create("capsfilter", NULL);
	ms_retvm_if(!filter || !encoder_elem || !encoder_bin || !encoder_parser, (GstElement *)NULL,
	            "Error: creating elements for video encoder bin");

	format_prefix = g_strdup_printf("video/x-%s,stream-format=byte-stream,profile=main",
	                                __ms_convert_mime_to_string(mime));
	GstCaps *videoCaps = gst_caps_from_string(format_prefix);
	g_object_set(G_OBJECT(filter), "caps", videoCaps, NULL);
	MS_SAFE_FREE(format_prefix);

	gst_caps_unref(videoCaps);

	gst_bin_add_many(GST_BIN(encoder_bin), encoder_elem, filter, encoder_parser, NULL);
	gst_ret = gst_element_link_many(encoder_elem, filter, encoder_parser, NULL);
	if (gst_ret != TRUE) {
		ms_error("Failed to link elements into encoder_bin");
		MS_SAFE_UNREF(encoder_bin);
	}

	__ms_add_ghostpad(encoder_parser, "src", encoder_bin, "src");
	__ms_add_ghostpad(encoder_elem, "sink", encoder_bin, "sink");

	return encoder_bin;
}

GstElement *__ms_video_decoder_element_create(dictionary *dict , media_format_mimetype_e mime)
{
	char *plugin_name = NULL;
	char *format_prefix = NULL;
	gboolean is_omx = FALSE;
	GstElement *last_elem = NULL;

	format_prefix = g_strdup_printf("%s:decoder", __ms_convert_mime_to_string(mime));
	plugin_name = __ms_ini_get_string(dict, format_prefix, DEFAULT_VIDEO_DECODER);
	GstElement *decoder_elem = __ms_element_create(plugin_name, NULL);
	MS_SAFE_FREE(format_prefix);
	MS_SAFE_FREE(plugin_name);

	format_prefix = g_strdup_printf("%s:parser", __ms_convert_mime_to_string(mime));
	plugin_name = __ms_ini_get_string(dict, format_prefix, DEFAULT_VIDEO_PARSER);
	GstElement *decoder_parser = __ms_element_create(plugin_name, NULL);

	if (mime == MEDIA_FORMAT_H264_SP) {
		g_object_set(G_OBJECT(decoder_parser), "config-interval", 5, NULL);
	}

	is_omx = g_strrstr(format_prefix, "omx");
	MS_SAFE_FREE(format_prefix);
	MS_SAFE_FREE(plugin_name);

	gboolean gst_ret = FALSE;
	GstElement *decoder_bin = gst_bin_new("video_decoder");
	GstElement *decoder_queue = __ms_element_create("queue", NULL);
	ms_retvm_if(!decoder_elem || !decoder_queue || !decoder_bin || !decoder_parser, (GstElement *)NULL,
	            "Error: creating elements for video decoder bin");

	gst_bin_add_many(GST_BIN(decoder_bin), decoder_queue, decoder_elem, decoder_parser, NULL);
	gst_ret = gst_element_link_many(decoder_queue, decoder_parser, decoder_elem, NULL);
	if (gst_ret != TRUE) {
		ms_error("Failed to link elements into decoder_bin");
		MS_SAFE_UNREF(decoder_bin);
		return NULL;
	}
	last_elem = decoder_elem;

	if (!is_omx) {
		GstElement *video_conv = __ms_element_create("videoconvert", NULL);
		GstElement *video_scale = __ms_element_create("videoscale", NULL);
		ms_retvm_if(!video_conv || !video_scale, (GstElement *)NULL,
		            "Error: creating elements for video decoder bin");
		gst_bin_add_many(GST_BIN(decoder_bin), video_conv, video_scale, NULL);
		gst_ret = gst_element_link_many(decoder_elem, video_conv, video_scale, NULL);
		if (gst_ret != TRUE) {
			ms_error("Failed to link elements into decoder_bin");
			MS_SAFE_UNREF(decoder_bin);
			return NULL;
		}
		last_elem = video_scale;
	}

	__ms_add_ghostpad(last_elem, "src", decoder_bin, "src");
	__ms_add_ghostpad(decoder_queue, "sink", decoder_bin, "sink");

	return decoder_bin;
}

GstElement *__ms_audio_encoder_element_create(void)
{
	gboolean gst_ret = FALSE;
	GstElement *audio_convert = __ms_element_create("audioconvert", NULL);
	GstElement *audio_filter = __ms_element_create("capsfilter", NULL);
	GstElement *audio_enc_bin = gst_bin_new("audio_encoder");
	ms_retvm_if(!audio_convert || !audio_filter || !audio_enc_bin, (GstElement *)NULL,
	            "Error: creating elements for encoder bin");

	GstCaps *audioCaps = gst_caps_from_string(MEDIA_STREAMER_DEFAULT_AUDIO_FORMAT);
	g_object_set(G_OBJECT(audio_filter), "caps", audioCaps, NULL);
	gst_caps_unref(audioCaps);

	gst_bin_add_many(GST_BIN(audio_enc_bin), audio_convert, audio_filter, NULL);
	gst_ret = gst_element_link_many(audio_filter, audio_convert, NULL);
	if (gst_ret != TRUE) {
		ms_error("Failed to link elements into decoder_bin");
		MS_SAFE_UNREF(audio_enc_bin);
	}

	__ms_add_ghostpad(audio_convert, "src", audio_enc_bin, "src");
	__ms_add_ghostpad(audio_filter, "sink", audio_enc_bin, "sink");

	return audio_enc_bin;
}

GstElement *__ms_rtp_element_create(media_streamer_node_s *ms_node)
{
	ms_retvm_if(ms_node == NULL, (GstElement *)NULL, "Error empty rtp node Handle");

	GstElement *rtp_container = gst_bin_new("rtp_container");
	GstElement *rtp_elem = __ms_element_create("rtpbin", "rtpbin");
	ms_retvm_if(!rtp_container || !rtp_elem, (GstElement *)NULL,
	            "Error: creating elements for rtp container");

	gst_bin_add(GST_BIN(rtp_container), rtp_elem);
	g_signal_connect(rtp_elem, "pad-added", G_CALLBACK(__ms_rtpbin_pad_added_cb), ms_node);

	return rtp_container;
}

gboolean __ms_get_rtp_elements(media_streamer_node_s *ms_node,
                               GstElement **rtp_elem, GstElement **rtcp_elem, const gchar *elem_name)
{
	gboolean ret = FALSE;
	gchar *rtp_elem_name = NULL;
	gchar *rtcp_elem_name = NULL;
	gchar *plugin_name = NULL;

	GstElement *rtpbin = gst_bin_get_by_name(GST_BIN(ms_node->gst_element), "rtpbin");

	ms_retvm_if(!elem_name, FALSE, "Empty rtp element name.");

	if (MS_ELEMENT_IS_SOURCE(elem_name)) {
		plugin_name = g_strdup("udpsrc");
	} else if (MS_ELEMENT_IS_SINK(elem_name)) {
		plugin_name = g_strdup("udpsink");
	} else {
		ms_error("Error: invalid parameter name [%s]", elem_name);
		return FALSE;
	}

	rtp_elem_name = g_strdup_printf("%s_rtp", elem_name);
	rtcp_elem_name = g_strdup_printf("%s_rtcp", elem_name);

	/* Find video udp rtp/rtcp element if it present. */
	*rtp_elem = gst_bin_get_by_name(GST_BIN(ms_node->gst_element), rtp_elem_name);
	*rtcp_elem = gst_bin_get_by_name(GST_BIN(ms_node->gst_element), rtcp_elem_name);

	/* Create new udp element if it did not found. */
	if ((NULL == *rtp_elem) && (NULL == *rtcp_elem)) {
		*rtp_elem = __ms_element_create(plugin_name, rtp_elem_name);
		*rtcp_elem = __ms_element_create(plugin_name, rtcp_elem_name);
	} else {
		/*rtp/rtcp elements already into rtp bin. */
		MS_SAFE_GFREE(rtp_elem_name);
		MS_SAFE_GFREE(rtcp_elem_name);
		MS_SAFE_GFREE(plugin_name);
		return TRUE;
	}

	gst_bin_add_many(GST_BIN(ms_node->gst_element),
	                 *rtp_elem, *rtcp_elem, NULL);

	if (MS_ELEMENT_IS_SINK(elem_name)) {
		g_object_set(GST_OBJECT(*rtcp_elem), "sync", FALSE, NULL);
		g_object_set(GST_OBJECT(*rtcp_elem), "async", FALSE, NULL);

		if (MS_ELEMENT_IS_VIDEO(elem_name)) {
			__ms_add_ghostpad(rtpbin, "send_rtp_sink_0", ms_node->gst_element, "video_sink");
			ret = gst_element_link_pads(rtpbin, "send_rtp_src_0", *rtp_elem, "sink") &&
			      gst_element_link_pads(rtpbin, "send_rtcp_src_0", *rtcp_elem, "sink");
		} else {
			__ms_add_ghostpad(rtpbin, "send_rtp_sink_1", ms_node->gst_element, "audio_sink");
			ret = gst_element_link_pads(rtpbin, "send_rtp_src_1", *rtp_elem, "sink") &&
			      gst_element_link_pads(rtpbin, "send_rtcp_src_1", *rtcp_elem, "sink");
		}
	} else {
		if (MS_ELEMENT_IS_VIDEO(elem_name)) {
			ret = gst_element_link_pads(*rtp_elem, "src", rtpbin, "recv_rtp_sink_0") &&
			      gst_element_link_pads(*rtcp_elem, "src", rtpbin, "recv_rtcp_sink_0");
			__ms_add_ghostpad(NULL, NULL, ms_node->gst_element, "video_source");
		} else {
			ret = gst_element_link_pads(*rtp_elem, "src", rtpbin, "recv_rtp_sink_1") &&
			      gst_element_link_pads(*rtcp_elem, "src", rtpbin, "recv_rtcp_sink_1");
			__ms_add_ghostpad(NULL, NULL, ms_node->gst_element, "audio_source");
		}
	}

	if (!ret) {
		ms_error("Can not link [rtpbin] pad to [%s] pad, ret code [%d] ", rtp_elem, ret);
		ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	__ms_generate_dots(ms_node->gst_element, "rtp");
	MS_SAFE_GFREE(rtp_elem_name);
	MS_SAFE_GFREE(rtcp_elem_name);
	MS_SAFE_GFREE(plugin_name);

	return ret;
}

int __ms_add_node_into_bin(media_streamer_s *ms_streamer, media_streamer_node_s *ms_node)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Handle is NULL");
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Handle is NULL");

	ms_info("Try to add [%s] node into streamer, node type/subtype [%d/%d]",
	        ms_node->name, ms_node->type, ms_node->subtype);

	gchar *bin_name = NULL;
	gboolean gst_ret = FALSE;

	switch (ms_node->type) {
		case MEDIA_STREAMER_NODE_TYPE_SRC:
			gst_ret = gst_bin_add(GST_BIN(ms_streamer->src_bin), ms_node->gst_element);
			bin_name = g_strdup(MEDIA_STREAMER_SRC_BIN_NAME);
			break;
		case MEDIA_STREAMER_NODE_TYPE_SINK:
			switch (ms_node->subtype) {
				case MEDIA_STREAMER_NODE_SINK_TYPE_SCREEN:
					gst_ret = gst_bin_add(GST_BIN(ms_streamer->sink_video_bin), ms_node->gst_element);
					bin_name = g_strdup(MEDIA_STREAMER_VIDEO_SINK_BIN_NAME);
					break;
				case MEDIA_STREAMER_NODE_SINK_TYPE_AUDIO:
					gst_ret = gst_bin_add(GST_BIN(ms_streamer->sink_audio_bin), ms_node->gst_element);
					bin_name = g_strdup(MEDIA_STREAMER_AUDIO_SINK_BIN_NAME);
					break;
				default:
					gst_ret = gst_bin_add(GST_BIN(ms_streamer->topology_bin), ms_node->gst_element);
					bin_name = g_strdup(MEDIA_STREAMER_TOPOLOGY_BIN_NAME);
					break;
			}
			break;
		case MEDIA_STREAMER_NODE_TYPE_AUDIO_ENCODER:
			gst_ret = gst_bin_add(GST_BIN(ms_streamer->topology_bin), ms_node->gst_element);
			bin_name = g_strdup(MEDIA_STREAMER_TOPOLOGY_BIN_NAME);
			break;
		case MEDIA_STREAMER_NODE_TYPE_VIDEO_DECODER:
			gst_ret = gst_bin_add(GST_BIN(ms_streamer->sink_video_bin), ms_node->gst_element);
			bin_name = g_strdup(MEDIA_STREAMER_VIDEO_SINK_BIN_NAME);
			break;
		case MEDIA_STREAMER_NODE_TYPE_VIDEO_DEPAY:
			gst_ret = gst_bin_add(GST_BIN(ms_streamer->sink_video_bin), ms_node->gst_element);
			bin_name = g_strdup(MEDIA_STREAMER_VIDEO_SINK_BIN_NAME);
			break;
		case MEDIA_STREAMER_NODE_TYPE_AUDIO_DEPAY:
			gst_ret = gst_bin_add(GST_BIN(ms_streamer->sink_audio_bin), ms_node->gst_element);
			bin_name = g_strdup(MEDIA_STREAMER_AUDIO_SINK_BIN_NAME);
			break;
		case MEDIA_STREAMER_NODE_TYPE_AUDIO_RESAMPLE:
			gst_ret = gst_bin_add(GST_BIN(ms_streamer->sink_audio_bin), ms_node->gst_element);
			bin_name = g_strdup(MEDIA_STREAMER_AUDIO_SINK_BIN_NAME);
			break;
		case MEDIA_STREAMER_NODE_TYPE_AUDIO_CONVERTER:
			gst_ret = gst_bin_add(GST_BIN(ms_streamer->sink_audio_bin), ms_node->gst_element);
			bin_name = g_strdup(MEDIA_STREAMER_AUDIO_SINK_BIN_NAME);
			break;
		default:
			/* Another elements will be add into topology bin */
			gst_ret = gst_bin_add(GST_BIN(ms_streamer->topology_bin), ms_node->gst_element);
			bin_name = g_strdup(MEDIA_STREAMER_TOPOLOGY_BIN_NAME);
			break;
	}

	if (!gst_ret) {
		ms_error("Failed to add Element [%s] into [%s] bin.", ms_node->name, bin_name);
		ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	} else {
		ms_info("Success added Element [%s] into [%s] bin.", ms_node->name, bin_name);
		ret = MEDIA_STREAMER_ERROR_NONE;
	}

	MS_SAFE_GFREE(bin_name);

	return ret;
}


static gboolean __ms_bus_cb(GstBus *bus, GstMessage *message, gpointer userdata)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;
	media_streamer_s *ms_streamer = (media_streamer_s *)userdata;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(ms_streamer->pipeline == NULL, MEDIA_STREAMER_ERROR_INVALID_STATE, "Pipeline doesn`t exist");

	/* Parse message */
	if (message != NULL) {
		switch (GST_MESSAGE_TYPE(message)) {
			case GST_MESSAGE_ERROR: {
					GError *err;
					gchar *debug;
					gst_message_parse_error(message, &err, &debug);

					ms_error("[Source: %s] Error: %s", GST_OBJECT_NAME(GST_OBJECT_CAST(GST_ELEMENT(GST_MESSAGE_SRC(message)))), err->message);

					MS_SAFE_FREE(err);
					MS_SAFE_FREE(debug);
					break;
				}

			case GST_MESSAGE_STATE_CHANGED: {
					if (GST_MESSAGE_SRC(message) == GST_OBJECT(ms_streamer->pipeline)) {
						GstState state_old, state_new, state_pending;
						gchar *state_transition_name;

						gst_message_parse_state_changed(message, &state_old, &state_new, &state_pending);
						ms_info("GST_MESSAGE_STATE_CHANGED: [%s] %s -> %s\n",
						        gst_object_get_name(GST_MESSAGE_SRC(message)),
						        _ms_state_to_string(state_old),
						        _ms_state_to_string(state_new));

						state_transition_name = g_strdup_printf("%s_%s",
						                                        gst_element_state_get_name(state_old),
						                                        gst_element_state_get_name(state_new));

						__ms_generate_dots(ms_streamer->pipeline, state_transition_name);

						MS_SAFE_GFREE(state_transition_name);

						if (state_old == GST_STATE_NULL && state_new == GST_STATE_READY) {
							ms_info("[Success] GST_STATE_NULL => GST_STATE_READY");

							/* Pause Media_Streamer */
							ret = __ms_element_set_state(ms_streamer->pipeline, GST_STATE_PAUSED);
							if (ret != MEDIA_STREAMER_ERROR_NONE) {
								ms_error("ERROR - Pause pipeline");
								return FALSE;
							}
						}

						if (state_old == GST_STATE_READY && state_new == GST_STATE_PAUSED) {
							ms_info("[Success] GST_STATE_READY => GST_STATE_PAUSED");

							ret = __ms_element_set_state(ms_streamer->pipeline, GST_STATE_PLAYING);
							if (ret != MEDIA_STREAMER_ERROR_NONE) {
								ms_error("ERROR - Play Pipeline");
								return FALSE;
							}
						}
					}
					break;
				}

			case GST_MESSAGE_EOS: {
					ms_info("GST_MESSAGE_EOS end-of-stream");
					ret = __ms_element_set_state(ms_streamer->pipeline, GST_STATE_PAUSED);
					if (ret != MEDIA_STREAMER_ERROR_NONE) {
						ms_error("ERROR - Pause Pipeline");
						return FALSE;
					}
					break;
				}
			default:
				break;
		}
	}
	return TRUE;
}

int __ms_pipeline_create(media_streamer_s *ms_streamer)
{
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	gst_init(NULL, NULL);

	ms_streamer->pipeline = gst_pipeline_new(MEDIA_STREAMER_PIPELINE_NAME);
	ms_retvm_if(ms_streamer->pipeline == NULL,
	            MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error creating pipeline");

	ms_streamer->bus = gst_pipeline_get_bus(GST_PIPELINE(ms_streamer->pipeline));
	ms_retvm_if(ms_streamer->bus == NULL,
	            MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error getting the bus of the pipeline");

	ms_streamer->bus_watcher = gst_bus_add_watch(ms_streamer->bus, (GstBusFunc)__ms_bus_cb, ms_streamer);

	ms_streamer->src_bin = gst_bin_new(MEDIA_STREAMER_SRC_BIN_NAME);
	ms_retvm_if(ms_streamer->src_bin == NULL,
	            MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error creating Src bin");

	/*	g_signal_connect(ms_streamer->src_bin, "element-added", G_CALLBACK(__src_bin_element_added_cb), ms_streamer); */

	ms_streamer->sink_video_bin = gst_bin_new(MEDIA_STREAMER_VIDEO_SINK_BIN_NAME);
	ms_retvm_if(ms_streamer->sink_video_bin == NULL,
	            MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error creating Sink bin");
	/*	g_signal_connect(ms_streamer->sink_bin, "element-added", G_CALLBACK(__sink_bin_element_added_cb), ms_streamer); */

	ms_streamer->sink_audio_bin = gst_bin_new(MEDIA_STREAMER_AUDIO_SINK_BIN_NAME);
	ms_retvm_if(ms_streamer->sink_audio_bin == NULL,
	            MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error creating Audio Sink bin");

	ms_streamer->topology_bin = gst_bin_new(MEDIA_STREAMER_TOPOLOGY_BIN_NAME);
	ms_retvm_if(ms_streamer->topology_bin == NULL,
	            MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error creating Topology bin");
	/*	g_signal_connect(ms_streamer->topology_bin, "element-added", G_CALLBACK(__bin_element_added_cb), ms_streamer); */

	gst_bin_add_many(GST_BIN(ms_streamer->pipeline), ms_streamer->src_bin,
	                 ms_streamer->topology_bin, NULL);

	return MEDIA_STREAMER_ERROR_NONE;
}

static GstCaps *__ms_create_caps_from_fmt(media_format_h fmt)
{
	GstCaps *caps = NULL;
	media_format_mimetype_e mime;
	gchar *format_name = NULL;
	int width;
	int height;
	int avg_bps;
	int max_bps;
	int channel;
	int samplerate;
	int bit;

	if (media_format_get_video_info(fmt, &mime, &width, &height, &avg_bps, &max_bps) == MEDIA_PACKET_ERROR_NONE) {

		ms_info("Creating video Caps from media format [width=%d, height=%d, bps=%d, mime=%d]",
		        width, height, avg_bps, mime);

		if (mime & MEDIA_FORMAT_RAW) {
			format_name = g_strdup(__ms_convert_mime_to_string(mime));
			caps = gst_caps_new_simple("video/x-raw",
			                           /*"framerate", GST_TYPE_FRACTION, max_bps, avg_bps, */
			                           "format", G_TYPE_STRING, format_name,
			                           "width", G_TYPE_INT, width,
			                           "height", G_TYPE_INT, height, NULL);
		} else {
			/*mime & MEDIA_FORMAT_ENCODED */
			format_name = g_strdup_printf("video/x-%s", __ms_convert_mime_to_string(mime));
			caps = gst_caps_new_simple(format_name,
			                           "framerate", GST_TYPE_FRACTION, max_bps, avg_bps,
			                           "width", G_TYPE_INT, width,
			                           "height", G_TYPE_INT, height, NULL);
		}

	} else if (media_format_get_audio_info(fmt, &mime, &channel, &samplerate, &bit, &avg_bps) == MEDIA_PACKET_ERROR_NONE) {
		ms_info("Creating audio Caps from media format [channel=%d, samplerate=%d, bit=%d, avg_bps=%d, mime=%d]",
		        channel, samplerate, bit, avg_bps, mime);

		if (mime & MEDIA_FORMAT_RAW) {
			format_name = g_strdup(__ms_convert_mime_to_string(mime));
			caps = gst_caps_new_simple("audio/x-raw",
			                           "channels", G_TYPE_INT, channel,
			                           "format", G_TYPE_STRING, format_name,
			                           "rate", G_TYPE_INT, samplerate, NULL);
		} else {
			ms_error("Encoded audio formats does not supported yet.");
		}
	} else {
		ms_error("Failed getting media info from fmt.");
	}
	MS_SAFE_GFREE(format_name);

	return caps;
}

static media_format_h __ms_create_fmt_from_caps(GstCaps *caps)
{
	media_format_h fmt;
	GstStructure *pad_struct;
	ms_retvm_if(caps == NULL, NULL, "Error: empty caps!");

	if (gst_caps_is_any(caps)) {
		ms_debug("Can not get format info from any caps!");
		return NULL;
	}

	int fmt_ret = MEDIA_FORMAT_ERROR_NONE;

	fmt_ret  = media_format_create(&fmt);
	ms_retvm_if(fmt_ret != MEDIA_FORMAT_ERROR_NONE, NULL,
			"Error: while creating media format object, err[%d]!", fmt_ret);

	pad_struct = gst_caps_get_structure(caps, 0);
	const gchar *pad_type = gst_structure_get_name(pad_struct);
	const gchar *pad_format = pad_type;

	/* Got raw format type if needed */
	if (g_strrstr(pad_type, "/x-raw")) {
		pad_format = gst_structure_get_string(pad_struct, "format");
	}

	ms_debug("Pad type is [%s], format: [%s]", pad_type, pad_format);
	if (MS_ELEMENT_IS_VIDEO(pad_type)) {
		int width, height;
		media_format_set_video_mime(fmt, __ms_convert_string_format_to_mime(pad_format));
		gst_structure_get_int(pad_struct, "width", &width);
		media_format_set_video_width(fmt, width);

		gst_structure_get_int(pad_struct, "height", &height);
		media_format_set_video_height(fmt, height);
	} else if (MS_ELEMENT_IS_AUDIO(pad_type)) {
		int channels, bps;
		media_format_set_audio_mime(fmt, __ms_convert_string_format_to_mime(pad_format));
		gst_structure_get_int(pad_struct, "channels", &channels);
		media_format_set_audio_channel(fmt, channels);
		gst_structure_get_int(pad_struct, "rate", &bps);
		media_format_set_audio_avg_bps(fmt, bps);
	}

	return fmt;
}

media_format_h __ms_element_get_pad_fmt(GstElement *gst_element, const char* pad_name)
{
	media_format_h fmt;
	GstCaps *caps = NULL;

	GstPad *pad = gst_element_get_static_pad(gst_element, pad_name);
	gchar *element_name = gst_element_get_name(gst_element);

	if (pad == NULL) {
		ms_error("Fail to get pad [%s] from element [%s].", pad_name, element_name);
		MS_SAFE_FREE(element_name);
		return NULL;
	}

	caps = gst_pad_get_allowed_caps(pad);
	if (caps == NULL) {
		ms_error("Fail to get caps from element [%s] and pad [%s].", element_name, pad_name);
		MS_SAFE_FREE(element_name);
		MS_SAFE_UNREF(pad);
		return NULL;
	}

	fmt = __ms_create_fmt_from_caps(caps);

	MS_SAFE_FREE(element_name);
	MS_SAFE_UNREF(pad);
	return fmt;
}

int __ms_element_set_fmt(media_streamer_node_s *node, media_format_h fmt)
{
	GstCaps *caps = NULL;
	GObject *obj = NULL;
	GValue value = G_VALUE_INIT;
	caps = __ms_create_caps_from_fmt(fmt);
	ms_retvm_if(caps == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Fail creating caps from fmt.");

	obj = __ms_get_property_owner(node->gst_element, "caps", &value);
	gst_value_set_caps(&value, caps);
	g_object_set_property(obj, "caps", &value);
	gst_caps_unref(caps);

	return MEDIA_STREAMER_ERROR_NONE;
}

int __ms_element_push_packet(GstElement *src_element, media_packet_h packet)
{
	GstBuffer *buffer;
	GstFlowReturn gst_ret = GST_FLOW_OK;
	guint64 pts = 0;
	guint64 duration = 0;
	guchar *buffer_data = NULL;
	guint64 size  = 0;

	ms_retvm_if(src_element == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	if (packet == NULL) {
		g_signal_emit_by_name(G_OBJECT(src_element), "end-of-stream", &gst_ret, NULL);
		return MEDIA_STREAMER_ERROR_NONE;
	}

	media_packet_get_buffer_size(packet, &size);
	media_packet_get_buffer_data_ptr(packet, &buffer_data);
	media_packet_get_pts(packet, &pts);
	media_packet_get_duration(packet, &duration);

	buffer = gst_buffer_new_wrapped(buffer_data, size);

	GST_BUFFER_PTS(buffer) = pts;
	GST_BUFFER_DURATION(buffer) = duration;

	g_signal_emit_by_name(G_OBJECT(src_element), "push-buffer", buffer, &gst_ret, NULL);

	gst_buffer_unref(buffer);

	if (gst_ret != GST_FLOW_OK) {
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	return MEDIA_STREAMER_ERROR_NONE;
}

int __ms_element_pull_packet(GstElement *sink_element, media_packet_h *packet)
{
	GstBuffer *buffer = NULL;
	GstFlowReturn gst_ret = GST_FLOW_OK;
	GstSample *sample = NULL;
	GstMapInfo map;

	ms_retvm_if(sink_element == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	/* Retrieve the buffer */
	g_signal_emit_by_name(sink_element, "pull-sample", &sample, NULL);
	ms_retvm_if(sample == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Pull sample failed!");

	buffer = gst_sample_get_buffer(sample);
	gst_buffer_map(buffer, &map, GST_MAP_READ);

	media_format_h fmt = __ms_element_get_pad_fmt(sink_element, "sink");
	ms_retvm_if(fmt == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION,
			"Error while getting media format from sink pad");

	media_packet_create_from_external_memory(&fmt, (void *)map.data, map.size, NULL, NULL, packet);
	media_packet_set_pts(*packet, GST_BUFFER_PTS(buffer));
	media_packet_set_dts(*packet, GST_BUFFER_DTS(buffer));
	media_packet_set_pts(*packet, GST_BUFFER_DURATION(buffer));

	media_format_unref(fmt);
	gst_buffer_unmap(buffer, &map);
	gst_sample_unref(sample);

	return MEDIA_STREAMER_ERROR_NONE;
}
