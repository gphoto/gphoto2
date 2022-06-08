/* server.c:
 *
 * Copyright (c) 2002 Lutz M�ller <lutz@users.sourceforge.net>
 * Copyright (c) 2022 Thorsten Ludewig <t.ludewig@gmail.com>
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

#include "config.h"
#include "actions.h"
#include "globals.h"
#include "i18n.h"
#include "main.h"
#include "server.h"
#include "version.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_RL
#  include <readline/readline.h>
#  include <readline/history.h>
#endif

#include "mongoose.h"

static const char *s_http_addr = "http://0.0.0.0:8866";    // HTTP port

static const char *content_type_application_json = "Content-Type: application/json\r\n";

static const char *http_chunked_header = "HTTP/1.1 200 OK\r\n"
																				 "Content-Type: application/json\r\n"
																				 "Transfer-Encoding: chunked\r\n\r\n";

#define MG_HTTP_CHUNK_START mg_printf(c, http_chunked_header )
#define MG_HTTP_CHUNK_END		mg_http_printf_chunk(c, "")

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifdef __GNUC__
#define __unused__ __attribute__((unused))
#else
#define __unused__
#endif

#define CR(result)       {int __r=(result); if (__r<0) return __r;}
#define CRU(result,file) {int __r=(result); if (__r<0) {gp_file_unref(file);return __r;}}
#define CL(result,list)  {int __r=(result); if (__r<0) {gp_list_free(list); return __r;}}

#define CHECK_NULL(x) { if (x == NULL) { return(-1); /* FIXME: what code? */ } }

static GPParams *p = NULL;
static int mg_server_done;

static size_t
my_strftime(char *s, size_t max, const char *fmt, const struct tm *tm)
{
	return strftime(s, max, fmt, tm);
}

static int
server_print_widget (GPParams *p, struct mg_connection *c, const char *name, CameraWidget *widget) 
{
	const char *label;
	CameraWidgetType	type;
	int ret, readonly;

	ret = gp_widget_get_type (widget, &type);
	if (ret != GP_OK)
		return ret;
	ret = gp_widget_get_label (widget, &label);
	if (ret != GP_OK)
		return ret;
		
	ret = gp_widget_get_readonly (widget, &readonly);
	if (ret != GP_OK)
		return ret;

	mg_http_printf_chunk(c,", \"label\": \"%s\"", label); /* "Label:" is not i18ned, the "label" variable is */
	mg_http_printf_chunk(c,", \"readonly\": %s", (readonly) ? "true" : "false" );

	switch (type) {
	case GP_WIDGET_TEXT: {		/* char *		*/
		char *txt;

		ret = gp_widget_get_value (widget, &txt);
		if (ret == GP_OK) {
			mg_http_printf_chunk(c,", \"type\": \"TEXT\""); /* parsed by scripts, no i18n */
			mg_http_printf_chunk(c,", \"current\": \"%s\"", txt);
		} else {
			gp_context_error (p->context, _("Failed to retrieve value of text widget %s."), name);
		}
		break;
	}
	case GP_WIDGET_RANGE: {	/* float		*/
		float	f, t,b,s;

		ret = gp_widget_get_range (widget, &b, &t, &s);
		if (ret == GP_OK)
			ret = gp_widget_get_value (widget, &f);
		if (ret == GP_OK) {
			mg_http_printf_chunk(c,", \"type\": \"RANGE\"");	/* parsed by scripts, no i18n */
			mg_http_printf_chunk(c,", \"current\": %g\n", f);	/* parsed by scripts, no i18n */
			mg_http_printf_chunk(c,", \"bottom\": %g\n", b);	/* parsed by scripts, no i18n */
			mg_http_printf_chunk(c,", \"top\": %g\n", t);	/* parsed by scripts, no i18n */
			mg_http_printf_chunk(c,", \"step\": %g\n", s);	/* parsed by scripts, no i18n */
		} else {
			gp_context_error (p->context, _("Failed to retrieve values of range widget %s."), name);
		}
		break;
	}
	case GP_WIDGET_TOGGLE: {	/* int		*/
		int	t;

		ret = gp_widget_get_value (widget, &t);
		if (ret == GP_OK) {
			mg_http_printf_chunk(c,", \"type\": \"TOGGLE\"");
			mg_http_printf_chunk(c,", \"current\": %d",t);
		} else {
			gp_context_error (p->context, _("Failed to retrieve values of toggle widget %s."), name);
		}
		break;
	}
	case GP_WIDGET_DATE:  {		/* int			*/
		int	t;
		time_t	xtime;
		struct tm *xtm;
		char	timebuf[200];

		ret = gp_widget_get_value (widget, &t);
		if (ret != GP_OK) {
			gp_context_error (p->context, _("Failed to retrieve values of date/time widget %s."), name);
			break;
		}
		xtime = t;
		xtm = localtime (&xtime);
		ret = my_strftime (timebuf, sizeof(timebuf), "%c", xtm);
		mg_http_printf_chunk(c,", \"type\": \"DATE\"");
		mg_http_printf_chunk(c,", \"current\": %d", t);
		mg_http_printf_chunk(c,", \"printable\": \"%s\"", timebuf);
		mg_http_printf_chunk(c,", \"help\": \"%s\"", _("Use 'now' as the current time when setting."));
		break;
	}
	case GP_WIDGET_MENU:
	case GP_WIDGET_RADIO: { /* char *		*/
		int cnt, i;
		char *current;

		ret = gp_widget_get_value (widget, &current);
		if (ret == GP_OK) {
			cnt = gp_widget_count_choices (widget);
			if (type == GP_WIDGET_MENU)
				mg_http_printf_chunk(c,", \"type\": \"MENU\"");
			else
				mg_http_printf_chunk(c,", \"type\": \"RADIO\"");

			mg_http_printf_chunk(c,", \"current\": \"%s\"",current);
			mg_http_printf_chunk(c,", \"choice\": [",current);

			for ( i=0; i<cnt; i++) {
				const char *choice;
				ret = gp_widget_get_choice (widget, i, &choice);
				mg_http_printf_chunk(c,"%s{\"index\": %d, \"value\": \"%s\"}", (i==0) ? "" : ",", i, choice);
			}
			mg_http_printf_chunk(c,"]",current);


		} else {
			gp_context_error (p->context, _("Failed to retrieve values of radio widget %s."), name);
		}
		break;
	}

	/* ignore: */
	case GP_WIDGET_WINDOW:
	case GP_WIDGET_SECTION:
	case GP_WIDGET_BUTTON:
		break;
	}
	
	return GP_OK;
}


