/* actions.c
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

#include <config.h>
#include "actions.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-setting.h>

#include "main.h"
#include "globals.h"

#ifdef HAVE_AA
#  include "gphoto2-cmd-capture.h"
#endif

#ifdef HAVE_EXIF
#  include <libexif/exif-data.h>
#endif

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define CR(result)       {int r=(result); if (r<0) return r;}
#define CRU(result,file) {int r=(result); if (r<0) {gp_file_unref(file);return r;}}

int
delete_all_action (ActionParams *p)
{
	return (gp_camera_folder_delete_all (p->camera, p->folder, p->context));
}

int
action_camera_upload_file (Camera *camera, const char *folder, const char *path)
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
	if (glob_filename && strcmp (glob_filename, "")) {
		res = gp_file_set_name (file, glob_filename);
		if (res < 0) {
			gp_file_unref (file);
			return (res);
		}
	}

	res = gp_camera_folder_put_file (camera, folder, file,
					 glob_context);
	gp_file_unref (file);

	return (res);
}

int
num_files_action (ActionParams *p)
{
	CameraList list;
	int n;

	CR (gp_camera_folder_list_files (p->camera, p->folder,
					 &list, p->context));
	CR (n = gp_list_count (&list));
	if (glob_quiet)
		fprintf (stdout, "%i\n", n);
	else
		fprintf (stdout, _("Number of files in "
				   "folder '%s': %i\n"), p->folder, n);

	return (GP_OK);
}

int
list_folders_action (ActionParams *p)
{
	CameraList list;
	int count;
	const char *name;
	unsigned int i;

	CR (gp_camera_folder_list_folders (p->camera, p->folder, &list,
					   p->context));
	CR (count = gp_list_count (&list));
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
		CR (gp_list_get_name (&list, i, &name));
		printf (" - %s\n", name);
	}

	return (GP_OK);
}

int
list_files_action (ActionParams *p)
{
	CameraList list;
	int count;
	const char *name;
	unsigned int i;

	CR (gp_camera_folder_list_files (p->camera, p->folder, &list,
					 p->context));
	CR (count = gp_list_count (&list));
	switch (count) {
	case 0:
		printf (_("There are no files in folder '%s'."), p->folder);
		printf ("\n");
		break;
	case 1:
		printf (_("There is one file in folder '%s':"), p->folder);
		printf ("\n");
		break;
	default:
		printf (_("There are %i files in folder '%s':"), count,
			p->folder);
		printf ("\n");
		break;
	}
	for (i = 0; i < count; i++) {
		CR (gp_list_get_name (&list, i, &name));
		CR (print_file_action (p, name));
	}

	return (GP_OK);
}

int
print_info_action (ActionParams *p, const char *filename)
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
print_file_action (ActionParams *p, const char *filename)
{
	static int x=0;

	if (glob_quiet)
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
save_file_action (ActionParams *p, const char *filename)
{
	return (save_file_to_file (p->camera, p->context, p->folder, filename,
				   GP_FILE_TYPE_NORMAL));
}

int
save_exif_action (ActionParams *p, const char *filename)
{
	return (save_file_to_file (p->camera, p->context, p->folder, filename,
				   GP_FILE_TYPE_EXIF));
}

int
save_thumbnail_action (ActionParams *p, const char *filename)
{
	return (save_file_to_file (p->camera, p->context, p->folder, filename,
				   GP_FILE_TYPE_PREVIEW));
}

int
save_raw_action (ActionParams *p, const char *filename)
{
	return (save_file_to_file (p->camera, p->context, p->folder, filename,
				   GP_FILE_TYPE_RAW));
}

int
save_audio_action (ActionParams *p, const char *filename)
{
	return (save_file_to_file (p->camera, p->context, p->folder, filename,
				   GP_FILE_TYPE_AUDIO));
}

int
delete_file_action (ActionParams *p, const char *filename)
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
                printf ("%-59.59s", exif_entry_get_value (e));
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
print_exif_action (ActionParams *p, const char *filename)
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
                gp_context_error (glob_context,
				  _("Could not parse EXIF data."));
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
	gp_context_error (glob_context, _("gphoto2 has been compiled without "
		"EXIF support."));
	return (GP_ERROR_NOT_SUPPORTED);
#endif
}

int
list_cameras_action (void)
{
	int r = GP_OK, n, i;
	CameraAbilities a;

	r = gp_abilities_list_count (glob_abilities_list);
	if (r < 0)
		return (r);
	if (glob_quiet)
		fprintf (stdout, "%i\n", r);
	else {
		fprintf (stdout, _("Number of supported "
					"cameras: %i\n"), r);
		fprintf (stdout, _("Supported cameras:\n"));
	}
	n = r;
	for (i = 0; i < n; i++) {
		r = gp_abilities_list_get_abilities (glob_abilities_list,
						     i, &a);
		if (r < 0)
			break;
		if (glob_quiet)
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
list_ports_action (void)
{
	GPPortInfoList *list;
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

	if (!glob_quiet) {
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
auto_detect_action (void)
{
	int x, count;
        CameraList list;
        CameraAbilitiesList *al;
        GPPortInfoList *il;
        const char *name, *value;

        gp_abilities_list_new (&al);
        gp_abilities_list_load (al, glob_context);
        gp_port_info_list_new (&il);
        gp_port_info_list_load (il);
        gp_abilities_list_detect (al, il, &list, glob_context);
        gp_abilities_list_free (al);
        gp_port_info_list_free (il);

        CR (count = gp_list_count (&list));

        printf(_("%-30s %-16s\n"), _("Model"), _("Port"));
        printf(_("----------------------------------------------------------\n"));
        for (x = 0; x < count; x++) {
                CR (gp_list_get_name  (&list, x, &name));
                CR (gp_list_get_value (&list, x, &value));
                printf(_("%-30s %-16s\n"), name, value);
        }

        return GP_OK;
}

int
action_camera_show_abilities (Camera *camera)
{
	CameraAbilities a;
	int i;

	CR (gp_camera_get_abilities (camera, &a));
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
	if (a.operations & GP_OPERATION_CAPTURE_IMAGE)
		printf (_("                                 : Image\n"));
	if (a.operations & GP_OPERATION_CAPTURE_VIDEO)
		printf (_("                                 : Video\n"));
	if (a.operations & GP_OPERATION_CAPTURE_AUDIO)
		printf (_("                                 : Audio\n"));
	if (a.operations & GP_OPERATION_CAPTURE_PREVIEW)
		printf (_("                                 : Preview\n"));
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
action_camera_set_port (Camera *camera, const char *port)
{
	GPPortInfoList *il;
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
	r = gp_camera_set_port_info (camera, info);
	if (r < 0)
		return (r);
	gp_setting_set ("gphoto2", "port", info.path);

	return (GP_OK);
}

int
action_camera_about (Camera *camera)
{
	CameraText text;

	CR (gp_camera_get_about (camera, &text, glob_context));
	
	fprintf (stdout, _("About the camera driver:"));
	fputc ('\n', stdout);
	fprintf (stdout, "%s\n", _(text.text));

	return (GP_OK);
}

int
action_camera_summary (Camera *camera)
{
	CameraText text;

	CR (gp_camera_get_summary (camera, &text, glob_context));

	fprintf (stdout, _("Camera summary:"));
	fputc ('\n', stdout);
	fprintf (stdout, "%s\n", _(text.text));

	return (GP_OK);
}

int
action_camera_manual (Camera *camera)
{
	CameraText text;

	CR (gp_camera_get_manual (camera, &text, glob_context));

	fprintf (stdout, _("Camera manual:"));
	fputc ('\n', stdout);
	fprintf (stdout, "%s\n", _(text.text));

	return (GP_OK);
}

int
action_camera_set_speed (Camera *camera, unsigned int speed)
{
	GPPortInfo info;

	/* Make sure we've got a serial port. */
	CR (gp_camera_get_port_info (camera, &info));
	if (info.type != GP_PORT_SERIAL) {
		if (!glob_quiet) {
			fprintf (stderr, _("You can only specify speeds for "
					   "serial ports."));
			fputc ('\n', stderr);
		}
		return (GP_ERROR_BAD_PARAMETERS);
	}

	/* Set the speed. */
	CR (gp_camera_set_port_speed (camera, speed));

	return (GP_OK);
}

