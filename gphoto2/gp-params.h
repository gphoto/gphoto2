/* gp-params.h
 *
 * Copyright (C) 2002 Lutz Müller <lutz@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#ifndef __GP_PARAMS_H__
#define __GP_PARAMS_H__

#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-abilities-list.h>
#include <gphoto2/gphoto2-context.h>

typedef enum _ForEachFlags ForEachFlags;
enum _ForEachFlags {
	FOR_EACH_FLAGS_RECURSE = 1 << 0,
	FOR_EACH_FLAGS_REVERSE = 1 << 1
};

typedef struct _GPParams GPParams;
struct _GPParams {
	Camera *camera;
	GPContext *context;
	char *folder;
	char *filename;

	unsigned int cols;
	char quiet, force;

	ForEachFlags flags;

	CameraAbilitiesList *abilities_list;
};

void gp_params_init (GPParams *params);
void gp_params_exit (GPParams *params);

#endif
