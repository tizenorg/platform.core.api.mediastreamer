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

static int __ms_add_no_target_ghostpad(GstElement *gst_bin,
                                       const char *ghost_pad_name,
                                       GstPadDirection pad_direction)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;

	gchar *bin_name = gst_element_get_name(gst_bin);
	GstPad *ghost_pad = gst_ghost_pad_new_no_target(ghost_pad_name, pad_direction);
	if (gst_element_add_pad(GST_ELEMENT(gst_bin), ghost_pad)) {
		ms_info("Added [%s] empty ghostpad into [%s]", ghost_pad_name, bin_name);
	} else {
		ms_info("Error: Failed to add empty [%s] ghostpad into [%s]", ghost_pad_name, bin_name);
		ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	MS_SAFE_GFREE(bin_name);
	return ret;
}
static int __ms_add_ghostpad(GstElement *gst_element,
                             const char *pad_name,
                             GstElement *gst_bin,
                             const char *ghost_pad_name)
{
	ms_retvm_if(!gst_element || !pad_name || !ghost_pad_name || !gst_bin, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	int ret = MEDIA_STREAMER_ERROR_NONE;

	GstPad *ghost_pad = NULL;
	gchar *bin_name = gst_element_get_name(gst_bin);

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
		param = g_object_class_find_property(G_OBJECT_GET_CLASS(obj), key);
	}

	g_value_init(value, param->value_type);

	if (!param->flags & G_PARAM_WRITABLE) {

		/* Skip properties which user can not change. */
		ms_error("Error: node param [%s] is not writable!", key);
		return NULL;
	}
	ms_info("%-20s: %s\n", g_param_spec_get_name(param), g_param_spec_get_blurb(param));

	return obj;
}

