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

#include <gst/gst.h>

#include <media_streamer_priv.h>

#define DOT_FILE_NAME "streamer"

#define MEDIA_STREAMER_PIPELINE_NAME "media-streamer-pipeline"
#define MEDIA_STREAMER_SRC_BIN_NAME "streamer_src"
#define MEDIA_STREAMER_VIDEO_SINK_BIN_NAME "streamer_video_sink"
#define MEDIA_STREAMER_AUDIO_SINK_BIN_NAME "streamer_audio_sink"
#define MEDIA_STREAMER_TOPOLOGY_BIN_NAME "streamer_topology"

/**
 * @brief Generates dot files for GStreamer pipeline.
 *
 * @since_tizen 3.0
 */
void __ms_generate_dots(GstElement *bin, gchar *name_tag);

/**
 * @brief Returns the string representation of GST_STATE.
 *
 * @since_tizen 3.0
 */
const char *_ms_state_to_string(GstState state);

/**
 * @brief Creates GstElement by plugin name.
 *
 * @since_tizen 3.0
 */
GstElement *__ms_element_create(const char *plugin_name, const char *name);

/**
 * @brief Creates camera GstElement by camera plugin name.
 *
 * @since_tizen 3.0
 */
GstElement *__ms_camera_element_create(const char *microphone_plugin_name);

/**
 * @brief Creates encoder GstElement by mime type.
 *
 * @since_tizen 3.0
 */
GstElement *__ms_video_encoder_element_create(dictionary *dict, media_format_mimetype_e mime);

/**
 * @brief Creates decoder GstElement by mime type.
 *
 * @since_tizen 3.0
 */
GstElement *__ms_video_decoder_element_create(dictionary *dict, media_format_mimetype_e mime);

/**
 * @brief Creates audio encoder GstElement.
 *
 * @since_tizen 3.0
 */
GstElement *__ms_audio_encoder_element_create(void);

/**
 * @brief Creates rtp container GstElement.
 *
 * @since_tizen 3.0
 */
GstElement *__ms_rtp_element_create(media_streamer_node_s *ms_node);

/**
 * @brief Parse param for RTP node type.
 *
 * @since_tizen 3.0
 */
int __ms_rtp_set_param(
				media_streamer_node_s *node,
				const gchar *param_key,
				const gchar *param_value);

/**
 * @brief Creates pipeline, bus and src/sink/topology bins.
 *
 * @since_tizen 3.0
 */
int __ms_pipeline_create(media_streamer_s *ms_streamer);

/**
 * @brief Adds node to bin
 *
 * @since_tizen 3.0
 */
int __ms_add_node_into_bin(media_streamer_s *ms_streamer,media_streamer_node_s *ms_node);

/**
 * @brief Sets GstElement into state.
 *
 * @since_tizen 3.0
 */
int __ms_element_set_state(GstElement *gst_element, GstState gst_state);

/**
 * @brief Sets mediaformat into GstElement.
 *
 * @since_tizen 3.0
 */
int __ms_element_set_fmt(media_streamer_node_s *node, media_format_h fmt);