static void 
server_display_widgets (GPParams *p, struct mg_connection *c, CameraWidget *widget, 
                 char *prefix, int dumpval, char **firstChar) {
	int 	ret, n, i;
	char	*newprefix;
	const char *label, *name, *uselabel;
	CameraWidgetType	type;

	gp_widget_get_label (widget, &label);
	ret = gp_widget_get_name (widget, &name);
	gp_widget_get_type (widget, &type);

	if (strlen(name))
		uselabel = name;
	else
		uselabel = label;

	n = gp_widget_count_children (widget);

	newprefix = malloc(strlen(prefix)+1+strlen(uselabel)+1);

	if (!newprefix)
		abort();

	sprintf(newprefix,"%s/%s",prefix,uselabel);

	if ((type != GP_WIDGET_WINDOW) && (type != GP_WIDGET_SECTION)) 
	{
		if (dumpval) {
			mg_http_printf_chunk(c,"%s{\"path\":\"%s\"",*firstChar,newprefix);
			server_print_widget (p, c, newprefix, widget);
			mg_http_printf_chunk(c,"}\n",*firstChar,newprefix);
		} 
		else 
		{
  		mg_http_printf_chunk(c,"%s\"%s\"\n",*firstChar,newprefix);
		}
		*firstChar = ",";
	}

	for (i=0; i<n; i++) {
		CameraWidget *child;
	
		ret = gp_widget_get_child (widget, i, &child);
		if (ret != GP_OK)
			continue;

		server_display_widgets (p, c, child, newprefix, dumpval, firstChar );
	}
	free(newprefix);
}


static int 
server_list_config(GPParams *p, struct mg_connection *c, int dumpval) 
{
	CameraWidget *rootconfig;
	char *firstChar = " ";
	int	ret;

	ret = gp_camera_get_config (p->camera, &rootconfig, p->context);
  mg_http_printf_chunk(c, "{\"return_code\":%d", ret );

	if (ret == GP_OK)
  { 
    mg_http_printf_chunk(c, ",\"result\":[\n" );
	  server_display_widgets (p, c, rootconfig, "", dumpval, &firstChar );
    mg_http_printf_chunk(c, "]\n" );
    gp_widget_free (rootconfig);
	}

  mg_http_printf_chunk(c, "}\n" );
	return (ret);
}


