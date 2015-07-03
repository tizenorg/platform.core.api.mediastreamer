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

#include <mm_types.h>
#include <dlog.h>

#include <media_streamer.h>
#include <media_streamer_priv.h>
#include <media_streamer_node.h>
#include <media_streamer_gst.h>

/*
* Public Implementation
*/

int media_streamer_node_create_src(media_streamer_node_src_type_e type,
                              media_streamer_node_h *src)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;

	media_streamer_node_s *ms_src = (media_streamer_node_s *)src;
	ms_retvm_if(ms_src == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	ms_src = (media_streamer_node_s *)calloc(1, sizeof(media_streamer_node_s));
	ms_retvm_if(ms_src == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error allocation memory");

	ms_src->type = MEDIA_STREAMER_NODE_TYPE_SRC;
	ms_src->subtype = (media_streamer_node_src_type_e)type;

	ret = __ms_src_node_create(ms_src);
	if (ret != MEDIA_STREAMER_ERROR_NONE) {
		MS_SAFE_FREE(ms_src);
		ms_error("Error creating Src node [%d]", ret);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	ms_info("Source node [%s] created", ms_src->name);
	*src = (media_streamer_node_h)ms_src;

	return ret;
}

int media_streamer_node_create_sink(media_streamer_node_sink_type_e type,
                               media_streamer_node_h *sink)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;

	media_streamer_node_s *ms_sink = (media_streamer_node_s *)sink;
	ms_retvm_if(ms_sink == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	ms_sink = (media_streamer_node_s *)calloc(1, sizeof(media_streamer_node_s));
	ms_retvm_if(ms_sink == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error allocation memory");

	ms_sink->type = MEDIA_STREAMER_NODE_TYPE_SINK;
	ms_sink->subtype = (media_streamer_node_sink_type_e)type;

	ret = __ms_sink_node_create(ms_sink);
	if (ret != MEDIA_STREAMER_ERROR_NONE) {
		MS_SAFE_FREE(ms_sink);
		ms_error("Error creating Sink node [%d]", ret);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	ms_info("Sink node [%s] created", ms_sink->name);
	*sink = (media_streamer_node_h)ms_sink;

	return ret;
}

int media_streamer_node_create(media_streamer_node_type_e type,
                               media_format_h in_fmt,
                               media_format_h out_fmt,
                               media_streamer_node_h *node)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;

	media_streamer_node_s *ms_node = (media_streamer_node_s *)node;
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	ms_node = (media_streamer_node_s *)calloc(1, sizeof(media_streamer_node_s));
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error allocation memory");

	ms_node->type = type;
	ms_node->subtype = 0;

	ret = __ms_node_create(ms_node, in_fmt, out_fmt);
	if (ret != MEDIA_STREAMER_ERROR_NONE) {
		MS_SAFE_FREE(ms_node);
		ms_error("Error creating Node [%d]", ret);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	ms_info("Node [%s] created", ms_node->name);
	*node = (media_streamer_node_h)ms_node;

	return ret;
}

int media_streamer_node_destroy(media_streamer_node_h node)
{
	media_streamer_node_s *ms_node = (media_streamer_node_s *)node;
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	if (ms_node->parent_streamer == NULL) {
		/* This node was not added into any media streamer */
		__ms_node_destroy(ms_node);
	} else {
		ms_error("Node destroy error: needed to unlink node and remove it from media streamer before destroying.");
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	ms_info("Node destroyed successfully");
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_node_remove(media_streamer_h streamer,
                               media_streamer_node_h node)
{
	media_streamer_node_s *ms_node = (media_streamer_node_s *)node;
	media_streamer_s *ms_streamer = (media_streamer_s *)streamer;

	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(ms_streamer->nodes_table == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	ms_retvm_if(ms_streamer != ms_node->parent_streamer, MEDIA_STREAMER_ERROR_INVALID_PARAMETER,
	            "Node [%s] added into another Media Streamer object", ms_node->name);

	g_mutex_lock(&ms_streamer->mutex_lock);

	if (g_hash_table_steal(ms_streamer->nodes_table, (gpointer)ms_node->name) &&
	    __ms_element_unlink(ms_node->gst_element)) {
		ms_node->parent_streamer = NULL;
		ms_info("Node removed from Media Streamer");
	} else {
		ms_error("Error: Node [%s] remove failed", ms_node->name);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}
	g_mutex_unlock(&ms_streamer->mutex_lock);

	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_node_add(media_streamer_h streamer,
                            media_streamer_node_h node)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;

	media_streamer_s *ms_streamer = (media_streamer_s *)streamer;
	ms_retvm_if(streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	media_streamer_node_s *ms_node = (media_streamer_node_s *)node;
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	g_mutex_lock(&ms_streamer->mutex_lock);

	__ms_node_insert_into_table(ms_streamer->nodes_table, ms_node);
	ms_node->parent_streamer = ms_streamer;

	__ms_add_node_into_bin(ms_streamer, ms_node);

	g_mutex_unlock(&ms_streamer->mutex_lock);

	return ret;
}

int media_streamer_prepare(media_streamer_h streamer)
{
	media_streamer_s *ms_streamer = (media_streamer_s *)streamer;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(ms_streamer->pipeline == NULL, MEDIA_STREAMER_ERROR_INVALID_STATE, "Pipeline doesn`t exist");

	g_mutex_lock(&ms_streamer->mutex_lock);

	if (ms_streamer->state > MEDIA_STREAMER_STATE_IDLE) {
		ms_error("Error: Media streamer already prepared [%d]!",
		         MEDIA_STREAMER_ERROR_INVALID_OPERATION);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	if (__ms_state_change(ms_streamer, MEDIA_STREAMER_STATE_READY) != MEDIA_STREAMER_ERROR_NONE) {
		ms_error("Error: can not set state [%d]", MEDIA_STREAMER_ERROR_INVALID_OPERATION);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}
	__ms_generate_dots(ms_streamer->pipeline, "prepare");

	g_mutex_unlock(&ms_streamer->mutex_lock);

	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_unprepare(media_streamer_h streamer)
{
	media_streamer_s *ms_streamer = (media_streamer_s *)streamer;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(ms_streamer->pipeline == NULL, MEDIA_STREAMER_ERROR_INVALID_STATE, "Pipeline doesn`t exist");

	g_mutex_lock(&ms_streamer->mutex_lock);

	if (__ms_state_change(ms_streamer, MEDIA_STREAMER_STATE_IDLE) != MEDIA_STREAMER_ERROR_NONE) {
		ms_error("Error: can not set state [%d]", MEDIA_STREAMER_ERROR_INVALID_OPERATION);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	g_mutex_unlock(&ms_streamer->mutex_lock);

	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_play(media_streamer_h streamer)
{
	media_streamer_s *ms_streamer = (media_streamer_s *)streamer;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	g_mutex_lock(&ms_streamer->mutex_lock);

	if (ms_streamer->state < MEDIA_STREAMER_STATE_READY) {
		ms_error("Error: Media streamer must be prepared first [%d]!",
		         MEDIA_STREAMER_ERROR_INVALID_OPERATION);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	if (__ms_state_change(ms_streamer, MEDIA_STREAMER_STATE_PLAYING) != MEDIA_STREAMER_ERROR_NONE) {
		ms_error("Error: can not set state [%d]", MEDIA_STREAMER_ERROR_INVALID_OPERATION);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	g_mutex_unlock(&ms_streamer->mutex_lock);

	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_create(media_streamer_h *streamer)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;
	media_streamer_s *ms_streamer = NULL;
	ms_retvm_if(streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	ms_streamer = (media_streamer_s *)calloc(1, sizeof(media_streamer_s));
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error allocation memory");

	/* create streamer lock */
	g_mutex_init(&ms_streamer->mutex_lock);

	ms_streamer->state = MEDIA_STREAMER_STATE_NONE;

	ret = __ms_create(ms_streamer);
	if (ret != MEDIA_STREAMER_ERROR_NONE) {
		ms_error("Error creating Media Streamer");
		__ms_streamer_destroy(ms_streamer);

		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	if (__ms_state_change(ms_streamer, MEDIA_STREAMER_STATE_IDLE) != MEDIA_STREAMER_ERROR_NONE) {
		ms_error("Error: can not set state [%d]", MEDIA_STREAMER_ERROR_INVALID_OPERATION);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	*streamer = ms_streamer;
	ms_info("Media Streamer created successfully");
	return ret;
}

int media_streamer_destroy(media_streamer_h streamer)
{
	media_streamer_s *ms_streamer = (media_streamer_s *)streamer;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	g_mutex_lock(&ms_streamer->mutex_lock);

	__ms_streamer_destroy(ms_streamer);

	ms_info("Media Streamer destroyed successfully");

	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_set_error_cb(media_streamer_h streamer,
                                media_streamer_error_cb callback,
                                void *data)
{
	media_streamer_s *ms_streamer = (media_streamer_s *)streamer;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(callback == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Callback is NULL");

	g_mutex_lock(&ms_streamer->mutex_lock);

	ms_streamer->error_cb.callback = callback;
	ms_streamer->error_cb.user_data = data;

	g_mutex_unlock(&ms_streamer->mutex_lock);

	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_unset_error_cb(media_streamer_h streamer)
{
	media_streamer_s *ms_streamer = (media_streamer_s *)streamer;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	g_mutex_lock(&ms_streamer->mutex_lock);

	ms_streamer->error_cb.callback = NULL;
	ms_streamer->error_cb.user_data = NULL;

	g_mutex_unlock(&ms_streamer->mutex_lock);

	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_set_state_change_cb(media_streamer_h streamer,
                                       media_streamer_state_changed_cb callback,
                                       void *data)
{
	media_streamer_s *ms_streamer = (media_streamer_s *)streamer;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(callback == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Callback is NULL");

	g_mutex_lock(&ms_streamer->mutex_lock);

	ms_streamer->state_changed_cb.callback = callback;
	ms_streamer->state_changed_cb.user_data = data;

	g_mutex_unlock(&ms_streamer->mutex_lock);

	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_unset_state_change_cb(media_streamer_h streamer)
{
	media_streamer_s *ms_streamer = (media_streamer_s *)streamer;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	g_mutex_lock(&ms_streamer->mutex_lock);

	ms_streamer->state_changed_cb.callback = NULL;
	ms_streamer->state_changed_cb.user_data = NULL;

	g_mutex_unlock(&ms_streamer->mutex_lock);

	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_src_set_buffer_status_cb(media_streamer_node_h source,
                                            media_streamer_custom_buffer_status_cb callback,
                                            void *user_data)
{
	media_streamer_node_s *ms_src = (media_streamer_node_s *)source;
	media_streamer_callback_s *src_callback = NULL;
	ms_retvm_if(ms_src == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	if (!ms_src->callbacks_structure) {
		src_callback = (media_streamer_callback_s *) calloc(1, sizeof(media_streamer_callback_s));
		ms_retvm_if(src_callback == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error allocation memory");
	} else {
		src_callback = (media_streamer_callback_s *)ms_src->callbacks_structure;
	}

	src_callback->callback = callback;
	src_callback->user_data = user_data;

	ms_src->callbacks_structure = (void *)src_callback;
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_src_unset_buffer_status_cb(media_streamer_node_h source)
{
	media_streamer_node_s *ms_src = (media_streamer_node_s *)source;
	ms_retvm_if(ms_src == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	media_streamer_callback_s *src_callback = (media_streamer_callback_s *) ms_src->callbacks_structure;
	src_callback->callback = NULL;
	src_callback->user_data = NULL;

	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_sink_set_data_ready_cb(media_streamer_node_h sink,
                                          media_streamer_sink_data_ready_cb callback,
                                          void *data)
{
	media_streamer_node_s *ms_sink = (media_streamer_node_s *)sink;
	ms_retvm_if(ms_sink == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	media_streamer_sink_callbacks_s *sink_callbacks = ms_sink->callbacks_structure;
	if (!sink_callbacks) {
		sink_callbacks = (media_streamer_sink_callbacks_s *) calloc(1, sizeof(media_streamer_sink_callbacks_s));
		ms_retvm_if(sink_callbacks == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error allocation memory");
	}

	sink_callbacks->data_ready_cb.callback = callback;
	sink_callbacks->data_ready_cb.user_data = data;

	ms_sink->callbacks_structure = (void *)sink_callbacks;
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_sink_unset_data_ready_cb(media_streamer_node_h sink)
{
	media_streamer_node_s *ms_sink = (media_streamer_node_s *)sink;
	ms_retvm_if(ms_sink == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	media_streamer_sink_callbacks_s *sink_callbacks = ms_sink->callbacks_structure;
	ms_retvm_if(sink_callbacks == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Callback didn't set yet");

	sink_callbacks->data_ready_cb.callback = NULL;
	sink_callbacks->data_ready_cb.user_data = NULL;

	ms_sink->callbacks_structure = (void *)sink_callbacks;
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_sink_set_eos_cb(media_streamer_node_h sink,
                                   media_streamer_sink_eos_cb callback,
                                   void *data)
{
	media_streamer_node_s *ms_sink = (media_streamer_node_s *)sink;
	ms_retvm_if(ms_sink == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	media_streamer_sink_callbacks_s *sink_callbacks = ms_sink->callbacks_structure;
	if (!sink_callbacks) {
		sink_callbacks = (media_streamer_sink_callbacks_s *) calloc(1, sizeof(media_streamer_sink_callbacks_s));
		ms_retvm_if(sink_callbacks == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error allocation memory");
	}

	sink_callbacks->eos_cb.callback = callback;
	sink_callbacks->eos_cb.user_data = data;

	ms_sink->callbacks_structure = (void *)sink_callbacks;
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_sink_unset_eos_cb(media_streamer_node_h sink)
{
	media_streamer_node_s *ms_sink = (media_streamer_node_s *)sink;
	ms_retvm_if(ms_sink == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	media_streamer_sink_callbacks_s *sink_callbacks = ms_sink->callbacks_structure;
	ms_retvm_if(sink_callbacks == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Callback didn't set yet");

	sink_callbacks->data_ready_cb.callback = NULL;
	sink_callbacks->data_ready_cb.user_data = NULL;

	ms_sink->callbacks_structure = (void *)sink_callbacks;
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_pause(media_streamer_h streamer)
{
	media_streamer_s *ms_streamer = (media_streamer_s *)streamer;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	g_mutex_lock(&ms_streamer->mutex_lock);

	if (__ms_state_change(ms_streamer, MEDIA_STREAMER_STATE_PAUSED) != MEDIA_STREAMER_ERROR_NONE) {
		ms_error("Error: can not set state [%d]", MEDIA_STREAMER_ERROR_INVALID_OPERATION);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	g_mutex_unlock(&ms_streamer->mutex_lock);

	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_stop(media_streamer_h streamer)
{
	media_streamer_s *ms_streamer = (media_streamer_s *)streamer;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	g_mutex_lock(&ms_streamer->mutex_lock);

	if (ms_streamer->state < MEDIA_STREAMER_STATE_READY) {
		ms_error("Error: Media streamer must be prepared first [%d]!",
		         MEDIA_STREAMER_ERROR_INVALID_OPERATION);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	if (__ms_state_change(ms_streamer, MEDIA_STREAMER_STATE_READY) != MEDIA_STREAMER_ERROR_NONE) {
		ms_error("Error: can not set state [%d]", MEDIA_STREAMER_ERROR_INVALID_OPERATION);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	g_mutex_unlock(&ms_streamer->mutex_lock);

	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_get_state(media_streamer_h streamer,
                             media_streamer_state_e *state)
{
	media_streamer_s *ms_streamer = (media_streamer_s *)streamer;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	*state = ms_streamer->state;

	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_node_push_packet(media_streamer_node_h src,
                               media_packet_h packet)
{
	media_streamer_node_s *ms_node = (media_streamer_node_s *)src;
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	ms_retvm_if(ms_node->type != MEDIA_STREAMER_NODE_TYPE_SRC,
			MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Node type must be Src type for pushing packets.");
	ms_retvm_if(ms_node->subtype != MEDIA_STREAMER_NODE_SRC_TYPE_CUSTOM,
			MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Source Node must be a custom type for pushing packets.");

	return __ms_element_push_packet(ms_node->gst_element, packet);
}

int media_streamer_node_pull_packet(media_streamer_node_h sink,
                               media_packet_h *packet)
{
	media_streamer_node_s *ms_node = (media_streamer_node_s *)sink;
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(packet == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Media packet is NULL");

	ms_retvm_if(ms_node->gst_element == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(ms_node->type != MEDIA_STREAMER_NODE_TYPE_SINK,
			MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Node type must be Sink type for pulling packets.");
	ms_retvm_if(ms_node->subtype != MEDIA_STREAMER_NODE_SINK_TYPE_CUSTOM,
			MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Sink Node must be a custom type for pulling packets.");

	return __ms_element_pull_packet(ms_node->gst_element, packet);
}

int media_streamer_node_link(media_streamer_node_h src_node,
                             const char *src_pad_name,
                             media_streamer_node_h dest_node,
                             const char *sink_pad_name)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;
	media_streamer_node_s *ms_src_node = (media_streamer_node_s *)src_node;
	ms_retvm_if(ms_src_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	media_streamer_node_s *ms_dest_node = (media_streamer_node_s *)dest_node;
	ms_retvm_if(ms_dest_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	gchar *src_element_name = gst_element_get_name(ms_src_node->gst_element);
	gchar *sink_element_name = gst_element_get_name(ms_dest_node->gst_element);

	ms_retvm_if(src_pad_name == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Pad is NULL");
	ms_retvm_if(sink_pad_name == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Pad is NULL");

	gboolean link_ret;

	link_ret = gst_element_link_pads(ms_src_node->gst_element, src_pad_name, ms_dest_node->gst_element, sink_pad_name);
	if (!link_ret) {
		ms_error("Can not link [%s]->%s pad to [%s]->%s pad, ret code [%d] ", src_pad_name, sink_pad_name, src_element_name, sink_element_name, link_ret);
		ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	MS_SAFE_GFREE(src_element_name);
	MS_SAFE_GFREE(sink_element_name);
	return ret;
}

int media_streamer_node_set_pad_format(media_streamer_node_h node,
                                       const char *pad_name,
                                       media_format_h fmt)
{
	media_streamer_node_s *ms_node = (media_streamer_node_s *)node;
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(fmt == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Format is NULL");

	/* By default it sets format to object's property 'caps'*/
	return __ms_element_set_fmt(node, fmt);
}

int media_streamer_node_get_pad_format(media_streamer_node_h node,
                                       const char *pad_name,
                                       media_format_h *fmt)
{
	media_streamer_node_s *ms_node = (media_streamer_node_s *)node;
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(pad_name == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Empty pad name");
	ms_retvm_if(fmt == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Format is NULL");

	*fmt = __ms_element_get_pad_fmt(ms_node->gst_element, pad_name);

	ms_retvm_if(*fmt == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error while getting node fmt");

	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_node_set_params(media_streamer_node_h node,
                                   bundle *param_list)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;

	media_streamer_node_s *ms_node = (media_streamer_node_s *)node;
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(ms_node->gst_element == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(param_list == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Parameters list is NULL");

	ret = __ms_node_read_params_from_bundle(ms_node, param_list);
	ms_retvm_if(ret != MEDIA_STREAMER_ERROR_NONE, MEDIA_STREAMER_ERROR_INVALID_OPERATION,
	            "Parameters list is NULL");

	return ret;
}

int media_streamer_node_get_params(media_streamer_node_h node,
                                   bundle **param_list)
{
	media_streamer_node_s *ms_node = (media_streamer_node_s *)node;
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(param_list == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Param list pionter is NULL");

	bundle *ms_params = NULL;
	ms_params = bundle_create();
	ms_retvm_if(ms_params == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Error creating new params object");

	if (__ms_node_write_params_into_bundle(ms_node, ms_params) != MEDIA_STREAMER_ERROR_NONE) {
		ms_info("Node [%s] do not have any params.", ms_node->name);
		bundle_free(ms_params);
		*param_list = NULL;

		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	*param_list = ms_params;
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_node_set_param(media_streamer_node_h node,
                                 const char *param_name, const char *param_value)
{
	media_streamer_node_s *ms_node = (media_streamer_node_s *)node;
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(ms_node->gst_element == NULL && ms_node->set_param,
	            MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(param_name == NULL || param_value == NULL,
	            MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Parameters name or value is NULL");

	return ms_node->set_param(ms_node, param_name, param_value);
}

int media_streamer_node_get_param(media_streamer_node_h node,
                                  const char *param_name, char **param_value)
{
	media_streamer_node_s *ms_node = (media_streamer_node_s *)node;
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(param_name == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Param name is NULL");

  // TBD

	return MEDIA_STREAMER_ERROR_NONE;
}

