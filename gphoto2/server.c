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
#define _GNU_SOURCE

// this is definition not really nessesary, it is to tell
// intellisense (VScode) that GPHOTO2_WEBAPI is allways defined only.
// so we get less warnings within VScode
#ifndef GPHOTO2_WEBAPI
#define GPHOTO2_WEBAPI
#endif

#include "config.h"
#include "actions.h"
#include "globals.h"
#include "i18n.h"
#include "gphoto2-webapi.h"
#include "server.h"
#include "mongoose.h"
#include "version.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#define CR(result)      \
  {                     \
    int __r = (result); \
    if (__r < 0)        \
      return __r;       \
  }

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

#if MG_ARCH == MG_ARCH_WIN32 && MG_ENABLE_WINSOCK
#define MG_SOCK_ERRNO WSAGetLastError()
#ifndef SO_EXCLUSIVEADDRUSE
#define SO_EXCLUSIVEADDRUSE ((int)(~SO_REUSEADDR))
#pragma comment(lib, "ws2_32.lib")
#endif
#elif MG_ARCH == MG_ARCH_FREERTOS_TCP
#define MG_SOCK_ERRNO errno
typedef Socket_t SOCKET;
#define INVALID_SOCKET FREERTOS_INVALID_SOCKET
#elif MG_ARCH == MG_ARCH_TIRTOS
#define MG_SOCK_ERRNO errno
#define closesocket(x) close(x)
#else
#define MG_SOCK_ERRNO errno
#ifndef closesocket
#define closesocket(x) close(x)
#endif
#define INVALID_SOCKET (-1)
typedef int SOCKET;
#endif

#define FD(c_) ((SOCKET)(size_t)(c_)->fd)

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

static int
show_preview(struct mg_connection *c)
{
  CameraFile *file;
  char *data;
  unsigned long int size;

  CR(gp_file_new(&file));
  CR(gp_camera_capture_preview(p->camera, file, p->context));
  CR(gp_file_get_data_and_size(file, (const char **)&data, &size));

  const char *http_header = "HTTP/1.1 200 OK\r\n"
                            "Content-Length: %lu\r\n"
                            "Content-Type: image/jpeg\r\n\r\n";

  mg_printf(c, http_header, size);
  mg_send(c, data, size);

  free(data);

  return GP_OK;
}

static void start_thread(void (*f)(void *), void *p)
{

#ifdef _WIN32
  _beginthread((void(__cdecl *)(void *))f, 0, p);
#else

#define closesocket(x) close(x)

#undef __USE_EXTERN_INLINES
#include <pthread.h>

  pthread_t thread_id = (pthread_t)0;
  pthread_attr_t attr;
  (void)pthread_attr_init(&attr);
  (void)pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  pthread_create(&thread_id, &attr, (void *(*)(void *))f, p);
  pthread_attr_destroy(&attr);

#endif
}

