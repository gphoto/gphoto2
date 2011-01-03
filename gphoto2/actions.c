/* actions.c
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

#define _XOPEN_SOURCE	/* strptime proto, but this hides other prototypes */
#define _GNU_SOURCE	/* get all the other prototypes */
#define __EXTENSIONS__	/* for solaris to get back strdup and strcasecmp */

#include "config.h"
#include "actions.h"
#include "i18n.h"
#include "main.h"
#include "version.h"

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

#include <time.h>
#include <sys/time.h>

#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-setting.h>
#include <gphoto2/gphoto2-filesys.h>
#include <gphoto2/globals.h>

#ifdef HAVE_AA
#  include "gphoto2-cmd-capture.h"
#endif

#ifdef HAVE_LIBEXIF
#  include <libexif/exif-data.h>
#endif

#define CR(result)       {int r=(result); if (r<0) return r;}
#define CRU(result,file) {int r=(result); if (r<0) {gp_file_unref(file);return r;}}
#define CL(result,list)  {int r=(result); if (r<0) {gp_list_free(list); return r;}}

#ifdef __GNUC__
#define __unused__ __attribute__((unused))
#else
#define __unused__
#endif

int
delete_all_action (GPParams *p)
{
	return (gp_camera_folder_delete_all (p->camera, p->folder, p->context));
}

int
action_camera_upload_file (GPParams *p, const char *folder, const char *path)
{
	CameraFile *file;
	int res;

	gp_log (GP_LOG_DEBUG, "main", "Uploading file...");

	CR (gp_file_new_from_fd (&file, -1));
	res = gp_file_open (file, path);
	if (res < GP_OK) {
		gp_file_unref (file);
		return res;
	}

	/* Check if the user specified a filename */
	if (p->filename && strcmp (p->filename, "")) {
		res = gp_file_set_name (file, p->filename);
		if (res < 0) {
			gp_file_unref (file);
			return (res);
		}
	}
	res = gp_camera_folder_put_file (p->camera, folder, file,
					 p->context);
	gp_file_unref (file);
	return (res);
}

int
action_camera_upload_metadata (GPParams *p, const char *folder, const char *path)
{
	CameraFile *file;
	int res;

	gp_log (GP_LOG_DEBUG, "main", "Uploading metadata...");

	CR (gp_file_new (&file));
	res = gp_file_open (file, path);
	if (res < 0) {
		gp_file_unref (file);
		return (res);
	}

	/* Check if the user specified a filename */
	if (p->filename && strcmp (p->filename, "")) {
		res = gp_file_set_name (file, p->filename);
		if (res < 0) {
			gp_file_unref (file);
			return (res);
		}
	} else if (path == strstr(path, "meta_")) {
		res = gp_file_set_name (file, path+5);
		if (res < 0) {
			gp_file_unref (file);
			return (res);
		}
	}
	gp_file_set_type (file, GP_FILE_TYPE_METADATA);

	res = gp_camera_folder_put_file (p->camera, folder, file,
					 p->context);
	gp_file_unref (file);
	return (res);
}

int
num_files_action (GPParams *p)
{
	CameraList *list;
	int count, filecount;

	CR (gp_list_new (&list));
	CL (gp_camera_folder_list_files (p->camera, p->folder,
					 list, p->context), list);
	CL (count = gp_list_count (list), list);

	if (p->flags & FLAGS_NEW) {
		int i;
		const char *name;

		filecount = 0;
		for (i = 0; i < count; i++) {
			CameraFileInfo info;

			CL (gp_list_get_name (list, i, &name), list);
			CR (gp_camera_file_get_info (p->camera, p->folder,
						     name, &info, p->context));
			if (info.file.fields & GP_FILE_INFO_STATUS &&
			    info.file.status != GP_FILE_STATUS_DOWNLOADED)
				filecount++;
		}
	}
	else
	  filecount = count;

	gp_list_free (list);
	if (p->flags & FLAGS_QUIET)
		printf ("%i\n", filecount);
	else
		printf (_("Number of files in folder '%s': %i\n"),
			p->folder, filecount);

	return (GP_OK);
}

int
list_folders_action (GPParams *p)
{
	CameraList *list;
	int count;
	const char *name;
	int i;
	
	CR (gp_list_new (&list));

	CL (gp_camera_folder_list_folders (p->camera, p->folder, list,
					   p->context), list);
	CL (count = gp_list_count (list), list);
        printf(ngettext(
		"There is %d folder in folder '%s'.\n", 
		"There are %d folders in folder '%s'.\n",
		count
	), count, p->folder);
	for (i = 0; i < count; i++) {
		CL (gp_list_get_name (list, i, &name), list);
		printf (" - %s\n", name);
	}
	gp_list_free (list);
	return (GP_OK);
}

int
list_files_action (GPParams *p)
{
	CameraList *list;
	int count, filecount;
	const char *name;
	int i;

	CR (gp_list_new (&list));
	CL (gp_camera_folder_list_files (p->camera, p->folder, list,
					 p->context), list);
	CL (count = gp_list_count (list), list);
	if (p->flags & FLAGS_NEW) {
		filecount = 0;
		for (i = 0; i < count; i++) {
			CameraFileInfo info;

			CL (gp_list_get_name (list, i, &name), list);
			CR (gp_camera_file_get_info (p->camera, p->folder,
						     name, &info, p->context));
			if (info.file.fields & GP_FILE_INFO_STATUS &&
			    info.file.status != GP_FILE_STATUS_DOWNLOADED)
				filecount++;
		}
	}
	else
	  filecount = count;
        if (filecount == 0) { /* 0 is weird still, despite ngettext() */
	   printf(_("There is no file in folder '%s'.\n"), p->folder);
        } else {
	   printf(ngettext(
		"There is %d file in folder '%s'.\n",
		"There are %d files in folder '%s'.\n",
		filecount
	   ), filecount, p->folder);
        }
	for (i = 0; i < count; i++) {
		CL (gp_list_get_name (list, i, &name), list);
		CL (print_file_action (p, name), list);
	}
	gp_list_free (list);
	return (GP_OK);
}

