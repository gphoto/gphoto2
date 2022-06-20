/*
 * Copyright 2022 Thorsten Ludewig <t.ludewig@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#define _DARWIN_C_SOURCE

#ifndef WEBAPI
#define WEBAPI
#endif

#include "config.h"
#include "actions.h"
#include "globals.h"
#include "i18n.h"
#include "gphoto2-webapi.h"
#include "server.h"
#include "mongoose/mongoose.h"
#include "version.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#define CHECK_NULL(x) \
	{                   \
		if (x == NULL)    \
		{                 \
			return (-1);    \
		}                 \
	}

#define CHECK(result) \
	{                   \
		int r = (result); \
		if (r < 0)        \
			return (r);     \
	}
#define CL(result, list)  \
	{                       \
		int r = (result);     \
		if (r < 0)            \
		{                     \
			gp_list_free(list); \
			return (r);         \
		}                     \
	}

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

//////////////////////////////////////////////////////////////////////////////////////

WebAPIServerConfig webcfg;

static GPParams *p = NULL;
static const char *s_http_addr = "http://0.0.0.0:8866";

static const char *http_chunked_header = "HTTP/1.1 200 OK\r\n"
																				 "Content-Type: application/json\r\n"
																				 "Transfer-Encoding: chunked\r\n\r\n";

static const char *content_type_application_json = "Content-Type: application/json\r\n";
static const char *content_type_text_html = "Content-Type: text/html\r\n";

#define MG_HTTP_CHUNK_START mg_printf(c, http_chunked_header)
#define MG_HTTP_CHUNK_END mg_http_printf_chunk(c, "")

static int
server_http_version(struct mg_connection *c)
{
	MG_HTTP_CHUNK_START;
	JSON_PRINTF(c, "{ \"result\": [{ \"name\": \"mongoose\", \"version\": \"%s\"}", MG_VERSION);
	JSON_PRINTF(c, ",{ \"name\": \"webapi_server\", \"version\": \"%s\"}", WEBAPI_SERVER_VERSION);

	int n;

	for (n = 0; module_versions[n].name != NULL; n++)
	{
		int i;
		const char **v = NULL;
		char *name = module_versions[n].name;
		GPVersionFunc func = module_versions[n].version_func;
		CHECK_NULL(name);
		CHECK_NULL(func);
		v = func(GP_VERSION_SHORT);
		CHECK_NULL(v);
		CHECK_NULL(v[0]);

		mg_http_printf_chunk(c, ", { \"name\": \"%s\", \"version\": \"%s\", \"libs\": \"", name, v[0]);

		for (i = 1; v[i] != NULL; i++)
		{
			if (v[i + 1] != NULL)
				mg_http_printf_chunk(c, "%s, ", v[i]);
			else
				mg_http_printf_chunk(c, "%s", v[i]);
		}

		mg_http_printf_chunk(c, "\"}");
	}

	mg_http_printf_chunk(c, "]}\n");
	MG_HTTP_CHUNK_END;
	return 0;
}

static int list_files(struct mg_connection *c, const char *path)
{
	CameraList *list;
	const char *name;

	CHECK(gp_list_new(&list));
	CL(gp_camera_folder_list_folders(p->camera, path, list, p->context), list);

	JSON_PRINTF(c, "\"path\":\"%s\",\"files\":[", path);

	int sizeD = gp_list_count(list);
	for (int i = 0; i < sizeD; i++)
	{
		CL(gp_list_get_name(list, i, &name), list);
		JSON_PRINTF(c, "%s{ \"name\":\"%s\",\"isFolder\": true }", (i > 0) ? "," : "", name);
	}

	CL(gp_camera_folder_list_files(p->camera, path, list, p->context), list);

	int sizeF = gp_list_count(list);
	for (int i = 0; i < sizeF; i++)
	{
		CL(gp_list_get_name(list, i, &name), list);
		JSON_PRINTF(c, "%s{ \"name\":\"%s\",\"isFolder\": false }", (i > 0) ? "," : "", name);
	}

	JSON_PRINTF(c, "],");
	JSON_PRINTF(c, "\"entries\":%d,", sizeD + sizeF);

	gp_list_free(list);
	return GP_OK;
}

static void
fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
	if (ev == MG_EV_HTTP_MSG)
	{
		struct mg_http_message *hm = (struct mg_http_message *)ev_data;

		if (mg_http_match_uri(hm, "/"))
		{
			server_http_version(c);
			return;
		}

		if (webcfg.auth_enabled)
		{
			char remote_user[WEBCFG_STR_LEN + 1], password[WEBCFG_STR_LEN + 1];
			remote_user[0] = 0;
			password[0] = 0;

			mg_http_creds(hm, remote_user, sizeof(remote_user), password, sizeof(password));

			if (mg_http_match_uri(hm, "/api/#"))
			{
				if (remote_user[0] == 0 || strncmp(remote_user, webcfg.auth_user, WEBCFG_STR_LEN) != 0 || password[0] == 0 || strncmp(password, webcfg.auth_password, WEBCFG_STR_LEN) != 0)
				{
					mg_printf(c, "HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"gphoto2-webapi\"\r\nContent-Length: 0\r\n\r\n");
					return;
				}
			}
		}

		if (mg_http_match_uri(hm, "/api/version"))
		{
			server_http_version(c);
		}

		else if (mg_http_match_uri(hm, "/api/server/shutdown"))
		{
			mg_http_reply(c, 200, content_type_application_json, "{\"return_code\":0}\n");
			webcfg.server_done = TRUE;
		}

		else if (mg_http_match_uri(hm, "/api/auto-detect"))
		{
			MG_HTTP_CHUNK_START;
			auto_detect_action(c, p);
			MG_HTTP_CHUNK_END;
		}

		else if (mg_http_match_uri(hm, "/api/trigger-capture"))
		{
			MG_HTTP_CHUNK_START;
			mg_http_printf_chunk(c, "{\"return_code\":%d}\n", gp_camera_trigger_capture(p->camera, p->context));
			MG_HTTP_CHUNK_END;
		}

		else if (mg_http_match_uri(hm, "/api/config/list"))
		{
			MG_HTTP_CHUNK_START;
			mg_http_printf_chunk(c, "{");
			mg_http_printf_chunk(c, "\"return_code\":%d}", list_config_action(c, p));
			MG_HTTP_CHUNK_END;
		}

		else if (mg_http_match_uri(hm, "/api/config/list-all"))
		{
			MG_HTTP_CHUNK_START;
			mg_http_printf_chunk(c, "{");
			mg_http_printf_chunk(c, "\"return_code\":%d}", list_all_config_action(c, p));
			MG_HTTP_CHUNK_END;
		}

		else if (mg_http_match_uri(hm, "/api/config/set-index/#"))
		{
			int ret = -1;

			if (hm->query.len >= 3 && hm->query.ptr[0] == 'i' && hm->query.ptr[1] == '=' && hm->uri.len >= 22)
			{
				char buffer[256];
				char buffer2[6];
				strncpy(buffer, hm->uri.ptr, MIN((int)hm->uri.len, 255));
				strncpy(buffer2, hm->query.ptr, MIN((int)hm->query.len, 5));
				buffer[MIN((int)hm->uri.len, 255)] = 0;
				buffer2[MIN((int)hm->query.len, 5)] = 0;
				char *name = buffer + 21;
				char *value = buffer2 + 2;
				ret = set_config_index_action(p, name, value);
			}

			mg_http_reply(c, 200, content_type_application_json, "{\"return_code\":%d}\n", ret);
		}

		else if (mg_http_match_uri(hm, "/api/capture-image"))
		{
			MG_HTTP_CHUNK_START;
			mg_http_printf_chunk(c, "{");
			mg_http_printf_chunk(c, "\"return_code\":%d}\n", capture_generic(c, GP_CAPTURE_IMAGE, NULL, 0));
			MG_HTTP_CHUNK_END;
		}

		else if (mg_http_match_uri(hm, "/api/capture-image-download"))
		{
			MG_HTTP_CHUNK_START;
			mg_http_printf_chunk(c, "{");
			mg_http_printf_chunk(c, "\"return_code\":%d}\n", capture_generic(c, GP_CAPTURE_IMAGE, NULL, 1));
			MG_HTTP_CHUNK_END;
		}

		else if (mg_http_match_uri(hm, "/api/file/get/#"))
		{
			if (hm->uri.len >= 14)
			{
				char buffer[256];
				strncpy(buffer, hm->uri.ptr, MIN((int)hm->uri.len, 255));
				buffer[MIN((int)hm->uri.len, 255)] = 0;
				char *path = buffer + 13;
				if (get_file_http_common(c, path, GP_FILE_TYPE_NORMAL) != GP_OK)
				{
					mg_http_reply(c, 404, content_type_application_json, "{ \"error\":\"file not found\",\"return_code\":-1}\n");
				}
			}
		}

		else if (mg_http_match_uri(hm, "/api/file/delete/#"))
		{
			if (hm->uri.len >= 17)
			{
				char buffer[256];
				char *filename = NULL, *folder = NULL;

				strncpy(buffer, hm->uri.ptr, MIN((int)hm->uri.len, 255));
				buffer[MIN((int)hm->uri.len, 255)] = 0;
				char *path = buffer + 16;

				char *lr = strrchr(path, '/');
				if (lr != NULL)
				{
					*lr = 0;
					folder = path;
					filename = lr + 1;
				}

				MG_HTTP_CHUNK_START;
				if (filename != NULL && strlen(filename) > 0)
				{
					mg_http_printf_chunk(c, "{");
					mg_http_printf_chunk(c, "\"return_code\":%d}\n", delete_file_action(p, folder, filename));
				}
				else
				{
					mg_http_printf_chunk(c, "{ \"return_code\": -1 }\n");
				}
				MG_HTTP_CHUNK_END;

				MG_HTTP_CHUNK_START;
				mg_http_printf_chunk(c, "{");
				mg_http_printf_chunk(c, "\"return_code\":%d}\n", 0);
				MG_HTTP_CHUNK_END;
			}
		}

		else if (mg_http_match_uri(hm, "/api/file/list/#"))
		{
			if (hm->uri.len >= 16)
			{
				char buffer[256];
				strncpy(buffer, hm->uri.ptr, MIN((int)hm->uri.len, 255));
				buffer[MIN((int)hm->uri.len, 255)] = 0;
				char *path = buffer + 14;
				char *lastChar = path + strlen(path) - 1;

				if (*lastChar != '/')
				{
					strcat(lastChar, "/");
				}

				MG_HTTP_CHUNK_START;
				mg_http_printf_chunk(c, "{");
				mg_http_printf_chunk(c, "\"return_code\":%d}\n", list_files(c, path));
				MG_HTTP_CHUNK_END;
			}
		}

#ifdef HAVE_LIBEXIF
		else if (mg_http_match_uri(hm, "/api/file/exif/#"))
		{
			if (hm->uri.len >= 15)
			{
				char *newfilename = NULL, *newfolder = NULL;

				char buffer[256];
				strncpy(buffer, hm->uri.ptr, MIN((int)hm->uri.len, 255));
				buffer[MIN((int)hm->uri.len, 255)] = 0;
				char *path = buffer + 14;

				char *lr = strrchr(path, '/');
				if (lr != NULL)
				{
					*lr = 0;
					newfolder = path;
					newfilename = lr + 1;
				}

				if (newfilename != NULL && strlen(newfilename) > 0)
				{
					MG_HTTP_CHUNK_START;
					mg_http_printf_chunk(c, "{");
					mg_http_printf_chunk(c, "\"return_code\":%d}\n", print_exif_action(c, p, newfolder, newfilename));
					MG_HTTP_CHUNK_END;
				}
				else
				{
					mg_http_reply(c, 404, content_type_application_json, "{ \"error\":\"file not found\",\"return_code\":-1}\n");
				}
			}
		}
#endif

		else
		{
			mg_http_reply(c, 404, content_type_text_html, "<html><head><title>404</title></head><body><h1>Error: 404</h1>Page not found.</body></html>");
		}
	}
}

void webapi_server_initialize(void)
{
	webcfg.server_url[0] = 0;
	webcfg.auth_enabled = FALSE;
	webcfg.auth_user[0] = 0;
	webcfg.auth_password[0] = 0;
	webcfg.server_done = FALSE;
}

int webapi_server(GPParams *params)
{
	struct mg_mgr mgr;

	p = params;

	if (!webcfg.server_url[0])
	{
		strcpy(webcfg.server_url, s_http_addr);
	}

	printf("Starting GPhoto2 " VERSION " - WebAPI server " WEBAPI_SERVER_VERSION " - %s\n", webcfg.server_url);

	mg_log_set("2");
	mg_mgr_init(&mgr);
	mg_http_listen(&mgr, webcfg.server_url, fn, NULL);

	do
	{
		mg_mgr_poll(&mgr, 1000);
	} while (webcfg.server_done == FALSE);

	mg_mgr_poll(&mgr, 1000);
	mg_mgr_free(&mgr);

	puts("WebAPI server stopped.");
	return GP_OK;
}
