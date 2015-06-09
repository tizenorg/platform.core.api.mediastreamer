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

int media_streamer_src_create(media_streamer_src_type_e type,
				media_streamer_node_h *src)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;

	media_streamer_node_s *ms_src = (media_streamer_node_s*)src;
	ms_retvm_if(ms_src == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	ms_src = (media_streamer_node_s *)calloc(1, sizeof(media_streamer_node_s));
	ms_retvm_if(ms_src == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error allocation memory");

	ms_src->type = MEDIA_STREAMER_NODE_TYPE_SRC;
	ms_src->subtype = (media_streamer_src_type_e)type;
	ret = __ms_src_node_create(ms_src);
	if(ret != MEDIA_STREAMER_ERROR_NONE)
	{
		MS_SAFE_FREE(ms_src);
		ms_error( "Error creating Src node [%d]",ret);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	ms_info("Source node [%s] created", ms_src->name);
	*src = (media_streamer_node_h)ms_src;

	return ret;
}

int media_streamer_sink_create(media_streamer_sink_type_e type,
				media_streamer_node_h *sink)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;

	media_streamer_node_s *ms_sink = (media_streamer_node_s*)sink;
	ms_retvm_if(ms_sink == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	ms_sink = (media_streamer_node_s *)calloc(1, sizeof(media_streamer_node_s));
	ms_retvm_if(ms_sink == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error allocation memory");

	ms_sink->type = MEDIA_STREAMER_NODE_TYPE_SINK;
	ms_sink->subtype = (media_streamer_sink_type_e)type;
	ret = __ms_sink_node_create(ms_sink);
	if(ret != MEDIA_STREAMER_ERROR_NONE)
	{
		MS_SAFE_FREE(ms_sink);
		ms_error( "Error creating Sink node [%d]",ret);
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

	media_streamer_node_s *ms_node = (media_streamer_node_s*)node;
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	ms_node = (media_streamer_node_s *)calloc(1, sizeof(media_streamer_node_s));
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error allocation memory");

	ms_node->type = type;
	ms_node->subtype = 0;

	ret = __ms_node_create(ms_node, in_fmt, out_fmt);
	if(ret != MEDIA_STREAMER_ERROR_NONE)
	{
		MS_SAFE_FREE(ms_node);
		ms_error( "Error creating Node [%d]",ret);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	ms_info("Node [%s] created", ms_node->name);
	*node = (media_streamer_node_h)ms_node;

	return ret;
}

int media_streamer_node_destroy(media_streamer_node_h node)
{
	media_streamer_node_s *ms_node = (media_streamer_node_s*)node;
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	if (ms_node->parent_streamer == NULL)
	{
		// This node was not added into any media streamer
		__ms_node_destroy(ms_node);
	}
	else
	{
		int ret = __ms_node_remove_from_table(ms_node->parent_streamer->nodes_table, ms_node);
		ms_retvm_if(ret != MEDIA_STREAMER_ERROR_NONE, MEDIA_STREAMER_ERROR_INVALID_OPERATION,
				"Current key was not removed from nodes_table");
	}

	ms_info("Node destroyed successfully");
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_node_add(media_streamer_h streamer,
				media_streamer_node_h node)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;

	media_streamer_s *ms_streamer = (media_streamer_s*)streamer;
	ms_retvm_if(streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	media_streamer_node_s* ms_node = (media_streamer_node_s*)node;
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	g_mutex_lock(&ms_streamer->mutex_lock);

	__ms_node_insert_into_table(ms_streamer->nodes_table,ms_node);
	ms_node->parent_streamer = ms_streamer;

	__ms_add_node_into_bin(ms_streamer, ms_node);

	g_mutex_unlock(&ms_streamer->mutex_lock);

	return ret;
}

int media_streamer_prepare(media_streamer_h streamer)
{
	media_streamer_s *ms_streamer = (media_streamer_s*)streamer;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(ms_streamer->pipeline == NULL, MEDIA_STREAMER_ERROR_INVALID_STATE, "Pipeline doesn`t exist");

	g_mutex_lock(&ms_streamer->mutex_lock);

	if(ms_streamer->state > MEDIA_STREAMER_STATE_IDLE)
	{
		ms_error("Error: Media streamer already prepared [%d]!",
				MEDIA_STREAMER_ERROR_INVALID_OPERATION);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	if(__ms_state_change(ms_streamer, MEDIA_STREAMER_STATE_READY) != MEDIA_STREAMER_ERROR_NONE)
	{
		ms_error("Error: can not set state [%d]", MEDIA_STREAMER_ERROR_INVALID_OPERATION);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}
	__ms_generate_dots(ms_streamer->pipeline, "prepare");

	g_mutex_unlock(&ms_streamer->mutex_lock);

	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_unprepare(media_streamer_h streamer)
{
	media_streamer_s *ms_streamer = (media_streamer_s*)streamer;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(ms_streamer->pipeline == NULL, MEDIA_STREAMER_ERROR_INVALID_STATE, "Pipeline doesn`t exist");

	g_mutex_lock(&ms_streamer->mutex_lock);

	if(__ms_state_change(ms_streamer, MEDIA_STREAMER_STATE_IDLE) != MEDIA_STREAMER_ERROR_NONE)
	{
		ms_error("Error: can not set state [%d]", MEDIA_STREAMER_ERROR_INVALID_OPERATION);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	g_mutex_unlock(&ms_streamer->mutex_lock);

	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_play(media_streamer_h streamer)
{
	media_streamer_s *ms_streamer = (media_streamer_s*)streamer;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	g_mutex_lock(&ms_streamer->mutex_lock);

	if(ms_streamer->state < MEDIA_STREAMER_STATE_READY)
	{
		ms_error("Error: Media streamer must be prepared first [%d]!",
				MEDIA_STREAMER_ERROR_INVALID_OPERATION);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	if(__ms_state_change(ms_streamer, MEDIA_STREAMER_STATE_PLAYING) != MEDIA_STREAMER_ERROR_NONE)
	{
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
	if(ret!=MEDIA_STREAMER_ERROR_NONE)
	{
		ms_error("Error creating Media Streamer");
		__ms_streamer_destroy(ms_streamer);

		g_mutex_clear(&ms_streamer->mutex_lock);

		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	if(__ms_state_change(ms_streamer, MEDIA_STREAMER_STATE_IDLE) != MEDIA_STREAMER_ERROR_NONE)
	{
		ms_error("Error: can not set state [%d]", MEDIA_STREAMER_ERROR_INVALID_OPERATION);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	*streamer = ms_streamer;
	ms_info("Media Streamer created successfully");
	return ret;
}

int media_streamer_destroy(media_streamer_h streamer)
{
	media_streamer_s *ms_streamer = (media_streamer_s*)streamer;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	g_mutex_lock(&ms_streamer->mutex_lock);

	__ms_streamer_destroy(ms_streamer);

	g_mutex_unlock(&ms_streamer->mutex_lock);
	g_mutex_clear(&ms_streamer->mutex_lock);

	ms_info("Media Streamer destroyed successfully");

	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_set_error_cb(media_streamer_h streamer,
				media_streamer_error_cb callback,
				void *data)
{
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_unset_error_cb(media_streamer_h streamer)
{
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_set_state_change_cb(media_streamer_h streamer,
				media_streamer_state_changed_cb callback,
				void *data)
{
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_unset_state_change_cb(media_streamer_h streamer)
{
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_src_set_buffer_status_cb(media_streamer_node_h source,
				media_streamer_custom_buffer_status_cb callback,
				void *user_data)
{
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_src_unset_buffer_status_cb(media_streamer_node_h source)
{
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_sink_set_data_ready_cb(media_streamer_node_h sink,
				media_streamer_sink_data_ready_cb callback,
				void *data)
{
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_sink_unset_data_ready_cb(media_streamer_node_h source)
{
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_sink_set_eos_cb(media_streamer_node_h sink,
				media_streamer_sink_eos_cb callback,
				void *data)
{
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_sink_unset_eos_cb(media_streamer_node_h source)
{
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_pause(media_streamer_h streamer)
{
	media_streamer_s *ms_streamer = (media_streamer_s*)streamer;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	g_mutex_lock(&ms_streamer->mutex_lock);

	if(__ms_state_change(ms_streamer, MEDIA_STREAMER_STATE_PAUSED) != MEDIA_STREAMER_ERROR_NONE)
	{
		ms_error("Error: can not set state [%d]", MEDIA_STREAMER_ERROR_INVALID_OPERATION);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	g_mutex_unlock(&ms_streamer->mutex_lock);

	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_stop(media_streamer_h streamer)
{
	media_streamer_s *ms_streamer = (media_streamer_s*)streamer;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	g_mutex_lock(&ms_streamer->mutex_lock);

	if(ms_streamer->state < MEDIA_STREAMER_STATE_READY)
	{
		ms_error("Error: Media streamer must be prepared first [%d]!",
				MEDIA_STREAMER_ERROR_INVALID_OPERATION);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	if(__ms_state_change(ms_streamer, MEDIA_STREAMER_STATE_READY) != MEDIA_STREAMER_ERROR_NONE)
	{
		ms_error("Error: can not set state [%d]", MEDIA_STREAMER_ERROR_INVALID_OPERATION);
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	g_mutex_unlock(&ms_streamer->mutex_lock);

	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_get_state(media_streamer_h streamer,
				media_streamer_state_e *state)
{
	media_streamer_s *ms_streamer = (media_streamer_s*)streamer;
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	*state = ms_streamer->state;

	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_seek(media_streamer_h streamer,
				media_streamer_time_value time)
{
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_push_packet(media_streamer_node_h src,
				media_packet_h packet)
{
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_pull_packet(media_streamer_node_h sink,
				media_packet_h *packet)
{
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_node_set_format(media_streamer_node_h node,
				media_format_h fmt)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;
	media_streamer_node_s *ms_node = (media_streamer_node_s*)node;
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(fmt == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Format is NULL");

	ret = __ms_element_set_fmt(node, fmt);
	return ret;
}

int media_streamer_node_get_format(media_streamer_node_h node,
				media_format_h *fmt)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;
	media_streamer_node_s *ms_node = (media_streamer_node_s*)node;
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(fmt == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Format is NULL");


	return ret;
}

int media_streamer_node_link(media_streamer_node_h src_node,
				const char *src_pad,
				media_streamer_node_h dest_node,
				const char *sink_pad)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;
	media_streamer_node_s *ms_src_node = (media_streamer_node_s*)src_node;
	ms_retvm_if(ms_src_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	media_streamer_node_s *ms_dest_node = (media_streamer_node_s*)dest_node;
	ms_retvm_if(ms_dest_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	gchar *src_element_name = gst_element_get_name(ms_src_node->gst_element);
	gchar *sink_element_name = gst_element_get_name(ms_dest_node->gst_element);

	ms_retvm_if(src_pad == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Pad is NULL");
	ms_retvm_if(sink_pad == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Pad is NULL");

	gboolean link_ret;

	link_ret = gst_element_link_pads (ms_src_node->gst_element, src_pad, ms_dest_node->gst_element, sink_pad);
	if(!link_ret)
	{
		ms_error("Can not link [%s]->%s pad to [%s]->%s pad, ret code [%d] ", src_pad, sink_pad, src_element_name, sink_element_name, link_ret);
		ret = MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	MS_SAFE_GFREE(src_element_name);
	MS_SAFE_GFREE(sink_element_name);
	return ret;
}

int media_streamer_node_get_pad_format(media_streamer_node_h node,
				char **in_fmt,
				char **out_fmt)
{
	return MEDIA_STREAMER_ERROR_NONE;
}

int media_streamer_node_set_params(media_streamer_node_h node,
				bundle *param_list)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;

	media_streamer_node_s *ms_node = (media_streamer_node_s*)node;
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(ms_node->gst_element == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(param_list == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Parameters list is NULL");

	ret = __ms_node_read_params_from_bundle(ms_node,param_list);
	ms_retvm_if(ret != MEDIA_STREAMER_ERROR_NONE, MEDIA_STREAMER_ERROR_INVALID_OPERATION,
			"Parameters list is NULL");

	return ret;
}

int media_streamer_node_get_param_list(media_streamer_node_h node,
				bundle **param_list)
{
	media_streamer_node_s *ms_node =(media_streamer_node_s*)node;
	ms_retvm_if(ms_node == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(param_list == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Param list pionter is NULL");

	bundle *ms_params = NULL;
	ms_params = bundle_create();
	ms_retvm_if(ms_params == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Error creating new params object");

	if (__ms_node_write_params_into_bundle(ms_node, ms_params) != MEDIA_STREAMER_ERROR_NONE)
	{
		ms_info("Node [%s] do not have any params.", ms_node->name);
		bundle_free(ms_params);
		*param_list = NULL;

		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	*param_list = ms_params;
	return MEDIA_STREAMER_ERROR_NONE;
}
