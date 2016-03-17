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

#include <glib/gstdio.h>

#include <media_streamer.h>
#include <media_streamer_util.h>

static void __ms_check_ini_status(void);

gchar *__ms_ini_get_string(dictionary *dict, const char *ini_path, char *default_str)
{
	gchar *result_str = NULL;

	ms_retvm_if(ini_path == NULL, NULL, "Invalid ini path");

	if (dict == NULL) {
		result_str = g_strdup(default_str);
	} else {
		gchar *str = NULL;
		str = iniparser_getstring(dict, ini_path, default_str);
		if (str && (strlen(str) > 0) && (strlen(str) < MEDIA_STREAMER_INI_MAX_STRLEN))
			result_str = g_strdup(str);
		else
			result_str = g_strdup(default_str);
	}
	return result_str;
}

gboolean __ms_load_ini_dictionary(dictionary **dict)
{
	ms_retvm_if(dict == NULL, FALSE, "Handle is NULL");

	__ms_check_ini_status();

	dictionary *ms_dict = NULL;

	/* loading existing ini file */
	ms_dict = iniparser_load(MEDIA_STREAMER_INI_PATH);

	/* if no file exists. create one with set of default values */
	if (!ms_dict) {
		ms_debug("Could not open ini [%s]. Media-streamer will use default values.", MEDIA_STREAMER_INI_PATH);
		return FALSE;
	} else {
		ms_debug("Open ini file [%s].", MEDIA_STREAMER_INI_PATH);
	}

	*dict = ms_dict;
	return TRUE;
}

gboolean __ms_destroy_ini_dictionary(dictionary *dict)
{
	ms_retvm_if(dict == NULL, FALSE, "Handle is null");

	/* free dict as we got our own structure */
	iniparser_freedict(dict);

	return TRUE;
}

static void __ms_ini_read_list(dictionary *dict, const char* key, gchar ***list)
{
	ms_retm_if(!dict || !list || !key, "Handle is NULL");

	/* Read exclude elements list */
	gchar *str = iniparser_getstring(dict, key, NULL);
	if (str && strlen(str) > 0) {
		gchar *strtmp = g_strdup(str);
		g_strstrip(strtmp);

		*list = g_strsplit(strtmp, ",", 10);
		MS_SAFE_FREE(strtmp);
	}
}

void __ms_load_ini_settings(media_streamer_ini_t *ini)
{
	dictionary *dict = NULL;

	/* get ini values */
	memset(ini, 0, sizeof(media_streamer_ini_t));

	if (__ms_load_ini_dictionary(&dict)) {
		/* general */
		ini->generate_dot = iniparser_getboolean(dict, "general:generate dot", DEFAULT_GENERATE_DOT);
		if (ini->generate_dot == TRUE) {
			gchar *dot_path = iniparser_getstring(dict, "general:dot dir", MEDIA_STREAMER_DEFAULT_DOT_DIR);
			ms_debug("generate_dot is TRUE, dot file will be stored into %s", dot_path);
			g_setenv("GST_DEBUG_DUMP_DOT_DIR", dot_path, FALSE);
		}

		ini->use_decodebin = iniparser_getboolean(dict, "general:use decodebin", DEFAULT_USE_DECODEBIN);

		/* Read exclude elements list */
		__ms_ini_read_list(dict, "general:exclude elements", &ini->exclude_elem_names);
		/* Read gstreamer arguments list */
		__ms_ini_read_list(dict, "general:gstreamer arguments", &ini->gst_args);

	} else {
		/* if dict is not available just fill the structure with default values */
		ini->generate_dot = DEFAULT_GENERATE_DOT;
		ini->use_decodebin = DEFAULT_USE_DECODEBIN;
	}

	__ms_destroy_ini_dictionary(dict);

	/* general */
	ms_debug("Media Streamer param [generate_dot] : %d", ini->generate_dot);
	ms_debug("Media Streamer param [use_decodebin] : %d", ini->use_decodebin);
}

static void __ms_check_ini_status(void)
{
	FILE *fp = fopen(MEDIA_STREAMER_INI_PATH, "r");
	int file_size = 0;
	int status = 0;

	if (fp == NULL) {
		ms_debug("Failed to get media streamer ini file.");
	} else {
		fseek(fp, 0, SEEK_END);
		file_size = ftell(fp);
		fclose(fp);
		if (file_size < 5) {
			ms_debug("media_streamer.ini file size=%d, Corrupted! Removed", file_size);
			status = g_remove(MEDIA_STREAMER_INI_PATH);
			if (status == -1)
				ms_error("failed to delete corrupted ini");
		}
	}
}

