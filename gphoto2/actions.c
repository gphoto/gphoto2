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

#include "config.h"
#include "actions.h"
#include "i18n.h"
#include "main.h"
#include "version.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <time.h>
#include <sys/time.h>

#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-setting.h>

#ifdef HAVE_AA
#  include "gphoto2-cmd-capture.h"
#endif

#ifdef HAVE_EXIF
#  include <libexif/exif-data.h>
#endif

#define CR(result)       {int r=(result); if (r<0) return r;}
#define CRU(result,file) {int r=(result); if (r<0) {gp_file_unref(file);return r;}}
#define CL(result,list)  {int r=(result); if (r<0) {gp_list_free(list); return r;}}

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
	}

	res = gp_camera_folder_put_file (p->camera, folder, file,
					 p->context);
	gp_file_unref (file);

	return (res);
}

int
num_files_action (GPParams *p)
{
	CameraList *list;
	int n;

	CR (gp_list_new (&list));
	CL (gp_camera_folder_list_files (p->camera, p->folder,
					 list, p->context), list);
	CL (n = gp_list_count (list), list);
	gp_list_free (list);
	if (p->quiet)
		fprintf (stdout, "%i\n", n);
	else
		fprintf (stdout, _("Number of files in "
				   "folder '%s': %i\n"), p->folder, n);

	return (GP_OK);
}

int
list_folders_action (GPParams *p)
{
	CameraList *list;
	int count;
	const char *name;
	unsigned int i;
	
	CR (gp_list_new (&list));

	CL (gp_camera_folder_list_folders (p->camera, p->folder, list,
					   p->context), list);
	CL (count = gp_list_count (list), list);
	switch (count) {
        case 0:
                printf (_("There are no folders in folder '%s'."), p->folder);
                printf ("\n");
                break;
        case 1:
                printf (_("There is one folder in folder '%s':"), p->folder);
                printf ("\n");
                break;
        default:
                printf (_("There are %i folders in folder '%s':"),
			 count, p->folder);
                printf ("\n");
                break;
	}
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
	int count;
	const char *name;
	unsigned int i;

	CR (gp_list_new (&list));
	CL (gp_camera_folder_list_files (p->camera, p->folder, list,
					 p->context), list);
	CL (count = gp_list_count (list), list);
	switch (count) {
	case 0:
		fprintf (stdout, _("There are no files in folder '%s'."),
			 p->folder);
		fputc ('\n', stdout);
		break;
	case 1:
		fprintf (stdout, _("There is one file in folder '%s':"),
			 p->folder);
		fputc ('\n', stdout);
		break;
	default:
		fprintf (stdout, _("There are %i files in folder '%s':"),
			 count, p->folder);
		fputc ('\n', stdout);
		break;
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
			printf (_("  Size:        %li byte(s)\n"), info.file.size);
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
			printf ("\n");
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
			printf (_("  Size:        %li byte(s)\n"), info.preview.size);
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
			printf (_("  Size:       %li byte(s)\n"), info.audio.size);
		if (info.audio.fields & GP_FILE_INFO_STATUS)
			printf (_("  Downloaded: %s\n"),
				(info.audio.status == GP_FILE_STATUS_DOWNLOADED) ? _("yes") : _("no"));
	}

	return (GP_OK);
}

int
print_file_action (GPParams *p, const char *filename)
{
	static int x=0;

	if (p->quiet)
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
			printf(" %5ld KB", (info.file.size+1023) / 1024);
		    if ((info.file.fields & GP_FILE_INFO_WIDTH) && +
			    (info.file.fields & GP_FILE_INFO_HEIGHT))
			printf(" %4dx%-4d", info.file.width, info.file.height);
		    if (info.file.fields & GP_FILE_INFO_TYPE)
			printf(" %s", info.file.type);
			printf("\n");
		} else {
		    printf("#%-5i %s\n", x+1, filename);
		}
	}
	x++;
	return (GP_OK);
}

int
save_file_action (GPParams *p, const char *filename)
{
	return (save_file_to_file (p->camera, p->context, p->folder, filename,
				   GP_FILE_TYPE_NORMAL));
}

