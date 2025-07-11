/* actions.h
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef GPHOTO2_ACTIONS_H
#define GPHOTO2_ACTIONS_H

#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-context.h>

#include <gp-params.h>

#ifdef GPHOTO2_WEBAPI
#include "mongoose.h"
#define JSON_PRINTF mg_http_printf_chunk
#else
#define JSON_PRINTF(x,...)
#endif


enum wait_type {
	WAIT_TIME,
	WAIT_EVENTS,
	WAIT_FRAMES,
	WAIT_STRING,
};
enum download_type { DT_NO_DOWNLOAD, DT_DOWNLOAD };
struct waitparams {
	union {
		int milliseconds;
		int events;
		int frames;
		char *str;
	} u;
	enum wait_type type;
	enum download_type downloadtype;
};

/* Image actions */
typedef int FileAction    (GPParams *, const char *folder, const char *filename);
int print_file_action     (GPParams *, const char *folder, const char *filename);
int print_info_action     (GPParams *, const char *folder, const char *filename);
int save_file_action      (GPParams *, const char *folder, const char *filename);
int save_thumbnail_action (GPParams *, const char *folder, const char *filename);
int save_raw_action       (GPParams *, const char *folder, const char *filename);
int save_audio_action     (GPParams *, const char *folder, const char *filename);
int save_all_audio_action (GPParams *, const char *folder, const char *filename);
int save_exif_action      (GPParams *, const char *folder, const char *filename);
int save_meta_action      (GPParams *, const char *folder, const char *filename);
int delete_file_action    (GPParams *, const char *folder, const char *filename);

/* Folder actions */
typedef int FolderAction  (GPParams *);
int delete_all_action     (GPParams *);
int list_files_action     (GPParams *);
int list_folders_action   (GPParams *);
int num_files_action      (GPParams *);

/* Camera actions */
int action_camera_about           (GPParams *);
int action_camera_summary         (GPParams *);
int action_camera_manual          (GPParams *);
int action_camera_set_port        (GPParams *, const char *port);
int action_camera_set_speed       (GPParams *, unsigned int speed);
int action_camera_set_model       (GPParams *, const char *model);
int action_camera_show_abilities  (GPParams *);
int action_camera_upload_file     (GPParams *, const char *folder,
				   const char *path);
int action_camera_upload_metadata (GPParams *, const char *folder,
				   const char *path);
int action_camera_capture_movie   (GPParams *, const char *arg);
int action_camera_wait_event      (GPParams *, enum download_type dt, const char*args);
int action_camera_show_preview    (GPParams *);
int action_camera_capture_preview (GPParams *);

/* Other actions */
int list_cameras_action    (GPParams *);
int list_ports_action      (GPParams *);
int set_folder_action      (GPParams *, const char *folder);
int set_filename_action    (GPParams *, const char *filename);
int print_version_action   (GPParams *);
int override_usbids_action (GPParams *, int usb_vendor, int usb_product,
			    int usb_vendor_modified, int usb_product_modified);
int debug_action           (GPParams *, const char *debug_loglevel, const char *debug_logfile_name);
int set_config_action      (GPParams *, const char *name, const char *value);
int set_config_index_action      (GPParams *, const char *name, const char *value);
int set_config_value_action      (GPParams *, const char *name, const char *value);
int print_storage_info     (GPParams *);

void _get_portinfo_list	(GPParams *p);

#ifdef GPHOTO2_WEBAPI
int auto_detect_action     (struct mg_connection *, GPParams *);
int list_config_action     (struct mg_connection *, GPParams *);
int list_all_config_action (struct mg_connection *, GPParams *);
int print_exif_action      (struct mg_connection *, GPParams *, const char *folder, const char *filename);
int get_config_action      (struct mg_connection *, GPParams *, const char *name);
#else
int auto_detect_action     (GPParams *);
int list_config_action     (GPParams *);
int list_all_config_action (GPParams *);
int print_exif_action      (GPParams *, const char *folder, const char *filename);
int get_config_action      (GPParams *, const char *name);
#endif

#endif /* !defined(GPHOTO2_ACTIONS_H) */


/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
