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

#define MS_PADS_UNLINK(pad, peer) GST_PAD_IS_SRC(pad) ? \
		gst_pad_unlink(pad, peer) : \
		gst_pad_unlink(peer, pad)
#define H264_PARSER_CONFIG_INTERVAL 5
#define H264_ENCODER_ZEROLATENCY 0x00000004


void __ms_generate_dots(GstElement *bin, gchar *name_tag)
{
	gchar *dot_name;
	ms_retm_if(bin == NULL, "Handle is NULL");

	if (!name_tag)
		dot_name = g_strdup(DOT_FILE_NAME);
	else
		dot_name = g_strconcat(DOT_FILE_NAME, ".", name_tag, NULL);

	GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(bin), GST_DEBUG_GRAPH_SHOW_ALL, dot_name);

	MS_SAFE_GFREE(dot_name);
}

static int __ms_add_no_target_ghostpad(GstElement *gst_bin, const char *ghost_pad_name, GstPadDirection pad_direction)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;

	gchar *bin_name = gst_element_get_name(gst_bin);
	GstPad *ghost_pad = gst_ghost_pad_new_no_target(ghost_pad_name, pad_direction);
	if (gst_element_add_pad(GST_ELEMENT(gst_bin), ghost_pad)) {
		gst_pad_set_active(ghost_pad, TRUE);
		ms_info("Added [%s] empty ghostpad into [%s]", ghost_pad_name, bin_name);
	} else {
		ms_info("Error: Failed to add empty [%s] ghostpad into [%s]", ghost_pad_name, bin_name);
		ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	MS_SAFE_GFREE(bin_name);
	return ret;
}

static gboolean __ms_add_ghostpad(GstElement *gst_element, const char *pad_name, GstElement *gst_bin, const char *ghost_pad_name)
{
	ms_retvm_if(!gst_element || !pad_name || !ghost_pad_name || !gst_bin, FALSE, "Handle is NULL");

	gboolean ret = FALSE;

	GstPad *ghost_pad = NULL;
	GstPad *element_pad = gst_element_get_static_pad(gst_element, pad_name);

	/* Check, if pad is not static, get it by request */
	if (element_pad == NULL)
		element_pad = gst_element_get_request_pad(gst_element, pad_name);

	ghost_pad = gst_ghost_pad_new(ghost_pad_name, element_pad);
	gst_pad_set_active(ghost_pad, TRUE);

	ret = gst_element_add_pad(GST_ELEMENT(gst_bin), ghost_pad);

	if (ret)
		ms_info("Added %s ghostpad from [%s] into [%s]", pad_name, GST_ELEMENT_NAME(gst_element), GST_ELEMENT_NAME(gst_bin));
	else
		ms_error("Error: element [%s] does not have valid [%s] pad for adding into [%s] bin", GST_ELEMENT_NAME(gst_element), pad_name, GST_ELEMENT_NAME(gst_bin));

	MS_SAFE_UNREF(element_pad);
	return ret;
}

/* This unlinks from its peer and ghostpads on its way */
static gboolean __ms_pad_peer_unlink(GstPad *pad)
{
	if (!gst_pad_is_linked(pad))
		return TRUE;

	gboolean ret = TRUE;
	GstPad *peer_pad = gst_pad_get_peer(pad);
	ms_retvm_if(!peer_pad, FALSE, "Fail to get peer pad");

	if (GST_IS_PROXY_PAD(peer_pad)) {

		GstObject *ghost_object = gst_pad_get_parent(peer_pad);
		if (GST_IS_GHOST_PAD(ghost_object)) {
			GstPad *ghost_pad = GST_PAD(ghost_object);
			GstPad *target_pad = gst_pad_get_peer(ghost_pad);

			if (GST_IS_GHOST_PAD(target_pad))
				ret = ret && gst_element_remove_pad(GST_ELEMENT(GST_PAD_PARENT(target_pad)), target_pad);
			else {
				/* This is a usual static pad */
				if (target_pad)
					ret = ret && MS_PADS_UNLINK(ghost_pad, target_pad);
				else
					ms_info("Ghost Pad [%s] does not have target.", GST_PAD_NAME(ghost_pad));
			}
			ret = ret && gst_element_remove_pad(GST_ELEMENT(GST_PAD_PARENT(ghost_pad)), ghost_pad);

			MS_SAFE_UNREF(target_pad);
			MS_SAFE_UNREF(ghost_pad);
		} else {
			MS_SAFE_UNREF(ghost_object);
		}
	} else if (GST_IS_GHOST_PAD(peer_pad)) {
		ret = ret && gst_element_remove_pad(GST_ELEMENT(GST_PAD_PARENT(peer_pad)), peer_pad);
	} else {
		/* This is a usual static pad */
		ret = ret && MS_PADS_UNLINK(pad, peer_pad);
	}

	MS_SAFE_UNREF(peer_pad);

	return ret;
}

static GstElement *__ms_pad_get_peer_element(GstPad *pad)
{
	if (!gst_pad_is_linked(pad)) {
		ms_info("Pad [%s:%s] is not linked yet", GST_DEBUG_PAD_NAME(pad));
		return NULL;
	}

	GstPad *peer_pad = gst_pad_get_peer(pad);
	ms_retvm_if(!peer_pad, FALSE, "Fail to get peer pad");

	GstElement *ret = gst_pad_get_parent_element(peer_pad);
	if (!ret) {
		if (GST_IS_PROXY_PAD(peer_pad)) {
			GstPad *ghost_pad = GST_PAD(gst_pad_get_parent(peer_pad));
			GstPad *target_pad = gst_pad_get_peer(ghost_pad);
			if (GST_GHOST_PAD(target_pad)) {
				GstPad *element_pad = gst_ghost_pad_get_target(GST_GHOST_PAD(target_pad));
				ret = gst_pad_get_parent_element(element_pad);
				MS_SAFE_UNREF(element_pad);
			} else {
				/* This is a usual static pad */
				ret = gst_pad_get_parent_element(target_pad);
			}

			MS_SAFE_UNREF(target_pad);
			MS_SAFE_UNREF(ghost_pad);
		} else if (GST_IS_GHOST_PAD(peer_pad)) {
			GstPad *element_pad = gst_ghost_pad_get_target(GST_GHOST_PAD(peer_pad));
			ret = gst_pad_get_parent_element(element_pad);
			MS_SAFE_UNREF(element_pad);
		} else {
			/* This is a usual static pad */
			ret = gst_pad_get_parent_element(peer_pad);
		}
	}
	MS_SAFE_UNREF(peer_pad);
	return ret;
}

gboolean __ms_element_unlink(GstElement *element)
{
	gboolean ret = TRUE;
	GstPad *pad = NULL;
	GValue elem = G_VALUE_INIT;

	GstIterator *pad_iterator = gst_element_iterate_pads(element);
	while (GST_ITERATOR_OK == gst_iterator_next(pad_iterator, &elem)) {
		pad = (GstPad *) g_value_get_object(&elem);
		ret = ret && __ms_pad_peer_unlink(pad);
		g_value_reset(&elem);
	}
	g_value_unset(&elem);
	gst_iterator_free(pad_iterator);

	return ret;
}

gboolean __ms_bin_remove_element(GstElement *element)
{
	GstElement *parent = (GstElement *) gst_element_get_parent(element);
	gboolean ret = FALSE;

	/* Remove node's element from bin that decreases ref count */
	if (parent != NULL) {
		ret = gst_bin_remove(GST_BIN(parent), element);
		if (ret)
			ms_debug("Element [%s] removed from [%s] bin", GST_ELEMENT_NAME(element), GST_ELEMENT_NAME(parent));
	}

	MS_SAFE_UNREF(parent);
	return ret;
}

gboolean __ms_bin_add_element(GstElement *bin, GstElement *element, gboolean do_ref)
{
	GstElement *parent = (GstElement *) gst_element_get_parent(element);
	gboolean ret = FALSE;

	/* Add node's element into bin and increases ref count if needed */
	if (parent == NULL) {
		ret = gst_bin_add(GST_BIN(bin), element);
		if (ret && do_ref) {
			ms_debug("Element [%s] added into [%s] bin", GST_ELEMENT_NAME(element), GST_ELEMENT_NAME(bin));
			gst_object_ref(element);
		}
	}

	MS_SAFE_UNREF(parent);
	return ret;
}

const gchar *__ms_get_pad_type(GstPad *element_pad)
{
	const gchar *pad_type = NULL;
	GstCaps *pad_caps = gst_pad_query_caps(element_pad, 0);
	MS_GET_CAPS_TYPE(pad_caps, pad_type);
	gst_caps_unref(pad_caps);
	return pad_type;
}

static GstElement *__ms_find_peer_element_by_type(GstElement *previous_element, GstPad *prev_elem_src_pad, node_info_s *node_klass_type)
{
	GstElement *peer_element = NULL;
	GstIterator *src_pad_iterator = NULL;
	GValue src_pad_value = G_VALUE_INIT;
	const gchar *found_klass = NULL;

	if (prev_elem_src_pad) {

		/* Check if previous element`s source pad is connected with element */
		peer_element = __ms_pad_get_peer_element(prev_elem_src_pad);
		if (peer_element) {
			found_klass = gst_element_factory_get_klass(gst_element_get_factory(peer_element));
			if (g_strrstr(found_klass, node_klass_type->klass_name) || g_strrstr(GST_ELEMENT_NAME(peer_element), node_klass_type->default_name))
				ms_info(" Found peer element [%s] ", GST_ELEMENT_NAME(peer_element));
			else
				peer_element = NULL;
		}
	} else {

		/* Check if any of previous element`s source pads is connected with element */
		src_pad_iterator = gst_element_iterate_src_pads(previous_element);
		while (GST_ITERATOR_OK == gst_iterator_next(src_pad_iterator, &src_pad_value)) {
			GstPad *src_pad = (GstPad *) g_value_get_object(&src_pad_value);
			peer_element = __ms_pad_get_peer_element(src_pad);
			if (peer_element) {
				found_klass = gst_element_factory_get_klass(gst_element_get_factory(peer_element));
				if (g_strrstr(found_klass, node_klass_type->klass_name) || g_strrstr(GST_ELEMENT_NAME(peer_element), node_klass_type->default_name)) {
					ms_info(" Found peer element [%s] ", GST_ELEMENT_NAME(peer_element));
					break;
				} else {
					peer_element = NULL;
				}
			}
			g_value_reset(&src_pad_value);
		}
		g_value_unset(&src_pad_value);
		gst_iterator_free(src_pad_iterator);
	}

	if (!peer_element)
		ms_info(" Element [%s] is not connected", GST_ELEMENT_NAME(previous_element));

	return peer_element;
}

