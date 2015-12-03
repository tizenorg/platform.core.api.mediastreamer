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
#define MEDIA_STREAMER_SINK_BIN_NAME "streamer_sink"
#define MEDIA_STREAMER_TOPOLOGY_BIN_NAME "streamer_topology"

#define MEDIA_STREAMER_PAYLOADER_KLASS "Codec/Payloader/Network/RTP"
#define MEDIA_STREAMER_RTP_KLASS "Filter/Network/RTP"

#define MEDIA_STREAMER_DEPAYLOADER_KLASS "Depayloader/Network/RTP"
#define MEDIA_STREAMER_BIN_KLASS "Generic/Bin"
#define MEDIA_STREAMER_QUEUE_KLASS "Generic"
#define MEDIA_STREAMER_OVERLAY_KLASS "Filter/Editor"
#define MEDIA_STREAMER_PARSER_KLASS "Codec/Parser/Converter"
#define MEDIA_STREAMER_CONVERTER_KLASS "Filter/Converter"
#define MEDIA_STREAMER_DECODER_KLASS "Codec/Decoder"
#define MEDIA_STREAMER_SINK_KLASS "Sink"

/**
 * @brief Generates dot files for GStreamer pipeline.
 *
 * @since_tizen 3.0
 */
void __ms_generate_dots(GstElement *bin, gchar *name_tag);

/**
 * @brief Finds GstElement by klass name.
 *
 * @since_tizen 3.0
 */
GstElement *__ms_bin_find_element_by_klass(GstElement *sink_bin, GstElement *previous_elem,
					GstPad *source_pad, const gchar *klass_name, const gchar *bin_name);

/**
 * @brief Creates GstElement by klass name.
 *
 * @since_tizen 3.0
 */
GstElement *__ms_create_element_by_registry(GstPad *src_pad, const gchar *klass_name);

/**
 * @brief Links two Gstelements and returns the last one.
 *
 * @since_tizen 3.0
 */
GstElement *__ms_link_with_new_element(GstElement *previous_element,
					GstElement *new_element);

/**
 * @brief Creates GstElement by plugin name.
 *
 * @since_tizen 3.0
 */
GstElement *__ms_element_create(const char *plugin_name, const char *name);

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
 * @brief Find Udp elements into rtp container by name.
 *        It returns elements with increased ref count.
 * @since_tizen 3.0
 */
gboolean __ms_get_rtp_elements(media_streamer_node_s *ms_node, GstElement **rtp_elem,
					GstElement **rtcp_elem, const gchar *elem_name, const gchar *direction, gboolean auto_create);

/**
 * @brief Converts key-value property into needed GType
 * and sets this property into GstElement.
 *
 * @since_tizen 3.0
 */
gboolean __ms_element_set_property(GstElement *src_element, const char *key, const gchar *param_value);

/**
 * @brief Unlink all pads into GstElement.
 *
 * @since_tizen 3.0
 */
gboolean __ms_element_unlink(GstElement *src_element);

/**
 * @brief Remove GstElement from bin.
 *
 * @since_tizen 3.0
 */
gboolean __ms_bin_remove_element(GstElement *element);

/**
 * @brief Add GstElement into bin if not added before and refs it.
 *
 * @since_tizen 3.0
 */
gboolean __ms_bin_add_element(GstElement *bin, GstElement *element, gboolean do_ref);

/**
 * @brief Gets pad type of element pad.
 *
 * @since_tizen 3.0
 */
const gchar * __ms_get_pad_type(GstPad *element_pad);
/**
 * @brief Creates decodebin to link with sink part.
 *
 * @since_tizen 3.0
 */
GstElement *__ms_decodebin_create(media_streamer_s *ms_streamer);

/**
 * @brief Creates next element by klass or by properties and links with the previous one.
 *
 * @since_tizen 3.0
 */
GstElement *__ms_combine_next_element(GstElement *previous_element, GstPad *prev_elem_src_pad, GstElement *bin_to_find_in,
					const gchar *next_elem_klass_name, const gchar *next_elem_bin_name, gchar *default_element);

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
int __ms_add_node_into_bin(media_streamer_s *ms_streamer, media_streamer_node_s *ms_node);

/**
 * @brief Sets GstElement into state.
 *
 * @since_tizen 3.0
 */
int __ms_element_set_state(GstElement *gst_element, GstState gst_state);

/**
 * @brief Iterates pas inside gst element.
 *
 * @since_tizen 3.0
 */
int __ms_element_pad_names(GstElement *gst_element, GstPadDirection pad_type, char ***pad_name_array, int *pads_count);

/**
 * @brief Gets mediaformat from the GstElement's pad by pad name.
 *
 * @since_tizen 3.0
 */
media_format_h __ms_element_get_pad_fmt(GstElement *gst_element, const char *pad_name);

/**
 * @brief Sets mediaformat into GstElement.
 *
 * @since_tizen 3.0
 */
int __ms_element_set_fmt(media_streamer_node_s *node, const char *pad_name, media_format_h fmt);

/**
 * @brief Seeks GstElement to according time value.
 *
 * @since_tizen 3.0
 */
gboolean __ms_gst_seek(GstElement *element, gint64 g_time, GstSeekFlags seek_flag);

/**
 * @brief Push the media packet buffer to the source element.
 *
 * @since_tizen 3.0
 */
int __ms_element_push_packet(GstElement *src_element, media_packet_h packet);

/**
 * @brief Pull the media packet buffer from sink element.
 *
 * @since_tizen 3.0
 */
int __ms_element_pull_packet(GstElement *sink_element, media_packet_h *packet);