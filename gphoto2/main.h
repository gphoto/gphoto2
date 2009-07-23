/* main.h
 *
 * Copyright © 2002 Lutz Müller <lutz@users.sourceforge.net>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __MAIN_H__
#define __MAIN_H__

#include <gphoto2/gphoto2-file.h>
#include <gphoto2/gphoto2-camera.h>
#include <gp-params.h>

#define MAX_IMAGE_NUMBER                16384

#ifdef WIN32
#include <io.h>
#define VERSION "2"
#endif

void 	cli_error_print(char *format, ...);

int	camera_file_exists (Camera *camera, GPContext *context,
			    const char *folder, const char *filename,
			    CameraFileType type);
int	save_file_to_file (Camera *camera, GPContext *context, Flags flags,
			   const char *folder, const char *filename,
			   CameraFileType type);
int	save_camera_file_to_file (const char *folder, CameraFile *file, const char *tmpname);
int	capture_generic (CameraCaptureType type, const char *name, int download);
int	get_file_common (const char *arg, CameraFileType type );

#endif /* __MAIN_H__ */


/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
