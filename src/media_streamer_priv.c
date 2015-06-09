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

#include "media_streamer_priv.h"
#include "media_streamer_util.h"
#include "media_streamer_node.h"
#include "media_streamer_gst.h"

int __ms_state_change(media_streamer_s *ms_streamer, media_streamer_state_e state)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;

	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	media_streamer_state_e previous_state= ms_streamer->state;
	ms_retvm_if(previous_state == state, MEDIA_STREAMER_ERROR_NONE, "Media streamer already in this state");

	switch(state)
	{
		case MEDIA_STREAMER_STATE_NONE:
			/*
			 * Media streamer must be in IDLE state
			 * Unlink and destroy all bins and elements.
			 */
			if (previous_state != MEDIA_STREAMER_STATE_IDLE)
			{
				__ms_state_change(ms_streamer, MEDIA_STREAMER_STATE_IDLE);
			}
			break;
		case MEDIA_STREAMER_STATE_IDLE:
			/*
			 * Unlink all gst_elements, set pipeline into state NULL
			 */
			if (previous_state != MEDIA_STREAMER_STATE_NONE)
			{
				ret = __ms_element_set_state(ms_streamer->pipeline, GST_STATE_NULL);
			}
			break;
		case MEDIA_STREAMER_STATE_READY:
			break;
		case MEDIA_STREAMER_STATE_PLAYING:
			ret = __ms_element_set_state(ms_streamer->pipeline, GST_STATE_PLAYING);
			break;
		case MEDIA_STREAMER_STATE_PAUSED:
			ret = __ms_element_set_state(ms_streamer->pipeline, GST_STATE_PAUSED);
			break;
		case MEDIA_STREAMER_STATE_SEEKING:
		default:
		{
			ms_info("Error: invalid state [%s]", state);
			return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
		}
	}

	if(ret != MEDIA_STREAMER_ERROR_NONE)
	{
		ms_error("Failed change state");
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	ms_streamer->state = state;
	ms_info("Media streamer state changed to [%d]", state);
	return ret;
}

int __ms_create(media_streamer_s *ms_streamer)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;

	ret = __ms_load_ini_settings(&ms_streamer->ini);
	ms_retvm_if(ret!=MEDIA_STREAMER_ERROR_NONE,
			MEDIA_STREAMER_ERROR_INVALID_OPERATION,"Error load ini file");

	ms_streamer->nodes_table = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, __ms_node_destroy);
	ms_retvm_if(ms_streamer->nodes_table == NULL,
			MEDIA_STREAMER_ERROR_INVALID_OPERATION,"Error creating hash table");

	ret = __ms_pipeline_create(ms_streamer);
	if (ret != MEDIA_STREAMER_ERROR_NONE)
	{
		ms_error("Error while creating media streamer pipeline.");
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	ms_info("Media streamer pipeline created successfully.");

	return ret;
}

static void __node_remove_cb(gpointer key,
				gpointer value,
				gpointer user_data)
{
	media_streamer_s *ms_streamer = (media_streamer_s*)user_data;
	ms_retm_if(ms_streamer == NULL, "Handle is NULL");

	media_streamer_node_s *ms_node = (media_streamer_node_s*)value;
	ms_retm_if(ms_node == NULL, "Handle is NULL");

	ms_info("Try to delete [%s] node from streamer, node type/subtype [%d/%d]",
			ms_node->name, ms_node->type, ms_node->subtype);

	gchar *bin_name = NULL;
	gboolean gst_ret = FALSE;

	__ms_element_set_state(ms_node->gst_element, GST_STATE_NULL);
	gst_object_ref(ms_node->gst_element);

	switch(ms_node->type)
	{
		case MEDIA_STREAMER_NODE_TYPE_SRC:
			gst_ret = gst_bin_remove(GST_BIN(ms_streamer->src_bin),ms_node->gst_element);
			bin_name = g_strdup(MEDIA_STREAMER_SRC_BIN_NAME);
			break;
		case MEDIA_STREAMER_NODE_TYPE_SINK:
			gst_ret = gst_bin_remove(GST_BIN(ms_streamer->sink_video_bin),ms_node->gst_element);
			bin_name = g_strdup(MEDIA_STREAMER_VIDEO_SINK_BIN_NAME);
			break;
		default:
			gst_ret = gst_bin_remove(GST_BIN(ms_streamer->topology_bin),ms_node->gst_element);
			bin_name = g_strdup(MEDIA_STREAMER_TOPOLOGY_BIN_NAME);
			break;
	}

	if(!gst_ret)
	{
		ms_error("Failed to remove Element [%s] from bin [%s]", ms_node->name, bin_name);
	}
	else
	{
		ms_info("Success removed Element [%s] from bin [%s]", ms_node->name, bin_name);
	}

	MS_SAFE_GFREE(bin_name);

}


void __ms_streamer_destroy(media_streamer_s *ms_streamer)
{
	if(__ms_state_change(ms_streamer, MEDIA_STREAMER_STATE_NONE) != MEDIA_STREAMER_ERROR_NONE)
	{
		ms_error("Error: can not set state [%d]", MEDIA_STREAMER_ERROR_INVALID_OPERATION);
	}

	gst_element_unlink_many(ms_streamer->src_bin,
				ms_streamer->topology_bin,
				ms_streamer->sink_video_bin, NULL);

	g_hash_table_foreach(ms_streamer->nodes_table, __node_remove_cb,(gpointer)ms_streamer);

	if(ms_streamer->src_bin && !gst_bin_remove(GST_BIN(ms_streamer->pipeline),ms_streamer->src_bin))
	{
		ms_error("Failed to remove src_bin from pipeline");
	}
	else
	{
		ms_info("src_bin removed from pipeline");
	}

	if(ms_streamer->sink_video_bin && !gst_bin_remove(GST_BIN(ms_streamer->pipeline),ms_streamer->sink_video_bin))
	{
		ms_error("Failed to remove sink_bin from pipeline");
	}
	else
	{
		ms_info("sink_bin removed from pipeline");
	}


	if(ms_streamer->topology_bin && !gst_bin_remove(GST_BIN(ms_streamer->pipeline),ms_streamer->topology_bin))
	{
		ms_error("Failed to remove topology_bin from pipeline");
	}
	else
	{
		ms_info("topology_bin removed from pipeline");
	}


	ms_streamer->state = MEDIA_STREAMER_STATE_NONE;

	MS_TABLE_SAFE_UNREF(ms_streamer->nodes_table);
	MS_SAFE_UNREF(ms_streamer->bus);
	MS_SAFE_UNREF(ms_streamer->pipeline);

	MS_SAFE_FREE(ms_streamer);

//	gst_deinit();
}
