/* gphoto2-webapi.h
 *
 * Copyright © 2002 Lutz Müller <lutz@users.sourceforge.net>
 * Copyright © 2022 Thorsten Ludewig <t.ludewig@gmail.com>
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

#ifndef GPHOTO2_WEBAPI_H
#define GPHOTO2_WEBAPI_H

#include <gphoto2/gphoto2-file.h>
#include <gphoto2/gphoto2-camera.h>
#include <gp-params.h>

#include "mongoose.h"
#define JSON_PRINTF mg_http_printf_chunk

#define MAX_IMAGE_NUMBER                65536

#ifdef WIN32
#include <io.h>
#define VERSION "2"
#endif

#define WEBAPI_SERVER_VERSION "0.0.4"

void cli_error_print(char *format, ...);
void dissolve_filename ( const char *folder, const char *filename, char **newfolder, char **newfilename );
size_t strncpy_lower(char *dst, const char *src, size_t count);

int	camera_file_exists (Camera *camera, GPContext *context,
			    const char *folder, const char *filename,
			    CameraFileType type);
int	save_file_to_file (struct mg_connection *c, Camera *camera, GPContext *context, Flags flags,
			   const char *folder, const char *filename, CameraFileType type, int webapi);
int	save_camera_file_to_file (const char *folder, const char *fn, CameraFileType type, CameraFile *file, const char *tmpname);
int	capture_generic (struct mg_connection *c, CameraCaptureType type, const char *name, int download);
int	get_file_common (struct mg_connection *c, const char *arg, CameraFileType type );
int get_file_http_common (struct mg_connection *c, const char *arg, CameraFileType type );

int	trigger_capture (void);

#endif /* !defined(GPHOTO2_WEBAPI_H) */


/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
