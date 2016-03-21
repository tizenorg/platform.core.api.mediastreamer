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

static gboolean __ms_get_peer_element(GstPad *source_pad, GstElement *found_element)
{
	GstElement *peer_element = NULL;

	peer_element = __ms_pad_get_peer_element(source_pad);
	if (peer_element && (peer_element == found_element))
		/* Source pad of previous element has peer pad element */
		/* Previous element is connected to found element */
		return TRUE;
	else
		/* Source pad of previous element doesn`t have peer pad element */
		/* or is not connected to found element */
		return FALSE;
}

const gchar *__ms_get_pad_type(GstPad *element_pad)
{
	const gchar *element_pad_type = NULL;
	GstCaps *element_pad_caps = gst_pad_query_caps(element_pad, 0);
	if (gst_caps_get_size(element_pad_caps) > 0) {
		GstStructure *element_pad_struct = gst_caps_get_structure(element_pad_caps, 0);
		element_pad_type = gst_structure_get_name(element_pad_struct);
	} else {
		ms_debug("Caps size of element is 0");
	}
	gst_caps_unref(element_pad_caps);
	return element_pad_type;
}

static gboolean __ms_intersect_pads(GstPad *src_pad, GstPad *sink_pad)
{
	GstCaps *src_pad_caps = NULL;
	const gchar *src_pad_type = NULL;
	GstStructure *src_pad_struct = NULL;
	const gchar *src_pad_struct_string = NULL;

	GstCaps *sink_pad_caps = NULL;
	const gchar *sink_pad_type = NULL;
	GstStructure *sink_pad_struct = NULL;
	const gchar *sink_pad_struct_string = NULL;

	gboolean can_intersect = FALSE;
	gboolean intersect_res = FALSE;

	src_pad_caps = gst_pad_query_caps(src_pad, 0);
	if (gst_caps_get_size(src_pad_caps) > 0) {
		src_pad_struct = gst_caps_get_structure(src_pad_caps, 0);
		src_pad_type = gst_structure_get_name(src_pad_struct);
	}

	sink_pad_caps = gst_pad_query_caps(sink_pad, 0);
	if (gst_caps_get_size(sink_pad_caps) > 0) {
		sink_pad_struct = gst_caps_get_structure(sink_pad_caps, 0);
		sink_pad_type = gst_structure_get_name(sink_pad_struct);
	}

	if (src_pad_struct && sink_pad_struct) {
		src_pad_struct_string = gst_structure_get_string(src_pad_struct, "media");
		sink_pad_struct_string = gst_structure_get_string(sink_pad_struct, "media");
		can_intersect = gst_structure_can_intersect(src_pad_struct, sink_pad_struct);
		if (can_intersect || (src_pad_struct_string && (g_strrstr(src_pad_type, sink_pad_struct_string) ||
			g_strrstr(sink_pad_type, src_pad_struct_string) || g_strrstr(src_pad_struct_string, sink_pad_struct_string))) ||
			(!src_pad_struct_string && g_strrstr(src_pad_type, sink_pad_type)))
			intersect_res = TRUE;
		else
			intersect_res = FALSE;
	} else {
		intersect_res = FALSE;
	}

	gst_caps_unref(src_pad_caps);
	gst_caps_unref(sink_pad_caps);
	return intersect_res;
}