static int 
server_auto_detect(GPParams *p, struct mg_connection *c)
{
	int x, count;
  CameraList *list;
  const char *name = NULL, *value = NULL;

	_get_portinfo_list (p);
	count = gp_port_info_list_count (p->portinfo_list);

	CR (gp_list_new (&list));
  gp_abilities_list_detect (gp_params_abilities_list(p), p->portinfo_list, list, p->context);

  CL (count = gp_list_count (list), list);

  mg_http_printf_chunk( c, "{\"return_code\":%d,\"result\":[", 0 );
  char *firstChar = " ";

  for (x = 0; x < count; x++) {
    CL (gp_list_get_name  (list, x, &name), list);
    CL (gp_list_get_value (list, x, &value), list);
    mg_http_printf_chunk( c, "%s{\"model\":\"%s\",\"port\":\"%s\"}\n", firstChar, name, value );
		firstChar = ",";
  }

  mg_http_printf_chunk( c, "]}\n" );
	gp_list_free (list);
  return GP_OK;
}

static int
server_version(struct mg_connection *c)
{
	int n;

	for (n = 0; module_versions[n].name != NULL; n++) {
	  int i;
	  const char **v = NULL;
	  char *name = module_versions[n].name;
	  GPVersionFunc func = module_versions[n].version_func;
	  CHECK_NULL (name);
	  CHECK_NULL (func);
	  v = func(GP_VERSION_SHORT);
	  CHECK_NULL (v);
	  CHECK_NULL (v[0]);

	  mg_http_printf_chunk(c,", { \"name\": \"%s\", \"version\": \"%s\", \"libs\": \"", name, v[0]);
	  
		for (i = 1; v[i] != NULL; i++) {
		  if (v[i+1] != NULL)
			  mg_http_printf_chunk(c,"%s, ", v[i]);
		  else
			  mg_http_printf_chunk(c,"%s", v[i]);
	  }

	  mg_http_printf_chunk(c,"\"}");
	}

	return (GP_OK);
}


static int
server_find_widget_by_name (GPParams *p, const char *name, CameraWidget **child, CameraWidget **rootconfig) {
	int	ret;

	*rootconfig = NULL;
	ret = gp_camera_get_single_config (p->camera, name, child, p->context);
	if (ret == GP_OK) {
		*rootconfig = *child;
		return GP_OK;
	}

	ret = gp_camera_get_config (p->camera, rootconfig, p->context);
	if (ret != GP_OK) return ret;
	ret = gp_widget_get_child_by_name (*rootconfig, name, child);
	if (ret != GP_OK) 
		ret = gp_widget_get_child_by_label (*rootconfig, name, child);
	if (ret != GP_OK) {
		char		*part, *s, *newname;

		newname = strdup (name);
		if (!newname)
			return GP_ERROR_NO_MEMORY;

		*child = *rootconfig;
		part = newname;
		while (part[0] == '/')
			part++;
		while (1) {
			CameraWidget *tmp;

			s = strchr (part,'/');
			if (s)
				*s='\0';
			ret = gp_widget_get_child_by_name (*child, part, &tmp);
			if (ret != GP_OK)
				ret = gp_widget_get_child_by_label (*child, part, &tmp);
			if (ret != GP_OK)
				break;
			*child = tmp;
			if (!s) {
				/* end of path */
				free (newname);
				return GP_OK;
			}
			part = s+1;
			while (part[0] == '/')
				part++;
		}
		gp_context_error (p->context, _("%s not found in configuration tree."), newname);
		free (newname);
		gp_widget_free (*rootconfig);
		return GP_ERROR;
	}
	return GP_OK;
}


static int
server_set_config_index(GPParams *p, struct mg_connection *c, const char *name, const char *value) {
	CameraWidget *rootconfig,*child;
	int	ret;
	const char *label;
	CameraWidgetType	type;

	ret = server_find_widget_by_name (p, name, &child, &rootconfig);
	if (ret != GP_OK)
		return ret;

	ret = gp_widget_get_type (child, &type);
	if (ret != GP_OK) {
		gp_widget_free (rootconfig);
		return ret;
	}
	ret = gp_widget_get_label (child, &label);
	if (ret != GP_OK) {
		gp_widget_free (rootconfig);
		return ret;
	}

	switch (type) {
	case GP_WIDGET_MENU:
	case GP_WIDGET_RADIO: { /* char *		*/
		int cnt, i;

		cnt = gp_widget_count_choices (child);
		if (cnt < GP_OK) {
			ret = cnt;
			break;
		}
		ret = GP_ERROR_BAD_PARAMETERS;
		if (sscanf (value, "%d", &i)) {
			if ((i>= 0) && (i < cnt)) {
				const char *choice;

				ret = gp_widget_get_choice (child, i, &choice);
				if (ret == GP_OK)
					ret = gp_widget_set_value (child, choice);
				break;
			}
		}
		gp_context_error (p->context, _("Choice %s not found within list of choices."), value);
		break;
	}

	/* ignore: */
	case GP_WIDGET_TOGGLE:
	case GP_WIDGET_TEXT:
	case GP_WIDGET_RANGE:
	case GP_WIDGET_DATE: 
	case GP_WIDGET_WINDOW:
	case GP_WIDGET_SECTION:
	case GP_WIDGET_BUTTON:
		gp_context_error (p->context, _("The %s widget has no indexed list of choices. Use --set-config-value instead."), name);
		ret = GP_ERROR_BAD_PARAMETERS;
		break;
	}
	if (ret == GP_OK) {
		if (child == rootconfig)
			ret = gp_camera_set_single_config (p->camera, name, child, p->context);
		else
			ret = gp_camera_set_config (p->camera, rootconfig, p->context);
		if (ret != GP_OK)
			gp_context_error (p->context, _("Failed to set new configuration value %s for configuration entry %s."), value, name);
	}
	gp_widget_free (rootconfig);
	return (ret);
}