gboolean __ms_link_two_elements(GstElement *previous_element, GstPad *prev_elem_src_pad, GstElement *found_element)
{
	GValue src_pad_value = G_VALUE_INIT;
	gboolean elements_linked = FALSE;
	GstPad * found_sink_pad = NULL;

	if (prev_elem_src_pad) {
		if (!gst_pad_is_linked(prev_elem_src_pad)) {

			/* Check compatibility of previous element and unlinked found element */
			found_sink_pad = gst_element_get_compatible_pad(found_element, prev_elem_src_pad, NULL);
			if (found_sink_pad)
				elements_linked = gst_element_link_pads_filtered(previous_element, GST_PAD_NAME(prev_elem_src_pad), found_element, GST_PAD_NAME(found_sink_pad), NULL);
		} else if (__ms_pad_get_peer_element(prev_elem_src_pad) == found_element) {
			elements_linked = TRUE;
		}
	} else {

		/* Check if previous element has any unlinked src pad */
		GstIterator *src_pad_iterator = gst_element_iterate_src_pads(previous_element);
		while (GST_ITERATOR_OK == gst_iterator_next(src_pad_iterator, &src_pad_value)) {
			GstPad *src_pad = (GstPad *) g_value_get_object(&src_pad_value);
			if (!gst_pad_is_linked(src_pad)) {

				/* Check compatibility of previous element and unlinked found element */
				found_sink_pad = gst_element_get_compatible_pad(found_element, src_pad, NULL);
				if (found_sink_pad) {
					elements_linked = gst_element_link_pads_filtered(previous_element, GST_PAD_NAME(src_pad), found_element, GST_PAD_NAME(found_sink_pad), NULL);
					if (elements_linked)
						break;
				}
			} else if (__ms_pad_get_peer_element(src_pad) == found_element) {
				elements_linked = TRUE;
			}
			g_value_reset(&src_pad_value);
		}
		g_value_unset(&src_pad_value);
		gst_iterator_free(src_pad_iterator);
	}

	if (found_sink_pad) {
		if (elements_linked)
			ms_info("Succeeded to link [%s] -> [%s]", GST_ELEMENT_NAME(previous_element), GST_ELEMENT_NAME(found_element));
		else
			ms_debug("Failed to link [%s] and [%s]", GST_ELEMENT_NAME(previous_element), GST_ELEMENT_NAME(found_element));
	} else {
		ms_info("Element [%s] has no free pad to be connected with [%s] or linked", GST_ELEMENT_NAME(found_element), GST_ELEMENT_NAME(previous_element));
	}

	return elements_linked;
}

static GstElement *__ms_bin_find_element_by_type(GstElement *previous_element, GstPad *prev_elem_src_pad, GstElement *search_bin, node_info_s *node_klass_type)
{
	GValue element_value = G_VALUE_INIT;
	GstElement *found_element = NULL;
	gboolean elements_linked = FALSE;

	GstIterator *bin_iterator = gst_bin_iterate_sorted(GST_BIN(search_bin));
	while (GST_ITERATOR_OK == gst_iterator_next(bin_iterator, &element_value)) {
		found_element = (GstElement *) g_value_get_object(&element_value);
		const gchar *found_klass = gst_element_factory_get_klass(gst_element_get_factory(found_element));

		/* Check if found element is of appropriate needed plugin class */
		if (g_strrstr(found_klass, node_klass_type->klass_name) || g_strrstr(GST_ELEMENT_NAME(found_element), node_klass_type->default_name)) {
			ms_info("Found element by type [%s]", GST_ELEMENT_NAME(found_element));
			if (__ms_link_two_elements(previous_element, prev_elem_src_pad, found_element)) {
				elements_linked = TRUE;
				break;
			}
		}
		g_value_reset(&element_value);
	}
	g_value_unset(&element_value);
	gst_iterator_free(bin_iterator);

	return elements_linked ? found_element : NULL;
}

int __ms_factory_rank_compare(GstPluginFeature * first_feature, GstPluginFeature * second_feature)
{
	return (gst_plugin_feature_get_rank(second_feature) - gst_plugin_feature_get_rank(first_feature));
}

gboolean __ms_feature_filter(GstPluginFeature * feature, gpointer data)
{
	if (!GST_IS_ELEMENT_FACTORY(feature))
		return FALSE;
	return TRUE;
}

GstElement *__ms_create_element_by_registry(GstPad * src_pad, const gchar * klass_name)
{
	const GList *pads;
	GstElement *next_element = NULL;

	GstCaps *new_pad_caps = gst_pad_query_caps(src_pad, NULL);

	GList *factories = gst_registry_feature_filter(gst_registry_get(),
												   (GstPluginFeatureFilter) __ms_feature_filter, FALSE, NULL);
	factories = g_list_sort(factories, (GCompareFunc) __ms_factory_rank_compare);
	GList *factories_iter = factories;

	for (; factories_iter != NULL; factories_iter = factories_iter->next) {
		GstElementFactory *factory = GST_ELEMENT_FACTORY(factories_iter->data);

		if (g_strrstr(gst_element_factory_get_klass(GST_ELEMENT_FACTORY(factory)), klass_name)) {
			for (pads = gst_element_factory_get_static_pad_templates(factory); pads != NULL; pads = pads->next) {

				GstCaps *intersect_caps = NULL;
				GstCaps *static_caps = NULL;
				GstStaticPadTemplate *pad_temp = pads->data;

				if (pad_temp->presence != GST_PAD_ALWAYS || pad_temp->direction != GST_PAD_SINK)
					continue;

				if (GST_IS_CAPS(&pad_temp->static_caps.caps))
					static_caps = gst_caps_ref(pad_temp->static_caps.caps);
				else
					static_caps = gst_caps_from_string(pad_temp->static_caps.string);

				intersect_caps = gst_caps_intersect_full(new_pad_caps, static_caps, GST_CAPS_INTERSECT_FIRST);

				if (!gst_caps_is_empty(intersect_caps)) {
					if (!next_element)
						next_element = __ms_element_create(GST_OBJECT_NAME(factory), NULL);
				}
				gst_caps_unref(intersect_caps);
				gst_caps_unref(static_caps);
			}
		}
	}
	gst_caps_unref(new_pad_caps);
	gst_plugin_feature_list_free(factories);
	return next_element;
}

GstElement *__ms_combine_next_element(GstElement *previous_element, GstPad *prev_elem_src_pad, GstElement *bin_to_find_in, media_streamer_node_type_e node_type)
{
	if (!previous_element)
		return NULL;

	GstCaps *prev_caps = NULL;
	GstElement *found_element = NULL;
	node_info_s *node_klass_type = __ms_node_get_klass_by_its_type(node_type);

	/* - 1 - If previous element is linked  - check for peer element */
	found_element = __ms_find_peer_element_by_type(GST_ELEMENT(previous_element), prev_elem_src_pad, node_klass_type);

	/* - 2 - If previous element is not linked  - find in bin by type */
	if (!found_element)
		found_element = __ms_bin_find_element_by_type(previous_element, prev_elem_src_pad, bin_to_find_in, node_klass_type);

	/* - 3 - If element by type is not found in bin - create element by type */
	if (!found_element) {

		/* Create Node type information for a New element */
		GstPad *src_pad = NULL;
		if (prev_elem_src_pad)
			prev_caps = gst_pad_query_caps(prev_elem_src_pad, 0);
		else {
			src_pad = gst_element_get_static_pad(previous_element, "src");
			if (src_pad)
				prev_caps = gst_pad_query_caps(src_pad, 0);
		}
		node_plug_s plug_info = {node_klass_type, NULL, prev_caps, NULL};

		/* Create Node by ini or registry */
		found_element = __ms_node_element_create(&plug_info, node_type);
		if (found_element) {
			ms_info("New Element [%s] is created ", GST_ELEMENT_NAME(found_element));

			/* Add created element */
			if (__ms_bin_add_element(bin_to_find_in, found_element, FALSE)) {
				gst_element_sync_state_with_parent(found_element);
				__ms_link_two_elements(previous_element, prev_elem_src_pad, found_element);
				__ms_generate_dots(bin_to_find_in, GST_ELEMENT_NAME(found_element));
			} else {
				ms_error("Element [%s] was not added into [%s] bin", GST_ELEMENT_NAME(found_element), GST_ELEMENT_NAME(bin_to_find_in));
				MS_SAFE_UNREF(found_element);
				found_element = NULL;
			}
		} else
			ms_info("New Element is not created ");
	} else
		ms_info("Next element is not combined");

	return found_element;
}

static gint __decodebin_autoplug_select_cb(GstElement * bin, GstPad * pad, GstCaps * caps, GstElementFactory * factory, gpointer data)
{
	/* NOTE : GstAutoplugSelectResult is defined in gstplay-enum.h but not exposed */
	typedef enum {
		GST_AUTOPLUG_SELECT_TRY,
		GST_AUTOPLUG_SELECT_EXPOSE,
		GST_AUTOPLUG_SELECT_SKIP
	} GstAutoplugSelectResult;

	media_streamer_s *ms_streamer = (media_streamer_s *) data;
	ms_retvm_if(!ms_streamer, GST_AUTOPLUG_SELECT_TRY, "Handle is NULL");

	gchar *factory_name = NULL;
	const gchar *klass = NULL;
	GstAutoplugSelectResult result = GST_AUTOPLUG_SELECT_TRY;

	factory_name = GST_OBJECT_NAME(factory);
	klass = gst_element_factory_get_metadata(factory, GST_ELEMENT_METADATA_KLASS);

	ms_debug("Decodebin: found new element [%s] to link [%s]", factory_name, klass);

	if (ms_streamer->ini.exclude_elem_names) {
		/* Search if such plugin must be excluded */

		int index = 0;
		for ( ; ms_streamer->ini.exclude_elem_names[index]; ++index) {
			if (g_strrstr(factory_name, ms_streamer->ini.exclude_elem_names[index])) {
				ms_debug("Decodebin: skipping [%s] as excluded", factory_name);
				return GST_AUTOPLUG_SELECT_SKIP;
			}
		}
	}

	return result;
}