static gboolean __ms_check_unlinked_element(GstElement *previous_elem,
		GstPad *prev_elem_src_pad, GstPad *found_elem_sink_pad)
{
	gboolean intersect_res = FALSE;
	GstIterator *src_pad_iterator = NULL;
	GValue src_pad_value = G_VALUE_INIT;
	gboolean ret = FALSE;

	GstElement *found_element = gst_pad_get_parent_element(found_elem_sink_pad);
	ms_retvm_if(!found_element, FALSE, "Fail to get parent element");

	if (previous_elem) {
		/* Previous element is set. */
		if (prev_elem_src_pad) {
			/* Previous element`s source pad is set. */
			/* Compare if previous element`s source pad caps are compatible with found element`s */
			intersect_res = __ms_intersect_pads(prev_elem_src_pad, found_elem_sink_pad);
		} else {
			/* Previous element`s source pad is not set. */
			/* Compare if any of previous element`s source pads caps are compatible with found element`s */
			src_pad_iterator = gst_element_iterate_src_pads(previous_elem);
			while (GST_ITERATOR_OK == gst_iterator_next(src_pad_iterator, &src_pad_value)) {
				GstPad *src_pad = (GstPad *) g_value_get_object(&src_pad_value);
				if (!gst_pad_is_linked(src_pad)) {
					intersect_res = __ms_intersect_pads(src_pad, found_elem_sink_pad);
					if (intersect_res)
						break;
				}
				g_value_reset(&src_pad_value);
			}
			g_value_unset(&src_pad_value);
			gst_iterator_free(src_pad_iterator);
		}
		/* If previous element and found element are compatible, return found element */
		if (intersect_res) {
			ret = TRUE;
		} else {
			ms_info(" Element [%s] and element [%s] have incompatible pads to be linked", GST_ELEMENT_NAME(previous_elem), GST_ELEMENT_NAME(found_element));
			ret = FALSE;
		}
	} else {
		/* Previous element is not set. Nothing to compare with. Choose found element */
		ms_info("Previous element is not set. Next element will be [%s]", GST_ELEMENT_NAME(found_element));
		ret = TRUE;
	}

	MS_SAFE_UNREF(found_element);
	return ret;
}

static gboolean __ms_check_peer_element(GstElement *previous_elem, GstPad *prev_elem_src_pad, GstElement *found_element)
{
	gboolean peer_element_found = FALSE;
	GstIterator *src_pad_iterator = NULL;
	GValue src_pad_value = G_VALUE_INIT;
	gboolean ret = FALSE;

	if (previous_elem) {
		/* Previous element is set. */
		if (prev_elem_src_pad) {
			/* Previous element`s source pad is set. */
			/* Check if found element is connected with previous element */
			peer_element_found = __ms_get_peer_element(prev_elem_src_pad, found_element);
		} else {
			/* Previous element`s source pad is not set. */
			/* Check if any of previous element`s source pads is connected with found element */
			src_pad_iterator = gst_element_iterate_src_pads(previous_elem);
			while (GST_ITERATOR_OK == gst_iterator_next(src_pad_iterator, &src_pad_value)) {
				GstPad *src_pad = (GstPad *) g_value_get_object(&src_pad_value);
				peer_element_found = __ms_get_peer_element(src_pad, found_element);
				if (peer_element_found)
					break;
				g_value_reset(&src_pad_value);
			}
			g_value_unset(&src_pad_value);
			gst_iterator_free(src_pad_iterator);
		}
		/* If previous element and found element are already connected, return found element */
		if (peer_element_found) {
			ms_info("Elements [%s] and [%s] are already linked to each other!", GST_ELEMENT_NAME(previous_elem), GST_ELEMENT_NAME(found_element));
			ret = TRUE;
		} else {
			ms_info("Found element [%s] has already been linked with other element", GST_ELEMENT_NAME(found_element));
			ret = FALSE;
		}
	} else {
		/* Previous element is not set. */
		/* Previous element is not set. Nothing to check the connection with. Choose found element */
		ret = TRUE;
	}
	return ret;
}