int
action_camera_set_model (Camera *camera, const char *model)
{
	CameraAbilities a;
	int m;

	CR (m = gp_abilities_list_lookup_model (glob_abilities_list, model));
	CR (gp_abilities_list_get_abilities (glob_abilities_list, m, &a));
	CR (gp_camera_set_abilities (camera, a));
	gp_setting_set ("gphoto2", "model", a.model);

	return (GP_OK);
}

int
set_folder_action (const char *folder)
{
	if (glob_folder)
		free (glob_folder);
	glob_folder = malloc (sizeof (char) * (strlen (folder) + 1));
	if (!glob_folder)
		return (GP_ERROR_NO_MEMORY);
	strcpy (glob_folder, folder);

	return (GP_OK);
}

int
set_filename_action (const char *filename)
{
	if (glob_filename)
		free (glob_filename);
	glob_filename = malloc (sizeof (char) * (strlen (filename) + 1));
	if (!glob_filename)
		return (GP_ERROR_NO_MEMORY);
	strcpy (glob_filename, filename);

	return (GP_OK);
}

int
print_version_action (void)
{
	printf (_("gPhoto (v%s) - Cross-platform digital camera library.\n"
		  "Copyright (C) 2000-2002 Scott Fritzinger and others\n"
		  "%s"
		  "Licensed under the Library GNU Public License (LGPL).\n"),
			VERSION,
#ifdef OS2
			_("OS/2 port by Bart van Leeuwen\n")
#else
			""
#endif
			);

	return (GP_OK);
}

int
action_camera_capture_preview (Camera *camera)
{
	CameraFile *file;
	int r;

	CR (gp_file_new (&file));
#ifdef HAVE_AA
	r = gp_cmd_capture_preview (camera, file, glob_context);
#else
	r = gp_camera_capture_preview (camera, file, glob_context);
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
override_usbids_action (int usb_vendor, int usb_product,
			int usb_vendor_modified, int usb_product_modified)
{
	CameraAbilitiesList *al = NULL;
	int r, n, i;
	CameraAbilities a;

	CR (gp_abilities_list_new (&al));

	n = gp_abilities_list_count (glob_abilities_list);
	for (i = 0; i < n; i++) {
		r = gp_abilities_list_get_abilities (glob_abilities_list, i,
						     &a);
		if (r < 0)
			continue;
		if ((a.usb_vendor  == usb_vendor) &&
		    (a.usb_product == usb_product)) {
			gp_log (GP_LOG_DEBUG, "main",
				"Overriding USB vendor/product id "
				"0x%x/0x%x with 0x%x/0x%x",
				a.usb_vendor, a.usb_product,
				usb_vendor_modified, usb_product_modified);
			a.usb_vendor  = usb_vendor_modified;
			a.usb_product = usb_product_modified;
		}
		gp_abilities_list_append (al, a);
	}
	gp_abilities_list_free (glob_abilities_list);
	glob_abilities_list = al;

	return (GP_OK);
}