int
save_exif_action (GPParams *p, const char *filename)
{
	return (save_file_to_file (p->camera, p->context, p->folder, filename,
				   GP_FILE_TYPE_EXIF));
}

int
save_thumbnail_action (GPParams *p, const char *filename)
{
	return (save_file_to_file (p->camera, p->context, p->folder, filename,
				   GP_FILE_TYPE_PREVIEW));
}

int
save_raw_action (GPParams *p, const char *filename)
{
	return (save_file_to_file (p->camera, p->context, p->folder, filename,
				   GP_FILE_TYPE_RAW));
}

int
save_audio_action (GPParams *p, const char *filename)
{
	return (save_file_to_file (p->camera, p->context, p->folder, filename,
				   GP_FILE_TYPE_AUDIO));
}

int
save_all_audio_action (GPParams *p, const char *filename)
{
	/* not every file has an associated audio file */
	if (camera_file_exists(p->camera, p->context, p->folder, filename,
			       GP_FILE_TYPE_AUDIO))
		return (save_file_to_file (p->camera, p->context, p->folder, filename,
					   GP_FILE_TYPE_AUDIO));
	else
		return GP_OK;
}

int
delete_file_action (GPParams *p, const char *filename)
{
	return (gp_camera_file_delete (p->camera, p->folder, filename,
				       p->context));
}