static gint __pad_type_compare(gconstpointer a, gconstpointer b)
{
	GstPad *a_pad = GST_PAD(a);
	GstPad *b_pad = GST_PAD(b);

	const gchar *a_pad_type = __ms_get_pad_type(a_pad);
	const gchar *b_pad_type = __ms_get_pad_type(b_pad);

	if (MS_ELEMENT_IS_TEXT(a_pad_type))
		return -1;
	else if (MS_ELEMENT_IS_TEXT(b_pad_type))
		return 1;
	else
		return 0;
}

static void __decodebin_newpad_cb(GstElement * decodebin, GstPad * new_pad, gpointer user_data)
{
	media_streamer_s *ms_streamer = (media_streamer_s *) user_data;
	ms_retm_if(ms_streamer == NULL, "Handle is NULL");

	g_mutex_lock(&ms_streamer->mutex_lock);

	g_object_ref(new_pad);
	ms_streamer->pads_types_list = g_list_insert_sorted(ms_streamer->pads_types_list, new_pad, __pad_type_compare);

	g_mutex_unlock(&ms_streamer->mutex_lock);
}

static void __decodebin_nomore_pads_combine(GstPad *src_pad, media_streamer_s *ms_streamer, gboolean is_server_part)
{
	GstElement *found_element = NULL;
	GstElement *parent_element = gst_pad_get_parent_element(src_pad);
	const gchar *new_pad_type = __ms_get_pad_type(src_pad);

	if (MS_ELEMENT_IS_VIDEO(new_pad_type)) {
		found_element = __ms_bin_find_element_by_type(parent_element, src_pad, ms_streamer->topology_bin, __ms_node_get_klass_by_its_type(MEDIA_STREAMER_NODE_TYPE_TEXT_OVERLAY));
		if (found_element) {
			ms_info(" Overlay Element [%s]", GST_ELEMENT_NAME(found_element));
			src_pad = NULL;
		} else {
			found_element = parent_element;
		}
		if (is_server_part) {
			found_element = __ms_combine_next_element(found_element, src_pad, ms_streamer->topology_bin, MEDIA_STREAMER_NODE_TYPE_VIDEO_ENCODER);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_NODE_TYPE_VIDEO_PAY);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_NODE_TYPE_RTP);
		} else {
			found_element = __ms_combine_next_element(found_element, src_pad, ms_streamer->topology_bin, MEDIA_STREAMER_NODE_TYPE_VIDEO_CONVERTER);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->sink_bin, MEDIA_STREAMER_NODE_TYPE_SINK);
		}
	} else if (MS_ELEMENT_IS_AUDIO(new_pad_type)) {
		if (is_server_part) {
			found_element = __ms_combine_next_element(parent_element, src_pad, ms_streamer->topology_bin, MEDIA_STREAMER_NODE_TYPE_AUDIO_ENCODER);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_NODE_TYPE_AUDIO_PAY);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_NODE_TYPE_RTP);
		} else {
			found_element = __ms_combine_next_element(parent_element, src_pad, ms_streamer->sink_bin, MEDIA_STREAMER_NODE_TYPE_SINK);
		}
	} else if (MS_ELEMENT_IS_TEXT(new_pad_type)) {
		found_element = __ms_combine_next_element(parent_element, src_pad, ms_streamer->topology_bin, MEDIA_STREAMER_NODE_TYPE_TEXT_OVERLAY);
	} else {
		ms_error("Unsupported pad type [%s]!", new_pad_type);
		return;
	}
	__ms_generate_dots(ms_streamer->pipeline, "after_sink_linked");
}

static void __decodebin_nomore_pads_cb(GstElement *decodebin, gpointer user_data)
{
	media_streamer_s *ms_streamer = (media_streamer_s *) user_data;
	ms_retm_if(ms_streamer == NULL, "Handle is NULL");

	g_mutex_lock(&ms_streamer->mutex_lock);

	gboolean is_server_part = FALSE;
	media_streamer_node_s *rtp_node = (media_streamer_node_s *)g_hash_table_lookup(ms_streamer->nodes_table, "rtp_container");

	/* It`s server part of Streaming Scenario*/
	if (rtp_node) {
		char *param_value = NULL;
		media_streamer_node_get_param(rtp_node, MEDIA_STREAMER_PARAM_VIDEO_OUT_PORT, &param_value);
		if (param_value && (strtol(param_value, NULL, 10) > 0))
			is_server_part = TRUE;

		MS_SAFE_FREE(param_value);
	}

	GList *iterator = NULL;
	GList *list = ms_streamer->pads_types_list;
	for (iterator = list; iterator; iterator = iterator->next) {
		GstPad *src_pad = GST_PAD(iterator->data);
		__decodebin_nomore_pads_combine(src_pad, ms_streamer, is_server_part);
	}

	g_mutex_unlock(&ms_streamer->mutex_lock);
}

GstElement *__ms_decodebin_create(media_streamer_s * ms_streamer)
{
	ms_retvm_if(!ms_streamer, NULL, "Handle is NULL");

	GstElement *decodebin = __ms_element_create(DEFAULT_DECODEBIN, NULL);
	__ms_bin_add_element(ms_streamer->topology_bin, decodebin, FALSE);
	gst_element_sync_state_with_parent(decodebin);

	__ms_signal_create(&ms_streamer->autoplug_sig_list, decodebin, "pad-added", G_CALLBACK(__decodebin_newpad_cb), ms_streamer);
	__ms_signal_create(&ms_streamer->autoplug_sig_list, decodebin, "autoplug-select", G_CALLBACK(__decodebin_autoplug_select_cb), ms_streamer);
	__ms_signal_create(&ms_streamer->autoplug_sig_list, decodebin, "no-more-pads", G_CALLBACK(__decodebin_nomore_pads_cb), ms_streamer);

	return decodebin;
}

static gboolean __ms_sink_bin_prepare(media_streamer_s * ms_streamer, GstPad * source_pad, const gchar * src_pad_type)
{
	GstElement *found_element = NULL;
	GstElement *previous_element = NULL;

	/* Getting Depayloader */
	GstElement *parent_rtp_element = gst_pad_get_parent_element(source_pad);

	if (MS_ELEMENT_IS_VIDEO(src_pad_type)) {
		previous_element = __ms_combine_next_element(parent_rtp_element, source_pad, ms_streamer->topology_bin, MEDIA_STREAMER_NODE_TYPE_VIDEO_DEPAY);
		found_element = __ms_combine_next_element(previous_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_NODE_TYPE_VIDEO_DECODER);

		if (!found_element) {
			if (ms_streamer->ini.use_decodebin) {
				found_element = __ms_decodebin_create(ms_streamer);
				__ms_link_two_elements(previous_element, NULL, found_element);
				return TRUE;
			} else {
				previous_element = __ms_combine_next_element(previous_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_NODE_TYPE_VIDEO_DECODER);
				previous_element = __ms_combine_next_element(previous_element, NULL, ms_streamer->sink_bin, MEDIA_STREAMER_NODE_TYPE_SINK);
			}
		} else {
			__ms_link_two_elements(previous_element, NULL, found_element);
			previous_element = __ms_combine_next_element(found_element, NULL, ms_streamer->sink_bin, MEDIA_STREAMER_NODE_TYPE_SINK);
		}
	} else if (MS_ELEMENT_IS_AUDIO(src_pad_type)) {
		previous_element = __ms_combine_next_element(parent_rtp_element, source_pad, ms_streamer->topology_bin, MEDIA_STREAMER_NODE_TYPE_AUDIO_DEPAY);
		previous_element = __ms_combine_next_element(previous_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_NODE_TYPE_AUDIO_DECODER);
		previous_element = __ms_combine_next_element(previous_element, NULL, ms_streamer->sink_bin, MEDIA_STREAMER_NODE_TYPE_SINK);
	} else {
		ms_info("Unknown media type received from rtp element!");
	}
	MS_SAFE_UNREF(parent_rtp_element);

	return TRUE;
}

static void __ms_rtpbin_pad_added_cb(GstElement * src, GstPad * new_pad, gpointer user_data)
{
	ms_debug("Pad [%s] added on [%s]", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));

	media_streamer_node_s *ms_node = (media_streamer_node_s *) user_data;
	ms_retm_if(ms_node == NULL, "Handle is NULL");

	if (g_str_has_prefix(GST_PAD_NAME(new_pad), "recv_rtp_src")) {
		media_streamer_s *ms_streamer = ms_node->parent_streamer;
		ms_retm_if(ms_streamer == NULL, "Node's parent streamer handle is NULL");
		g_mutex_lock(&ms_streamer->mutex_lock);

		GstPad *target_pad = gst_ghost_pad_get_target(GST_GHOST_PAD(new_pad));
		GstCaps *src_pad_caps = gst_pad_query_caps(target_pad, NULL);

		gchar *source_pad_name = NULL;
		GstStructure *src_pad_struct = NULL;
		src_pad_struct = gst_caps_get_structure(src_pad_caps, 0);

		const gchar *src_pad_type = gst_structure_get_string(src_pad_struct, "media");
		ms_debug("type is [%s]", src_pad_type);
		if (MS_ELEMENT_IS_VIDEO(src_pad_type))
			source_pad_name = g_strdup_printf("%s_out", "video");
		else if (MS_ELEMENT_IS_AUDIO(src_pad_type))
			source_pad_name = g_strdup_printf("%s_out", "audio");

		if (source_pad_name != NULL) {

			GstPad *source_pad = gst_element_get_static_pad(ms_node->gst_element, source_pad_name);
			gst_ghost_pad_set_target(GST_GHOST_PAD(source_pad), new_pad);

			if (__ms_sink_bin_prepare(ms_streamer, source_pad, src_pad_type)) {
				__ms_element_set_state(ms_node->gst_element, GST_STATE_PLAYING);
				__ms_generate_dots(ms_streamer->pipeline, "rtpbin_playing");
			} else {
				ms_error("Failed to prepare sink_bin for pad type [%s]", src_pad_type);
			}

			MS_SAFE_GFREE(source_pad_name);
		}
		g_mutex_unlock(&ms_node->parent_streamer->mutex_lock);

		gst_caps_unref(src_pad_caps);
		MS_SAFE_UNREF(target_pad);
	}
}