int
print_info_action (GPParams *p, const char *filename)
{
	CameraFileInfo info;

	CR (gp_camera_file_get_info (p->camera, p->folder, filename, &info,
				     p->context));

	printf (_("Information on file '%s' (folder '%s'):\n"),
		filename, p->folder);
	printf (_("File:\n"));
	if (info.file.fields == GP_FILE_INFO_NONE)
		printf (_("  None available.\n"));
	else {
		if (info.file.fields & GP_FILE_INFO_NAME)
			printf (_("  Name:        '%s'\n"), info.file.name);
		if (info.file.fields & GP_FILE_INFO_TYPE)
			printf (_("  Mime type:   '%s'\n"), info.file.type);
		if (info.file.fields & GP_FILE_INFO_SIZE)
			printf (_("  Size:        %lu byte(s)\n"), (unsigned long int)info.file.size);
		if (info.file.fields & GP_FILE_INFO_WIDTH)
			printf (_("  Width:       %i pixel(s)\n"), info.file.width);
		if (info.file.fields & GP_FILE_INFO_HEIGHT)
			printf (_("  Height:      %i pixel(s)\n"), info.file.height);
		if (info.file.fields & GP_FILE_INFO_STATUS)
			printf (_("  Downloaded:  %s\n"),
				(info.file.status == GP_FILE_STATUS_DOWNLOADED) ? _("yes") : _("no"));
		if (info.file.fields & GP_FILE_INFO_PERMISSIONS) {
			printf (_("  Permissions: "));
			if ((info.file.permissions & GP_FILE_PERM_READ) &&
			    (info.file.permissions & GP_FILE_PERM_DELETE))
				printf (_("read/delete"));
			else if (info.file.permissions & GP_FILE_PERM_READ)
				printf (_("read"));
			else if (info.file.permissions & GP_FILE_PERM_DELETE)
				printf (_("delete"));
			else
				printf (_("none"));
			putchar ('\n');
		}
		if (info.file.fields & GP_FILE_INFO_MTIME)
			printf (_("  Time:        %s"),
				asctime (localtime (&info.file.mtime)));
	}
	printf (_("Thumbnail:\n"));
	if (info.preview.fields == GP_FILE_INFO_NONE)
		printf (_("  None available.\n"));
	else {
		if (info.preview.fields & GP_FILE_INFO_TYPE)
			printf (_("  Mime type:   '%s'\n"), info.preview.type);
		if (info.preview.fields & GP_FILE_INFO_SIZE)
			printf (_("  Size:        %lu byte(s)\n"), (unsigned long int)info.preview.size);
		if (info.preview.fields & GP_FILE_INFO_WIDTH)
			printf (_("  Width:       %i pixel(s)\n"), info.preview.width);
		if (info.preview.fields & GP_FILE_INFO_HEIGHT)
			printf (_("  Height:      %i pixel(s)\n"), info.preview.height);
		if (info.preview.fields & GP_FILE_INFO_STATUS)
			printf (_("  Downloaded:  %s\n"),
				(info.preview.status == GP_FILE_STATUS_DOWNLOADED) ? _("yes") : _("no"));
	}
	printf (_("Audio data:\n"));
	if (info.audio.fields == GP_FILE_INFO_NONE)
		printf (_("  None available.\n"));
	else {
		if (info.audio.fields & GP_FILE_INFO_TYPE)
			printf (_("  Mime type:  '%s'\n"), info.audio.type);
		if (info.audio.fields & GP_FILE_INFO_SIZE)
			printf (_("  Size:       %lu byte(s)\n"), (unsigned long int)info.audio.size);
		if (info.audio.fields & GP_FILE_INFO_STATUS)
			printf (_("  Downloaded: %s\n"),
				(info.audio.status == GP_FILE_STATUS_DOWNLOADED) ? _("yes") : _("no"));
	}

	return (GP_OK);
}

int
print_file_action (GPParams *p, const char *filename)
{
	static int x = 0;

	if (p->flags & FLAGS_NEW) {
		CameraFileInfo info;
		
		CR (gp_camera_file_get_info (p->camera, p->folder,
					     filename, &info, p->context));
		if (info.file.fields & GP_FILE_INFO_STATUS &&
		    info.file.status == GP_FILE_STATUS_DOWNLOADED) {
			x++;
			return (GP_OK);
		}
	}

	if (p->flags & FLAGS_QUIET)
		printf ("\"%s\"\n", filename);
	else {
		CameraFileInfo info;
		if (gp_camera_file_get_info (p->camera, p->folder, filename,
					     &info, NULL) == GP_OK) {
		    printf("#%-5i %-27s", x+1, filename);
		    if (info.file.fields & GP_FILE_INFO_PERMISSIONS) {
			printf("%s%s",
			       (info.file.permissions & GP_FILE_PERM_READ) ? "r" : "-",
			       (info.file.permissions & GP_FILE_PERM_DELETE) ? "d" : "-");
		    }
		    if (info.file.fields & GP_FILE_INFO_SIZE)
			printf(" %5ld KB", (unsigned long int)((info.file.size+1023) / 1024));
		    if ((info.file.fields & GP_FILE_INFO_WIDTH) && +
			    (info.file.fields & GP_FILE_INFO_HEIGHT))
			printf(" %4dx%-4d", info.file.width, info.file.height);
		    if (info.file.fields & GP_FILE_INFO_TYPE)
			printf(" %s", info.file.type);
		    putchar ('\n');
		} else
		    printf("#%-5i %s\n", x+1, filename);
	}
	x++;
	return (GP_OK);
}

int
save_file_action (GPParams *p, const char *filename)
{
	return (save_file_to_file (p->camera, p->context, p->flags,
				   p->folder, filename, GP_FILE_TYPE_NORMAL));
}

int
save_exif_action (GPParams *p, const char *filename)
{
	return (save_file_to_file (p->camera, p->context, p->flags,
				   p->folder, filename, GP_FILE_TYPE_EXIF));
}

int
save_meta_action (GPParams *p, const char *filename)
{
	return (save_file_to_file (p->camera, p->context, p->flags,
				   p->folder, filename, GP_FILE_TYPE_METADATA));
}

int
save_thumbnail_action (GPParams *p, const char *filename)
{
	return (save_file_to_file (p->camera, p->context, p->flags,
				   p->folder, filename, GP_FILE_TYPE_PREVIEW));
}

int
save_raw_action (GPParams *p, const char *filename)
{
	return (save_file_to_file (p->camera, p->context, p->flags,
				   p->folder, filename, GP_FILE_TYPE_RAW));
}

int
save_audio_action (GPParams *p, const char *filename)
{
	return (save_file_to_file (p->camera, p->context, p->flags,
				   p->folder, filename, GP_FILE_TYPE_AUDIO));
}

int
save_all_audio_action (GPParams *p, const char *filename)
{
	/* not every file has an associated audio file */
	if (camera_file_exists(p->camera, p->context, p->folder, filename,
			       GP_FILE_TYPE_AUDIO))
		return (save_file_to_file (p->camera, p->context, p->flags,
					   p->folder, filename,
					   GP_FILE_TYPE_AUDIO));
	return GP_OK;
}

int
delete_file_action (GPParams *p, const char *filename)
{
	if (p->flags & FLAGS_NEW) {
		CameraFileInfo info;
		
		CR (gp_camera_file_get_info (p->camera, p->folder, filename,
					     &info, p->context));
		if (info.file.fields & GP_FILE_INFO_STATUS &&
		    info.file.status == GP_FILE_STATUS_DOWNLOADED)
			return (GP_OK);
	}
	return (gp_camera_file_delete (p->camera, p->folder, filename,
				       p->context));
}

#ifdef HAVE_LIBEXIF
static void
show_ifd (ExifContent *content)
{
        ExifEntry *e;
        unsigned int i;

        for (i = 0; i < content->count; i++) {
                e = content->entries[i];
                printf ("%-20.20s", exif_tag_get_name (e->tag));
                printf ("|");
#ifdef HAVE_LIBEXIF_LOG
		{char b[1024];
		printf ("%-59.59s", exif_entry_get_value (e, b, sizeof (b)));
		}
#else
                printf ("%-59.59s", exif_entry_get_value (e));
#endif
                printf ("\n");
        }
}

static void
print_hline (void)
{
        int i;

        for (i = 0; i < 20; i++)
                putchar ('-');
        printf ("+");
        for (i = 0; i < 59; i++)
                putchar ('-');
        putchar ('\n'); 
}
#endif