static void server_http_version(struct mg_connection *c)
{
  MG_HTTP_CHUNK_START;
  mg_http_printf_chunk(c, "{ \"result\": [{ \"name\": \"mongoose\", \"version\": \"%s\"}", MG_VERSION );
  server_version(c);
  mg_http_printf_chunk(c, "]}\n");
  MG_HTTP_CHUNK_END;
}


static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {

	if (ev == MG_EV_HTTP_MSG) 
	{
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;

    if (mg_http_match_uri(hm, "/")) 
		{
      server_http_version(c);
    }

		else if (mg_http_match_uri(hm, "/api/version")) 
		{
      server_http_version(c);
    } 
		
		else if (mg_http_match_uri(hm, "/api/server/shutdown")) 
		{
      mg_http_reply(c, 200, content_type_application_json, "{\"return_code\":0}\n" );
			mg_server_done = TRUE;
    } 
		
		else if (mg_http_match_uri(hm, "/api/auto-detect")) 
		{
      MG_HTTP_CHUNK_START;
   	  server_auto_detect(p, c);
			MG_HTTP_CHUNK_END;
    } 
		
		else if (mg_http_match_uri(hm, "/api/trigger-capture")) 
		{			
      MG_HTTP_CHUNK_START;
      mg_http_printf_chunk(c, "{\"return_code\":%d}\n", trigger_capture());
			MG_HTTP_CHUNK_END;
    } 

		else if (mg_http_match_uri(hm, "/api/config/list")) 
		{	
      MG_HTTP_CHUNK_START;
			server_list_config(p, c, 0);
			MG_HTTP_CHUNK_END;
    } 
		
		else if (mg_http_match_uri(hm, "/api/config/list/all")) 
		{	
      MG_HTTP_CHUNK_START;
			server_list_config(p, c, 1);
			MG_HTTP_CHUNK_END;
    } 
		
		else if (mg_http_match_uri( hm, "/api/config/set-index/#")) 
		{	
			int ret = -1;

			if ( hm->query.len >= 3 && hm->query.ptr[0] == 'i' 
			   && hm->query.ptr[1] == '=' && hm->uri.len >= 22 )
			{
  			char buffer[256];
	  		char buffer2[6];
		  	strncpy( buffer, hm->uri.ptr, MIN((int)hm->uri.len,255));
			  strncpy( buffer2, hm->query.ptr, MIN((int)hm->query.len,5));
			  buffer[MIN((int)hm->uri.len,255)] = 0;
			  buffer2[MIN((int)hm->query.len,5)] = 0;
			  char *name = buffer+21;
			  char *value = buffer2+2;
				ret = server_set_config_index( p, c, name, value );
			}

      mg_http_reply(c, 200, content_type_application_json, "{\"return_code\":%d}\n", ret );
    } 

    else
		{
			mg_http_reply(c, 404, "", "Page not found.\n" );
		}
  }
  (void) fn_data;
}


// server main
int server(GPParams *params)
{
	p = params;
  struct mg_mgr mgr;
  mg_server_done = FALSE;

	puts( "Starting GPhoto2 " VERSION " REST server.");

  mg_log_set("2"); 
  mg_mgr_init(&mgr);
  mg_http_listen(&mgr, s_http_addr, fn, NULL);

	do
	{
    mg_mgr_poll(&mgr, 1000); 
	}
	while( mg_server_done == FALSE );

  mg_mgr_poll(&mgr, 1000);
  mg_mgr_free(&mgr);

	return 0;
}
