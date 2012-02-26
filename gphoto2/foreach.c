/* frontend.c
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

#include "config.h"
#include "actions.h"
#include "foreach.h"
#include "i18n.h"
#include "main.h"
#include "range.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <gphoto2/gphoto2-port-log.h>

#define GP_ERROR_FRONTEND_BAD_ID -10000
#define CR(result) {int __r=(result); if(__r<0) return(__r);}
#define CL(result,list) {int __r=(result); if(__r<0) {gp_list_free(list);return(__r);}}

#define GP_MODULE "frontend"

static struct {
	FolderAction *action;
	const char *name;
} FolderActions[] = {
	{delete_all_action, "Delete all Files"},
	{list_files_action, "List Files"},
	{list_folders_action, "List Folders"},
	{num_files_action, "Number of Files"},
	{NULL, NULL}
};

int
for_each_folder (GPParams *p, FolderAction action)
{
	CameraList *list;
	int r, i, count;
	const char *name = NULL;
	char *f = NULL;

	if (!p)
		return (GP_ERROR_BAD_PARAMETERS);

	/* Execute the action for this folder. */
	for (i = 0; FolderActions[i].name; i++)
		if (FolderActions[i].action == action)
			break;
	gp_log (GP_LOG_DEBUG, "foreach",
		"Executing action '%s' for folder '%s'.",
		FolderActions[i].name, p->folder);
	CR (action (p));

	/* If no recursion is requested, we are done. */
	if (!(p->flags & FLAGS_RECURSE))
		return GP_OK;

	CR (gp_list_new (&list));
	/* Recursion requested. Descend into subfolders. */
	CL (gp_camera_folder_list_folders (p->camera, p->folder, list,
					   p->context), list);
	CL (count = gp_list_count (list), list); 
	if (p->flags & FLAGS_REVERSE) {
		for (i = count - 1; i >= 0; i--) {
			CL (gp_list_get_name (list, i, &name), list);
			f = p->folder;
			p->folder = malloc (sizeof (char) *
				(strlen (f) + 1 + strlen (name) + 1));
			if (!p->folder) {
				p->folder = f;
				gp_list_free (list);
				return (GP_ERROR_NO_MEMORY);
			}
			strcpy (p->folder, f);
			if (p->folder[strlen (p->folder) - 1] != '/')
				strcat (p->folder, "/");
			strcat (p->folder, name);
			r = for_each_folder (p, action);
			free (p->folder);
			p->folder = f;
			CL (r, list);
		}
	} else {
		for (i = 0; i < count; i++) {
			CL (gp_list_get_name (list, i, &name), list);
			f = p->folder;
			p->folder = malloc (sizeof (char) * 
				(strlen (f) + 1 + strlen (name) + 1));
			if (!p->folder) {
				p->folder = f;
				gp_list_free (list);
				return (GP_ERROR_NO_MEMORY);
			}
			strcpy (p->folder, f);
			if (p->folder[strlen (p->folder) - 1] != '/')
				strcat (p->folder, "/");
			strcat (p->folder, name);
			r = for_each_folder (p, action);
			free (p->folder);
			p->folder = f;
			CL (r, list);
		}
	}
	gp_list_free (list);
	return (GP_OK);
}

int
for_each_file (GPParams *p, FileAction action)
{
	CameraList *list;
	int i, count, r;
	const char *name = NULL;
	char *f = NULL;

	CR (gp_list_new (&list));
	/* Iterate on all files */
	CR (gp_camera_folder_list_files (p->camera, p->folder, list,
					 p->context));
	CR (count = gp_list_count (list));
	if (p->flags & FLAGS_REVERSE) {
		for (i = count ; i--; ) {
			CL (gp_list_get_name (list, i, &name), list);
			CL (action (p, name), list);
		}
	} else {
		for (i = 0; i < count; i++) {
			CL (gp_list_get_name (list, i, &name), list);
			CL (action (p, name), list);
		}
	}

	/* If no recursion is requested, we are done. */
	if (!(p->flags & FLAGS_RECURSE)) {
		gp_list_free (list);
		return (GP_OK);
	}

	/* Recursion requested. Descend into subfolders. */
	CL (gp_camera_folder_list_folders (p->camera, p->folder,
					   list, p->context), list);
	CL (count = gp_list_count (list), list);
	for (i = 0; i < count; i++) {
		CL (gp_list_get_name (list, i, &name), list);
		f = p->folder;
		p->folder = malloc (sizeof (char) *
				(strlen (f) + 1 + strlen (name) + 1));
		if (!p->folder) {
			p->folder = f;
			gp_list_free (list);
			return (GP_ERROR_NO_MEMORY);
		}
		strcpy (p->folder, f);
		if (p->folder[strlen (p->folder) - 1] != '/')
			strcat (p->folder, "/");
		strcat (p->folder, name);
		r = for_each_file (p, action);
		free (p->folder);
		p->folder = f;
		CL (r, list);
	}
	gp_list_free (list);
	return (GP_OK);
}

#define MAX_FOLDER_LEN 1024
#define MAX_FILE_LEN   1024