#ifdef HAVE_EXIF
static void
show_ifd (ExifContent *content)
{
        ExifEntry *e;
        unsigned int i;

        for (i = 0; i < content->count; i++) {
                e = content->entries[i];
                printf ("%-20.20s", exif_tag_get_name (e->tag));
                printf ("|");
#ifdef HAVE_EXIF_0_6_9
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
                printf ("-");
        printf ("+");
        for (i = 0; i < 59; i++)
                printf ("-");
        printf ("\n"); 
}
#endif

int
print_exif_action (GPParams *p, const char *filename)
{
#ifdef HAVE_EXIF
        CameraFile *file;
        const char *data;
        unsigned long size;
        ExifData *ed;
#ifdef HAVE_EXIF_0_5_4
	unsigned int i;
#endif

        CR (gp_file_new (&file));
        CRU (gp_camera_file_get (p->camera, p->folder, filename,
				 GP_FILE_TYPE_EXIF, file, p->context), file);
        CRU (gp_file_get_data_and_size (file, &data, &size), file);
        ed = exif_data_new_from_data (data, size);
        gp_file_unref (file);
        if (!ed) {
                gp_context_error (p->context, _("Could not parse EXIF data."));
                return (GP_ERROR);
        }

        printf (_("EXIF tags:"));
        printf ("\n");
        print_hline ();
        printf ("%-20.20s", _("Tag"));
        printf ("|");
        printf ("%-59.59s", _("Value"));
        printf ("\n");
        print_hline ();
#ifdef HAVE_EXIF_0_5_4
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
                printf ("\n");
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

	r = gp_abilities_list_count (p->abilities_list);
	if (r < 0)
		return (r);
	if (p->quiet)
		fprintf (stdout, "%i\n", r);
	else {
		fprintf (stdout, _("Number of supported "
					"cameras: %i\n"), r);
		fprintf (stdout, _("Supported cameras:\n"));
	}
	n = r;
	for (i = 0; i < n; i++) {
		r = gp_abilities_list_get_abilities (p->abilities_list,
						     i, &a);
		if (r < 0)
			break;
		if (p->quiet)
			fprintf (stdout, "%s\n", a.model);
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

int
list_ports_action (GPParams *p)
{
	GPPortInfoList *list = NULL;
	GPPortInfo info;
	int x, count, result = GP_OK;

	CR (gp_port_info_list_new (&list));
	result = gp_port_info_list_load (list);
	if (result < 0) {
		gp_port_info_list_free (list);
		return (result);
	}
	count = gp_port_info_list_count (list);
	if (count < 0) {
		gp_port_info_list_free (list);
		return (count);
	}

	if (!p->quiet) {
		printf(_("Devices found: %i\n"), count);
		printf(_("Path                             Description\n"
			"--------------------------------------------------------------\n"));
	} else
		printf("%i\n", count);

	/* Now list the ports */
	for (x = 0; x < count; x++) {
		result = gp_port_info_list_get_info (list, x, &info);
		if (result < 0)
			break;
		printf ("%-32s %-32s\n", info.path, info.name);
	}

	gp_port_info_list_free (list);

	return (result);

}

int
auto_detect_action (GPParams *p)
{
	int x, count;
        CameraList *list;
        CameraAbilitiesList *al = NULL;
        GPPortInfoList *il = NULL;
        const char *name = NULL, *value = NULL;

	CR (gp_list_new (&list));
        gp_abilities_list_new (&al);
        gp_abilities_list_load (al, p->context);
        gp_port_info_list_new (&il);
        gp_port_info_list_load (il);
        gp_abilities_list_detect (al, il, list, p->context);
        gp_abilities_list_free (al);
        gp_port_info_list_free (il);

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
	printf (_("Delete files on camera support   : %s\n"),
		(a.file_operations & GP_FILE_OPERATION_DELETE) ?
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
	GPPortInfoList *il = NULL;
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
	r = gp_port_info_list_new (&il);
	if (r < 0)
		return (r);
	r = gp_port_info_list_load (il);
	if (r < 0) {
		gp_port_info_list_free (il);
		return (r);
	}

	/* Search our port in the list. */
	p = gp_port_info_list_lookup_path (il, verified_port);
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
	if (p < 0) {
		gp_port_info_list_free (il);
		return (p);
	}

	/* Get info about our port. */
	r = gp_port_info_list_get_info (il, p, &info);
	gp_port_info_list_free (il);
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
	
	fprintf (stdout, _("About the camera driver:"));
	fputc ('\n', stdout);
	fprintf (stdout, "%s\n", _(text.text));

	return (GP_OK);
}

int
action_camera_summary (GPParams *params)
{
	CameraText text;

	CR (gp_camera_get_summary (params->camera, &text, params->context));

	fprintf (stdout, _("Camera summary:"));
	fputc ('\n', stdout);
	fprintf (stdout, "%s\n", _(text.text));

	return (GP_OK);
}

int
action_camera_manual (GPParams *params)
{
	CameraText text;

	CR (gp_camera_get_manual (params->camera, &text, params->context));

	fprintf (stdout, _("Camera manual:"));
	fputc ('\n', stdout);
	fprintf (stdout, "%s\n", _(text.text));

	return (GP_OK);
}

int
action_camera_set_speed (GPParams *p, unsigned int speed)
{
	GPPortInfo info;

	/* Make sure we've got a serial port. */
	CR (gp_camera_get_port_info (p->camera, &info));
	if (info.type != GP_PORT_SERIAL) {
		if (!p->quiet) {
			fprintf (stderr, _("You can only specify speeds for "
					   "serial ports."));
			fputc ('\n', stderr);
		}
		return (GP_ERROR_BAD_PARAMETERS);
	}

	/* Set the speed. */
	CR (gp_camera_set_port_speed (p->camera, speed));

	return (GP_OK);
}

int
action_camera_set_model (GPParams *p, const char *model)
{
	CameraAbilities a;
	int m;

	CR (m = gp_abilities_list_lookup_model (p->abilities_list, model));
	CR (gp_abilities_list_get_abilities (p->abilities_list, m, &a));
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
print_version_action (GPParams *p)
{
	int n;
	printf (_("gphoto2 %s\n"
		  "\n"
		  "Copyright (c) 2000-2004 Lutz Mueller and others\n"
		  "%s"
		  "\n"
		  "gphoto2 comes with NO WARRANTY, to the extent permitted by law. You may\n"
		  "redistribute copies of gphoto2 under the terms of the GNU General Public\n"
		  "License. For more information about these matters, see the files named COPYING.\n"
		  "\n"
		  "This version of gphoto2 is using the following software versions and options:\n"),

		VERSION,
#ifdef OS2
			_("OS/2 port by Bart van Leeuwen\n")
#else
			""
#endif
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
	  printf ("%-17s %-12s ", name, v[0]);
	  for (i = 1; v[i] != NULL; i++) {
		  if (v[i+1] != NULL) {
			  printf ("%s, ", v[i]);
		  } else {
			  printf ("%s", v[i]);
		  }
	  }
	  printf ("\n");
	}

	return (GP_OK);
}

int
action_camera_capture_preview (GPParams *p)
{
	CameraFile *file;
	int r;

	CR (gp_file_new (&file));
#ifdef HAVE_AA
	r = gp_cmd_capture_preview (p->camera, file, p->context);
#else
	r = gp_camera_capture_preview (p->camera, file, p->context);
#endif
	if (r < 0) {
		gp_file_unref (file);
		return (r);
	}

	r = save_camera_file_to_file (NULL, file);
	gp_file_unref (file);
	if (r < 0) 
		return (r);

	return (GP_OK);
}

int
override_usbids_action (GPParams *p, int usb_vendor, int usb_product,
			int usb_vendor_modified, int usb_product_modified)
{
	CameraAbilitiesList *al = NULL;
	int r, n, i;
	CameraAbilities a;

	CR (gp_abilities_list_new (&al));

	n = gp_abilities_list_count (p->abilities_list);
	for (i = 0; i < n; i++) {
		r = gp_abilities_list_get_abilities (p->abilities_list, i,
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
	gp_abilities_list_free (p->abilities_list);
	p->abilities_list = al;

	return (GP_OK);
}

/* time zero for debug log time stamps */
struct timeval glob_tv_zero = { 0, 0 };

static void
#ifdef __GNUC__
		__attribute__((__format__(printf,3,0)))
#endif
debug_func (GPLogLevel level, const char *domain, const char *format,
	    va_list args, void *data)
{
	struct timeval tv;
	long sec, usec;

	gettimeofday (&tv,NULL);
	sec = tv.tv_sec  - glob_tv_zero.tv_sec;
	usec = tv.tv_usec - glob_tv_zero.tv_usec;
	if (usec < 0) {sec--; usec += 1000000L;}
	fprintf (stderr, "%li.%06li %s(%i): ", sec, usec, domain, level);
	vfprintf (stderr, format, args);
	fprintf (stderr, "\n");
}

int
debug_action (GPParams *p)
{
	int n;

	/* make sure we're only executed once */
	static int debug_flag = 0;
	if (debug_flag != 0)
		return(GP_OK);
	debug_flag = 1;

	gettimeofday (&glob_tv_zero, NULL);

	CR (p->debug_func_id = gp_log_add_func (GP_LOG_ALL, debug_func, NULL));
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
	sprintf(newprefix,"%s/%s",prefix,uselabel);

	if ((type != GP_WIDGET_WINDOW) && (type != GP_WIDGET_SECTION)) {
		printf("%s\n",newprefix);
	}
	for (i=0; i<n; i++) {
		CameraWidget *child;
	
		ret = gp_widget_get_child (widget, i, &child);
		if (ret != GP_OK)
			continue;
		display_widgets (child, newprefix);
	}
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
		ret = strftime (timebuf, sizeof(timebuf), "%c", xtm);
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
		if (	strcasecmp (value, "off")	|| strcasecmp (value, "no")	||
			strcasecmp (value, "false")	|| strcmp (value, "0")		||
			strcasecmp (value, _("off"))	|| strcasecmp (value, _("no"))	||
			strcasecmp (value, _("false"))
		)
			t = 0;
		if (	strcasecmp (value, "on")	|| strcasecmp (value, "yes")	||
			strcasecmp (value, "true")	|| strcmp (value, "0")		||
			strcasecmp (value, _("on"))	|| strcasecmp (value, _("yes"))	||
			strcasecmp (value, _("true"))
		)
			t = 1;
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
	case GP_WIDGET_WINDOW:
	case GP_WIDGET_SECTION:
	case GP_WIDGET_BUTTON:
		gp_context_error (p->context, _("The %s widget is not configurable.\n"), name);
		ret = GP_ERROR_BAD_PARAMETERS;
		break;
	}
	if (ret == GP_OK) {
		ret = gp_camera_set_config (p->camera, rootconfig, p->context);
		if (ret != GP_OK)
			gp_context_error (p->context, _("Failed to set new configuration value %s for configuration entry %s.\n"), value, name);
	}
	gp_widget_free (rootconfig);
	return (ret);
}