const gchar *__ms_convert_mime_to_string(media_format_mimetype_e mime)
{
	switch (mime) {
	case MEDIA_FORMAT_I420:
		return "I420";
	case MEDIA_FORMAT_YV12:
		return "YV12";
	case MEDIA_FORMAT_H263:
		return "h263";
	case MEDIA_FORMAT_H264_HP:
	case MEDIA_FORMAT_H264_MP:
	case MEDIA_FORMAT_H264_SP:
		return "h264";
	case MEDIA_FORMAT_PCM:
		return DEFAULT_AUDIO;
	default:
		ms_error("Invalid or Unsupported media format [%d].", mime);
		return NULL;
	}
}

const gchar *__ms_convert_mime_to_rtp_format(media_format_mimetype_e mime)
{
	switch (mime) {
	case MEDIA_FORMAT_I420:
	case MEDIA_FORMAT_YV12:
		return "RAW";
	case MEDIA_FORMAT_H263:
		return "H263";
	case MEDIA_FORMAT_H264_HP:
	case MEDIA_FORMAT_H264_MP:
	case MEDIA_FORMAT_H264_SP:
		return "H264";
	case MEDIA_FORMAT_PCM:
		return "L16";
	default:
		ms_error("Invalid or Unsupported media format [%d].", mime);
		return NULL;
	}
}

media_format_mimetype_e __ms_convert_string_format_to_mime(const char *format_type)
{
	if (g_strrstr(format_type, "I420")) {
		return MEDIA_FORMAT_I420;
	} else if (g_strrstr(format_type, "YV12")) {
		return MEDIA_FORMAT_YV12;
	} else if (g_strrstr(format_type, "h263")) {
		return MEDIA_FORMAT_H263;
	} else if (g_strrstr(format_type, "h264")) {
		return MEDIA_FORMAT_H264_SP;
	} else if (g_strrstr(format_type, DEFAULT_AUDIO)) {
		return MEDIA_FORMAT_PCM;
	} else {
		ms_error("Invalid or Unsupported media format [%s].", format_type);
		return MEDIA_FORMAT_NONE;
	}
}

void __ms_signal_create(GList **sig_list, GstElement *obj, const char *sig_name, GCallback cb, gpointer user_data)
{
	ms_retm_if(!sig_list || !obj || !sig_name, "Empty signal data!");

	media_streamer_signal_s *sig_data = (media_streamer_signal_s *) g_try_malloc(sizeof(media_streamer_signal_s));
	if (!sig_data) {
		ms_error("Failed to create signal [%s] for object [%s]", sig_name, GST_OBJECT_NAME(obj));
		return;
	}

	sig_data->obj = G_OBJECT(obj);
	sig_data->signal_id = g_signal_connect(sig_data->obj, sig_name, cb, user_data);

	if (sig_data->signal_id > 0) {
		*sig_list = g_list_append(*sig_list, sig_data);
		ms_debug("Signal [%s] with id[%lu] connected to object [%s].", sig_name, sig_data->signal_id, GST_OBJECT_NAME(sig_data->obj));
	} else {
		ms_error("Failed to connect signal [%s] for object [%s]", sig_name, GST_OBJECT_NAME(obj));
		MS_SAFE_GFREE(sig_data);
	}
}

void __ms_signal_destroy(void *data)
{
	media_streamer_signal_s *sig_data = (media_streamer_signal_s *) data;
	ms_retm_if(!sig_data, "Empty signal data!");

	if (sig_data->obj && GST_IS_ELEMENT(sig_data->obj)) {
		if (g_signal_handler_is_connected(sig_data->obj, sig_data->signal_id)) {
			g_signal_handler_disconnect(sig_data->obj, sig_data->signal_id);
			ms_debug("Signal with id[%lu] disconnected from object [%s].", sig_data->signal_id, GST_OBJECT_NAME(sig_data->obj));
		}
	}
	MS_SAFE_GFREE(sig_data);
}

void __ms_rtp_param_value_destroy(gpointer data)
{
	GValue *val = (GValue *)data;
	ms_retm_if(!data, "Empty object data!");

	if (GST_VALUE_HOLDS_CAPS(val))
		gst_caps_unref(GST_CAPS(gst_value_get_caps(val)));

	g_value_unset(val);
	MS_SAFE_GFREE(val);
}