static int
get_path_for_id_rec (GPParams *p,
		     const char *base_folder, unsigned int id,
		     unsigned int *base_id, char *folder,
		     char *filename)
{
	char subfolder[1024];
	int n_folders, n_files, r;
	unsigned int i;
	const char *name;
	CameraList *list;

	strncpy (folder, base_folder, MAX_FOLDER_LEN);
	CR (gp_list_new(&list));
	CL (gp_camera_folder_list_files (p->camera, base_folder, list,
					 p->context), list);
	CL (n_files = gp_list_count (list), list);
	if (id - *base_id < (unsigned int) n_files) {

		/* ID is in this folder */
		GP_DEBUG ("ID %i is in folder '%s'.", id, base_folder);
		CL (gp_list_get_name (list, id - *base_id, &name), list);
		strncpy (filename, name, MAX_FILE_LEN);
		gp_list_free (list);
		return (GP_OK);
	} else {
		/* Look for IDs in subfolders */
		GP_DEBUG ("ID %i is not in folder '%s'.", id, base_folder);
		*base_id += n_files;
		CL (gp_camera_folder_list_folders (p->camera, base_folder,
						   list, p->context), list);
		CL (n_folders = gp_list_count (list), list);
		for (i = 0; i < (unsigned int)n_folders; i++) {
			CL (gp_list_get_name (list, i, &name), list);
			strncpy (subfolder, base_folder, sizeof (subfolder));
			if (strlen (base_folder) > 1)
				strncat (subfolder, "/", sizeof (subfolder) - strlen(subfolder) - 1);
			strncat (subfolder, name, sizeof (subfolder) - strlen(subfolder) - 1);
			r = get_path_for_id_rec (p, subfolder, id, base_id,
						 folder, filename);
			switch (r) {
			case GP_ERROR_FRONTEND_BAD_ID:
				break;
			default:
				gp_list_free (list);
				return (r);
			}
		}
		gp_list_free (list);
		return (GP_ERROR_FRONTEND_BAD_ID);
	}
}

static int
get_path_for_id (GPParams *p, const char *base_folder,
		 unsigned int id, char *folder, char *filename)
{
	int r;
	unsigned int base_id;
	const char *name;

	strncpy (folder, base_folder, MAX_FOLDER_LEN);
	if (p->flags & FLAGS_RECURSE) {
                base_id = 0;
                r = get_path_for_id_rec (p, base_folder, id, &base_id, folder,
                                         filename);
                switch (r) {
                case GP_ERROR_FRONTEND_BAD_ID:
                        gp_context_error (p->context, _("Bad file number. "
                                "You specified %i, but there are only %i "
                                "files available in '%s' or its subfolders. "
                                "Please obtain a valid file number from "
                                "a file listing first."), id + 1, base_id,
                                base_folder);
                        return (GP_ERROR_BAD_PARAMETERS);
                default:
                        return (r);
                }
	} else {
		CameraList *list;
		int list_count;

		/* If we have no recursion, things are easy. */
		GP_DEBUG ("No recursion. Taking file %i from folder '%s'.",
			  id, base_folder);
		CR (gp_list_new (&list));
		CL (gp_camera_folder_list_files (p->camera, base_folder,
						 list, p->context), list);
		CL ((list_count = gp_list_count (list)), list);
		if (id >= (unsigned int) list_count) {
			switch (list_count) {
			case 0:
				gp_context_error (p->context,
					_("There are no files in "
					"folder '%s'."), base_folder);
				gp_list_free (list);
				return (GP_ERROR_BAD_PARAMETERS);
			case 1:
				gp_context_error (p->context, 
					_("Bad file number. "
					"You specified %i, but there is only "
					"1 file available in '%s'."), id + 1,
					base_folder);
				gp_list_free (list);
				return (GP_ERROR_BAD_PARAMETERS);
			default:
				gp_context_error (p->context,
					_("Bad file number. "
					"You specified %i, but there are only "
					"%i files available in '%s'. "
					"Please obtain a valid file number "
					"from a file listing first."), id + 1,
					gp_list_count (list), base_folder);
				gp_list_free (list);
				return (GP_ERROR_BAD_PARAMETERS);
			}
		}
		CL (gp_list_get_name (list, id, &name), list);
		strncpy (filename, name, MAX_FILE_LEN);
		gp_list_free (list);
		return (GP_OK);
	}
}

int
for_each_file_in_range (GPParams *p, FileAction action,
			const char *range)
{
	char	index[MAX_IMAGE_NUMBER];
	int 	i, max = 0, r;
	char ffolder[MAX_FOLDER_LEN], ffile[MAX_FILE_LEN], *f;

	memset(index, 0, MAX_IMAGE_NUMBER);

	CR (parse_range (range, index, p->context));

	for (max = MAX_IMAGE_NUMBER - 1; !index[max]; max--);
	
	if (p->flags & FLAGS_REVERSE) {
		for (i = max; 0 <= i; i--)
			if (index[i]) {
				CR (get_path_for_id (p, p->folder,
					(unsigned int) i, ffolder, ffile));
				r = action (p, ffile);
				if (r == GP_OK) continue;
				/* some cameras do not support downloads of some files */
				if (r == GP_ERROR_NOT_SUPPORTED) continue;
				return r;
			}
	} else {
		unsigned int count;

		/*
		 * File deletion modifies the CameraFilesystem. Therefore
		 * the IDs of subsequent images change. This only affects us
		 * if not deleting reversely. This is the case here and we need
		 * to adjust the image IDs after a successful deletion.
		 */
		for (count = i = 0; i <= max; i++)
			if (index[i]) {
				GP_DEBUG ("Now processing ID %i "
					  "(originally %i)...", i - count, i);
				CR (get_path_for_id (p, p->folder,
					(unsigned int) i - count,
					ffolder, ffile));
				f = p->folder;
				p->folder = ffolder;
				r = action (p, ffile);
				p->folder = f;
				/* some cameras do not support downloads of some files */
				if ((r != GP_OK) && (r != GP_ERROR_NOT_SUPPORTED)) return r;
				if (action == delete_file_action)
					count++;
			}
	}
		
	return (GP_OK);
}


/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
