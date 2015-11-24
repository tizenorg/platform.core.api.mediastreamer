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

int __ms_state_change(media_streamer_s * ms_streamer, media_streamer_state_e state)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;

	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	media_streamer_state_e previous_state = ms_streamer->state;
	ms_retvm_if(previous_state == state, MEDIA_STREAMER_ERROR_NONE, "Media streamer already in this state");

	switch (state) {
	case MEDIA_STREAMER_STATE_NONE:
		/*
		 * Media streamer must be in IDLE state
		 * Unlink and destroy all bins and elements.
		 */
		if (previous_state != MEDIA_STREAMER_STATE_IDLE)
			__ms_state_change(ms_streamer, MEDIA_STREAMER_STATE_IDLE);
		break;
	case MEDIA_STREAMER_STATE_IDLE:
		/*
		 * Unlink all gst_elements, set pipeline into state NULL
		 */
		if (previous_state != MEDIA_STREAMER_STATE_NONE)
			ret = __ms_element_set_state(ms_streamer->pipeline, GST_STATE_NULL);
		break;
	case MEDIA_STREAMER_STATE_READY:
		ret = __ms_element_set_state(ms_streamer->pipeline, GST_STATE_READY);
		break;
	case MEDIA_STREAMER_STATE_PLAYING:
		ret = __ms_element_set_state(ms_streamer->pipeline, GST_STATE_PLAYING);
		break;
	case MEDIA_STREAMER_STATE_PAUSED:
		ret = __ms_element_set_state(ms_streamer->pipeline, GST_STATE_PAUSED);
		break;
	case MEDIA_STREAMER_STATE_SEEKING:
	default:{
			ms_info("Error: invalid state [%d]", state);
			return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
		}
	}

	if (ret != MEDIA_STREAMER_ERROR_NONE) {
		ms_error("Failed change state");
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	}

	ms_streamer->state = state;
	ms_info("Media streamer state changed to [%d]", state);
	return ret;
}

int __ms_create(media_streamer_s * ms_streamer)
{
	__ms_load_ini_settings(&ms_streamer->ini);

	ms_streamer->nodes_table = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, __ms_node_remove_from_table);
	ms_retvm_if(ms_streamer->nodes_table == NULL, MEDIA_STREAMER_ERROR_INVALID_OPERATION, "Error creating hash table");

	return __ms_pipeline_create(ms_streamer);
}

int __ms_get_position(media_streamer_s *ms_streamer, int *time)
{
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(time == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Return value is NULL");

	gint64 current = -1;

	if (!gst_element_query_position(ms_streamer->pipeline, GST_FORMAT_TIME, &current)) {
		ms_error("Could not query current position.");
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	} else {
		*time = (int)(((GstClockTime)(current)) / GST_MSECOND);
		ms_info("Media streamer queried position at [%d] msec successfully.", *time);
	}

	return MEDIA_STREAMER_ERROR_NONE;
}

int __ms_get_duration(media_streamer_s *ms_streamer, int *time)
{
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");
	ms_retvm_if(time == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Return value is NULL");

	gint64 duration = -1;

	if (!gst_element_query_duration(ms_streamer->pipeline, GST_FORMAT_TIME, &duration)) {
		ms_error("Could not query current duration.");
		return MEDIA_STREAMER_ERROR_INVALID_OPERATION;
	} else {
		*time = (int)(((GstClockTime)(duration)) / GST_MSECOND);
		ms_info("Media streamer queried duration [%d] msec successfully.", *time);
	}

	return MEDIA_STREAMER_ERROR_NONE;
}

int __ms_streamer_seek(media_streamer_s *ms_streamer, int g_time, bool flag)
{
	ms_retvm_if(ms_streamer == NULL, MEDIA_STREAMER_ERROR_INVALID_PARAMETER, "Handle is NULL");

	GstSeekFlags seek_flag;

	if (flag)
		seek_flag = GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE;
	else
		seek_flag = GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT;

	if (!__ms_gst_seek(ms_streamer->pipeline, (gint64) g_time * GST_MSECOND, seek_flag)) {
		ms_error("Error while seeking media streamer to [%d]\n", g_time);
		return MEDIA_STREAMER_ERROR_SEEK_FAILED;
	} else {
		ms_streamer->is_seeking = TRUE;
	}

	ms_info("Media streamer pipeline seeked successfully to [%d] position", g_time);

	return MEDIA_STREAMER_ERROR_NONE;
}

int __ms_streamer_destroy(media_streamer_s * ms_streamer)
{
	int ret = MEDIA_STREAMER_ERROR_NONE;

	g_mutex_lock(&ms_streamer->mutex_lock);

	ret = __ms_state_change(ms_streamer, MEDIA_STREAMER_STATE_NONE);
	MS_TABLE_SAFE_UNREF(ms_streamer->nodes_table);

	__ms_bin_remove_element(ms_streamer->sink_video_bin);
	__ms_bin_remove_element(ms_streamer->sink_audio_bin);

	MS_SAFE_UNREF(ms_streamer->bus);
	MS_SAFE_UNREF(ms_streamer->pipeline);
	MS_SAFE_UNREF(ms_streamer->sink_video_bin);
	MS_SAFE_UNREF(ms_streamer->sink_audio_bin);

	g_mutex_unlock(&ms_streamer->mutex_lock);
	g_mutex_clear(&ms_streamer->mutex_lock);
	MS_SAFE_FREE(ms_streamer);

	return ret;
}