int
print_exif_action (GPParams *p, const char *filename)
{
#ifdef HAVE_LIBEXIF
        CameraFile *file;
        const char *data;
        unsigned long size;
        ExifData *ed;
#ifdef HAVE_LIBEXIF_IFD
	unsigned int i;
#endif

        CR (gp_file_new (&file));
        CRU (gp_camera_file_get (p->camera, p->folder, filename,
				 GP_FILE_TYPE_EXIF, file, p->context), file);
        CRU (gp_file_get_data_and_size (file, &data, &size), file);
        ed = exif_data_new_from_data ((unsigned char *)data, size);
        gp_file_unref (file);
        if (!ed) {
                gp_context_error (p->context, _("Could not parse EXIF data."));
                return (GP_ERROR);
        }

        printf (_("EXIF tags:"));
        putchar ('\n');
        print_hline ();
        printf ("%-20.20s", _("Tag"));
        printf ("|");
        printf ("%-59.59s", _("Value"));
        putchar ('\n');
        print_hline ();
#ifdef HAVE_LIBEXIF_IFD
	for (i = 0; i < EXIF_IFD_COUNT; i++)
		if (ed->ifd[i])
			show_ifd (ed->ifd[i]);
#else
        if (ed->ifd0)
                show_ifd (ed->ifd0);
        if (ed->ifd1)
                show_ifd (ed->ifd1);
        if (ed->ifd_exif)
                show_ifd (ed->ifd_exif);
        if (ed->ifd_gps)
                show_ifd (ed->ifd_gps);
        if (ed->ifd_interoperability)
                show_ifd (ed->ifd_interoperability);
#endif
        print_hline ();
        if (ed->size) {
                printf (_("EXIF data contains a thumbnail (%i bytes)."),
                        ed->size);
                putchar ('\n');
        }

        exif_data_unref (ed);

        return (GP_OK);
#else
	gp_context_error (p->context, _("gphoto2 has been compiled without "
		"EXIF support."));
	return (GP_ERROR_NOT_SUPPORTED);
#endif
}

int
list_cameras_action (GPParams *p)
{
	int r = GP_OK, n, i;
	CameraAbilities a;

	r = gp_abilities_list_count (gp_params_abilities_list(p));
	if (r < 0)
		return (r);
	if (p->flags & FLAGS_QUIET)
		printf ("%i\n", r);
	else {
		printf (_("Number of supported cameras: %i\n"), r);
		printf (_("Supported cameras:\n"));
	}
	n = r;
	for (i = 0; i < n; i++) {
		r = gp_abilities_list_get_abilities (gp_params_abilities_list(p),
						     i, &a);
		if (r < 0)
			break;
		if (p->flags & FLAGS_QUIET)
			printf ("%s\n", a.model);
		else
			switch (a.status) {
			case GP_DRIVER_STATUS_TESTING:
				printf (_("\t\"%s\" (TESTING)\n"), a.model);
				break;
			case GP_DRIVER_STATUS_EXPERIMENTAL:
				printf (_("\t\"%s\" (EXPERIMENTAL)\n"),
					a.model);
				break;
			case GP_DRIVER_STATUS_PRODUCTION:
			default:
				printf (_("\t\"%s\"\n"), a.model);
				break;
			}
	}

	return (r);
}

void
_get_portinfo_list (GPParams *p) {
	int count, result;
	GPPortInfoList *list = NULL;

	if (p->portinfo_list)
		return;

	if (gp_port_info_list_new (&list) < GP_OK)
		return;
	result = gp_port_info_list_load (list);
	if (result < 0) {
		gp_port_info_list_free (list);
		return;
	}
	count = gp_port_info_list_count (list);
	if (count < 0) {
		gp_port_info_list_free (list);
		return;
	}
	p->portinfo_list = list;
	return;
}

int
list_ports_action (GPParams *p)
{
	GPPortInfo info;
	int x, count, result = GP_OK;

	_get_portinfo_list (p);
	count = gp_port_info_list_count (p->portinfo_list);

	if (p->flags & FLAGS_QUIET)
		printf("%i\n", count);
	else {
		printf(_("Devices found: %i\n"), count);
		printf(_("Path                             Description\n"
			"--------------------------------------------------------------\n"));
	}

	/* Now list the ports */
	for (x = 0; x < count; x++) {
		result = gp_port_info_list_get_info (p->portinfo_list, x, &info);
		if (result < 0)
			break;
		printf ("%-32s %-32s\n", info.path, info.name);
	}
	return (result);

}

int
auto_detect_action (GPParams *p)
{
	int x, count;
        CameraList *list;
        const char *name = NULL, *value = NULL;

	_get_portinfo_list (p);
	count = gp_port_info_list_count (p->portinfo_list);

	CR (gp_list_new (&list));
        gp_abilities_list_detect (gp_params_abilities_list(p), p->portinfo_list, list, p->context);

        CL (count = gp_list_count (list), list);

        printf(_("%-30s %-16s\n"), _("Model"), _("Port"));
        printf(_("----------------------------------------------------------\n"));
        for (x = 0; x < count; x++) {
                CL (gp_list_get_name  (list, x, &name), list);
                CL (gp_list_get_value (list, x, &value), list);
                printf(_("%-30s %-16s\n"), name, value);
        }
	gp_list_free (list);
        return GP_OK;
}

int
action_camera_show_abilities (GPParams *p)
{
	CameraAbilities a;
	int i;
	int has_capture = 0;

	CR (gp_camera_get_abilities (p->camera, &a));
	printf (_("Abilities for camera             : %s\n"), a.model);
	printf (_("Serial port support              : %s\n"),
		(a.port & GP_PORT_SERIAL) ? _("yes"):_("no"));
	printf (_("USB support                      : %s\n"),
		(a.port & GP_PORT_USB) ? _("yes"):_("no"));
	if (a.speed[0] != 0) {
		printf (_("Transfer speeds supported        :\n"));
		for (i = 0; a.speed[i]; i++)
			printf (_("                                 : %i\n"),
				a.speed[i]);
	}
	printf (_("Capture choices                  :\n"));
	if (a.operations & GP_OPERATION_CAPTURE_IMAGE) {
		printf (_("                                 : Image\n"));
		has_capture = 1;
	}
	if (a.operations & GP_OPERATION_CAPTURE_VIDEO) {
		printf (_("                                 : Video\n"));
		has_capture = 1;
	}
	if (a.operations & GP_OPERATION_CAPTURE_AUDIO) {
		printf (_("                                 : Audio\n"));
		has_capture = 1;
	}
	if (a.operations & GP_OPERATION_CAPTURE_PREVIEW) {
		printf (_("                                 : Preview\n"));
		has_capture = 1;
	}
	if (has_capture == 0) {
		printf (_("                                 : Capture not supported by the driver\n"));
	}
	printf (_("Configuration support            : %s\n"),
		(a.operations & GP_OPERATION_CONFIG) ? _("yes"):_("no"));
	printf (_("Delete selected files on camera  : %s\n"),
		(a.file_operations & GP_FILE_OPERATION_DELETE) ?
							_("yes"):_("no"));
        printf (_("Delete all files on camera       : %s\n"),
        	(a.folder_operations & GP_FOLDER_OPERATION_DELETE_ALL) ?
                					_("yes"):_("no"));
	printf (_("File preview (thumbnail) support : %s\n"),
		(a.file_operations & GP_FILE_OPERATION_PREVIEW) ? 
							_("yes"):_("no"));
	printf (_("File upload support              : %s\n"),
		(a.folder_operations & GP_FOLDER_OPERATION_PUT_FILE) ?
							_("yes"):_("no"));

	return (GP_OK);
}