GstElement *__ms_bin_find_element_by_klass(GstElement *sink_bin, GstElement *previous_elem, GstPad *source_pad, const gchar *klass_name, const gchar *bin_name)
{
	GValue element_value = G_VALUE_INIT;
	GstElement *found_element = NULL;

	GValue sink_pad_value = G_VALUE_INIT;
	gboolean element_found = FALSE;

	GstIterator *bin_iterator = gst_bin_iterate_sorted(GST_BIN(sink_bin));
	while (GST_ITERATOR_OK == gst_iterator_next(bin_iterator, &element_value)) {

		found_element = (GstElement *) g_value_get_object(&element_value);
		const gchar *found_klass = gst_element_factory_get_klass(gst_element_get_factory(found_element));
		ms_retvm_if(!found_element, NULL, "Fail to get element from element value");

		/* Check if found element is of appropriate needed plugin class */
		if ((klass_name && g_strrstr(found_klass, klass_name)) && (bin_name == NULL || g_strrstr(GST_ELEMENT_NAME(found_element), bin_name))) {

			/* Check if found element has any unlinked sink pad */
			GstIterator *sink_pad_iterator = gst_element_iterate_sink_pads(found_element);
			while (GST_ITERATOR_OK == gst_iterator_next(sink_pad_iterator, &sink_pad_value)) {
				GstPad *sink_pad = (GstPad *) g_value_get_object(&sink_pad_value);

				if (!gst_pad_is_linked(sink_pad)) {
					/* Check compatibility of previous element and unlinked found element */
					element_found = __ms_check_unlinked_element(previous_elem, source_pad, sink_pad);
					if (element_found)
						break;
				} else {
					/* Check if previous element linked to found element */
					element_found = __ms_check_peer_element(previous_elem, source_pad, found_element);
					if (element_found)
						break;
				}
				g_value_reset(&sink_pad_value);
			}
			g_value_unset(&sink_pad_value);
			gst_iterator_free(sink_pad_iterator);

			if (element_found && (bin_name == NULL || g_strrstr(GST_ELEMENT_NAME(found_element), bin_name)))
				break;
		}
		element_found = FALSE;
		g_value_reset(&element_value);
	}
	g_value_unset(&element_value);
	gst_iterator_free(bin_iterator);
	return element_found ? found_element : NULL;
}

int __ms_get_rank_increase(const char *factory_name)
{
	gint rank_priority = 20;
	gint rank_second = 10;
	gint rank_skip = -10;
	gint ret = 0;

	if (g_strrstr(factory_name, "av"))
		ret = rank_priority;
	else if (g_strrstr(factory_name, "omx"))
		ret = rank_second;
	else if (g_strrstr(factory_name, "v4l2video"))
		ret = rank_skip;
	else if (g_strrstr(factory_name, "rtph263ppay"))
		ret = rank_skip;
	else if (g_strrstr(factory_name, "sprd"))
		ret = rank_skip;

	return ret;
}

