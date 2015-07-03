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
int __ms_node_create(media_streamer_node_s *node,
                     media_format_h in_fmt,
                     media_format_h out_fmt);

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
void __ms_node_destroy(void *data);

/**
 * @brief Inserts media streamer node into nodes table.
 *
 * @since_tizen 3.0
 */
void __ms_node_insert_into_table(GHashTable *nodes_table,
                                 media_streamer_node_s *ms_node);

/**
 * @brief Remove media streamer node from nodes table.
 *
 * @since_tizen 3.0
 */
int __ms_node_remove_from_table(GHashTable *nodes_table,
                                media_streamer_node_s *ms_node);

/**
 * @brief Auto link nodes if needed.
 *
 * @since_tizen 3.0
 */
int __ms_autoplug_prepare(media_streamer_s *ms_streamer);

/**
 * @brief Reads node parameters from user's bundle object.
 *
 * @since_tizen 3.0
 */
int __ms_node_read_params_from_bundle(media_streamer_node_s *node,
                                      bundle *param_list);

/**
 * @brief Writes GstElement properties into user's bundle object.
 *
 * @since_tizen 3.0
 */
int __ms_node_write_params_into_bundle(media_streamer_node_s *node,
                                       bundle *param_list);