gboolean __ms_element_set_property(GstElement *element, const char *key, const gchar *param_value)
{
	gchar *element_name = gst_element_get_name(element);
	GValue value = G_VALUE_INIT;

	char *init_name = NULL;
	int pint = 0;
	gboolean bool_val = false;

	__ms_node_check_param_name(element, TRUE, key, &init_name);

	if (init_name)
	{
		GObject *obj = __ms_get_property_owner(element, init_name, &value);

		if (obj == NULL) {
			ms_debug("Element [%s] does not have property [%s].", element_name, init_name);
			MS_SAFE_GFREE(element_name);
			return FALSE;
		}

		if (!g_strcmp0(init_name, MEDIA_STREAMER_PARAM_CAMERA_ID)) {
			pint = atoi(param_value);
			g_value_set_int(&value, pint);
			g_object_set(obj, init_name, pint, NULL);
			ms_info("Set int value: [%d]", g_value_get_int(&value));
		} else if (!g_strcmp0(init_name, MEDIA_STREAMER_PARAM_CAPTURE_WIDTH)) {
			pint = atoi(param_value);
			g_value_set_int(&value, pint);
			g_object_set(obj, init_name, pint, NULL);
			ms_info("Set int value: [%d]", g_value_get_int(&value));
		} else if (!g_strcmp0(init_name, MEDIA_STREAMER_PARAM_CAPTURE_HEIGHT)) {
			pint = atoi(param_value);
			g_value_set_int(&value, pint);
			g_object_set(obj, init_name, pint, NULL);
			ms_info("Set int value: [%d]", g_value_get_int(&value));
		} else if (!g_strcmp0(init_name, MEDIA_STREAMER_PARAM_IS_LIVE_STREAM)) {
			bool_val = !g_strcmp0(param_value, "true") ? TRUE : FALSE;
			g_value_set_boolean(&value, bool_val);
			g_object_set(obj, init_name, bool_val, NULL);
			ms_info("Set boolean value: [%d]", g_value_get_boolean(&value));
		} else if (!g_strcmp0(init_name, MEDIA_STREAMER_PARAM_URI)) {
			g_value_set_string(&value, param_value);
			g_object_set(obj, init_name, param_value, NULL);
			ms_info("Set string value: [%s]", g_value_get_string(&value));
		} else if (!g_strcmp0(init_name, MEDIA_STREAMER_PARAM_USER_AGENT)) {
			g_value_set_string(&value, param_value);
			g_object_set(obj, init_name, param_value, NULL);
			ms_info("Set string value: [%s]", g_value_get_string(&value));
		} else if (!g_strcmp0(init_name, MEDIA_STREAMER_PARAM_STREAM_TYPE)) {
			pint = atoi(param_value);
			g_object_set(obj, init_name, pint, NULL);
			ms_info("Set int value: [%d] ", pint);
		} else if (!g_strcmp0(init_name, MEDIA_STREAMER_PARAM_PORT)) {
			pint = atoi(param_value);
			g_value_set_int(&value, pint);
			g_object_set(obj, init_name, pint, NULL);
			ms_info("Set int value: [%d]", g_value_get_int(&value));
		} else if (!g_strcmp0(init_name, MEDIA_STREAMER_PARAM_VIDEO_IN_PORT)) {
			pint = atoi(param_value);
			g_value_set_int(&value, pint);
			g_object_set(obj, init_name, pint, NULL);
			ms_info("Set int value: [%d]", g_value_get_int(&value));
		} else if (!g_strcmp0(init_name, MEDIA_STREAMER_PARAM_AUDIO_IN_PORT)) {
			pint = atoi(param_value);
			g_value_set_int(&value, pint);
			g_object_set(obj, init_name, pint, NULL);
			ms_info("Set int value: [%d]", g_value_get_int(&value));
		} else if (!g_strcmp0(init_name, MEDIA_STREAMER_PARAM_VIDEO_OUT_PORT)) {
			pint = atoi(param_value);
			g_value_set_int(&value, pint);
			g_object_set(obj, init_name, pint, NULL);
			ms_info("Set int value: [%d]", g_value_get_int(&value));
		} else if (!g_strcmp0(init_name, MEDIA_STREAMER_PARAM_AUDIO_OUT_PORT)) {
			pint = atoi(param_value);
			g_value_set_int(&value, pint);
			g_object_set(obj, init_name, pint, NULL);
			ms_info("Set int value: [%d]", g_value_get_int(&value));
		} else if (!g_strcmp0(init_name, MEDIA_STREAMER_PARAM_IP_ADDRESS)) {
			g_value_set_string(&value, param_value);
			g_object_set(obj, init_name, param_value, NULL);
			ms_info("Set string value: [%s]", g_value_get_string(&value));
		} else if (!g_strcmp0(init_name, MEDIA_STREAMER_PARAM_AUDIO_DEVICE)) {
			g_value_set_string(&value, param_value);
			g_object_set(obj, init_name, param_value, NULL);
			ms_info("Set string value: [%s]", g_value_get_string(&value));
		} else if (!g_strcmp0(init_name, MEDIA_STREAMER_PARAM_CLOCK_SYNCHRONIZED)) {
			bool_val = !g_strcmp0(param_value, "true") ? TRUE : FALSE;
			g_value_set_boolean(&value, bool_val);
			g_object_set(obj, init_name, bool_val, NULL);
			ms_info("Set boolean value: [%d]", g_value_get_boolean(&value));
		} else if (!g_strcmp0(init_name, MEDIA_STREAMER_PARAM_ROTATE)) {
			pint = atoi(param_value);
			g_object_set(obj, init_name, pint, NULL);
			ms_info("Set int value: [%d] ", pint);
		} else if (!g_strcmp0(init_name, MEDIA_STREAMER_PARAM_FLIP)) {
			pint = atoi(param_value);
			g_object_set(obj, init_name, pint, NULL);
			ms_info("Set int value: [%d] ", pint);
		} else if (!g_strcmp0(init_name, MEDIA_STREAMER_PARAM_DISPLAY_GEOMETRY_METHOD)) {
			pint = atoi(param_value);
			g_object_set(obj, init_name, pint, NULL);
			ms_info("Set int value: [%d] ", pint);
		} else if (!g_strcmp0(init_name, MEDIA_STREAMER_PARAM_VISIBLE)) {
			bool_val = !g_strcmp0(param_value, "true") ? TRUE : FALSE;
			g_value_set_boolean(&value, bool_val);
			g_object_set(obj, init_name, bool_val, NULL);
			ms_info("Set boolean value: [%d]", g_value_get_boolean(&value));
		} else if (!g_strcmp0(init_name, MEDIA_STREAMER_PARAM_HOST)) {
			g_value_set_string(&value, param_value);
			g_object_set(obj, init_name, param_value, NULL);
			ms_info("Set string value: [%s]", g_value_get_string(&value));
		} else {
			ms_info("Got unknown type with param->value_type [%d]", G_VALUE_TYPE(&value));
			return FALSE;
		}
	} else {
		ms_info("Can not set parameter [%s] in the node [%s]\n", key, gst_element_get_name(element));
	}

	MS_SAFE_GFREE(element_name);

	return TRUE;
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
	parent = (GstElement *)gst_element_get_parent(GST_OBJECT_CAST(element));

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
#endif

static GstElement *__ms_bin_find_element_by_klass(GstElement *sink_bin, const gchar *klass_name)
{
	GValue elem = G_VALUE_INIT;
	GstIterator *bin_iterator = gst_bin_iterate_sorted(GST_BIN(sink_bin));
	GstElement *found_element;
	gboolean found = FALSE;

	while (GST_ITERATOR_OK == gst_iterator_next(bin_iterator, &elem)) {

		found_element = (GstElement *)g_value_get_object(&elem);
		const gchar *found_klass = gst_element_factory_get_klass(gst_element_get_factory(found_element));

		if (g_strrstr(found_klass, klass_name)) {
			found = TRUE;
			break;
		}

		g_value_reset(&elem);
	}

	g_value_unset(&elem);
	gst_iterator_free(bin_iterator);
	return found ? found_element : NULL;
}

gboolean __ms_feature_filter(GstPluginFeature *feature, gpointer data)
{
	if (!GST_IS_ELEMENT_FACTORY(feature))
		return FALSE;

	return TRUE;
}

static GstElement *__ms_create_element_by_registry(GstPad *src_pad, const gchar *klass_name)
{
	GList *factories;
	const GList *pads;
	GstElement *next_element = NULL;
	GstCaps *new_pad_caps = NULL;

	new_pad_caps = gst_pad_query_caps(src_pad, NULL);

	factories = gst_registry_feature_filter(gst_registry_get(),
                                               (GstPluginFeatureFilter)__ms_feature_filter, FALSE, NULL);

	for(; factories != NULL ; factories = factories->next) {
		GstElementFactory *factory = GST_ELEMENT_FACTORY(factories->data);

		if (g_strrstr(gst_element_factory_get_klass(GST_ELEMENT_FACTORY(factory)), klass_name))
		{
			for(pads = gst_element_factory_get_static_pad_templates(factory); pads != NULL; pads = pads->next) {

				GstCaps *intersect_caps = NULL;
				GstCaps *static_caps = NULL;
				GstStaticPadTemplate *pad_temp = pads->data;

				if (pad_temp->presence != GST_PAD_ALWAYS || pad_temp->direction != GST_PAD_SINK) {
					continue;
				}

				if (GST_IS_CAPS(&pad_temp->static_caps.caps)) {
					static_caps = gst_caps_ref(&pad_temp->static_caps.caps);
				} else {
					static_caps = gst_caps_from_string(pad_temp->static_caps.string);
				}

				intersect_caps = gst_caps_intersect_full(new_pad_caps, static_caps, GST_CAPS_INTERSECT_FIRST);

				if (!gst_caps_is_empty(intersect_caps)) {
					if (!next_element) {
						next_element = __ms_element_create(GST_OBJECT_NAME(factory), NULL);
					}
				}
				gst_caps_unref(intersect_caps);
				gst_caps_unref(static_caps);
			}
		}
	}

	return next_element;
}

static GstElement *__ms_link_with_new_element (GstElement *previous_element, GstElement *new_element)
{
	gboolean ret = gst_element_link_pads_filtered(previous_element, NULL, new_element, NULL, NULL);

	if (!ret) {
		ms_error("Failed to link [%s] and [%s] \n", GST_ELEMENT_NAME(previous_element),
				 GST_ELEMENT_NAME(new_element));
		new_element = NULL;
	} else {
		ms_info("Succeeded to link [%s] -> [%s]\n", GST_ELEMENT_NAME(previous_element),
				 GST_ELEMENT_NAME(new_element));
	}

	return new_element;
}

void __decodebin_newpad_cb(GstElement *decodebin, GstPad *new_pad, gpointer user_data)
{
	GstElement *previous_element = NULL;
	GstElement *found_element = NULL;
	GstElement *parent = NULL;

	GstCaps *new_pad_caps = NULL;
	GstStructure *new_pad_struct = NULL;
	const gchar *new_pad_type = NULL;

	ms_info("Received new pad '%s' from [%s]", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(decodebin));
	parent = (GstElement *)gst_element_get_parent(GST_OBJECT_CAST(decodebin));

	/* Check the new pad's type */
	new_pad_caps = gst_pad_query_caps(new_pad,0);
	new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
	new_pad_type = gst_structure_get_name(new_pad_struct);

	if (g_str_has_prefix(new_pad_type, "video")) {
		found_element = __ms_element_create("videoconvert", NULL);
	} else if (g_str_has_prefix(new_pad_type, "audio")) {
		found_element = __ms_element_create("audioconvert", NULL);
	} else {
		ms_error("Could not create converting element according to media pad type");
	}

	if (!gst_bin_add(GST_BIN(parent), found_element)) {
		ms_error("Failed to add element [%s] into bin [%s]", GST_ELEMENT_NAME(found_element),
				GST_ELEMENT_NAME(parent));
	}
	gst_element_sync_state_with_parent(found_element);

	previous_element = __ms_link_with_new_element(decodebin, found_element);
	if (!previous_element) {
		gst_bin_remove(GST_BIN(parent), found_element);
	}


	/* Getting Sink */
	found_element = __ms_bin_find_element_by_klass(parent, MEDIA_STREAMER_SINK_KLASS);
	if (!found_element) {
		ms_error("There are no any SINK elements created by user");
	}
	gst_element_sync_state_with_parent(found_element);

	previous_element = __ms_link_with_new_element(previous_element, found_element);
	if (!previous_element) {
		gst_bin_remove(GST_BIN(parent), found_element);
	}

	__ms_element_set_state(parent, GST_STATE_PLAYING);
}

static gboolean __ms_sink_bin_prepare(media_streamer_node_s *ms_node, GstElement *sink_bin, GstPad *source_pad)
{
	GstPad *src_pad = NULL;
	gboolean decodebin_usage = FALSE;
	GstElement *found_element = NULL;
	GstElement *previous_element = NULL;

	GstCaps *new_pad_caps = NULL;
	GstStructure *new_pad_struct = NULL;
	const gchar *new_pad_type = NULL;


	/* Getting Depayloader */
	found_element = __ms_bin_find_element_by_klass(sink_bin, MEDIA_STREAMER_DEPAYLOADER_KLASS);

	if (!found_element) {
		found_element = __ms_create_element_by_registry(source_pad, MEDIA_STREAMER_DEPAYLOADER_KLASS);
		if (!gst_bin_add(GST_BIN(sink_bin), found_element)) {
			ms_error("Failed to add element [%s] into bin [%s]", GST_ELEMENT_NAME(found_element),
                     GST_ELEMENT_NAME(sink_bin));
		}
	}
	previous_element = found_element;
	src_pad = gst_element_get_static_pad(found_element, "src");

	__ms_add_ghostpad(found_element, "sink", sink_bin, "sink");


	/* Getting Decodebin */
	decodebin_usage = ms_node->parent_streamer->ini.use_decodebin;

	found_element = __ms_bin_find_element_by_klass(sink_bin, MEDIA_STREAMER_DECODEBIN_KLASS);

	if (!found_element) {
		if (decodebin_usage) {
			found_element = __ms_create_element_by_registry(src_pad, MEDIA_STREAMER_DECODEBIN_KLASS);
			if (!found_element) {
				ms_error("Could not create element of MEDIA_STREAMER_DECODER_KLASS");
			}
			if (!gst_bin_add(GST_BIN(sink_bin), found_element)) {
				ms_error("Failed to add element [%s] into bin [%s]", GST_ELEMENT_NAME(found_element),
						 GST_ELEMENT_NAME(sink_bin));
			}
			if (ms_node->parent_streamer->ini.use_decodebin) {
				g_signal_connect(found_element, "pad-added", G_CALLBACK(__decodebin_newpad_cb), (gpointer)ms_node);
			}

			previous_element = __ms_link_with_new_element(previous_element, found_element);
			if (!previous_element) {
				gst_bin_remove(sink_bin, found_element);
				return FALSE;
			}
		} else {

			src_pad = gst_element_get_static_pad(previous_element, "src");
			new_pad_caps = gst_pad_query_caps(src_pad, 0);
			new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
			new_pad_type = gst_structure_get_name(new_pad_struct);

			if (g_str_has_prefix(new_pad_type, "video")) {

				/* Getting Parser */
				found_element = __ms_bin_find_element_by_klass(sink_bin, MEDIA_STREAMER_PARSER_KLASS);
				if (!found_element) {
					found_element = __ms_create_element_by_registry(src_pad, MEDIA_STREAMER_PARSER_KLASS);
					if (!found_element) {
						ms_error("Could not create element of MEDIA_STREAMER_PARSER_KLASS");
					}
					if (!gst_bin_add(GST_BIN(sink_bin), found_element)) {
						ms_error("Failed to add element [%s] into bin [%s]", GST_ELEMENT_NAME(found_element),
								 GST_ELEMENT_NAME(sink_bin));
					}
				}
				previous_element = __ms_link_with_new_element(previous_element, found_element);
				if (!previous_element) {
					gst_bin_remove(GST_BIN(sink_bin), found_element);
					return FALSE;
				}
				src_pad = gst_element_get_static_pad(found_element, "src");


				/* Getting Decoder */
				found_element = __ms_bin_find_element_by_klass(sink_bin, MEDIA_STREAMER_DECODER_KLASS);
				if (!found_element) {
					found_element = __ms_create_element_by_registry(src_pad, MEDIA_STREAMER_DECODER_KLASS);
					if (!found_element) {
						ms_error("Could not create element of MEDIA_STREAMER_DECODER_KLASS");
					}
					if (!gst_bin_add(GST_BIN(sink_bin), found_element)) {
						ms_error("Failed to add element [%s] into bin [%s]", GST_ELEMENT_NAME(found_element),
								 GST_ELEMENT_NAME(sink_bin));
					}
				}
				previous_element = __ms_link_with_new_element(previous_element, found_element);
				if (!previous_element) {
					gst_bin_remove(GST_BIN(sink_bin), found_element);
					return FALSE;
				}
				src_pad = gst_element_get_static_pad(found_element, "src");


				/* Getting Convertor */
				found_element = __ms_element_create("videoconvert", NULL);

			} else if (g_str_has_prefix(new_pad_type, "audio")) {
				found_element = __ms_element_create("audioconvert", NULL);
			} else {
				ms_error("Could not create converting element according to media pad type");
			}

			if (!found_element) {
				ms_error("Could not create element of MEDIA_STREAMER_CONVERTER_KLASS");
			}
			if (!gst_bin_add(GST_BIN(sink_bin), found_element)) {
				ms_error("Failed to add element [%s] into bin [%s]", GST_ELEMENT_NAME(found_element),
						 GST_ELEMENT_NAME(sink_bin));
			}
			previous_element = __ms_link_with_new_element(previous_element, found_element);
			if (!previous_element) {
				gst_bin_remove(GST_BIN(sink_bin), found_element);
				return FALSE;
			}


		/* Getting Sink */
		found_element = __ms_bin_find_element_by_klass(sink_bin, MEDIA_STREAMER_SINK_KLASS);
		if (!found_element) {
			ms_error("There are no any SINK elements created by user");
		}
		previous_element = __ms_link_with_new_element(previous_element, found_element);
		if (!previous_element) {
			gst_bin_remove(GST_BIN(sink_bin), found_element);
			return FALSE;
		}

		__ms_generate_dots(ms_node->parent_streamer->pipeline, "pipeline_linked");
		__ms_element_set_state(ms_node->parent_streamer->pipeline, GST_STATE_PLAYING);
		}

	} else {
		if(!gst_pad_is_linked(src_pad)) {
			previous_element = __ms_link_with_new_element(previous_element, found_element);
			if (!previous_element) {
				gst_bin_remove(GST_BIN(sink_bin), found_element);
				return FALSE;
			}
		} else {
			previous_element = found_element;
		}

		if (!decodebin_usage) {

			/* Getting Sink */
			found_element = __ms_bin_find_element_by_klass(sink_bin, MEDIA_STREAMER_SINK_KLASS);
			if (!found_element) {
				ms_error("There are no any SINK elements created by user");
			}

			if(!gst_pad_is_linked(src_pad)) {
				previous_element = __ms_link_with_new_element(previous_element, found_element);
				if (!previous_element) {
					gst_bin_remove(GST_BIN(sink_bin), found_element);
					return FALSE;
				}
			}

			__ms_generate_dots(ms_node->parent_streamer->pipeline, "pipeline_linked");
			__ms_element_set_state(ms_node->parent_streamer->pipeline, GST_STATE_PLAYING);
		}
	}

	MS_SAFE_UNREF(src_pad);

	return TRUE;
}

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
			source_pad_name = g_strdup_printf("%s_out", "video");
			sink_bin = ms_node->parent_streamer->sink_video_bin;
		} else if (MS_ELEMENT_IS_AUDIO(src_pad_type)) {
			source_pad_name = g_strdup_printf("%s_out", "audio");
			sink_bin = ms_node->parent_streamer->sink_audio_bin;
		}

		if (source_pad_name != NULL) {

			GstPad *source_pad = gst_element_get_static_pad(ms_node->gst_element, source_pad_name);
			gst_ghost_pad_set_target(GST_GHOST_PAD(source_pad), new_pad);
			gst_pad_set_active(source_pad, TRUE);

			g_mutex_lock(&ms_node->parent_streamer->mutex_lock);
			if (__ms_sink_bin_prepare(ms_node, sink_bin, source_pad)) {

				if (gst_object_get_parent(GST_OBJECT(sink_bin)) == NULL) {
					gst_bin_add(GST_BIN(ms_node->parent_streamer->pipeline), sink_bin);
				}

				gst_element_sync_state_with_parent(sink_bin);

				if (gst_element_link_pads(ms_node->gst_element, source_pad_name, sink_bin, "sink")) {
					__ms_element_set_state(ms_node->gst_element, GST_STATE_PLAYING);
					__ms_generate_dots(ms_node->parent_streamer->pipeline, "playing");
				} else {
					ms_error("Failed to link [rtp_containeer].[%s] and [sink_bin].[sink]\n", source_pad_name);
				}
			} else {
				ms_error("Failed to prepare sink_bin for pad type [%s]\n", src_pad_type);
			}

			g_mutex_unlock(&ms_node->parent_streamer->mutex_lock);

			MS_SAFE_UNREF(source_pad);
			MS_SAFE_GFREE(source_pad_name);
		}
	} else {
		ms_debug("Node doesn`t have parent streamer or caps media type\n");
	}

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
		ms_error("Failed to set element [%s] into %s state",
                 element_name, gst_element_state_get_name(gst_state));
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

	if (g_strrstr(format_prefix, "omx")) {
		is_omx = TRUE;
	}

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
                               GstElement **rtp_elem,
                               GstElement **rtcp_elem,
                               const gchar *elem_name,
                               const gchar *direction,
                               gboolean auto_create)
{
	gboolean ret = TRUE;
	gchar *rtp_elem_name = NULL;
	gchar *rtcp_elem_name = NULL;
	gchar *plugin_name = NULL;

	GstElement *rtpbin = gst_bin_get_by_name(GST_BIN(ms_node->gst_element), "rtpbin");

	ms_retvm_if(!elem_name || !direction, FALSE, "Empty rtp element name or direction.");

	if (MS_ELEMENT_IS_INPUT(direction)) {
		plugin_name = g_strdup(DEFAULT_UDP_SOURCE);
	} else if (MS_ELEMENT_IS_OUTPUT(direction)) {
		plugin_name = g_strdup(DEFAULT_UDP_SINK);
	} else {
		ms_error("Error: invalid parameter name [%s]", elem_name);
		return FALSE;
	}

	rtp_elem_name = g_strdup_printf("%s_%s_rtp", elem_name, direction);
	rtcp_elem_name = g_strdup_printf("%s_%s_rtcp", elem_name, direction);

	/* Find video udp rtp/rtcp element if it present. */
	*rtp_elem = gst_bin_get_by_name(GST_BIN(ms_node->gst_element), rtp_elem_name);
	*rtcp_elem = gst_bin_get_by_name(GST_BIN(ms_node->gst_element), rtcp_elem_name);

	/* Create new udp element if it did not found. */
	if ((NULL == *rtp_elem) && (NULL == *rtcp_elem) && auto_create) {
		*rtp_elem = __ms_element_create(plugin_name, rtp_elem_name);
		*rtcp_elem = __ms_element_create(plugin_name, rtcp_elem_name);
		gst_bin_add_many(GST_BIN(ms_node->gst_element),
		                 *rtp_elem, *rtcp_elem, NULL);
	} else {
		/*rtp/rtcp elements already into rtp bin. */
		MS_SAFE_GFREE(rtp_elem_name);
		MS_SAFE_GFREE(rtcp_elem_name);
		MS_SAFE_GFREE(plugin_name);
		return TRUE;
	}

	if (MS_ELEMENT_IS_OUTPUT(direction)) {
		g_object_set(GST_OBJECT(*rtcp_elem), "sync", FALSE, NULL);
		g_object_set(GST_OBJECT(*rtcp_elem), "async", FALSE, NULL);

		if (MS_ELEMENT_IS_VIDEO(elem_name)) {
			__ms_add_ghostpad(rtpbin, "send_rtp_sink_0", ms_node->gst_element, "video_in");
			ret = gst_element_link_pads(rtpbin, "send_rtp_src_0", *rtp_elem, "sink") &&
			      gst_element_link_pads(rtpbin, "send_rtcp_src_0", *rtcp_elem, "sink");
		} else {
			__ms_add_ghostpad(rtpbin, "send_rtp_sink_1", ms_node->gst_element, "audio_in");
			ret = gst_element_link_pads(rtpbin, "send_rtp_src_1", *rtp_elem, "sink") &&
			      gst_element_link_pads(rtpbin, "send_rtcp_src_1", *rtcp_elem, "sink");
		}
	} else {
		if (MS_ELEMENT_IS_VIDEO(elem_name)) {
			ret = gst_element_link_pads(*rtp_elem, "src", rtpbin, "recv_rtp_sink_0") &&
			      gst_element_link_pads(*rtcp_elem, "src", rtpbin, "recv_rtcp_sink_0");
			__ms_add_no_target_ghostpad(ms_node->gst_element, "video_out", GST_PAD_SRC);
			__ms_add_no_target_ghostpad(ms_node->gst_element, "video_in_rtp", GST_PAD_SINK);
		} else {
			ret = gst_element_link_pads(*rtp_elem, "src", rtpbin, "recv_rtp_sink_1") &&
			      gst_element_link_pads(*rtcp_elem, "src", rtpbin, "recv_rtcp_sink_1");
			__ms_add_no_target_ghostpad(ms_node->gst_element, "audio_out", GST_PAD_SRC);
			__ms_add_no_target_ghostpad(ms_node->gst_element, "audio_in_rtp", GST_PAD_SINK);
		}
	}

	if (!ret) {
		ms_error("Can not link [rtpbin] pad to [%s] pad", rtp_elem_name);
		ret = FALSE;
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

static gboolean
__ms_parse_gst_error(media_streamer_s *ms_streamer, GstMessage *message, GError *error)
{
	ms_retvm_if(!ms_streamer, FALSE, "Error: invalid Media Streamer handle.");
	ms_retvm_if(!error, FALSE, "Error: invalid error handle.");
	ms_retvm_if(!message, FALSE, "Error: invalid bus message handle.");

	media_streamer_error_e ret_error = MEDIA_STREAMER_ERROR_NONE;
	if (error->domain == GST_CORE_ERROR) {
		ret_error = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	} else if (error->domain == GST_LIBRARY_ERROR) {
		ret_error = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	} else if (error->domain == GST_RESOURCE_ERROR) {
		ret_error = MEDIA_STREAMER_ERROR_RESOURCE_CONFLICT;
	} else if (error->domain == GST_STREAM_ERROR) {
		ret_error = MEDIA_STREAMER_ERROR_CONNECTION_FAILED;
	} else {
		ret_error = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	/* post error to application */
	if(ms_streamer->error_cb.callback) {
		media_streamer_error_cb error_cb = (media_streamer_error_cb)ms_streamer->error_cb.callback;
		error_cb((media_streamer_h)ms_streamer, ret_error, ms_streamer->error_cb.user_data);
	}

	return TRUE;
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
					GError *err = NULL;
					gchar *debug = NULL;
					gst_message_parse_error(message, &err, &debug);

					/* Transform gst error code to media streamer error code.
					 * then post it to application if needed */
					__ms_parse_gst_error(ms_streamer, message, err);

					ms_error("[Source: %s] Error: %s",
					         GST_OBJECT_NAME(GST_OBJECT_CAST(GST_ELEMENT(GST_MESSAGE_SRC(message)))),
					         err->message);

					g_error_free(err);
					MS_SAFE_FREE(debug);
					break;
				}

			case GST_MESSAGE_STATE_CHANGED: {
					if (GST_MESSAGE_SRC(message) == GST_OBJECT(ms_streamer->pipeline)) {
						GstState state_old, state_new, state_pending;
						gchar *state_transition_name;

						gst_message_parse_state_changed(message, &state_old, &state_new, &state_pending);
						state_transition_name = g_strdup_printf("%s_%s",
						                                        gst_element_state_get_name(state_old),
						                                        gst_element_state_get_name(state_new));
						ms_info("GST_MESSAGE_STATE_CHANGED: [%s] %s",
						        gst_object_get_name(GST_MESSAGE_SRC(message)),
								state_transition_name);
						__ms_generate_dots(ms_streamer->pipeline, state_transition_name);

						MS_SAFE_GFREE(state_transition_name);
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
			                           "framerate", GST_TYPE_FRACTION, max_bps, avg_bps,
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
		int width, height, avg_bps, max_bps;

		gst_structure_get_int(pad_struct, "width", &width);
		gst_structure_get_fraction(pad_struct, "framerate", &max_bps, &avg_bps);
		gst_structure_get_int(pad_struct, "height", &height);

		media_format_set_video_mime(fmt, __ms_convert_string_format_to_mime(pad_format));
		media_format_set_video_width(fmt, width);
		media_format_set_video_height(fmt, height);
		media_format_set_video_avg_bps(fmt, avg_bps);
		media_format_set_video_max_bps(fmt, max_bps);
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

int __ms_iterate_pads(GstElement *gst_element, GstPadDirection pad_type, char ***pad_name_array, int *pads_count)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;

	int pad_number = 0;
	GValue elem = G_VALUE_INIT;
	GstPad *pad = NULL;
	char **pad_names = NULL;
	GstIterator *pad_iterator = NULL;

	if(pad_type == GST_PAD_SRC) {
		pad_iterator = gst_element_iterate_src_pads(gst_element);
	} else if(pad_type == GST_PAD_SINK) {
		pad_iterator = gst_element_iterate_sink_pads(gst_element);
	} else {
		pad_iterator = gst_element_iterate_pads(gst_element);
		ms_info("Iterating all pads of the Gst element");
	}

	while (GST_ITERATOR_OK == gst_iterator_next(pad_iterator, &elem)) {
		pad = (GstPad *)g_value_get_object(&elem);

		pad_names = (char **)realloc(pad_names, sizeof(char *) * (pad_number + 1));
		if(!pad_names){
			ms_error("Error allocation memory");
			ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
			pad_number = 0;
			MS_SAFE_FREE(pad_names);
			break;
		}

		pad_names[pad_number] = gst_pad_get_name(pad);
		++pad_number;

		g_value_reset(&elem);
	}

	g_value_unset(&elem);
	gst_iterator_free(pad_iterator);

	*pad_name_array = pad_names;
	*pads_count = pad_number;

	return ret;
}

media_format_h __ms_element_get_pad_fmt(GstElement *gst_element, const char *pad_name)
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

int __ms_element_set_fmt(media_streamer_node_s *node, const char *pad_name, media_format_h fmt)
{
	GstCaps *caps = NULL;
	GObject *obj = NULL;
	GValue value = G_VALUE_INIT;

	if (node->type == MEDIA_STREAMER_NODE_TYPE_RTP) {
		/* It is needed to set 'application/x-rtp' for audio and video udpsrc*/
		media_format_mimetype_e mime;
		GstElement *rtp_elem, *rtcp_elem;

		/*Check if it is a valid pad*/
		GstPad *pad = gst_element_get_static_pad(node->gst_element, pad_name);
		if (!pad) {
			ms_error("Error: Failed set format to pad [%s].[%s].", node->name, pad_name);
			return MEDIA_STREAMER_ERROR_INVALID_PARAMETER;
		}

		if (MEDIA_FORMAT_ERROR_NONE == media_format_get_video_info(fmt, &mime, NULL, NULL, NULL, NULL)) {
			__ms_get_rtp_elements(node, &rtp_elem, &rtcp_elem, "video", "in", FALSE);

			caps = gst_caps_from_string("application/x-rtp,media=video,clock-rate=90000,encoding-name=H264");
			ms_retvm_if(caps == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Fail creating caps from fmt.");

			obj = __ms_get_property_owner(rtp_elem, "caps", &value);
		} else if (MEDIA_FORMAT_ERROR_NONE == media_format_get_audio_info(fmt, &mime, NULL, NULL, NULL, NULL)) {
			__ms_get_rtp_elements(node, &rtp_elem, &rtcp_elem, "audio", "in", FALSE);

			caps = gst_caps_from_string("application/x-rtp,media=audio,clock-rate=44100,encoding-name=L16,"
			                            "encoding-params=1,channels=1,payload=96");
			ms_retvm_if(caps == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Fail creating caps from fmt.");

			obj = __ms_get_property_owner(rtp_elem, "caps", &value);
		} else {
			ms_error("Failed getting media info from fmt.");
			return MEDIA_STREAMER_ERROR_INVALID_PARAMETER;
		}

		MS_SAFE_UNREF(pad);
	} else {
		caps = __ms_create_caps_from_fmt(fmt);
		ms_retvm_if(caps == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Fail creating caps from fmt.");

		obj = __ms_get_property_owner(node->gst_element, "caps", &value);
	}

	gst_value_set_caps(&value, caps);
	g_object_set_property(obj, "caps", &value);
	gst_caps_unref(caps);

	return MEDIA_STREAMER_ERROR_NONE;
}

int __ms_element_push_packet(GstElement *src_element, media_packet_h packet)
{
	GstBuffer *buffer = NULL;
	GstFlowReturn gst_ret = GST_FLOW_OK;
	guchar *buffer_data = NULL;

	ms_retvm_if(src_element == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	if (packet == NULL) {
		g_signal_emit_by_name(G_OBJECT(src_element), "end-of-stream", &gst_ret, NULL);
		return MEDIA_STREAMER_ERROR_NONE;
	}

	media_packet_get_buffer_data_ptr(packet, (void **)&buffer_data);

	if(buffer_data != NULL) {
		GstMapInfo buff_info = GST_MAP_INFO_INIT;
		guint64 pts = 0;
		guint64 duration = 0;
		guint64 size  = 0;

		media_packet_get_buffer_size(packet, &size);

		buffer = gst_buffer_new_and_alloc (size);
		if (!buffer) {
			ms_error("Failed to allocate memory for push buffer");
			return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
		}

		if (gst_buffer_map (buffer, &buff_info, GST_MAP_READWRITE)) {
			memcpy (buff_info.data, buffer_data, size);
			buff_info.size = size;
			gst_buffer_unmap (buffer, &buff_info);
		}

		media_packet_get_pts(packet, &pts);
		GST_BUFFER_PTS(buffer) = pts;

		media_packet_get_duration(packet, &duration);
		GST_BUFFER_DURATION(buffer) = duration;

		g_signal_emit_by_name(G_OBJECT(src_element), "push-buffer", buffer, &gst_ret, NULL);
		gst_buffer_unref(buffer);

	} else {
		 g_signal_emit_by_name(G_OBJECT(src_element), "end-of-stream", &gst_ret, NULL);
	}

	if (gst_ret != GST_FLOW_OK) {
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	return MEDIA_STREAMER_ERROR_NONE;
}

int __ms_element_pull_packet(GstElement *sink_element, media_packet_h *packet)
{
	GstBuffer *buffer = NULL;
	GstSample *sample = NULL;
	GstMapInfo map;
	guint8 *buffer_res = NULL;

	ms_retvm_if(sink_element == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(packet == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	/* Retrieve the buffer */
	g_signal_emit_by_name(sink_element, "pull-sample", &sample, NULL);
	ms_retvm_if(sample == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Pull sample failed!");

	buffer = gst_sample_get_buffer(sample);
	gst_buffer_map(buffer, &map, GST_MAP_READ);

	media_format_h fmt = __ms_element_get_pad_fmt(sink_element, "sink");
	if (!fmt) {
		ms_error("Error while getting media format from sink pad");

		gst_buffer_unmap(buffer, &map);
		gst_sample_unref(sample);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	buffer_res = (guint8 *)calloc(map.size, sizeof(guint8));
	memcpy(buffer_res, map.data, map.size);

	media_packet_create_from_external_memory(fmt, (void *)buffer_res, map.size, NULL, NULL, packet);
	media_packet_set_pts(*packet, GST_BUFFER_PTS(buffer));
	media_packet_set_dts(*packet, GST_BUFFER_DTS(buffer));
	media_packet_set_pts(*packet, GST_BUFFER_DURATION(buffer));

	media_format_unref(fmt);
	gst_buffer_unmap(buffer, &map);
	gst_sample_unref(sample);

	return MEDIA_STREAMER_ERROR_NONE;
}