int __ms_factory_rank_compare(GstPluginFeature * first_feature, GstPluginFeature * second_feature)
{
	const gchar *name;
	int first_feature_rank_inc = 0, second_feature_rank_inc = 0;

	name = gst_plugin_feature_get_plugin_name(first_feature);
	first_feature_rank_inc = __ms_get_rank_increase(name);

	name = gst_plugin_feature_get_plugin_name(second_feature);
	second_feature_rank_inc = __ms_get_rank_increase(name);

	return (gst_plugin_feature_get_rank(second_feature) + second_feature_rank_inc) - (gst_plugin_feature_get_rank(first_feature) + first_feature_rank_inc);
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

GstElement *__ms_link_with_new_element(GstElement *previous_element, GstPad *prev_elem_src_pad, GstElement *new_element)
{
	const gchar *prev_elem_pad_type = NULL;
	GValue element_value = G_VALUE_INIT;

	gboolean ret = FALSE;
	GstPad *new_elem_sink_pad = NULL;
	gchar *sink_pad_name = NULL;
	gchar *src_pad_name = NULL;

	if (MS_ELEMENT_IS_RTP(GST_ELEMENT_NAME(new_element))) {
		GstPad *prev_elem_sink_pad = gst_element_get_static_pad(previous_element, "sink");
		prev_elem_pad_type = __ms_get_pad_type(prev_elem_sink_pad);
		if (MS_ELEMENT_IS_VIDEO(prev_elem_pad_type))
			sink_pad_name = g_strdup(MS_RTP_PAD_VIDEO_IN);
		else if (MS_ELEMENT_IS_AUDIO(prev_elem_pad_type))
			sink_pad_name = g_strdup(MS_RTP_PAD_AUDIO_IN);
		MS_SAFE_UNREF(prev_elem_sink_pad);
	}

	if (prev_elem_src_pad)
		src_pad_name = GST_PAD_NAME(prev_elem_src_pad);
	GstIterator *pad_iterator = gst_element_iterate_sink_pads(new_element);
	while (GST_ITERATOR_OK == gst_iterator_next(pad_iterator, &element_value)) {
		new_elem_sink_pad = (GstPad *) g_value_get_object(&element_value);
		if (!gst_pad_is_linked(new_elem_sink_pad)) {
			ret = gst_element_link_pads_filtered(previous_element, src_pad_name, new_element, sink_pad_name, NULL);
			break;
		} else {
			new_elem_sink_pad = NULL;
		}
		g_value_reset(&element_value);
	}
	g_value_unset(&element_value);
	gst_iterator_free(pad_iterator);

	if (new_elem_sink_pad) {
		if (ret)
			ms_info("Succeeded to link [%s] -> [%s]", GST_ELEMENT_NAME(previous_element), GST_ELEMENT_NAME(new_element));
		else
			ms_debug("Failed to link [%s] and [%s]", GST_ELEMENT_NAME(previous_element), GST_ELEMENT_NAME(new_element));
	} else {
		ms_info("Element [%s] has no free pad to be connected with [%s]", GST_ELEMENT_NAME(new_element), GST_ELEMENT_NAME(previous_element));
	}
	MS_SAFE_GFREE(sink_pad_name);
	return new_element;
}

GstElement *__ms_combine_next_element(GstElement * previous_element, GstPad * prev_elem_src_pad, GstElement * bin_to_find_in, const gchar * next_elem_klass_name, const gchar * next_elem_bin_name, gchar * default_element)
{
	GstElement *found_element = NULL;

	if (!previous_element)
		return NULL;

	/* Look for node created by user */
	if (next_elem_klass_name)
		found_element = __ms_bin_find_element_by_klass(bin_to_find_in, previous_element, prev_elem_src_pad, next_elem_klass_name, next_elem_bin_name);

	/* Link with found node created by user */
	if (found_element) {
		previous_element = __ms_link_with_new_element(previous_element, prev_elem_src_pad, found_element);
	} else {
		/* Create element by element name */
		if (!found_element && !next_elem_bin_name && default_element) {
			found_element = __ms_element_create(default_element, NULL);

			/* Create element by predefined format element type */
		} else if (!found_element && next_elem_bin_name && MS_ELEMENT_IS_ENCODER(next_elem_bin_name)) {

			dictionary *dict = NULL;

			__ms_load_ini_dictionary(&dict);

			if (MS_ELEMENT_IS_VIDEO(next_elem_bin_name))
				found_element = __ms_video_encoder_element_create(dict, MEDIA_FORMAT_H263);
			else
				found_element = __ms_audio_encoder_element_create();

			__ms_destroy_ini_dictionary(dict);

			/* Create element by caps of the previous element */
		} else if (!found_element) {
			GstPad *src_pad = NULL;
			if (!prev_elem_src_pad)
				src_pad = gst_element_get_static_pad(previous_element, "src");
			else
				src_pad = prev_elem_src_pad;
			found_element = __ms_create_element_by_registry(src_pad, next_elem_klass_name);
			MS_SAFE_UNREF(src_pad);
		}

		/* Add created element */
		if (found_element) {
			if (__ms_bin_add_element(bin_to_find_in, found_element, FALSE)) {
				gst_element_sync_state_with_parent(found_element);
				previous_element = __ms_link_with_new_element(previous_element, prev_elem_src_pad, found_element);
				__ms_generate_dots(bin_to_find_in, GST_ELEMENT_NAME(found_element));
			} else {
				ms_error("Element [%s] was not added into [%s] bin", GST_ELEMENT_NAME(found_element), GST_ELEMENT_NAME(bin_to_find_in));
				MS_SAFE_UNREF(found_element);
				found_element = NULL;
			}
		} else {
			ms_error("Could not find compatible element to link [%s]", GST_ELEMENT_NAME(previous_element));
		}
	}
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

static void _sink_node_unlock_state(const GValue * item, gpointer user_data)
{
	GstElement *sink_element = GST_ELEMENT(g_value_get_object(item));
	ms_retm_if(!sink_element, "Handle is NULL");

	if (gst_element_is_locked_state(sink_element)) {
		gst_element_set_locked_state(sink_element, FALSE);
		gst_element_sync_state_with_parent(sink_element);
	}
}
static void __decodebin_nomore_pads_streamer(GstElement * decodebin, media_streamer_s *ms_streamer)
{
	GstElement *found_element = NULL;
	GList *iterator = NULL;
	GList *list = ms_streamer->pads_types_list;
	const gchar *new_pad_type = NULL;

	for (iterator = list; iterator; iterator = iterator->next) {
		GstPad *new_pad = GST_PAD(iterator->data);
		new_pad_type = __ms_get_pad_type(new_pad);
		found_element = gst_pad_get_parent_element(new_pad);

		if (MS_ELEMENT_IS_VIDEO(new_pad_type)) {
			if(__ms_bin_find_element_by_klass(ms_streamer->topology_bin, NULL, NULL, MEDIA_STREAMER_OVERLAY_KLASS, NULL)) {
				found_element = __ms_combine_next_element(found_element, new_pad, ms_streamer->topology_bin, MEDIA_STREAMER_OVERLAY_KLASS, NULL, DEFAULT_TEXT_OVERLAY);
				new_pad = NULL;
			}
			found_element = __ms_combine_next_element(found_element, new_pad, ms_streamer->topology_bin, NULL, NULL, DEFAULT_QUEUE);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_BIN_KLASS, "video_encoder", NULL);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_PAYLOADER_KLASS, NULL, NULL);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_BIN_KLASS, "rtp_container", NULL);
		} else if (MS_ELEMENT_IS_AUDIO(new_pad_type)) {
			found_element = __ms_combine_next_element(found_element, new_pad, ms_streamer->topology_bin, NULL, NULL, DEFAULT_QUEUE);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_BIN_KLASS, "audio_encoder", NULL);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_PAYLOADER_KLASS, NULL, NULL);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_BIN_KLASS, "rtp_container", NULL);
		} else if (MS_ELEMENT_IS_TEXT(new_pad_type)) {
			found_element = __ms_combine_next_element(found_element, new_pad, ms_streamer->topology_bin, MEDIA_STREAMER_OVERLAY_KLASS, NULL, DEFAULT_TEXT_OVERLAY);
		} else {
			ms_error("Unsupported pad type [%s]!", new_pad_type);
			return;
		}
		__ms_generate_dots(ms_streamer->pipeline, "after_sink_linked");
	}
}

