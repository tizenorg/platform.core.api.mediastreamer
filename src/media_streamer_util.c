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

#ifndef __MEDIA_STREAMER_UTIL_C__
#define __MEDIA_STREAMER_UTIL_C__

#include <glib/gstdio.h>

#include <media_streamer.h>
#include <media_streamer_util.h>

#ifdef MEDIA_STREAMER_DEFAULT_INI
static gboolean __ms_generate_default_ini(void);
#endif

static void __ms_check_ini_status(void);

gchar *__ms_ini_get_string(dictionary *dict,
                           const char *ini_path, char *default_str)
{
	gchar *result_str = NULL;

	ms_retvm_if(ini_path == NULL, NULL, "Invalid ini path");

	if (dict == NULL) {
		result_str = g_strdup(default_str);
	} else {
		gchar *str = NULL;
		str = iniparser_getstring(dict, ini_path, default_str);
		if (str &&
		    (strlen(str) > 0) &&
		    (strlen(str) < MEDIA_STREAMER_INI_MAX_STRLEN)) {
			result_str = g_strdup(str);
		} else {
			result_str = g_strdup(default_str);
		}
	}
	return result_str;
}

gboolean __ms_load_ini_dictionary(dictionary **dict)
{
	ms_retvm_if(dict == NULL, FALSE, "Handle is NULL");

	__ms_check_ini_status();

	dictionary *ms_dict = NULL;

	/* loading existing ini file */
	ms_dict = iniparser_load(MEDIA_STREAMER_INI_DEFAULT_PATH);

	/* if no file exists. create one with set of default values */
	if (!ms_dict) {
#ifdef MEDIA_STREAMER_DEFAULT_INI
		ms_debug("No inifile found. Media streamer will create default inifile.");
		if (FALSE == __ms_generate_default_ini()) {
			ms_debug("Creating default .ini file failed. Media-streamer will use default values.");
		} else {
			/* load default ini */
			ms_dict = iniparser_load(MEDIA_STREAMER_INI_DEFAULT_PATH);
		}
#else
		ms_debug("No ini file found.");
		return FALSE;
#endif
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

int __ms_load_ini_settings(media_streamer_ini_t *ini)
{
	dictionary *dict = NULL;

	/* get ini values */
	memset(ini, 0, sizeof(media_streamer_ini_t));

	if (__ms_load_ini_dictionary(&dict)) {
		/* general */
		ini->generate_dot = iniparser_getboolean(dict, "general:generate dot", DEFAULT_GENERATE_DOT);
		if (ini->generate_dot == TRUE) {
			gchar *dot_path = iniparser_getstring(dict, "general:dot dir" , MEDIA_STREAMER_DEFAULT_DOT_DIR);
			ms_debug("generate_dot is TRUE, dot file will be stored into %s", dot_path);
			g_setenv("GST_DEBUG_DUMP_DOT_DIR", dot_path, FALSE);
		}

	} else { /* if dict is not available just fill the structure with default value */
		ms_debug("failed to load ini. using hardcoded default");

		/* general settings*/
		ini->generate_dot = DEFAULT_GENERATE_DOT;
	}

	__ms_destroy_ini_dictionary(dict);

	/* general */
	ms_debug("generate_dot : %d\n", ini->generate_dot);

	return MEDIA_STREAMER_ERROR_NONE;
}

static void __ms_check_ini_status(void)
{
	FILE *fp = fopen(MEDIA_STREAMER_INI_DEFAULT_PATH, "r");
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
			status = g_remove(MEDIA_STREAMER_INI_DEFAULT_PATH);
			if (status == -1) {
				ms_error("failed to delete corrupted ini");
			}
		}
	}
}

#ifdef MEDIA_STREAMER_DEFAULT_INI
static gboolean __ms_generate_default_ini(void)
{
	FILE *fp = NULL;
	gchar *default_ini = MEDIA_STREAMER_DEFAULT_INI;


	/* create new file */
	fp = fopen(MEDIA_STREAMER_INI_DEFAULT_PATH, "wt");

	if (!fp) {
		return FALSE;
	}

	/* writing default ini file */
	if (strlen(default_ini) != fwrite(default_ini, 1, strlen(default_ini), fp)) {
		fclose(fp);
		return FALSE;
	}

	fclose(fp);
	return TRUE;
}
#endif

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
			return "S16BE";
		default:
			ms_error("Invalid or Unsupported media format [%d].", mime);
			return NULL;
			break;
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
	} else if (g_strrstr(format_type, "S16BE")) {
		return MEDIA_FORMAT_PCM;
	} else {
		ms_error("Invalid or Unsupported media format [%s].", format_type);
		return MEDIA_FORMAT_NONE;
	}
}

#endif
