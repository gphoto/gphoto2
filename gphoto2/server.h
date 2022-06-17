#ifndef GPHOTO2_SERVER_H
#define GPHOTO2_SERVER_H

#include <gp-params.h>

#define WEBCFG_STR_LEN 63

typedef struct _webapi_server_config
{
  char server_url[WEBCFG_STR_LEN+1];
  char api_user[WEBCFG_STR_LEN+1];
  char api_password[WEBCFG_STR_LEN+1];
  int server_done;
} WebAPIServerConfig;


extern void webapi_server_initialize(void);
extern int webapi_server(GPParams *params);

extern WebAPIServerConfig webcfg;

#endif 