int __ms_element_set_state(GstElement * gst_element, GstState gst_state)
{
	ms_retvm_if(gst_element == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	GstStateChangeReturn ret_state;

	ret_state = gst_element_set_state(gst_element, gst_state);
	if (ret_state == GST_STATE_CHANGE_FAILURE) {
		ms_error("Failed to set element [%s] into %s state", GST_ELEMENT_NAME(gst_element), gst_element_state_get_name(gst_state));
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	return MEDIA_STREAMER_ERROR_NONE;
}

GstElement *__ms_element_create(const char *plugin_name, const char *name)
{
	GstElement *plugin_elem = NULL;
	ms_retvm_if(plugin_name == NULL, (GstElement *) NULL, "Error empty plugin name");
	ms_info("Creating [%s] element", plugin_name);
	plugin_elem = gst_element_factory_make(plugin_name, name);
	ms_retvm_if(plugin_elem == NULL, (GstElement *) NULL, "Error creating element [%s]", plugin_name);
	return plugin_elem;
}

static gboolean __ms_feature_node_filter(GstPluginFeature *feature, gpointer data)
{
	node_plug_s *plug_info = (node_plug_s*)data;

	if (!GST_IS_ELEMENT_FACTORY(feature))
		return FALSE;

	gboolean can_accept = FALSE;
	gboolean src_can_accept = FALSE;
	gboolean sink_can_accept = FALSE;
	GstElementFactory *factory = GST_ELEMENT_FACTORY(feature);
	const gchar *factory_klass = gst_element_factory_get_klass(factory);

	if (plug_info && g_strrstr(factory_klass, plug_info->info->klass_name)) {

		if (GST_IS_CAPS(plug_info->src_caps))
			src_can_accept = gst_element_factory_can_src_any_caps(factory, plug_info->src_caps);

		if (GST_IS_CAPS(plug_info->sink_caps))
			sink_can_accept = gst_element_factory_can_sink_any_caps(factory, plug_info->sink_caps);

		if (GST_IS_CAPS(plug_info->src_caps) && GST_IS_CAPS(plug_info->sink_caps)) {
			if (src_can_accept && sink_can_accept)
				can_accept = TRUE;
		} else if (src_can_accept || sink_can_accept)
			can_accept = TRUE;

		if (can_accept) {
			int index = 0;
			for ( ; plug_info->exclude_names && plug_info->exclude_names[index]; ++index) {
				if (g_strrstr(GST_OBJECT_NAME(factory), plug_info->exclude_names[index])) {
					ms_debug("Skipping compatible factory [%s] as excluded.", GST_OBJECT_NAME(factory));
					return FALSE;
				}
			}

			ms_info("[INFO] Found compatible factory [%s] for klass [%s]",
				GST_OBJECT_NAME(factory), factory_klass);
			return TRUE;
		}

	}

	return FALSE;
}

static GstElement *__ms_element_create_from_ini(node_plug_s *plug_info, media_streamer_node_type_e type)
{
	const gchar *src_type, *sink_type;
	const gchar *format_type;

	MS_GET_CAPS_TYPE(plug_info->src_caps, src_type);
	MS_GET_CAPS_TYPE(plug_info->sink_caps, sink_type);

	GstElement *gst_element = NULL;
	ms_info("Specified node formats types: in[%s] - out[%s]", sink_type, src_type);
	gchar conf_key[INI_MAX_STRLEN] = {0,};

	if (type == MEDIA_STREAMER_NODE_TYPE_VIDEO_ENCODER) {
		format_type = src_type;
		if (!format_type)
			MS_GET_CAPS_TYPE(gst_caps_from_string(MEDIA_STREAMER_DEFAULT_VIDEO_FORMAT), format_type);
	} else if (type == MEDIA_STREAMER_NODE_TYPE_AUDIO_ENCODER) {
		format_type = src_type;
		if (!format_type)
			MS_GET_CAPS_TYPE(gst_caps_from_string(MEDIA_STREAMER_DEFAULT_AUDIO_FORMAT), format_type);
	} else if (type == MEDIA_STREAMER_NODE_TYPE_VIDEO_DECODER) {
		format_type = sink_type;
		if (!format_type)
			MS_GET_CAPS_TYPE(gst_caps_from_string(MEDIA_STREAMER_DEFAULT_VIDEO_FORMAT), format_type);
	} else if (type == MEDIA_STREAMER_NODE_TYPE_AUDIO_DECODER) {
		format_type = sink_type;
		if (!format_type)
			MS_GET_CAPS_TYPE(gst_caps_from_string(MEDIA_STREAMER_DEFAULT_AUDIO_FORMAT), format_type);
	} else
		format_type = sink_type ? sink_type : src_type;

	if (snprintf(conf_key, INI_MAX_STRLEN, "node type %d:%s", type, format_type) >= INI_MAX_STRLEN) {
		ms_error("Failed to generate config field name, size >= %d", INI_MAX_STRLEN);
		return NULL;
	}
	gchar *plugin_name = __ms_ini_get_string(conf_key, NULL);

	if (plugin_name) {
		gst_element = __ms_element_create(plugin_name, NULL);
		MS_SAFE_GFREE(plugin_name);
	}
	return gst_element;
}

static GstElement *__ms_element_create_by_registry(node_plug_s *plug_info, media_streamer_node_type_e type)
{
	GstElement *gst_element = NULL;
	const gchar *src_type, *sink_type;
	MS_GET_CAPS_TYPE(plug_info->src_caps, src_type);
	MS_GET_CAPS_TYPE(plug_info->sink_caps, sink_type);

	__ms_ini_read_list("general:exclude elements", &plug_info->exclude_names);

	GList *factories = gst_registry_feature_filter(gst_registry_get(),
				__ms_feature_node_filter, FALSE, plug_info);
	factories = g_list_sort(factories, (GCompareFunc) __ms_factory_rank_compare);

	if (factories) {
		GstElementFactory *factory = GST_ELEMENT_FACTORY(factories->data);
		gst_element = __ms_element_create(GST_OBJECT_NAME(factory), NULL);
	} else {
		ms_debug("Could not find any compatible element for node [%d]: in[%s] - out[%s]",
				type, sink_type, src_type);
	}

	g_strfreev(plug_info->exclude_names);
	gst_plugin_list_free(factories);

	return gst_element;
}

GstElement *__ms_node_element_create(node_plug_s *plug_info, media_streamer_node_type_e type)
{
	GstElement *gst_element = NULL;

	const gchar *src_type, *sink_type;
	MS_GET_CAPS_TYPE(plug_info->src_caps, src_type);
	MS_GET_CAPS_TYPE(plug_info->sink_caps, sink_type);

	/* 1. Main priority:
	 * If Node klass defined as MEDIA_STREAMER_STRICT or ENCODER/DECODER types,
	 * element will be created immediately by format ot name */
	if (type == MEDIA_STREAMER_NODE_TYPE_AUDIO_ENCODER)
		gst_element = __ms_audio_encoder_element_create(plug_info, type);
	else if (type == MEDIA_STREAMER_NODE_TYPE_VIDEO_ENCODER)
		gst_element = __ms_video_encoder_element_create(plug_info, type);
	else if (type == MEDIA_STREAMER_NODE_TYPE_VIDEO_DECODER)
		gst_element = __ms_video_decoder_element_create(plug_info, type);
	else if (g_strrstr(MEDIA_STREAMER_STRICT, plug_info->info->klass_name) || (!src_type && !sink_type)) {
		if (type == MEDIA_STREAMER_NODE_TYPE_RTP)
			gst_element = __ms_rtp_element_create();
		else
			gst_element = __ms_element_create(plug_info->info->default_name, NULL);
	} else {

		/* 2. Second priority:
		 * Try to get plugin name that defined in ini file
		 * according with node type and specified format. */
		gst_element = __ms_element_create_from_ini(plug_info, type);
	}

	/* 3. Third priority:
	 * If previous cases did not create a valid gst_element,
	 * try to find compatible plugin in gstreamer registry.
	 * Elements that are compatible but defined as excluded will be skipped*/
	if (!gst_element) {
		/* Read exclude elements list */
		gst_element = __ms_element_create_by_registry(plug_info, type);
	}

	return gst_element;
}

GstElement *__ms_video_encoder_element_create(node_plug_s *plug_info, media_streamer_node_type_e type)
{
	GstCaps *enc_caps = plug_info->src_caps;
	if (!enc_caps) {
		enc_caps = gst_caps_from_string(MEDIA_STREAMER_DEFAULT_VIDEO_FORMAT);
		ms_debug("No Video encoding format is set! Deafault will be: [%s]", MEDIA_STREAMER_DEFAULT_VIDEO_FORMAT);
	}

	/* Creating Scaler, Converter */
	GstElement *video_scale = __ms_element_create(DEFAULT_VIDEO_SCALE, NULL);
	GstElement *video_convert = __ms_element_create(DEFAULT_VIDEO_CONVERT, NULL);

	/* Creating Video Encoder */
	node_info_s *node_klass_type = __ms_node_get_klass_by_its_type(type);
	node_plug_s encoder_info = {node_klass_type, enc_caps, plug_info->sink_caps, NULL};
	GstElement *encoder_elem = __ms_element_create_from_ini(&encoder_info, type);
	if (!encoder_elem)
		encoder_elem = __ms_element_create_by_registry(&encoder_info, type);

	/* Creating Video Parser */
	node_info_s node_info = {MEDIA_STREAMER_PARSER_KLASS, DEFAULT_VIDEO_PARSER};
	node_plug_s parser_info = {&node_info, enc_caps, enc_caps, NULL};
	GstElement *encoder_parser = __ms_element_create_from_ini(&parser_info, MEDIA_STREAMER_NODE_TYPE_PARSER);
	if (!encoder_parser)
		encoder_parser = __ms_element_create_by_registry(&parser_info, MEDIA_STREAMER_NODE_TYPE_PARSER);

	/* Creating bin - Video Encoder */
	gboolean gst_ret = FALSE;
	GstElement *encoder_bin = gst_bin_new("video_encoder");
	ms_retvm_if(!video_convert || !video_scale || !encoder_elem || !encoder_bin || !encoder_parser, (GstElement *) NULL, "Error: creating elements for video encoder bin");

	/* Settings if H264 format is set*/
	const gchar *src_type;
	MS_GET_CAPS_TYPE(enc_caps, src_type);
	media_format_mimetype_e encoder_type = __ms_convert_string_format_to_media_format(src_type);
	if (encoder_type == MEDIA_FORMAT_H264_SP) {
		g_object_set(GST_OBJECT(encoder_parser), "config-interval", H264_PARSER_CONFIG_INTERVAL, NULL);
		g_object_set(G_OBJECT(encoder_elem), "tune",  H264_ENCODER_ZEROLATENCY, NULL);
		g_object_set(G_OBJECT(encoder_elem), "byte-stream", TRUE, NULL);
		g_object_set(G_OBJECT(encoder_elem), "bitrate", 3000, NULL);
		g_object_set(G_OBJECT(encoder_elem), "threads", 2, NULL);
	}

	/* Adding elements to bin Video Encoder */
	gst_bin_add_many(GST_BIN(encoder_bin), video_convert, video_scale, encoder_elem, encoder_parser, NULL);
	gst_ret = gst_element_link_many(video_convert, video_scale, encoder_elem, encoder_parser, NULL);
	if (gst_ret != TRUE) {
		ms_error("Failed to link elements into encoder_bin");
		MS_SAFE_UNREF(encoder_bin);
	}

	__ms_add_ghostpad(encoder_parser, "src", encoder_bin, "src");
	__ms_add_ghostpad(video_convert, "sink", encoder_bin, "sink");

	return encoder_bin;
}

GstElement *__ms_video_decoder_element_create(node_plug_s *plug_info, media_streamer_node_type_e type)
{
	GstCaps *dec_caps = plug_info->sink_caps;
	if (!dec_caps) {
		dec_caps = gst_caps_from_string(MEDIA_STREAMER_DEFAULT_VIDEO_FORMAT);
		ms_debug("No Video decoding format is set! Deafault will be: [%s]", MEDIA_STREAMER_DEFAULT_VIDEO_FORMAT);
	}

	gboolean is_hw_codec = FALSE;
	GstElement *last_elem = NULL;

	/* Creating Video Decoder */
	node_info_s *node_klass_type = __ms_node_get_klass_by_its_type(type);
	node_plug_s decoder_info = {node_klass_type, plug_info->src_caps, dec_caps, NULL};
	GstElement *decoder_elem = __ms_element_create_from_ini(&decoder_info, MEDIA_STREAMER_NODE_TYPE_VIDEO_DECODER);
	if (!decoder_elem)
		decoder_elem = __ms_element_create_by_registry(&decoder_info, MEDIA_STREAMER_NODE_TYPE_VIDEO_DECODER);

	/* Creating Video Parser */
	node_info_s nodes_info = {MEDIA_STREAMER_PARSER_KLASS, DEFAULT_VIDEO_PARSER};
	node_plug_s parser_info = {&nodes_info, dec_caps, dec_caps, NULL};
	GstElement *decoder_parser = __ms_element_create_from_ini(&parser_info, MEDIA_STREAMER_NODE_TYPE_PARSER);
	if (!decoder_parser)
		decoder_parser = __ms_element_create_by_registry(&parser_info, MEDIA_STREAMER_NODE_TYPE_PARSER);

	/* Settings if H264 format is set*/
	const gchar *sink_type;
	MS_GET_CAPS_TYPE(dec_caps, sink_type);
	media_format_mimetype_e decoder_type = __ms_convert_string_format_to_media_format(sink_type);
	if (decoder_type == MEDIA_FORMAT_H264_SP)
		g_object_set(G_OBJECT(decoder_parser), "config-interval", H264_PARSER_CONFIG_INTERVAL, NULL);

	if (g_strrstr(plug_info->info->default_name, "omx") || g_strrstr(plug_info->info->default_name, "sprd"))
		is_hw_codec = TRUE;

	/* Creating bin - Video Decoder */
	gboolean gst_ret = FALSE;
	GstElement *decoder_bin = gst_bin_new("video_decoder");
	GstElement *decoder_queue = __ms_element_create("queue", NULL);
	ms_retvm_if(!decoder_elem || !decoder_queue || !decoder_bin || !decoder_parser, (GstElement *) NULL, "Error: creating elements for video decoder bin");

	/* Adding elements to bin Audio Encoder */
	gst_bin_add_many(GST_BIN(decoder_bin), decoder_queue, decoder_elem, decoder_parser, NULL);
	gst_ret = gst_element_link_many(decoder_queue, decoder_parser, decoder_elem, NULL);
	if (gst_ret != TRUE) {
		ms_error("Failed to link elements into decoder_bin");
		MS_SAFE_UNREF(decoder_bin);
		return NULL;
	}
	last_elem = decoder_elem;

	if (!is_hw_codec) {
		GstElement *video_conv = __ms_element_create("videoconvert", NULL);
		GstElement *video_scale = __ms_element_create("videoscale", NULL);
		ms_retvm_if(!video_conv || !video_scale, (GstElement *) NULL, "Error: creating elements for video decoder bin");
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

GstElement *__ms_audio_encoder_element_create(node_plug_s *plug_info, media_streamer_node_type_e type)
{
	GstCaps *enc_caps = plug_info->src_caps;
	if (!enc_caps) {
		enc_caps = gst_caps_from_string(MEDIA_STREAMER_DEFAULT_AUDIO_FORMAT);
		ms_debug("No Audio encoding format is set! Deafault will be: [%s]", MEDIA_STREAMER_DEFAULT_AUDIO_FORMAT);
	}

	gboolean gst_ret = FALSE;

	/* Creating Converter, Resampler, Filter */
	GstElement *audio_convert = __ms_element_create(DEFAULT_AUDIO_CONVERT, NULL);
	GstElement *audio_resample = __ms_element_create(DEFAULT_AUDIO_RESAMPLE, NULL);
	GstElement *audio_filter = __ms_element_create(DEFAULT_FILTER, NULL);
	GstElement *audio_postenc_convert = __ms_element_create(DEFAULT_AUDIO_CONVERT, NULL);

	/* Creating Audio Encoder */
	node_info_s *node_klass_type = __ms_node_get_klass_by_its_type(type);
	node_plug_s plug_info_encoder = {node_klass_type, enc_caps, plug_info->sink_caps, NULL};
	GstElement *audio_encoder = __ms_element_create_from_ini(&plug_info_encoder, type);
	if (!audio_encoder)
		audio_encoder = __ms_element_create_by_registry(&plug_info_encoder, type);

	/* Creating bin - Audio Encoder */
	GstElement *audio_enc_bin = gst_bin_new("audio_encoder");
	ms_retvm_if(!audio_convert || !audio_postenc_convert || !audio_filter || !audio_encoder || !audio_enc_bin, (GstElement *) NULL, "Error: creating elements for encoder bin");

	GstCaps *audioCaps = gst_caps_from_string(MEDIA_STREAMER_DEFAULT_AUDIO_RAW_FORMAT);
	g_object_set(G_OBJECT(audio_filter), "caps", audioCaps, NULL);
	gst_caps_unref(audioCaps);

	/* Adding elements to bin Audio Encoder */
	gst_bin_add_many(GST_BIN(audio_enc_bin), audio_convert, audio_postenc_convert, audio_filter, audio_resample, audio_encoder, NULL);
	gst_ret = gst_element_link_many(audio_convert, audio_resample, audio_filter, audio_postenc_convert, audio_encoder, NULL);
	if (gst_ret != TRUE) {
		ms_error("Failed to link elements into decoder_bin");
		MS_SAFE_UNREF(audio_enc_bin);
	}

	__ms_add_ghostpad(audio_encoder, "src", audio_enc_bin, "src");
	__ms_add_ghostpad(audio_convert, "sink", audio_enc_bin, "sink");

	return audio_enc_bin;
}

GstElement *__ms_rtp_element_create(void)
{
	GstElement *rtp_container = gst_bin_new("rtp_container");
	ms_retvm_if(!rtp_container, (GstElement *) NULL, "Error: creating elements for rtp container");

	__ms_add_no_target_ghostpad(rtp_container, MS_RTP_PAD_VIDEO_OUT, GST_PAD_SRC);
	__ms_add_no_target_ghostpad(rtp_container, MS_RTP_PAD_AUDIO_OUT, GST_PAD_SRC);
	__ms_add_no_target_ghostpad(rtp_container, MS_RTP_PAD_VIDEO_IN, GST_PAD_SINK);
	__ms_add_no_target_ghostpad(rtp_container, MS_RTP_PAD_AUDIO_IN, GST_PAD_SINK);

	__ms_add_no_target_ghostpad(rtp_container, MS_RTP_PAD_VIDEO_IN"_rtp", GST_PAD_SINK);
	__ms_add_no_target_ghostpad(rtp_container, MS_RTP_PAD_AUDIO_IN"_rtp", GST_PAD_SINK);

	/* Add RTP node parameters as GObject data with destroy function */
	MS_SET_INT_RTP_PARAM(rtp_container, MEDIA_STREAMER_PARAM_VIDEO_IN_PORT, RTP_STREAM_DISABLED);
	MS_SET_INT_RTP_PARAM(rtp_container, MEDIA_STREAMER_PARAM_AUDIO_IN_PORT, RTP_STREAM_DISABLED);
	MS_SET_INT_RTP_PARAM(rtp_container, MEDIA_STREAMER_PARAM_VIDEO_OUT_PORT, RTP_STREAM_DISABLED);
	MS_SET_INT_RTP_PARAM(rtp_container, MEDIA_STREAMER_PARAM_AUDIO_OUT_PORT, RTP_STREAM_DISABLED);
	MS_SET_INT_STATIC_STRING_PARAM(rtp_container, MEDIA_STREAMER_PARAM_HOST, "localhost");
	MS_SET_INT_CAPS_PARAM(rtp_container, MEDIA_STREAMER_PARAM_VIDEO_IN_FORMAT, gst_caps_new_any());
	MS_SET_INT_CAPS_PARAM(rtp_container, MEDIA_STREAMER_PARAM_AUDIO_IN_FORMAT, gst_caps_new_any());

	return rtp_container;
}

gboolean __ms_rtp_element_prepare(media_streamer_node_s *ms_node)
{
	ms_retvm_if(!ms_node, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Handle is NULL");

	GstElement *rtpbin = __ms_element_create("rtpbin", "rtpbin");
	ms_retvm_if(!rtpbin, FALSE, "Error: creating elements for rtp container");

	if (!__ms_bin_add_element(ms_node->gst_element, rtpbin, FALSE)) {
		MS_SAFE_UNREF(rtpbin);
		return FALSE;
	}
	__ms_signal_create(&ms_node->sig_list, rtpbin, "pad-added", G_CALLBACK(__ms_rtpbin_pad_added_cb), ms_node);

	gboolean ret = TRUE;
	GstElement *rtp_el = NULL;
	GstElement *rtcp_el = NULL;

	GValue *val = (GValue *)g_object_get_data(G_OBJECT(ms_node->gst_element), MEDIA_STREAMER_PARAM_HOST);
	const gchar *host = g_value_get_string(val);

	val = (GValue *)g_object_get_data(G_OBJECT(ms_node->gst_element), MEDIA_STREAMER_PARAM_VIDEO_IN_PORT);
	if (g_value_get_int(val) > RTP_STREAM_DISABLED) {
		rtp_el = __ms_element_create("udpsrc", MS_RTP_PAD_VIDEO_IN"_rtp");
		__ms_bin_add_element(ms_node->gst_element, rtp_el, FALSE);
		rtcp_el = __ms_element_create("udpsrc", MS_RTP_PAD_VIDEO_IN"_rctp");
		__ms_bin_add_element(ms_node->gst_element, rtcp_el, FALSE);

		ret = ret && gst_element_link_pads(rtp_el, "src", rtpbin, "recv_rtp_sink_0");
		ret = ret && gst_element_link_pads(rtcp_el, "src", rtpbin, "recv_rtcp_sink_0");

		g_object_set_property(G_OBJECT(rtp_el), MEDIA_STREAMER_PARAM_PORT, val);
		g_object_set(G_OBJECT(rtcp_el), MEDIA_STREAMER_PARAM_PORT, (g_value_get_int(val) + 1), NULL);

		val = (GValue *)g_object_get_data(G_OBJECT(ms_node->gst_element), MEDIA_STREAMER_PARAM_VIDEO_IN_FORMAT);
		g_object_set_property(G_OBJECT(rtp_el), "caps", val);
	}

	val = (GValue *)g_object_get_data(G_OBJECT(ms_node->gst_element), MEDIA_STREAMER_PARAM_AUDIO_IN_PORT);
	if (g_value_get_int(val) > RTP_STREAM_DISABLED) {
		rtp_el = __ms_element_create("udpsrc", MS_RTP_PAD_AUDIO_IN"_rtp");
		__ms_bin_add_element(ms_node->gst_element, rtp_el, FALSE);
		rtcp_el = __ms_element_create("udpsrc", MS_RTP_PAD_AUDIO_IN"_rctp");
		__ms_bin_add_element(ms_node->gst_element, rtcp_el, FALSE);

		ret = ret && gst_element_link_pads(rtp_el, "src", rtpbin, "recv_rtp_sink_1");
		ret = ret && gst_element_link_pads(rtcp_el, "src", rtpbin, "recv_rtcp_sink_1");

		g_object_set_property(G_OBJECT(rtp_el), MEDIA_STREAMER_PARAM_PORT, val);
		g_object_set(G_OBJECT(rtcp_el), MEDIA_STREAMER_PARAM_PORT, (g_value_get_int(val) + 1), NULL);

		val = (GValue *)g_object_get_data(G_OBJECT(ms_node->gst_element), MEDIA_STREAMER_PARAM_AUDIO_IN_FORMAT);
		g_object_set_property(G_OBJECT(rtp_el), "caps", val);
	}

	val = (GValue *)g_object_get_data(G_OBJECT(ms_node->gst_element), MEDIA_STREAMER_PARAM_VIDEO_OUT_PORT);
	if (g_value_get_int(val) > RTP_STREAM_DISABLED) {
		rtp_el = __ms_element_create("udpsink", MS_RTP_PAD_VIDEO_OUT"_rtp");
		__ms_bin_add_element(ms_node->gst_element, rtp_el, FALSE);
		rtcp_el = __ms_element_create("udpsink", MS_RTP_PAD_VIDEO_OUT"_rctp");
		__ms_bin_add_element(ms_node->gst_element, rtcp_el, FALSE);

		GstElement *video_filter = __ms_element_create("capsfilter", NULL);
		__ms_bin_add_element(ms_node->gst_element, video_filter, FALSE);
		GstCaps *video_caps = gst_caps_from_string(MEDIA_STREAMER_DEFAULT_VIDEO_RTP_FORMAT);
		g_object_set(G_OBJECT(video_filter), "caps", video_caps, NULL);
		gst_element_link_pads(video_filter, "src", rtpbin, "send_rtp_sink_0");

		GstGhostPad *ghost_pad = (GstGhostPad *)gst_element_get_static_pad(ms_node->gst_element, MS_RTP_PAD_VIDEO_IN);
		if (ghost_pad) {
			if (gst_ghost_pad_set_target(ghost_pad, gst_element_get_static_pad(video_filter, "sink")))
				ms_info(" Capsfilter for [%s] in RTP is set and linked", MS_RTP_PAD_VIDEO_IN);
		}

		ret = ret && gst_element_link_pads(rtpbin, "send_rtp_src_0", rtp_el, "sink");
		ret = ret && gst_element_link_pads(rtpbin, "send_rtcp_src_0", rtcp_el, "sink");

		g_object_set_property(G_OBJECT(rtp_el), MEDIA_STREAMER_PARAM_PORT, val);
		g_object_set(GST_OBJECT(rtp_el), MEDIA_STREAMER_PARAM_HOST, host, NULL);
		g_object_set(G_OBJECT(rtcp_el), MEDIA_STREAMER_PARAM_PORT, (g_value_get_int(val) + 1), NULL);
		g_object_set(GST_OBJECT(rtcp_el), "sync", FALSE, NULL);
		g_object_set(GST_OBJECT(rtcp_el), "async", FALSE, NULL);
		g_object_set(GST_OBJECT(rtcp_el), MEDIA_STREAMER_PARAM_HOST, host, NULL);
	}

	val = (GValue *)g_object_get_data(G_OBJECT(ms_node->gst_element), MEDIA_STREAMER_PARAM_AUDIO_OUT_PORT);
	if (g_value_get_int(val) > RTP_STREAM_DISABLED) {
		rtp_el = __ms_element_create("udpsink", MS_RTP_PAD_AUDIO_OUT"_rtp");
		__ms_bin_add_element(ms_node->gst_element, rtp_el, FALSE);
		rtcp_el = __ms_element_create("udpsink", MS_RTP_PAD_AUDIO_OUT"_rctp");
		__ms_bin_add_element(ms_node->gst_element, rtcp_el, FALSE);

		GstElement *audio_filter = __ms_element_create("capsfilter", NULL);
		__ms_bin_add_element(ms_node->gst_element, audio_filter, FALSE);
		GstCaps *audio_caps = gst_caps_from_string(MEDIA_STREAMER_DEFAULT_AUDIO_RTP_FORMAT);
		g_object_set(G_OBJECT(audio_filter), "caps", audio_caps, NULL);
		gst_element_link_pads(audio_filter, "src", rtpbin, "send_rtp_sink_1");

		GstGhostPad *ghost_pad = (GstGhostPad *)gst_element_get_static_pad(ms_node->gst_element, MS_RTP_PAD_AUDIO_IN);
		if (ghost_pad) {
			if (gst_ghost_pad_set_target(ghost_pad, gst_element_get_static_pad(audio_filter, "sink")))
				ms_info(" Capsfilter for [%s] in RTP is set and linked", MS_RTP_PAD_AUDIO_IN);
		}



		ret = ret && gst_element_link_pads(rtpbin, "send_rtp_src_1", rtp_el, "sink");
		ret = ret && gst_element_link_pads(rtpbin, "send_rtcp_src_1", rtcp_el, "sink");

		g_object_set_property(G_OBJECT(rtp_el), MEDIA_STREAMER_PARAM_PORT, val);
		g_object_set(GST_OBJECT(rtp_el), MEDIA_STREAMER_PARAM_HOST, host, NULL);
		g_object_set(G_OBJECT(rtcp_el), MEDIA_STREAMER_PARAM_PORT, (g_value_get_int(val) + 1), NULL);
		g_object_set(GST_OBJECT(rtcp_el), "sync", FALSE, NULL);
		g_object_set(GST_OBJECT(rtcp_el), "async", FALSE, NULL);
		g_object_set(GST_OBJECT(rtcp_el), MEDIA_STREAMER_PARAM_HOST, host, NULL);
	}

	__ms_generate_dots(ms_node->gst_element, "rtp_prepared");
	return ret;
}

int __ms_add_node_into_bin(media_streamer_s *ms_streamer, media_streamer_node_s *ms_node)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Handle is NULL");
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Handle is NULL");

	ms_info("Try to add [%s] node into streamer, node type/subtype [%d/%d]", ms_node->name, ms_node->type, ms_node->subtype);

	GstElement *bin = NULL;

	switch (ms_node->type) {
	case MEDIA_STREAMER_NODE_TYPE_SRC:
		bin = ms_streamer->src_bin;
		break;
	case MEDIA_STREAMER_NODE_TYPE_SINK:
		bin = ms_streamer->sink_bin;
		break;
	default:
		/* Another elements will be add into topology bin */
		bin = ms_streamer->topology_bin;
		break;
	}

	if (!__ms_bin_add_element(bin, ms_node->gst_element, TRUE)) {
		ms_error("Failed to add Element [%s] into [%s] bin.", ms_node->name, GST_ELEMENT_NAME(bin));
		ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	return ret;
}

static gboolean __ms_parse_gst_error(media_streamer_s *ms_streamer, GstMessage *message, GError *error)
{
	ms_retvm_if(!ms_streamer, FALSE, "Error: invalid Media Streamer handle.");
	ms_retvm_if(!error, FALSE, "Error: invalid error handle.");
	ms_retvm_if(!message, FALSE, "Error: invalid bus message handle.");

	media_streamer_error_e ret_error = MEDIA_STREAMER_ERROR_NONE;
	if (error->domain == GST_CORE_ERROR)
		ret_error = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	else if (error->domain == GST_LIBRARY_ERROR)
		ret_error = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	else if (error->domain == GST_RESOURCE_ERROR)
		ret_error = MEDIA_STREAMER_ERROR_RESOURCE_CONFLICT;
	else if (error->domain == GST_STREAM_ERROR)
		ret_error = MEDIA_STREAMER_ERROR_CONNECTION_FAILED;
	else
		ret_error = MEDIA_STREAMER_ERROR_INVALID_OPERATION;

	/* post error to application */
	if (ms_streamer->error_cb.callback) {
		media_streamer_error_cb error_cb = (media_streamer_error_cb) ms_streamer->error_cb.callback;
		error_cb((media_streamer_h) ms_streamer, ret_error, ms_streamer->error_cb.user_data);
	}

	return TRUE;
}

static GstPadProbeReturn __ms_element_event_probe(GstPad * pad, GstPadProbeInfo *info, gpointer user_data)
{
	GstElement *parent_element = gst_pad_get_parent_element(pad);
	GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
	if (GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM) {
		if (GST_EVENT_TYPE(event) == GST_EVENT_BUFFERSIZE) {
			GValue *val_ = (GValue *) g_object_get_data(G_OBJECT(parent_element), "pad_sink");
			ms_info("Set locking probe [%d] on [%s] pad of [%s] element", g_value_get_int(val_), GST_PAD_NAME(pad), GST_ELEMENT_NAME(parent_element));
			MS_SAFE_UNREF(parent_element);
			return GST_PAD_PROBE_OK;
		}
	}
	MS_SAFE_UNREF(parent_element);
	return GST_PAD_PROBE_PASS;
}

void __ms_element_lock_state(const GValue *item, gpointer user_data)
{
	GstElement *sink_element = GST_ELEMENT(g_value_get_object(item));
	ms_retm_if(!sink_element, "Handle is NULL");

	GstPad *sink_pad = gst_element_get_static_pad(sink_element, "sink");
	if (!gst_pad_is_blocked(sink_pad)) {
		int probe_id = gst_pad_add_probe(sink_pad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM, __ms_element_event_probe, NULL, NULL);

		GValue *val = g_malloc0(sizeof(GValue));
		g_value_init(val, G_TYPE_INT);
		g_value_set_int(val, probe_id);
		g_object_set_data(G_OBJECT(sink_element), "pad_sink", (gpointer)val);

		val = (GValue *) g_object_get_data(G_OBJECT(sink_element), "pad_sink");
		if (val)
			ms_info("Added locking probe [%d] on pad [%s] of element [%s]", g_value_get_int(val), GST_PAD_NAME(sink_pad), GST_ELEMENT_NAME(sink_element));
	} else {
		ms_info("Pad [%s] of element [%s] is already locked", GST_PAD_NAME(sink_pad), GST_ELEMENT_NAME(sink_element));
	}
	MS_SAFE_UNREF(sink_pad);
}

void __ms_element_unlock_state(const GValue *item, gpointer user_data)
{
	GstElement *sink_element = GST_ELEMENT(g_value_get_object(item));
	ms_retm_if(!sink_element, "Handle is NULL");

	GValue *val = (GValue *) g_object_get_data(G_OBJECT(sink_element), "pad_sink");
	if (val) {
		GstPad *sink_pad = gst_element_get_static_pad(sink_element, "sink");
		if (gst_pad_is_blocked(sink_pad)) {
			ms_info("Removing locking probe [%d] ID on [%s] pad of [%s] element", g_value_get_int(val), GST_PAD_NAME(sink_pad), GST_ELEMENT_NAME(sink_element));
			gst_pad_remove_probe(sink_pad, g_value_get_int(val));
		} else {
			ms_info("No locking Probe on pad [%s] of element [%s]", GST_PAD_NAME(sink_pad), GST_ELEMENT_NAME(sink_element));
		}
	}
}

static gboolean __ms_bus_cb(GstBus *bus, GstMessage *message, gpointer userdata)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;
	media_streamer_s *ms_streamer = (media_streamer_s *) userdata;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(ms_streamer->pipeline == NULL, MEDIA_STREAMER_ERROR_INVALID_STATE, "Pipeline doesn`t exist");

	/* Parse message */
	if (message != NULL) {
		switch (GST_MESSAGE_TYPE(message)) {
		case GST_MESSAGE_ERROR:{
				GError *err = NULL;
				gchar *debug = NULL;
				gst_message_parse_error(message, &err, &debug);

				/* Transform gst error code to media streamer error code.
				 * then post it to application if needed */
				__ms_parse_gst_error(ms_streamer, message, err);

				ms_error("[Source: %s] Error: %s", GST_OBJECT_NAME(GST_OBJECT_CAST(GST_ELEMENT(GST_MESSAGE_SRC(message)))), err->message);

				g_error_free(err);
				MS_SAFE_FREE(debug);
				break;
			}

		case GST_MESSAGE_STATE_CHANGED:{
				if (GST_MESSAGE_SRC(message) == GST_OBJECT(ms_streamer->pipeline)) {
					GstState state_old, state_new, state_pending;
					gchar *state_transition_name;

					gst_message_parse_state_changed(message, &state_old, &state_new, &state_pending);
					state_transition_name = g_strdup_printf("Old_[%s]_New_[%s]_Pending_[%s]", gst_element_state_get_name(state_old),
											gst_element_state_get_name(state_new), gst_element_state_get_name(state_pending));
					ms_info("GST_MESSAGE_STATE_CHANGED: [%s] %s. ", GST_OBJECT_NAME(GST_MESSAGE_SRC(message)), state_transition_name);
					__ms_generate_dots(ms_streamer->pipeline, state_transition_name);
					MS_SAFE_GFREE(state_transition_name);

					media_streamer_state_e old_state = ms_streamer->state;
					if (state_new >= GST_STATE_PAUSED)
					{
						if ((old_state == MEDIA_STREAMER_STATE_PLAYING) && (state_new <= GST_STATE_PAUSED))
							ms_streamer->pend_state = MEDIA_STREAMER_STATE_PAUSED;

						if (ms_streamer->pend_state != ms_streamer->state) {

							g_mutex_lock(&ms_streamer->mutex_lock);
							ms_streamer->state = ms_streamer->pend_state;
							g_mutex_unlock(&ms_streamer->mutex_lock);

							ms_info("Media streamer state changed to [%d] [%d]", old_state, ms_streamer->state);
							if (ms_streamer->state_changed_cb.callback) {
								media_streamer_state_changed_cb cb = (media_streamer_state_changed_cb) ms_streamer->state_changed_cb.callback;
								cb((media_streamer_h) ms_streamer, old_state, ms_streamer->state, ms_streamer->state_changed_cb.user_data);
							}
						}
					}
				}
				break;
			}

		case GST_MESSAGE_ASYNC_DONE:{
				if (GST_MESSAGE_SRC(message) == GST_OBJECT(ms_streamer->pipeline)
					&& ms_streamer->is_seeking) {

					g_mutex_lock(&ms_streamer->mutex_lock);
					ms_streamer->pend_state = MEDIA_STREAMER_STATE_SEEKING;
					g_mutex_unlock(&ms_streamer->mutex_lock);

					if (ms_streamer->seek_done_cb.callback) {
						media_streamer_position_changed_cb cb = (media_streamer_position_changed_cb) ms_streamer->seek_done_cb.callback;
						cb(ms_streamer->seek_done_cb.user_data);
					}

					g_mutex_lock(&ms_streamer->mutex_lock);
					ms_streamer->is_seeking = FALSE;
					ms_streamer->pend_state = MEDIA_STREAMER_STATE_PLAYING;
					ms_streamer->seek_done_cb.callback = NULL;
					ms_streamer->seek_done_cb.user_data = NULL;
					g_mutex_unlock(&ms_streamer->mutex_lock);
				}
				break;
			}
		case GST_MESSAGE_EOS:{
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

	GError *err = NULL;
	int ret = MEDIA_STREAMER_ERROR_NONE;

	int *argc = (int *)malloc(sizeof(int));
	char **argv = NULL;
	if (argc) {
		*argc = 1;
		if (ms_streamer->ini.gst_args)
			(*argc) += g_strv_length(ms_streamer->ini.gst_args);

		argv = (char **)calloc(*argc, sizeof(char*));
		if (argv) {
			argv[0] = g_strdup("MediaStreamer");

			if (ms_streamer->ini.gst_args) {
				int i = 0;
				for ( ; ms_streamer->ini.gst_args[i]; ++i) {
					argv[i+1] = ms_streamer->ini.gst_args[i];
					ms_debug("Add [%s] gstreamer parameter.", argv[i+1]);
				}
			}

		} else {
			MS_SAFE_FREE(argc);
			ms_error("Error allocation memory");
		}
	} else {
		ms_error("Error allocation memory");
	}

	gboolean gst_ret = gst_init_check(argc, &argv, &err);
	/* Clean memory of gstreamer arguments*/
	g_strfreev(ms_streamer->ini.gst_args);
	ms_streamer->ini.gst_args = NULL;
	MS_SAFE_FREE(argv[0]);
	MS_SAFE_FREE(argv);
	MS_SAFE_FREE(argc);

	if (!gst_ret) {
		ms_error("Error: Failed to initialize GStreamer [%s].", err->message);
		g_clear_error(&err);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	ms_streamer->pipeline = gst_pipeline_new(MEDIA_STREAMER_PIPELINE_NAME);
	ms_retvm_if(ms_streamer->pipeline == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error creating pipeline");

	ms_streamer->bus = gst_pipeline_get_bus(GST_PIPELINE(ms_streamer->pipeline));
	ms_retvm_if(ms_streamer->bus == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error getting the bus of the pipeline");

	ms_streamer->bus_watcher = gst_bus_add_watch(ms_streamer->bus, (GstBusFunc) __ms_bus_cb, ms_streamer);

	ms_streamer->src_bin = gst_bin_new(MEDIA_STREAMER_SRC_BIN_NAME);
	ms_retvm_if(ms_streamer->src_bin == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error creating Src bin");

	ms_streamer->sink_bin = gst_bin_new(MEDIA_STREAMER_SINK_BIN_NAME);
	ms_retvm_if(ms_streamer->sink_bin == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error creating Sink bin");

	ms_streamer->topology_bin = gst_bin_new(MEDIA_STREAMER_TOPOLOGY_BIN_NAME);
	ms_retvm_if(ms_streamer->topology_bin == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error creating Topology bin");

	gst_bin_add_many(GST_BIN(ms_streamer->pipeline), ms_streamer->src_bin, ms_streamer->sink_bin, ms_streamer->topology_bin, NULL);
	ms_info("Media streamer pipeline created successfully.");

	return ret;
}

GstCaps *__ms_create_caps_from_fmt(media_format_h fmt)
{
	GstCaps *caps = NULL;
	gchar *caps_name = NULL;
	media_format_mimetype_e mime;
	int width, height, avg_bps, max_bps, channel, samplerate, bit;

	if (!media_format_get_audio_info(fmt, &mime, &channel, &samplerate, &bit, &avg_bps)) {
		if (MEDIA_FORMAT_RAW == (mime & MEDIA_FORMAT_RAW))
			caps = gst_caps_new_simple("audio/x-raw", "channels", G_TYPE_INT, channel, "format",
								G_TYPE_STRING, __ms_convert_mime_to_string_format(mime), "rate", G_TYPE_INT, samplerate, NULL);
		else if (MEDIA_FORMAT_ENCODED == (mime & MEDIA_FORMAT_ENCODED))
			caps = gst_caps_new_simple(__ms_convert_mime_to_string_format(mime), "channels", G_TYPE_INT, channel, "rate", G_TYPE_INT, samplerate, NULL);
		caps_name = gst_caps_to_string(caps);
		ms_info("Creating Audio Caps from media format [%s]", caps_name);

	} else if (!media_format_get_video_info(fmt, &mime, &width, &height, &avg_bps, &max_bps)) {
		if (MEDIA_FORMAT_RAW == (mime & MEDIA_FORMAT_RAW))
			caps = gst_caps_new_simple("video/x-raw", "framerate", GST_TYPE_FRACTION, max_bps,
					avg_bps, "format", G_TYPE_STRING, __ms_convert_mime_to_string_format(mime), "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, NULL);
		else if (MEDIA_FORMAT_ENCODED == (mime & MEDIA_FORMAT_ENCODED))
			caps = gst_caps_new_simple(__ms_convert_mime_to_string_format(mime), "framerate", GST_TYPE_FRACTION, max_bps,
							avg_bps, "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, NULL);
		caps_name = gst_caps_to_string(caps);
		ms_info("Creating Video Caps from media format [%s]", caps_name);

	} else
		ms_error("Error getting media format information");

	MS_SAFE_GFREE(caps_name);
	return caps;
}

media_format_h __ms_create_fmt_from_caps(GstCaps *caps)
{
	media_format_h fmt;
	GstStructure *pad_struct;
	ms_retvm_if(caps == NULL, NULL, "Error: empty caps!");

	if (gst_caps_is_any(caps)) {
		ms_debug("Can not get format info from any caps!");
		return NULL;
	}

	int fmt_ret = MEDIA_FORMAT_ERROR_NONE;

	fmt_ret = media_format_create(&fmt);
	ms_retvm_if(fmt_ret != MEDIA_FORMAT_ERROR_NONE, NULL, "Error: while creating media format object, err[%d]!", fmt_ret);

	pad_struct = gst_caps_get_structure(caps, 0);
	const gchar *pad_type = gst_structure_get_name(pad_struct);
	const gchar *pad_format = pad_type;

	/* Got raw format type if needed */
	if (g_strrstr(pad_type, "/x-raw"))
		pad_format = gst_structure_get_string(pad_struct, "format");

	ms_debug("Pad type is [%s], format: [%s]", pad_type, pad_format);
	if (MS_ELEMENT_IS_VIDEO(pad_type)) {
		int width, height, avg_bps, max_bps;

		gst_structure_get_int(pad_struct, "width", &width);
		gst_structure_get_fraction(pad_struct, "framerate", &max_bps, &avg_bps);
		gst_structure_get_int(pad_struct, "height", &height);

		media_format_set_video_mime(fmt, __ms_convert_string_format_to_media_format(pad_format));
		media_format_set_video_width(fmt, width);
		media_format_set_video_height(fmt, height);
		media_format_set_video_avg_bps(fmt, avg_bps);
		media_format_set_video_max_bps(fmt, max_bps);
	} else if (MS_ELEMENT_IS_AUDIO(pad_type)) {
		int channels, bps;
		media_format_set_audio_mime(fmt, __ms_convert_string_format_to_media_format(pad_format));
		gst_structure_get_int(pad_struct, "channels", &channels);
		media_format_set_audio_channel(fmt, channels);
		gst_structure_get_int(pad_struct, "rate", &bps);
		media_format_set_audio_avg_bps(fmt, bps);
	}

	return fmt;
}

int __ms_element_pad_names(GstElement *gst_element, GstPadDirection pad_type, char ***pad_name_array, int *pads_count)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;

	int pad_number = 0;
	GValue elem = G_VALUE_INIT;
	GstPad *pad = NULL;
	char **pad_names = NULL;
	GstIterator *pad_iterator = NULL;

	if (pad_type == GST_PAD_SRC) {
		pad_iterator = gst_element_iterate_src_pads(gst_element);
	} else if (pad_type == GST_PAD_SINK) {
		pad_iterator = gst_element_iterate_sink_pads(gst_element);
	} else {
		pad_iterator = gst_element_iterate_pads(gst_element);
		ms_info("Iterating all pads of the Gst element");
	}

	while (GST_ITERATOR_OK == gst_iterator_next(pad_iterator, &elem)) {
		pad = (GstPad *) g_value_get_object(&elem);

		pad_names = (char **)realloc(pad_names, sizeof(char *) * (pad_number + 1));
		if (!pad_names) {
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

int __ms_element_get_pad_fmt(GstElement *gst_element, const char *pad_name, media_format_h *fmt)
{
	GstCaps *allowed_caps = NULL;
	GstCaps *format_caps = NULL;

	ms_retvm_if(gst_element == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Element handle is NULL");

	GstPad *pad = gst_element_get_static_pad(gst_element, pad_name);
	ms_retvm_if(pad == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Fail to get pad [%s] from element [%s].", pad_name, GST_ELEMENT_NAME(gst_element));

	GValue *value = (GValue *) g_object_get_data(G_OBJECT(gst_element), pad_name);
	if (value)
		format_caps = GST_CAPS(gst_value_get_caps(value));
	else
		ms_info(" No any format is set for pad [%s]", pad_name);

	int ret = MEDIA_STREAMER_ERROR_NONE;
	allowed_caps = gst_pad_get_allowed_caps(pad);
	if (allowed_caps) {
		if (gst_caps_is_empty(allowed_caps) || gst_caps_is_any(allowed_caps)) {
			if (format_caps)
				*fmt = __ms_create_fmt_from_caps(format_caps);
			else
				ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
		} else {
			*fmt = __ms_create_fmt_from_caps(allowed_caps);
		}
	} else {
		if (format_caps)
			*fmt = __ms_create_fmt_from_caps(format_caps);
		else
			ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	if (allowed_caps)
		gst_caps_unref(allowed_caps);

	MS_SAFE_UNREF(pad);
	return ret;
}

int __ms_element_set_fmt(media_streamer_node_s *node, const char *pad_name, media_format_h fmt)
{
	ms_retvm_if(!node || !pad_name || !fmt, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	gboolean can_accept = FALSE;

	GstCaps *fmt_caps = __ms_create_caps_from_fmt(fmt);
	ms_retvm_if(!fmt_caps, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Can't convert fmt into Caps");

	GstElementFactory *factory = gst_element_get_factory(node->gst_element);
	GstPad *node_pad = gst_element_get_static_pad(node->gst_element, pad_name);

	if (GST_PAD_IS_SRC(node_pad))
		can_accept  = gst_element_factory_can_src_any_caps(factory, fmt_caps);
	else if (GST_PAD_IS_SINK(node_pad))
		can_accept  = gst_element_factory_can_sink_any_caps(factory, fmt_caps);
	else
		ms_error(" Node [%s] doesn`t have valid pad [%s]", node->name, pad_name);

	if (!can_accept) {
		if (fmt_caps)
			gst_caps_unref(fmt_caps);
		ms_error("Node`s pad [%s] can`t be set with the given format", pad_name);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	} else {
		MS_SET_INT_CAPS_PARAM(node->gst_element, pad_name, fmt_caps);
		ms_info("Pad [%s] of node [%s] was set with given format", pad_name, node->name);
	}

	return MEDIA_STREAMER_ERROR_NONE;
}

gboolean __ms_gst_seek(GstElement *element, gint64 g_time, GstSeekFlags seek_flag)
{
	gboolean result = FALSE;

	ms_retvm_if(element == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Element is NULL");

	GstEvent *event = gst_event_new_seek(1.0, GST_FORMAT_TIME, seek_flag,
										 GST_SEEK_TYPE_SET, g_time,
										 GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
	if (event)
		result = gst_element_send_event(element, event);

	return result;
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

	if (buffer_data != NULL) {
		GstMapInfo buff_info = GST_MAP_INFO_INIT;
		guint64 pts = 0;
		guint64 duration = 0;
		guint64 size = 0;

		media_packet_get_buffer_size(packet, &size);

		buffer = gst_buffer_new_and_alloc(size);
		if (!buffer) {
			ms_error("Failed to allocate memory for push buffer");
			return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
		}

		if (gst_buffer_map(buffer, &buff_info, GST_MAP_READWRITE)) {
			memcpy(buff_info.data, buffer_data, size);
			buff_info.size = size;
			gst_buffer_unmap(buffer, &buff_info);
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

	if (gst_ret != GST_FLOW_OK)
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;

	return MEDIA_STREAMER_ERROR_NONE;
}

int __ms_element_pull_packet(GstElement *sink_element, media_packet_h *packet)
{
	GstSample *sample = NULL;
	media_format_h fmt = NULL;
	int ret = MEDIA_STREAMER_ERROR_NONE;

	ms_retvm_if(sink_element == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(packet == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	/* Retrieve the buffer */
	g_signal_emit_by_name(sink_element, "pull-sample", &sample, NULL);
	ms_retvm_if(sample == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Pull sample failed!");

	ret = __ms_element_get_pad_fmt(sink_element, "sink", &fmt);
	if (ret == MEDIA_STREAMER_ERROR_NONE) {
		GstMapInfo map;
		guint8 *buffer_res = NULL;
		GstBuffer *buffer = gst_sample_get_buffer(sample);
		gst_buffer_map(buffer, &map, GST_MAP_READ);

		buffer_res = (guint8 *) malloc(map.size * sizeof(guint8));
		if (buffer_res != NULL) {
			memcpy(buffer_res, map.data, map.size);

			media_packet_create_from_external_memory(fmt, (void *)buffer_res, map.size, NULL, NULL, packet);
			media_packet_set_pts(*packet, GST_BUFFER_PTS(buffer));
			media_packet_set_dts(*packet, GST_BUFFER_DTS(buffer));
			media_packet_set_pts(*packet, GST_BUFFER_DURATION(buffer));

			media_format_unref(fmt);
		} else {
			ms_error("Error allocation memory for packet data");
			ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
		}
		gst_buffer_unmap(buffer, &map);
	}

	gst_sample_unref(sample);
	return ret;
}