int
action_camera_set_port (GPParams *params, const char *port)
{
	int p, r;
	GPPortInfo info;
	char verified_port[1024];

	verified_port[sizeof (verified_port) - 1] = '\0';
	if (!strchr (port, ':')) {
		gp_log (GP_LOG_DEBUG, "main", _("Ports must look like "
			"'serial:/dev/ttyS0' or 'usb:', but '%s' is "
			"missing a colon so I am going to guess what you "
			"mean."), port);
		if (!strcmp (port, "usb")) {
			strncpy (verified_port, "usb:",
				 sizeof (verified_port) - 1);
		} else if (strncmp (port, "/dev/", 5) == 0) {
			strncpy (verified_port, "serial:",
				 sizeof (verified_port) - 1);
			strncat (verified_port, port,
				 sizeof (verified_port)
				 	- strlen (verified_port) - 1);
		} else if (strncmp (port, "/proc/", 6) == 0) {
			strncpy (verified_port, "usb:",
				 sizeof (verified_port) - 1);
			strncat (verified_port, port,
				 sizeof (verified_port)
				 	- strlen (verified_port) - 1);
		}
		gp_log (GP_LOG_DEBUG, "main", "Guessed port name. Using port "
			"'%s' from now on.", verified_port);
	} else
		strncpy (verified_port, port, sizeof (verified_port) - 1);

	/* Create the list of ports and load it. */
	_get_portinfo_list (params);

	/* Search our port in the list. */
	/* NOTE: This call can modify "il" for regexp matches! */
	p = gp_port_info_list_lookup_path (params->portinfo_list, verified_port);

	switch (p) {
	case GP_ERROR_UNKNOWN_PORT:
		fprintf (stderr, _("The port you specified "
			"('%s') can not be found. Please "
			"specify one of the ports found by "
			"'gphoto2 --list-ports' and make "
			"sure the spelling is correct "
			"(i.e. with prefix 'serial:' or 'usb:')."),
				verified_port);
		break;
	default:
		break;
	}

	/* Get info about our port. */
	r = gp_port_info_list_get_info (params->portinfo_list, p, &info);
	if (r < 0)
		return (r);

	/* Set the port of our camera. */
	r = gp_camera_set_port_info (params->camera, info);
	if (r < 0)
		return (r);
	gp_setting_set ("gphoto2", "port", info.path);
	return (GP_OK);
}

int
action_camera_about (GPParams *params)
{
	CameraText text;

	CR (gp_camera_get_about (params->camera, &text, params->context));
	
	printf (_("About the camera driver:"));
	printf ("\n%s\n", _(text.text));

	return (GP_OK);
}

int
action_camera_summary (GPParams *params)
{
	CameraText text;

	CR (gp_camera_get_summary (params->camera, &text, params->context));

	printf (_("Camera summary:"));
	printf ("\n%s\n", _(text.text));

	return (GP_OK);
}

int
action_camera_manual (GPParams *params)
{
	CameraText text;

	CR (gp_camera_get_manual (params->camera, &text, params->context));

	printf (_("Camera manual:"));
	printf ("\n%s\n", _(text.text));

	return (GP_OK);
}

int
action_camera_set_speed (GPParams *p, unsigned int speed)
{
	GPPortInfo info;

	/* Make sure we've got a serial port. */
	CR (gp_camera_get_port_info (p->camera, &info));
	if (info.type != GP_PORT_SERIAL) {
		if ((p->flags & FLAGS_QUIET) == 0) {
			fprintf (stderr, _("You can only specify speeds for "
					   "serial ports."));
			fputc ('\n', stderr);
		}
		return (GP_ERROR_BAD_PARAMETERS);
	}
	/* Set the speed. */
	return gp_camera_set_port_speed (p->camera, speed);
}

int
action_camera_set_model (GPParams *p, const char *model)
{
	CameraAbilities a;
	int m;

	CR (m = gp_abilities_list_lookup_model (gp_params_abilities_list(p), model));
	CR (gp_abilities_list_get_abilities (gp_params_abilities_list(p), m, &a));
	CR (gp_camera_set_abilities (p->camera, a));
	gp_setting_set ("gphoto2", "model", a.model);

	return (GP_OK);
}

int
set_folder_action (GPParams *p, const char *folder)
{
	if (p->folder)
		free (p->folder);
	p->folder = strdup (folder);
	return (p->folder ? GP_OK: GP_ERROR_NO_MEMORY);
}

int
set_filename_action (GPParams *p, const char *filename)
{
	if (p->filename)
		free (p->filename);
	p->filename = strdup (filename);
	return (p->filename ? GP_OK: GP_ERROR_NO_MEMORY);
}

#define CHECK_NULL(x) { if (x == NULL) { return(-1); /* FIXME: what code? */ } }

int
print_version_action (GPParams __unused__ *p)
{
	int n;
	const char *port_message =
#ifdef OS2
			_("OS/2 port by Bart van Leeuwen\n");
#else
			"";
#endif
	printf (_("gphoto2 %s\n"
		  "\n"
		  "Copyright (c) 2000-%d Lutz Mueller and others\n"
		  "%s"
		  "\n"
		  "gphoto2 comes with NO WARRANTY, to the extent permitted by law. You may\n"
		  "redistribute copies of gphoto2 under the terms of the GNU General Public\n"
		  "License. For more information about these matters, see the files named COPYING.\n"
		  "\n"
		  "This version of gphoto2 is using the following software versions and options:\n"),
		VERSION,
		2011, /* year of release! */
		port_message
		);

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
	  printf ("%-15s %-14s ", name, v[0]);
	  for (i = 1; v[i] != NULL; i++) {
		  if (v[i+1] != NULL)
			  printf ("%s, ", v[i]);
		  else
			  printf ("%s", v[i]);
	  }
	  putchar ('\n');
	}

	return (GP_OK);
}

int
action_camera_capture_preview (GPParams *p)
{
	CameraFile *file;
	int	r, fd;
        char    tmpname[20], *tmpfilename;

	strcpy (tmpname, "tmpfileXXXXXX");
	fd = mkstemp(tmpname);
	if (fd == -1) {
		CR (gp_file_new (&file));
		tmpfilename = NULL;
	} else {
		r = gp_file_new_from_fd (&file, fd);
		if (r < GP_OK) {
			close (fd);
			unlink (tmpname);
			return r;
		}
		tmpfilename = tmpname;
	}

#ifdef HAVE_AA
	r = gp_cmd_capture_preview (p->camera, file, p->context);
#else
	r = gp_camera_capture_preview (p->camera, file, p->context);
#endif
	if (r < 0) {
		gp_file_unref (file);
		if (fd != -1)
			unlink (tmpname);
		return (r);
	}

	r = save_camera_file_to_file (NULL, file, tmpfilename);
	gp_file_unref (file);
	if (r < 0) 
		return (r);

	return (GP_OK);
}

enum moviemode { MOVIE_ENDLESS, MOVIE_FRAMES, MOVIE_SECONDS };

