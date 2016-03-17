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

#ifndef __TIZEN_MEDIASTREAMER_PRIVATE_H__
#define __TIZEN_MEDIASTREAMER_PRIVATE_H__


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <gst/gst.h>
#include <stdio.h>

#include <media_streamer.h>
#include <media_streamer_util.h>

struct media_streamer_node_s;

/**
 * @brief Media Streamer callbacks structure.
 *
 * @since_tizen 3.0
 */
typedef struct {
	void *callback;
	void *user_data;
} media_streamer_callback_s;

/**
 * @brief Media Streamer sink callbacks structure.
 *
 * @since_tizen 3.0
 */
typedef struct {
	media_streamer_callback_s data_ready_cb;
	media_streamer_callback_s eos_cb;
} media_streamer_sink_callbacks_s;

/**
 * @brief Media Streamer param type handle.
 *
 * @since_tizen 3.0
 */
typedef struct {
	char *param_name;
	char *origin_name;
} param_s;

/**
 * @brief Media Streamer type handle.
 *
 * @since_tizen 3.0
 */
typedef struct {
	media_streamer_ini_t ini;
	GstElement *pipeline;

	GstElement *src_bin;
	GstElement *sink_bin;
	GstElement *topology_bin;

	GHashTable *nodes_table;
	GMutex mutex_lock;
	GList *autoplug_sig_list;
	GList *pads_types_list;

	GstBus *bus;
	guint bus_watcher;
	gboolean is_seeking;

	media_streamer_state_e state;

	media_streamer_callback_s error_cb;
	media_streamer_callback_s state_changed_cb;
	media_streamer_callback_s seek_done_cb;
} media_streamer_s;

/**
 * @brief Media Streamer node type handle.
 *
 * @since_tizen 3.0
 */
typedef struct {
	GstElement *gst_element;
	media_streamer_s *parent_streamer;

	char *name;
	media_streamer_node_type_e type;
	int subtype;

	gboolean linked_by_user;

	GList *sig_list;

	void *callbacks_structure;
} media_streamer_node_s;

/* Private functions definition */

/**
 * @brief Gets the play position of Media streamer element.
 *
 * @since_tizen 3.0
 */
int __ms_get_position(media_streamer_s *ms_streamer, int *time);

/**
 * @brief Gets the duration of Media streamer element.
 *
 * @since_tizen 3.0
 */
int __ms_get_duration(media_streamer_s *ms_streamer, int *time);

/**
 * @brief Seeks Media streamer element to the pointed position.
 *
 * @since_tizen 3.0
 */
int __ms_streamer_seek(media_streamer_s *ms_streamer, int time, bool flag);

/**
 * @brief Destroys media streamer structure.
 *
 * @since_tizen 3.0
 */
int __ms_streamer_destroy(media_streamer_s *ms_streamer);

/**
 * @brief Creates media streamer structure.
 *
 * @since_tizen 3.0
 */
int __ms_create(media_streamer_s *ms_streamer);

/**
 * @brief Changes state of media streamer.
 *
 * @since_tizen 3.0
 */
int __ms_state_change(media_streamer_s *ms_streamer, media_streamer_state_e state);

#ifdef __cplusplus
}

#endif /* __cplusplus */

#endif /* __TIZEN_MEDIASTREAMER_PRIVATE_H__ */