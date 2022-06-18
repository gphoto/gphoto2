/* gp-params.h
 *
 * Copyright © 2002 Lutz Müller <lutz@users.sourceforge.net>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef GPHOTO2_GP_PARAMS_H
#define GPHOTO2_GP_PARAMS_H

#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-abilities-list.h>
#include <gphoto2/gphoto2-context.h>

typedef enum {
	FLAGS_RECURSE		= 1 << 0,
	FLAGS_REVERSE		= 1 << 1,
	FLAGS_QUIET		= 1 << 2,
	FLAGS_FORCE_OVERWRITE	= 1 << 3,
	FLAGS_STDOUT		= 1 << 4,
	FLAGS_STDOUT_SIZE	= 1 << 5,
	FLAGS_NEW		= 1 << 6,
	FLAGS_RESET_CAPTURE_INTERVAL = 1 << 7,
	FLAGS_KEEP 		= 1 << 8,
	FLAGS_KEEP_RAW 		= 1 << 9,
	FLAGS_SKIP_EXISTING	= 1 << 10,
	FLAGS_PARSABLE		= 1 << 11,
} Flags;

typedef enum {
	MULTI_UPLOAD,
	MULTI_UPLOAD_META,
	MULTI_DOWNLOAD,
	MULTI_DELETE
} MultiType;

typedef struct _GPParams GPParams;
struct _GPParams {
	Camera		*camera;
	GPContext	*context;
	char		*folder;
	char		*filename;

	unsigned int	filenr;	/* for --filename %n */

	unsigned int	cols;

	Flags		flags;

	/** This field is supposed to be private. Usually, you use the
	 * gp_camera_abilities_list() function to access it.
	 */
	CameraAbilitiesList *_abilities_list;

	GPPortInfoList	*portinfo_list;
	int		debug_func_id;

	MultiType	multi_type;
	CameraFileType	download_type; /* for multi download */
 
	char		*hook_script; /* If non-NULL, hook script to run */
	char		**envp;  /* envp from the main() function */
};

void gp_params_init (GPParams *params, char **envp);
void gp_params_exit (GPParams *params);

/* Use only this function to access the abilities_list member of the
 * GPParams structure. This function makes sure that the
 * abilities_list is only iniatilized if it is actually used. */
CameraAbilitiesList *gp_params_abilities_list (GPParams *params);

int gp_params_run_hook (GPParams *params, const char *command, const char *argument);

#endif /* !defined(GPHOTO2_GP_PARAMS_H) */


/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
