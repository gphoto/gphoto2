#ifndef GPHOTO2_SERVER_H
#define GPHOTO2_SERVER_H

#include <gp-params.h>

#define WEBCFG_STR_LEN 63

typedef struct _webapi_server_config
{
  char server_url[WEBCFG_STR_LEN+1];
  char auth_enabled;
  char auth_user[WEBCFG_STR_LEN+1];
  char auth_password[WEBCFG_STR_LEN+1];
  char server_done;
  char html_root[256];
} WebAPIServerConfig;


extern void webapi_server_initialize(void);
extern int webapi_server(GPParams *params);

extern WebAPIServerConfig webcfg;

#endif 