int
action_camera_capture_movie (GPParams *p, const char *arg)
{
	CameraFile	*file;
	int		r;
	int		fd;
	time_t		st;
	enum moviemode	mm;
	int		frames;
	char		*xname;

	if (p->flags & FLAGS_STDOUT) {
		fd = dup(fileno(stdout));
		xname = "stdout";
	} else {
		fd = open("movie.mjpg",O_WRONLY|O_CREAT,0660);
		if (fd == -1) {
			cli_error_print(_("Could not open 'movie.mjpg'."));
			return GP_ERROR;
		}
		xname = "movie.mjpg";
	}
	if (!arg) {
		mm = MOVIE_ENDLESS;
		fprintf(stderr,_("Capturing preview frames as movie to '%s'. Press Ctrl-C to abort.\n"), xname);
	} else {
		if (strchr(arg,'s')) {
			sscanf (arg, "%ds", &frames);
			fprintf(stderr,_("Capturing preview frames as movie to '%s' for %d seconds.\n"), xname, frames);
			mm = MOVIE_SECONDS;
			time (&st);
		} else {
			sscanf (arg, "%d", &frames);
			fprintf(stderr,_("Capturing %d preview frames as movie to '%s'.\n"), frames, xname);
			mm = MOVIE_FRAMES;
		}
	}
	CR (gp_file_new_from_fd (&file, fd));
	while (1) {
		const char *mime;
		r = gp_camera_capture_preview (p->camera, file, p->context);
		if (r < 0) {
			cli_error_print(_("Movie capture error... Exiting."));
			break;
		}
		gp_file_get_mime_type (file, &mime);
                if (strcmp (mime, GP_MIME_JPEG)) {
			cli_error_print(_("Movie capture error... Unhandled MIME type '%s'."), mime);
			break;
		}

		if (glob_cancel) {
			fprintf(stderr, _("Ctrl-C pressed ... Exiting.\n"));
			break;
		}
		if (mm == MOVIE_FRAMES) {
			if (!frames--)
				break;
		}
		if (mm == MOVIE_SECONDS) {
			time_t	xt;
			time (&xt);
			if (xt >= st + frames)
				break;
		}
	}
	gp_file_unref (file);
	return (GP_OK);
}

/* count < 0 means exact seconds timer.
 * count > 0 means number of events (with 1 second timeout events).
 */
int
action_camera_wait_event (GPParams *p, int dodownload, int count)
{
	int ret;
	CameraEventType	event;
	void	*data = NULL;
	CameraFilePath	*fn;
	CameraFilePath last;
	struct timeval	xtime;

	gettimeofday (&xtime, NULL);
	memset(&last,0,sizeof(last));

	if (!count) count = 1;
	while (1) {
		int 		leftoverms = 1000;
		struct timeval	ytime;
		int		x;

		if (glob_cancel) break;
		if (!count) break;
		if (count > 0) count--;

		if (count < 0) { /* in exact seconds */
			gettimeofday (&ytime, NULL);

			x = (ytime.tv_usec-xtime.tv_usec)+(ytime.tv_sec-xtime.tv_sec)*1000000;
			if (x > (-count*1000000)) break;
			/* if left over time is < 1s, set it... otherwise wait at most 1s */
			if ((-(count*1000000)-x) < leftoverms*1000)
				leftoverms = (-(count*1000000)-x)/1000;
		}

		data = NULL;
		ret = gp_camera_wait_for_event (p->camera, leftoverms, &event, &data, p->context);
		if (ret != GP_OK)
			return ret;
		switch (event) {
		case GP_EVENT_UNKNOWN:
			if (data) {
				printf("UNKNOWN %s\n", (char*)data);
			} else {
				printf("UNKNOWN\n");
			}
			break;
		case GP_EVENT_TIMEOUT:
			/*printf("TIMEOUT\n");*/
			break;
		case GP_EVENT_CAPTURE_COMPLETE:
			printf("CAPTURECOMPLETE\n");
			break;
		case GP_EVENT_FILE_ADDED:
			fn = (CameraFilePath*)data;

			if (!dodownload) {
				printf("FILEADDED %s %s\n",fn->name, fn->folder);
				continue;
			}
			/* Otherwise download the image and continue... */
			if(strcmp(fn->folder, last.folder)) {
				strcpy(last.folder, fn->folder);
				ret = set_folder_action(p, fn->folder);
				if (ret != GP_OK) {
					cli_error_print(_("Could not set folder."));
					return (ret);
				}
			}
			ret = get_file_common (fn->name, GP_FILE_TYPE_NORMAL);
			if (ret != GP_OK) {
				cli_error_print (_("Could not get image."));
				if(ret == GP_ERROR_FILE_NOT_FOUND) {
					/* Buggy libcanon.so?
					* Can happen if this was the first capture after a
					* CF card format, or during a directory roll-over,
					* ie: CANON100 -> CANON101
					*/
					cli_error_print ( _("Buggy libcanon.so?"));
				}
				return (ret);
			}

			do {
				ret = delete_file_action (p, fn->name);
			} while (ret == GP_ERROR_CAMERA_BUSY);
			if (ret != GP_OK) {
				cli_error_print ( _("Could not delete image."));
				/* dont continue in event loop */
			}
			break;
		case GP_EVENT_FOLDER_ADDED:
			fn = (CameraFilePath*)data;
			printf("FOLDERADDED %s %s\n",fn->name, fn->folder);
			break;
		}
		free (data);
	}
	return GP_OK;
}

int
print_storage_info (GPParams *p)
{
	int			ret, i, nrofsinfos;
	CameraStorageInformation	*sinfos;

	ret = gp_camera_get_storageinfo (p->camera, &sinfos, &nrofsinfos, p->context);
	if (ret != GP_OK) {
		if (ret == GP_ERROR_NOT_SUPPORTED)
			printf (_("Getting storage information not supported for this camera.\n"));
		return ret;
	}
	for (i=0;i<nrofsinfos;i++) {
		printf ("[Storage %d]\n", i);
		if (sinfos[i].fields & GP_STORAGEINFO_LABEL)
			printf ("label=%s\n", sinfos[i].label);
		if (sinfos[i].fields & GP_STORAGEINFO_DESCRIPTION)
			printf ("description=%s\n", sinfos[i].description);
		if (sinfos[i].fields & GP_STORAGEINFO_BASE)
			printf ("basedir=%s\n", sinfos[i].basedir);
		if (sinfos[i].fields & GP_STORAGEINFO_ACCESS) {
			printf ("access=%d ", sinfos[i].access);
			switch (sinfos[i].access) {
			case GP_STORAGEINFO_AC_READWRITE:
				printf (_("Read-Write"));
				break;
			case GP_STORAGEINFO_AC_READONLY:
				printf (_("Read-Only"));
				break;
			case GP_STORAGEINFO_AC_READONLY_WITH_DELETE:
				printf (_("Read-only with delete"));
				break;
			default:
				printf (_("Unknown"));
				break;
			}
			printf("\n");
		}
		if (sinfos[i].fields & GP_STORAGEINFO_STORAGETYPE) {
			printf ("type=%d ", sinfos[i].type);
			switch (sinfos[i].type) {
			default:
			case GP_STORAGEINFO_ST_UNKNOWN:
				printf ( _("Unknown"));
				break;
			case GP_STORAGEINFO_ST_FIXED_ROM:
				printf ( _("Fixed ROM"));
				break;
			case GP_STORAGEINFO_ST_REMOVABLE_ROM:
				printf ( _("Removable ROM"));
				break;
			case GP_STORAGEINFO_ST_FIXED_RAM:
				printf ( _("Fixed RAM"));
				break;
			case GP_STORAGEINFO_ST_REMOVABLE_RAM:
				printf ( _("Removable RAM"));
				break;
			}
			printf("\n");
		}
		if (sinfos[i].fields & GP_STORAGEINFO_FILESYSTEMTYPE) {
			printf ("fstype=%d ", sinfos[i].type);
			switch (sinfos[i].fstype) {
			default:
			case GP_STORAGEINFO_FST_UNDEFINED:
				printf ( _("Undefined"));
				break;
			case GP_STORAGEINFO_FST_GENERICFLAT:
				printf ( _("Generic Flat"));
				break;
			case GP_STORAGEINFO_FST_GENERICHIERARCHICAL:
				printf ( _("Generic Hierarchical"));
				break;
			case GP_STORAGEINFO_FST_DCF:
				printf ( _("Camera layout (DCIM)"));
				break;
			}
			printf("\n");
		}
		if (sinfos[i].fields & GP_STORAGEINFO_MAXCAPACITY)
			printf ("totalcapacity=%lu KB\n", (unsigned long)sinfos[i].capacitykbytes);
		if (sinfos[i].fields & GP_STORAGEINFO_FREESPACEKBYTES)
			printf ("free=%lu KB\n", (unsigned long)sinfos[i].freekbytes);
		if (sinfos[i].fields & GP_STORAGEINFO_FREESPACEIMAGES)
			printf ("freeimages=%lu\n", (unsigned long)sinfos[i].freeimages);
	}
	return GP_OK;
}