static void __decodebin_nomore_pads_client(GstElement * decodebin, media_streamer_s *ms_streamer)
{

	GstElement *found_element = NULL;
	GList *iterator = NULL;
	GList *list = ms_streamer->pads_types_list;
	const gchar *pad_type = NULL;

	for (iterator = list; iterator; iterator = iterator->next) {
		GstPad *src_pad = GST_PAD(iterator->data);
		pad_type = __ms_get_pad_type(src_pad);
		found_element = gst_pad_get_parent_element(src_pad);

		if (MS_ELEMENT_IS_AUDIO(pad_type)) {
			found_element = __ms_combine_next_element(found_element, src_pad, ms_streamer->topology_bin, MEDIA_STREAMER_CONVERTER_KLASS, NULL, DEFAULT_AUDIO_CONVERT);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->sink_bin, MEDIA_STREAMER_QUEUE_KLASS, NULL, DEFAULT_QUEUE);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->sink_bin, MEDIA_STREAMER_SINK_KLASS, NULL, NULL);
		} else if (MS_ELEMENT_IS_VIDEO(pad_type)) {
			if(__ms_bin_find_element_by_klass(ms_streamer->topology_bin, NULL, NULL, MEDIA_STREAMER_OVERLAY_KLASS, NULL)) {
				found_element = __ms_combine_next_element(found_element, src_pad, ms_streamer->topology_bin, MEDIA_STREAMER_OVERLAY_KLASS, NULL, DEFAULT_TEXT_OVERLAY);
				src_pad = NULL;
			}
			found_element = __ms_combine_next_element(found_element, src_pad, ms_streamer->topology_bin, MEDIA_STREAMER_CONVERTER_KLASS, NULL, DEFAULT_VIDEO_CONVERT);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->sink_bin, MEDIA_STREAMER_QUEUE_KLASS, NULL, DEFAULT_QUEUE);
			found_element = __ms_combine_next_element(found_element, NULL, ms_streamer->sink_bin, MEDIA_STREAMER_SINK_KLASS, NULL, NULL);
		} else if (MS_ELEMENT_IS_TEXT(pad_type)) {
			found_element = __ms_combine_next_element(found_element, src_pad, ms_streamer->topology_bin, MEDIA_STREAMER_OVERLAY_KLASS, NULL, DEFAULT_TEXT_OVERLAY);
		} else {
			ms_error("Unsupported pad type [%s]!", pad_type);
			return;
		}
		__ms_generate_dots(ms_streamer->pipeline, "after_sink_linked");
	}
	MS_BIN_FOREACH_ELEMENTS(ms_streamer->sink_bin, _sink_node_unlock_state, NULL);
}

