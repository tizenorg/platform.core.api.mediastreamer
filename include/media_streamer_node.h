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

#include <media_streamer_priv.h>


/**
 * @brief Creates media streamer node using input and output format.
 *
 * @since_tizen 3.0
 */
int __ms_node_create(media_streamer_node_s *node, media_format_h in_fmt, media_format_h out_fmt);

/**
 * @brief Creates media streamer source node.
 *
 * @since_tizen 3.0
 */
int __ms_src_node_create(media_streamer_node_s *node);

/**
 * @brief Creates media streamer sink node.
 *
 * @since_tizen 3.0
 */
int __ms_sink_node_create(media_streamer_node_s *node);

/**
 * @brief Destroys media streamer node.
 *
 * @since_tizen 3.0
 */
void __ms_node_destroy(media_streamer_node_s *node);

/**
 * @brief Inserts media streamer node into nodes table.
 *
 * @since_tizen 3.0
 */
int __ms_node_insert_into_table(GHashTable *nodes_table, media_streamer_node_s *ms_node);

/**
 * @brief Removes media streamer node from nodes table.
 *
 * @since_tizen 3.0
 */
void __ms_node_remove_from_table(void *data);

/**
 * @brief Prepares Media Streamer pipeline and autoplug nodes if needed.
 *
 * @since_tizen 3.0
 */
int __ms_pipeline_prepare(media_streamer_s *ms_streamer);

/**
 * @brief Unprepares Media Streamer pipeline and unlink nodes
 *        which user did't link before.
 *
 * @since_tizen 3.0
 */
int __ms_pipeline_unprepare(media_streamer_s *ms_streamer);

/**
 * @brief Reads node parameters from user's bundle object.
 *
 * @since_tizen 3.0
 */
int __ms_node_set_params_from_bundle(media_streamer_node_s *node, bundle *param_list);

/**
 * @brief Writes GstElement properties into user's bundle object.
 *
 * @since_tizen 3.0
 */
int __ms_node_write_params_into_bundle(media_streamer_node_s *node, bundle *param_list);

/**
 * @brief Gets node's parameter by param_name.
 *
 * @since_tizen 3.0
 */
int __ms_node_get_param(media_streamer_node_s *node, const char *param_name, param_s **param);

/**
 * @brief Gets list of all node's parameters.
 *
 * @since_tizen 3.0
 */
int __ms_node_get_param_list(media_streamer_node_s *node, GList **param_list);

/**
 * @brief Gets string value of node's parameter.
 *
 * @since_tizen 3.0
 */
int __ms_node_get_param_value(media_streamer_node_s *node, param_s *param, char **string_value);

/**
 * @brief Sets parameter value into node's parameter.
 *
 * @since_tizen 3.0
 */
int __ms_node_set_param_value(media_streamer_node_s *ms_node, param_s *param, const gchar *param_value);

/**
 * @brief Sets media format value into node's pad.
 *
 * @since_tizen 3.0
 */
int __ms_node_set_pad_format(media_streamer_node_s *node, const char *pad_name, media_format_h fmt);