int
override_usbids_action (GPParams *p, int usb_vendor, int usb_product,
			int usb_vendor_modified, int usb_product_modified)
{
	CameraAbilitiesList *al = NULL;
	int r, n, i;
	CameraAbilities a;

	CR (gp_abilities_list_new (&al));

	/* The override_usbides_action() function is a notable
	 * exception to the rule that one is not supposed to use
	 * p->_abilities_list directly, because it has to and does so
	 * in a safe way. */
	n = gp_abilities_list_count (gp_params_abilities_list(p));
	for (i = 0; i < n; i++) {
		r = gp_abilities_list_get_abilities (gp_params_abilities_list(p), i,
						     &a);
		if (r < 0)
			continue;
		if ((a.usb_vendor  == usb_vendor) &&
		    (a.usb_product == usb_product)) {
			gp_log (GP_LOG_DEBUG, "main",
				_("Overriding USB vendor/product id "
				"0x%x/0x%x with 0x%x/0x%x"),
				a.usb_vendor, a.usb_product,
				usb_vendor_modified, usb_product_modified);
			a.usb_vendor  = usb_vendor_modified;
			a.usb_product = usb_product_modified;
		}
		gp_abilities_list_append (al, a);
	}

	gp_abilities_list_free (p->_abilities_list);
	p->_abilities_list = al;

	return (GP_OK);
}

/* time zero for debug log time stamps */
static struct timeval glob_tv_zero = { 0, 0 };

static void
#ifdef __GNUC__
		__attribute__((__format__(printf,3,0)))
#endif
debug_func (GPLogLevel level, const char *domain, const char *format,
	    va_list args, void *data)
{
	struct timeval tv;
	long sec, usec;
	FILE *logfile = (data != NULL)?(FILE *)data:stderr;

	gettimeofday (&tv,NULL);
	sec = tv.tv_sec  - glob_tv_zero.tv_sec;
	usec = tv.tv_usec - glob_tv_zero.tv_usec;
	if (usec < 0) {sec--; usec += 1000000L;}
	fprintf (logfile, "%li.%06li %s(%i): ", sec, usec, domain, level);
	vfprintf (logfile, format, args);
	fputc ('\n', logfile);
}

int
debug_action (GPParams *p, const char *debug_logfile_name)
{
	int n;
	FILE *logfile = NULL;

	/* make sure we're only executed once */
	static int debug_flag = 0;
	if (debug_flag != 0)
		return(GP_OK);
	debug_flag = 1;

	if (debug_logfile_name != NULL) {
	  /* FIXME: Handle fopen() error besides using stderr? */
	  logfile = fopen(debug_logfile_name, "a");
	}
	if (logfile == NULL) {
	  logfile = stderr;
	}
	setbuf(logfile, NULL);
	setbuf(stdout, NULL);

	gettimeofday (&glob_tv_zero, NULL);

	CR (p->debug_func_id = gp_log_add_func (GP_LOG_ALL, debug_func, (void *) logfile));
	gp_log (GP_LOG_DEBUG, "main", _("ALWAYS INCLUDE THE FOLLOWING LINES "
					"WHEN SENDING DEBUG MESSAGES TO THE "
					"MAILING LIST:"));

	for (n = 0; module_versions[n].name != NULL; n++) {
	  int i;
	  const char **v = NULL;
	  char *name = module_versions[n].name;
	  GPVersionFunc func = module_versions[n].version_func;
	  CHECK_NULL (name);
	  CHECK_NULL (func);
	  v = func(GP_VERSION_VERBOSE);
	  CHECK_NULL (v);
	  CHECK_NULL (v[0]);
	  gp_log (GP_LOG_DEBUG, "main", "%s %s", name, v[0]);
	  gp_log (GP_LOG_DEBUG, "main", _("%s has been compiled with the following options:"), name);
	  for (i = 1; v[i] != NULL; i++) {
	    gp_log (GP_LOG_DEBUG, "main", " + %s", v[i]);
	  }
	}

	if (1) {
		/* This is internal debug stuff for developers - no
		 * need for translation IMHO */
		const char *iolibs = getenv("IOLIBS");
		const char *camlibs = getenv("CAMLIBS");
		if (camlibs) {
			gp_log (GP_LOG_DEBUG, "main", "CAMLIBS = '%s'", camlibs);
		} else {
			gp_log (GP_LOG_DEBUG, "main", 
				"CAMLIBS env var not set, using compile-time default instead");
		}
		if (iolibs) {
			gp_log (GP_LOG_DEBUG, "main", "IOLIBS = '%s'", iolibs);
		} else {
			gp_log (GP_LOG_DEBUG, "main", 
				"IOLIBS env var not set, using compile-time default instead");
		}
	}

	return (GP_OK);
}

static void
display_widgets (CameraWidget *widget, char *prefix) {
	int 	ret, n, i;
	char	*newprefix;
	const char *label, *name, *uselabel;
	CameraWidgetType	type;

	gp_widget_get_label (widget, &label);
	/* fprintf(stderr,"label is %s\n", label); */
	ret = gp_widget_get_name (widget, &name);
	/* fprintf(stderr,"name is %s\n", name); */
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
		printf("%s\n",newprefix);
	for (i=0; i<n; i++) {
		CameraWidget *child;
	
		ret = gp_widget_get_child (widget, i, &child);
		if (ret != GP_OK)
			continue;
		display_widgets (child, newprefix);
	}
	free(newprefix);
}


int
list_config_action (GPParams *p) {
	CameraWidget *rootconfig;
	int	ret;

	ret = gp_camera_get_config (p->camera, &rootconfig, p->context);
	if (ret != GP_OK) return ret;
	display_widgets (rootconfig, "");
	gp_widget_free (rootconfig);
	return (GP_OK);
}

static int
_find_widget_by_name (GPParams *p, const char *name, CameraWidget **child, CameraWidget **rootconfig) {
	int	ret;

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
			if (!s) /* end of path */
				break;
			part = s+1;
			while (part[0] == '/')
				part++;
		}
		if (s) { /* if we have stuff left over, we failed */
			gp_context_error (p->context, _("%s not found in configuration tree."), newname);
			free (newname);
			gp_widget_free (*rootconfig);
			return GP_ERROR;
		}
		free (newname);
	}
	return GP_OK;
}

/* From the strftime(3) man page:
 * BUGS
 *     Some buggy versions of gcc complain about the use of %c: warning:
 *     %c yields only last 2 digits of year in some  locales.
 *     Of course programmers are encouraged to use %c, it gives the
 *     preferred date and time representation. One meets all kinds of
 *     strange obfuscations to circumvent this gcc problem. A relatively
 *     clean one is to add an intermediate function
 */