static void __decodebin_nomore_pads_cb(GstElement * decodebin, gpointer user_data)
{
	media_streamer_s *ms_streamer = (media_streamer_s *) user_data;
	ms_retm_if(ms_streamer == NULL, "Handle is NULL");

	g_mutex_lock(&ms_streamer->mutex_lock);

	char *param_value = NULL;
	media_streamer_node_s *rtp_node = (media_streamer_node_s *)g_hash_table_lookup(ms_streamer->nodes_table, "rtp_container");

	if (rtp_node) {
		/* It`s Streaming Scenario*/
		media_streamer_node_get_param(rtp_node, MEDIA_STREAMER_PARAM_VIDEO_OUT_PORT, &param_value);
		if (param_value && (strtol(param_value, NULL, 10) > 0))

			/* It`s Streamer part of Streaming */
			__decodebin_nomore_pads_streamer(decodebin, ms_streamer);
		else
			/* It`s Client part of Streaming */
			__decodebin_nomore_pads_client(decodebin, ms_streamer);
	} else {

		/* It`s Client Scenario*/
		__decodebin_nomore_pads_client(decodebin, ms_streamer);
	}

	MS_SAFE_FREE(param_value);
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

	previous_element = __ms_combine_next_element(parent_rtp_element, source_pad, ms_streamer->topology_bin, MEDIA_STREAMER_DEPAYLOADER_KLASS, NULL, NULL);
	GstPad *src_pad = gst_element_get_static_pad(previous_element, "src");
	MS_SAFE_UNREF(parent_rtp_element);

	if (MS_ELEMENT_IS_VIDEO(src_pad_type)) {
		found_element = __ms_bin_find_element_by_klass(ms_streamer->topology_bin, previous_element, src_pad, MEDIA_STREAMER_BIN_KLASS, "video_decoder");
		if (!found_element) {
			if (ms_streamer->ini.use_decodebin) {
				found_element = __ms_decodebin_create(ms_streamer);
				found_element = __ms_link_with_new_element(previous_element, src_pad, found_element);
				return TRUE;
			} else {
				previous_element = __ms_combine_next_element(previous_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_PARSER_KLASS, NULL, NULL);
				previous_element = __ms_combine_next_element(previous_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_DECODER_KLASS, NULL, NULL);
				previous_element = __ms_combine_next_element(previous_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_QUEUE_KLASS, NULL, DEFAULT_QUEUE);
				previous_element = __ms_combine_next_element(previous_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_CONVERTER_KLASS, NULL, DEFAULT_VIDEO_CONVERT);
				previous_element = __ms_combine_next_element(previous_element, NULL, ms_streamer->sink_bin, MEDIA_STREAMER_QUEUE_KLASS, NULL, DEFAULT_QUEUE);
				previous_element = __ms_combine_next_element(previous_element, NULL, ms_streamer->sink_bin, MEDIA_STREAMER_SINK_KLASS, NULL, NULL);
			}
		} else {
			previous_element = __ms_link_with_new_element(previous_element, src_pad, found_element);
			previous_element = __ms_combine_next_element(previous_element, NULL, ms_streamer->sink_bin, MEDIA_STREAMER_QUEUE_KLASS, NULL, DEFAULT_QUEUE);
			previous_element = __ms_combine_next_element(previous_element, NULL, ms_streamer->sink_bin, MEDIA_STREAMER_SINK_KLASS, NULL, NULL);
		}
	} else if (MS_ELEMENT_IS_AUDIO(src_pad_type)) {
		previous_element = __ms_combine_next_element(previous_element, NULL, ms_streamer->topology_bin, MEDIA_STREAMER_DECODER_KLASS, NULL, NULL);
		previous_element = __ms_combine_next_element(previous_element, NULL, ms_streamer->sink_bin, MEDIA_STREAMER_QUEUE_KLASS, NULL, DEFAULT_QUEUE);
		previous_element = __ms_combine_next_element(previous_element, NULL, ms_streamer->sink_bin, MEDIA_STREAMER_SINK_KLASS, NULL, NULL);
	} else {
		ms_info("Unknown media type received from rtp element!");
	}
	MS_BIN_FOREACH_ELEMENTS(ms_streamer->sink_bin, _sink_node_unlock_state, NULL);

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
	gchar *element_name = gst_element_get_name(gst_element);

	ret_state = gst_element_set_state(gst_element, gst_state);
	if (ret_state == GST_STATE_CHANGE_FAILURE) {
		ms_error("Failed to set element [%s] into %s state", element_name, gst_element_state_get_name(gst_state));
		MS_SAFE_GFREE(element_name);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	MS_SAFE_GFREE(element_name);
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

GstElement *__ms_video_encoder_element_create(dictionary * dict, media_format_mimetype_e mime)
{
	char *plugin_name = NULL;
	char *format_prefix = NULL;

	GstElement *video_scale = __ms_element_create(DEFAULT_VIDEO_SCALE, NULL);
	GstElement *video_convert = __ms_element_create(DEFAULT_VIDEO_CONVERT, NULL);

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
	ms_retvm_if(!video_convert || !video_scale || !encoder_elem || !encoder_bin || !encoder_parser, (GstElement *) NULL, "Error: creating elements for video encoder bin");

	if (mime == MEDIA_FORMAT_H264_SP) {
		g_object_set(GST_OBJECT(encoder_parser), "config-interval", H264_PARSER_CONFIG_INTERVAL, NULL);
		g_object_set(G_OBJECT(encoder_elem), "tune",  H264_ENCODER_ZEROLATENCY, NULL);
		g_object_set(G_OBJECT(encoder_elem), "byte-stream", TRUE, NULL);
		g_object_set(G_OBJECT(encoder_elem), "bitrate", 3000, NULL);
		g_object_set(G_OBJECT(encoder_elem), "threads", 2, NULL);
	}

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

GstElement *__ms_video_decoder_element_create(dictionary * dict, media_format_mimetype_e mime)
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

	if (mime == MEDIA_FORMAT_H264_SP)
		g_object_set(G_OBJECT(decoder_parser), "config-interval", H264_PARSER_CONFIG_INTERVAL, NULL);

	if (g_strrstr(format_prefix, "omx"))
		is_omx = TRUE;

	MS_SAFE_FREE(format_prefix);
	MS_SAFE_FREE(plugin_name);

	gboolean gst_ret = FALSE;
	GstElement *decoder_bin = gst_bin_new("video_decoder");
	GstElement *decoder_queue = __ms_element_create("queue", NULL);
	ms_retvm_if(!decoder_elem || !decoder_queue || !decoder_bin || !decoder_parser, (GstElement *) NULL, "Error: creating elements for video decoder bin");

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

GstElement *__ms_audio_encoder_element_create(void)
{
	gboolean gst_ret = FALSE;
	GstElement *audio_convert = __ms_element_create("audioconvert", NULL);
	GstElement *audio_resample = __ms_element_create("audioresample", NULL);
	GstElement *audio_filter = __ms_element_create("capsfilter", NULL);
	GstElement *audio_postenc_convert = __ms_element_create("audioconvert", NULL);
	GstElement *audio_encoder = __ms_element_create(DEFAULT_AUDIO_ENCODER, NULL);
	GstElement *audio_enc_bin = gst_bin_new("audio_encoder");
	ms_retvm_if(!audio_convert || !audio_postenc_convert || !audio_filter || !audio_encoder || !audio_enc_bin, (GstElement *) NULL, "Error: creating elements for encoder bin");

	GstCaps *audioCaps = gst_caps_from_string(MEDIA_STREAMER_DEFAULT_AUDIO_FORMAT);
	g_object_set(G_OBJECT(audio_filter), "caps", audioCaps, NULL);
	gst_caps_unref(audioCaps);

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

GstElement *__ms_rtp_element_create(media_streamer_node_s * ms_node)
{
	ms_retvm_if(ms_node == NULL, (GstElement *) NULL, "Error empty rtp node Handle");

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

		GstPad *rtp_sink = gst_element_get_request_pad(rtpbin, "send_rtp_sink_0");
		GstPad *ghost_sink = gst_element_get_static_pad(ms_node->gst_element, MS_RTP_PAD_VIDEO_IN);
		gst_ghost_pad_set_target(GST_GHOST_PAD(ghost_sink), rtp_sink);
		MS_SAFE_UNREF(rtp_sink);
		MS_SAFE_UNREF(ghost_sink);

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

		GstPad *rtp_sink = gst_element_get_request_pad(rtpbin, "send_rtp_sink_1");
		GstPad *ghost_sink = gst_element_get_static_pad(ms_node->gst_element, MS_RTP_PAD_AUDIO_IN);
		gst_ghost_pad_set_target(GST_GHOST_PAD(ghost_sink), rtp_sink);
		MS_SAFE_UNREF(rtp_sink);
		MS_SAFE_UNREF(ghost_sink);

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
	case MEDIA_STREAMER_NODE_TYPE_QUEUE:
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
					state_transition_name = g_strdup_printf("%s_%s", gst_element_state_get_name(state_old), gst_element_state_get_name(state_new));
					ms_info("GST_MESSAGE_STATE_CHANGED: [%s] %s", GST_OBJECT_NAME(GST_MESSAGE_SRC(message)), state_transition_name);
					__ms_generate_dots(ms_streamer->pipeline, state_transition_name);

					MS_SAFE_GFREE(state_transition_name);
				}
				break;
			}

		case GST_MESSAGE_ASYNC_DONE:{
				if (GST_MESSAGE_SRC(message) == GST_OBJECT(ms_streamer->pipeline)
					&& ms_streamer->is_seeking) {

					if (ms_streamer->seek_done_cb.callback) {
						media_streamer_position_changed_cb cb = (media_streamer_position_changed_cb) ms_streamer->seek_done_cb.callback;
						cb(ms_streamer->seek_done_cb.user_data);
					}

					ms_streamer->is_seeking = FALSE;
					ms_streamer->seek_done_cb.callback = NULL;
					ms_streamer->seek_done_cb.user_data = NULL;
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

static GstCaps *__ms_create_caps_from_fmt(media_format_h fmt)
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
			caps = gst_caps_new_simple(__ms_convert_mime_to_string_format(mime), "channels",G_TYPE_INT, channel, "rate", G_TYPE_INT, samplerate, NULL);
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
