/* actions.h
 *
 * Copyright (C) 2002 Lutz Müller <lutz@users.sourceforge.net>
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

#ifndef __ACTIONS_H__
#define __ACTIONS_H__

#include <gphoto2-camera.h>
#include <gphoto2-context.h>

typedef struct _ActionParams ActionParams;
struct _ActionParams {
	Camera *camera;
	GPContext *context;
	const char *folder;
};

/* Image actions */
typedef int FileAction    (ActionParams *params, const char *filename);
int print_file_action     (ActionParams *params, const char *filename);
int print_exif_action     (ActionParams *params, const char *filename);
int print_info_action     (ActionParams *params, const char *filename);
int save_file_action      (ActionParams *params, const char *filename);
int save_thumbnail_action (ActionParams *params, const char *filename);
int save_raw_action       (ActionParams *params, const char *filename);
int save_audio_action     (ActionParams *params, const char *filename);
int save_exif_action      (ActionParams *params, const char *filename);
int delete_file_action    (ActionParams *params, const char *filename);

/* Folder actions */
typedef int FolderAction  (ActionParams *params);
int delete_all_action     (ActionParams *params);
int list_files_action     (ActionParams *params);
int list_folders_action   (ActionParams *params);
int num_files_action      (ActionParams *params);

/* Camera actions */
int action_camera_about           (Camera *);
int action_camera_summary         (Camera *);
int action_camera_manual          (Camera *);
int action_camera_set_port        (Camera *, const char *port);
int action_camera_set_speed       (Camera *, unsigned int speed);
int action_camera_set_model       (Camera *, const char *model);
int action_camera_show_abilities  (Camera *);
int action_camera_upload_file     (Camera *, const char *folder,
				   const char *path);
int action_camera_capture_preview (Camera *);

/* Other actions */
int list_cameras_action    (void);
int list_ports_action      (void);
int auto_detect_action     (void);
int set_folder_action      (const char *folder);
int set_filename_action    (const char *filename);
int print_version_action   (void);
int override_usbids_action (int usb_vendor,          int usb_product, 
			    int usb_vendor_modified, int usb_product_modified);

#endif /* __ACTIONS_H__ */