static size_t
my_strftime(char *s, size_t max, const char *fmt, const struct tm *tm)
{
	return strftime(s, max, fmt, tm);
}



int
get_config_action (GPParams *p, const char *name) {
	CameraWidget *rootconfig,*child;
	int	ret;
	const char *label;
	CameraWidgetType	type;

	ret = _find_widget_by_name (p, name, &child, &rootconfig);
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

	printf ("Label: %s\n", label); /* "Label:" is not i18ned, the "label" variable is */
	switch (type) {
	case GP_WIDGET_TEXT: {		/* char *		*/
		char *txt;

		ret = gp_widget_get_value (child, &txt);
		if (ret == GP_OK) {
			printf ("Type: TEXT\n"); /* parsed by scripts, no i18n */
			printf ("Current: %s\n",txt);
		} else {
			gp_context_error (p->context, _("Failed to retrieve value of text widget %s."), name);
		}
		break;
	}
	case GP_WIDGET_RANGE: {	/* float		*/
		float	f, t,b,s;

		ret = gp_widget_get_range (child, &b, &t, &s);
		if (ret == GP_OK)
			ret = gp_widget_get_value (child, &f);
		if (ret == GP_OK) {
			printf ("Type: RANGE\n");	/* parsed by scripts, no i18n */
			printf ("Current: %g\n", f);	/* parsed by scripts, no i18n */
			printf ("Bottom: %g\n", b);	/* parsed by scripts, no i18n */
			printf ("Top: %g\n", t);	/* parsed by scripts, no i18n */
			printf ("Step: %g\n", s);	/* parsed by scripts, no i18n */
		} else {
			gp_context_error (p->context, _("Failed to retrieve values of range widget %s."), name);
		}
		break;
	}
	case GP_WIDGET_TOGGLE: {	/* int		*/
		int	t;

		ret = gp_widget_get_value (child, &t);
		if (ret == GP_OK) {
			printf ("Type: TOGGLE\n");
			printf ("Current: %d\n",t);
		} else {
			gp_context_error (p->context, _("Failed to retrieve values of toggle widget %s."), name);
		}
		break;
	}
	case GP_WIDGET_DATE:  {		/* int			*/
		int	ret, t;
		time_t	xtime;
		struct tm *xtm;
		char	timebuf[200];

		ret = gp_widget_get_value (child, &t);
		if (ret != GP_OK) {
			gp_context_error (p->context, _("Failed to retrieve values of date/time widget %s."), name);
			break;
		}
		xtime = t;
		xtm = localtime (&xtime);
		ret = my_strftime (timebuf, sizeof(timebuf), "%c", xtm);
		printf ("Type: DATE\n");
		printf ("Current: %d\n", t);
		printf ("Printable: %s\n", timebuf);
		break;
	}
	case GP_WIDGET_MENU:
	case GP_WIDGET_RADIO: { /* char *		*/
		int cnt, i;
		char *current;

		ret = gp_widget_get_value (child, &current);
		if (ret == GP_OK) {
			cnt = gp_widget_count_choices (child);
			if (type == GP_WIDGET_MENU)
				printf ("Type: MENU\n");
			else
				printf ("Type: RADIO\n");
			printf ("Current: %s\n",current);
			for ( i=0; i<cnt; i++) {
				const char *choice;
				ret = gp_widget_get_choice (child, i, &choice);
				printf ("Choice: %d %s\n", i, choice);
			}
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
	gp_widget_free (rootconfig);
	return (GP_OK);
}

int
set_config_action (GPParams *p, const char *name, const char *value) {
	CameraWidget *rootconfig,*child;
	int	ret, ro;
	CameraWidgetType	type;

	ret = _find_widget_by_name (p, name, &child, &rootconfig);
	if (ret != GP_OK)
		return ret;

	ret = gp_widget_get_readonly (child, &ro);
	if (ret != GP_OK) {
		gp_widget_free (rootconfig);
		return ret;
	}
	if (ro == 1) {
		gp_context_error (p->context, _("Property %s is read only."), name);
		gp_widget_free (rootconfig);
		return GP_ERROR;
	}
	ret = gp_widget_get_type (child, &type);
	if (ret != GP_OK) {
		gp_widget_free (rootconfig);
		return ret;
	}

	switch (type) {
	case GP_WIDGET_TEXT: {		/* char *		*/
		ret = gp_widget_set_value (child, value);
		if (ret != GP_OK)
			gp_context_error (p->context, _("Failed to set the value of text widget %s to %s."), name, value);
		break;
	}
	case GP_WIDGET_RANGE: {	/* float		*/
		float	f,t,b,s;

		ret = gp_widget_get_range (child, &b, &t, &s);
		if (ret != GP_OK)
			break;
		if (!sscanf (value, "%f", &f)) {
			gp_context_error (p->context, _("The passed value %s is not a floating point value."), value);
			ret = GP_ERROR_BAD_PARAMETERS;
			break;
		}
		if ((f < b) || (f > t)) {
			gp_context_error (p->context, _("The passed value %f is not within the expected range %f - %f."), f, b, t);
			ret = GP_ERROR_BAD_PARAMETERS;
			break;
		}
		ret = gp_widget_set_value (child, &f);
		if (ret != GP_OK)
			gp_context_error (p->context, _("Failed to set the value of range widget %s to %f."), name, f);
		break;
	}
	case GP_WIDGET_TOGGLE: {	/* int		*/
		int	t;

		t = 2;
		if (	!strcasecmp (value, "off")	|| !strcasecmp (value, "no")	||
			!strcasecmp (value, "false")	|| !strcmp (value, "0")		||
			!strcasecmp (value, _("off"))	|| !strcasecmp (value, _("no"))	||
			!strcasecmp (value, _("false"))
		)
			t = 0;
		if (	!strcasecmp (value, "on")	|| !strcasecmp (value, "yes")	||
			!strcasecmp (value, "true")	|| !strcmp (value, "1")		||
			!strcasecmp (value, _("on"))	|| !strcasecmp (value, _("yes"))	||
			!strcasecmp (value, _("true"))
		)
			t = 1;
		/*fprintf (stderr," value %s, t %d\n", value, t);*/
		if (t == 2) {
			gp_context_error (p->context, _("The passed value %s is not a valid toggle value."), value);
			ret = GP_ERROR_BAD_PARAMETERS;
			break;
		}
		ret = gp_widget_set_value (child, &t);
		if (ret != GP_OK)
			gp_context_error (p->context, _("Failed to set values %s of toggle widget %s."), value, name);
		break;
	}
	case GP_WIDGET_DATE:  {		/* int			*/
		int	t = -1;
		struct tm xtm;

#ifdef HAVE_STRPTIME
		if (strptime (value, "%c", &xtm) || strptime (value, "%Ec", &xtm))
			t = mktime (&xtm);
#endif
		if (t == -1) {
			if (!sscanf (value, "%d", &t)) {
				gp_context_error (p->context, _("The passed value %s is neither a valid time nor an integer."), value);
				ret = GP_ERROR_BAD_PARAMETERS;
				break;
			}
		}
		ret = gp_widget_set_value (child, &t);
		if (ret != GP_OK)
			gp_context_error (p->context, _("Failed to set new time of date/time widget %s to %s."), name, value);
		break;
	}
	case GP_WIDGET_MENU:
	case GP_WIDGET_RADIO: { /* char *		*/
		int cnt, i;
		char *endptr;

		cnt = gp_widget_count_choices (child);
		if (cnt < GP_OK) {
			ret = cnt;
			break;
		}
		ret = GP_ERROR_BAD_PARAMETERS;
		for ( i=0; i<cnt; i++) {
			const char *choice;

			ret = gp_widget_get_choice (child, i, &choice);
			if (ret != GP_OK)
				continue;
			if (!strcmp (choice, value)) {
				ret = gp_widget_set_value (child, value);
				break;
			}
		}
		if (i != cnt)
			break;

		/* make sure we parse just 1 integer, and there is nothing more. 
		 * sscanf just does not provide this, we need strtol.
		 */
		i = strtol (value, &endptr, 10);
		if ((value != endptr) && (*endptr == '\0')) {
			if ((i>= 0) && (i < cnt)) {
				const char *choice;

				ret = gp_widget_get_choice (child, i, &choice);
				if (ret == GP_OK)
					ret = gp_widget_set_value (child, choice);
				break;
			}
		}
		/* Lets just try setting the value directly, in case we have flexible setters,
		 * like PTP shutterspeed. */
		ret = gp_widget_set_value (child, value);
		if (ret == GP_OK)
			break;
		gp_context_error (p->context, _("Choice %s not found within list of choices."), value);
		break;
	}

	/* ignore: */
	case GP_WIDGET_WINDOW:
	case GP_WIDGET_SECTION:
	case GP_WIDGET_BUTTON:
		gp_context_error (p->context, _("The %s widget is not configurable."), name);
		ret = GP_ERROR_BAD_PARAMETERS;
		break;
	}
	if (ret == GP_OK) {
		ret = gp_camera_set_config (p->camera, rootconfig, p->context);
		if (ret != GP_OK)
			gp_context_error (p->context, _("Failed to set new configuration value %s for configuration entry %s."), value, name);
	}
	gp_widget_free (rootconfig);
	return (ret);
}

int
set_config_index_action (GPParams *p, const char *name, const char *value) {
	CameraWidget *rootconfig,*child;
	int	ret;
	const char *label;
	CameraWidgetType	type;

	ret = _find_widget_by_name (p, name, &child, &rootconfig);
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
		ret = gp_camera_set_config (p->camera, rootconfig, p->context);
		if (ret != GP_OK)
			gp_context_error (p->context, _("Failed to set new configuration value %s for configuration entry %s."), value, name);
	}
	gp_widget_free (rootconfig);
	return (ret);
}


int
set_config_value_action (GPParams *p, const char *name, const char *value) {
	CameraWidget *rootconfig,*child;
	int	ret;
	CameraWidgetType	type;

	ret = _find_widget_by_name (p, name, &child, &rootconfig);
	if (ret != GP_OK)
		return ret;

	ret = gp_widget_get_type (child, &type);
	if (ret != GP_OK) {
		gp_widget_free (rootconfig);
		return ret;
	}

	switch (type) {
	case GP_WIDGET_TEXT: {		/* char *		*/
		ret = gp_widget_set_value (child, value);
		if (ret != GP_OK)
			gp_context_error (p->context, _("Failed to set the value of text widget %s to %s."), name, value);
		break;
	}
	case GP_WIDGET_RANGE: {	/* float		*/
		float	f,t,b,s;

		ret = gp_widget_get_range (child, &b, &t, &s);
		if (ret != GP_OK)
			break;
		if (!sscanf (value, "%f", &f)) {
			gp_context_error (p->context, _("The passed value %s is not a floating point value."), value);
			ret = GP_ERROR_BAD_PARAMETERS;
			break;
		}
		if ((f < b) || (f > t)) {
			gp_context_error (p->context, _("The passed value %f is not within the expected range %f - %f."), f, b, t);
			ret = GP_ERROR_BAD_PARAMETERS;
			break;
		}
		ret = gp_widget_set_value (child, &f);
		if (ret != GP_OK)
			gp_context_error (p->context, _("Failed to set the value of range widget %s to %f."), name, f);
		break;
	}
	case GP_WIDGET_TOGGLE: {	/* int		*/
		int	t;

		t = 2;
		if (	!strcasecmp (value, "off")	|| !strcasecmp (value, "no")	||
			!strcasecmp (value, "false")	|| !strcmp (value, "0")		||
			!strcasecmp (value, _("off"))	|| !strcasecmp (value, _("no"))	||
			!strcasecmp (value, _("false"))
		)
			t = 0;
		if (	!strcasecmp (value, "on")	|| !strcasecmp (value, "yes")	||
			!strcasecmp (value, "true")	|| !strcmp (value, "1")		||
			!strcasecmp (value, _("on"))	|| !strcasecmp (value, _("yes"))	||
			!strcasecmp (value, _("true"))
		)
			t = 1;
		/*fprintf (stderr," value %s, t %d\n", value, t);*/
		if (t == 2) {
			gp_context_error (p->context, _("The passed value %s is not a valid toggle value."), value);
			ret = GP_ERROR_BAD_PARAMETERS;
			break;
		}
		ret = gp_widget_set_value (child, &t);
		if (ret != GP_OK)
			gp_context_error (p->context, _("Failed to set values %s of toggle widget %s."), value, name);
		break;
	}
	case GP_WIDGET_DATE:  {		/* int			*/
		int	t = -1;
		struct tm xtm;

#ifdef HAVE_STRPTIME
		if (strptime (value, "%c", &xtm) || strptime (value, "%Ec", &xtm))
			t = mktime (&xtm);
#endif
		if (t == -1) {
			if (!sscanf (value, "%d", &t)) {
				gp_context_error (p->context, _("The passed value %s is neither a valid time nor an integer."), value);
				ret = GP_ERROR_BAD_PARAMETERS;
				break;
			}
		}
		ret = gp_widget_set_value (child, &t);
		if (ret != GP_OK)
			gp_context_error (p->context, _("Failed to set new time of date/time widget %s to %s."), name, value);
		break;
	}
	case GP_WIDGET_MENU:
	case GP_WIDGET_RADIO: { /* char *		*/
		int cnt, i;

		cnt = gp_widget_count_choices (child);
		if (cnt < GP_OK) {
			ret = cnt;
			break;
		}
		ret = GP_ERROR_BAD_PARAMETERS;
		for ( i=0; i<cnt; i++) {
			const char *choice;

			ret = gp_widget_get_choice (child, i, &choice);
			if (ret != GP_OK)
				continue;
			if (!strcmp (choice, value)) {
				ret = gp_widget_set_value (child, value);
				break;
			}
		}
		if (i != cnt)
			break;
		/* Lets just try setting the value directly, in case we have flexible setters,
		 * like PTP shutterspeed. */
		ret = gp_widget_set_value (child, value);
		if (ret == GP_OK) break;
		gp_context_error (p->context, _("Choice %s not found within list of choices."), value);
		break;
	}

	/* ignore: */
	case GP_WIDGET_WINDOW:
	case GP_WIDGET_SECTION:
	case GP_WIDGET_BUTTON:
		gp_context_error (p->context, _("The %s widget is not configurable."), name);
		ret = GP_ERROR_BAD_PARAMETERS;
		break;
	}
	if (ret == GP_OK) {
		ret = gp_camera_set_config (p->camera, rootconfig, p->context);
		if (ret != GP_OK)
			gp_context_error (p->context, _("Failed to set new configuration value %s for configuration entry %s."), value, name);
	}
	gp_widget_free (rootconfig);
	return (ret);
}


/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