static void producer_thread_function_d(void *param)
{
  char buffer[256];
  MG_INFO(("start"));

  CameraFile *file;
  char *data = NULL;
  unsigned long int size;

  struct mg_connection *c = (struct mg_connection *)(size_t)param;
  // uint64_t counter = 1l;
  int r;

  r = gp_file_new(&file);

  while (r == GP_OK && c->is_closing == 0)
  {
    // MG_INFO(("d - get frame %lu", counter++));
    r = gp_camera_capture_preview(p->camera, file, p->context);
    gp_file_get_data_and_size(file, (const char **)&data, &size);

    if (data == NULL || size == 0)
      continue; // Skip on file read error

    sprintf(buffer,
            "--foo\r\nContent-Type: image/jpeg\r\n"
            "Content-Length: %lu\r\n\r\n",
            (unsigned long)size);
    send(FD(c), (char *)buffer, strlen(buffer), 0);
    send(FD(c), (char *)data, size, 0);
    send(FD(c), (char *)"\r\n", 2, 0);
    c->recv.len = 0;
  }

  MG_INFO(("end"));
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

    else if (mg_http_match_uri(hm, "/api/show-preview"))
    {
      int ret = show_preview(c);

      if (ret != GP_OK)
      {
        mg_http_reply(c, 200, content_type_application_json, "{\"return_code\":%d}\n", ret);
      }
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

      if (hm->query.ptr[1] == '=' && hm->uri.len >= 22)
      {
        char buffer[256];
        char index[4];
        strncpy(buffer, hm->uri.ptr, MIN((int)hm->uri.len, 255));
        buffer[MIN((int)hm->uri.len, 255)] = 0;
        char *name = buffer + 21;

        if (mg_http_get_var(&hm->query, "i", index, sizeof(index)) > 0)
        {
          ret = set_config_index_action(p, name, index);
        }
      }

      mg_http_reply(c, 200, content_type_application_json, "{\"return_code\":%d}\n", ret);
    }

    else if (mg_http_match_uri(hm, "/api/config/get/#"))
    {
      char buffer[256];
      strncpy(buffer, hm->uri.ptr, MIN((int)hm->uri.len, 255));
      buffer[MIN((int)hm->uri.len, 255)] = 0;
      char *name = buffer + 15;

      MG_HTTP_CHUNK_START;
      mg_http_printf_chunk(c, "{ \"path\": \"%s\"", name);
      mg_http_printf_chunk(c, ",\"return_code\":%d}", get_config_action(c, p, name));
      MG_HTTP_CHUNK_END;
    }

    else if (mg_http_match_uri(hm, "/api/config/set/#"))
    {
      int ret = -1;
      if (hm->query.ptr[1] == '=' && hm->uri.len >= 16)
      {
        char buffer[256];
        char value[1024];
        strncpy(buffer, hm->uri.ptr, MIN((int)hm->uri.len, 255));
        buffer[MIN((int)hm->uri.len, 255)] = 0;
        char *name = buffer + 15;

        if (mg_http_get_var(&hm->query, "v", value, sizeof(value)) > 0)
        {
          ret = set_config_action(p, name, value);
        }
      }
      mg_http_reply(c, 200, content_type_application_json, "{\"return_code\":%d}\n", ret);
    }

    else if (mg_http_match_uri(hm, "/api/capture-image"))
    {
      MG_HTTP_CHUNK_START;
      mg_http_printf_chunk(c, "{ \"images\":[");
      mg_http_printf_chunk(c, "],\"return_code\":%d}\n", capture_generic(c, GP_CAPTURE_IMAGE, NULL, 0));
      MG_HTTP_CHUNK_END;
    }

    else if (mg_http_match_uri(hm, "/api/capture-image-download"))
    {
      MG_HTTP_CHUNK_START;
      mg_http_printf_chunk(c, "{ \"images\":[");
      mg_http_printf_chunk(c, "],\"return_code\":%d}\n", capture_generic(c, GP_CAPTURE_IMAGE, NULL, 1));
      MG_HTTP_CHUNK_END;
    }

    else if (mg_http_match_uri(hm, "/api/capture-movie"))
    {
      c->label[0] = 'M';

      const char *http_header = "HTTP/1.0 200 OK\r\n"
                                "Cache-Control: no-cache\r\n"
                                "Pragma: no-cache\r\nExpires: Thu, 01 Dec 1994 16:00:00 GMT\r\n"
                                "Content-Type: multipart/x-mixed-replace; boundary=--foo\r\n\r\n";

      send(FD(c), (char *)http_header, strlen(http_header), 0);
      c->recv.len = 0;

      start_thread(producer_thread_function_d, (void *)(size_t)c);
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

  else if (ev == MG_EV_CLOSE)
  {
    printf("MG_EV_CLOSE connection label = %c (%d)\n", c->label[0], c->label[0]);
  }
}

#ifdef _WIN32
#define FILE_SEPARATOR "\\"
#else
#define FILE_SEPARATOR "/"
#endif

#define CFG_FOLDER ".gphoto"
#define CFG_FILENAME "webapi.config"
#define CFG_SERVER_URL "server_url="
#define CFG_AUTH_ENABLED "auth_enabled="
#define CFG_AUTH_USER "auth_user="
#define CFG_AUTH_PASSWORD "auth_password="
#define ISIZEOF(x) ((int)sizeof(x) - 1)

static char *rtrim(char *s)
{
  char *back = s + strlen(s);
  while (isspace(*--back))
    ;
  *(back + 1) = '\0';
  return s;
}

static void get_cfgfolder(char *cfgname)
{
  strcpy(cfgname, getenv("HOME"));
  strcat(cfgname, FILE_SEPARATOR);
  strcat(cfgname, CFG_FOLDER);
}

static void get_cfgname(char *cfgname)
{
  get_cfgfolder(cfgname);
  strcat(cfgname, FILE_SEPARATOR);
  strcat(cfgname, CFG_FILENAME);
}

static void webcfg_print_config(FILE *out)
{
  fprintf(out, CFG_SERVER_URL "%s\n", webcfg.server_url);
  fprintf(out, CFG_AUTH_ENABLED "%s\n", webcfg.auth_enabled ? "true" : "false");
  fprintf(out, CFG_AUTH_USER "%s\n", webcfg.auth_user);
  fprintf(out, CFG_AUTH_PASSWORD "%s\n", webcfg.auth_password);
}

static void webcfg_write_config()
{
  char cfgname[256];
  get_cfgfolder(cfgname);
  mkdir(cfgname, 0755);
  get_cfgname(cfgname);
  FILE *cfgfile = fopen(cfgname, "w");
  if (cfgfile == NULL)
  {
    fprintf(stderr, "ERROR: Can not write %s file.", cfgname);
    return;
  }
  webcfg_print_config(cfgfile);
  fclose(cfgfile);
}

static void webcfg_read_config()
{
  char cfgname[256];
  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  get_cfgname(cfgname);
  FILE *cfgfile = fopen(cfgname, "r");

  if (cfgfile == NULL)
    return;

  while ((read = getline(&line, &len, cfgfile)) != -1)
  {
    rtrim(line);

    if (strncmp(CFG_SERVER_URL, line, ISIZEOF(CFG_SERVER_URL)) == 0)
    {
      strcpy(webcfg.server_url, line + ISIZEOF(CFG_SERVER_URL));
      continue;
    }

    if (strncmp(CFG_AUTH_ENABLED, line, ISIZEOF(CFG_AUTH_ENABLED)) == 0)
    {
      if (strcmp("true", line + ISIZEOF(CFG_AUTH_ENABLED)) == 0)
      {
        webcfg.auth_enabled = true;
      }
      continue;
    }

    if (strncmp(CFG_AUTH_USER, line, ISIZEOF(CFG_AUTH_USER)) == 0)
    {
      strcpy(webcfg.auth_user, line + ISIZEOF(CFG_AUTH_USER));
      continue;
    }

    if (strncmp(CFG_AUTH_PASSWORD, line, ISIZEOF(CFG_AUTH_PASSWORD)) == 0)
    {
      strcpy(webcfg.auth_password, line + ISIZEOF(CFG_AUTH_PASSWORD));
      continue;
    }
  }

  free(line);
  fclose(cfgfile);
}

void webapi_server_initialize()
{
  webcfg.server_url[0] = 0;
  webcfg.auth_enabled = FALSE;
  webcfg.auth_user[0] = 0;
  webcfg.auth_password[0] = 0;
  webcfg.server_done = FALSE;
  webcfg_read_config();
}

int webapi_server(GPParams *params)
{
  struct mg_mgr mgr;

  p = params;

  if (!webcfg.server_url[0])
  {
    strcpy(webcfg.server_url, s_http_addr);
  }

  printf("\nStarting GPhoto2 " VERSION " - WebAPI server " WEBAPI_SERVER_VERSION " - %s\n", webcfg.server_url);
  webcfg_write_config();

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
