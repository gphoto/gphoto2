/*
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

#include "config.h"
#include "main.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>

#ifdef HAVE_POPT
#  include <popt.h>
/* POPT_TABLEEND is only defined from popt 1.6.1 */
# ifndef POPT_TABLEEND
#  define POPT_TABLEEND { NULL, '\0', 0, 0, 0, NULL, NULL }
# endif
#endif

#ifdef HAVE_RL
#  include <readline/readline.h>
#endif

#ifdef HAVE_PTHREAD
#  include <pthread.h>
#endif

#ifndef WIN32
#  include <signal.h>
#endif

#include "actions.h"
#include "foreach.h"
#include "options.h"
#include "range.h"
#include "shell.h"

#ifdef HAVE_CDK
#  include "gphoto2-cmd-config.h"
#endif

#include "gp-params.h"

#include "gphoto2-port-info-list.h"
#include "gphoto2-port-log.h"
#include "gphoto2-setting.h"

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

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define CR(result) {int r = (result); if (r < 0) return (r);}

/* Command-line option
   -----------------------------------------------------------------------
   this is funky and may look a little scary, but sounded cool to do.
   it makes sense since this is just a wrapper for a functional/flow-based
   library.
   AFTER NOTE: whoah. gnome does something like this. cool deal :)

   When the program starts it calls (in order):
        1) verify_options() to make sure all options are valid,
           and that any options needing an argument have one (or if they
           need an argument, they don't have one).
        2) (optional) option_is_present() to see if a particular option was
           typed in the command-line. This can be used to set up something
           before the next step.
        3) execute_options() to actually parse the command-line and execute
           the command-line options in order.

   This might sound a little complex, but makes adding options REALLY easy,
   and it is very flexible.

   How to add a command-line option:
*/

/* 1) Add a forward-declaration here                                    */
/*    ----------------------------------------------------------------- */
/*    Use the OPTION_CALLBACK(function) macro.                          */

#ifndef HAVE_POPT
OPTION_CALLBACK(abilities);
#ifdef HAVE_CDK
OPTION_CALLBACK(config);
#endif
#ifdef HAVE_EXIF
OPTION_CALLBACK(show_exif);
#endif
OPTION_CALLBACK(show_info);
OPTION_CALLBACK(help);
OPTION_CALLBACK(version);
OPTION_CALLBACK(shell);
OPTION_CALLBACK(list_cameras);
OPTION_CALLBACK(auto_detect);
OPTION_CALLBACK(list_ports);
OPTION_CALLBACK(filename);
OPTION_CALLBACK(port);
OPTION_CALLBACK(speed);
OPTION_CALLBACK(usbid);
OPTION_CALLBACK(model);
OPTION_CALLBACK(quiet);
OPTION_CALLBACK(debug);
OPTION_CALLBACK(use_folder);
OPTION_CALLBACK(recurse);
OPTION_CALLBACK(no_recurse);
OPTION_CALLBACK(use_stdout);
OPTION_CALLBACK(use_stdout_size);
OPTION_CALLBACK(list_folders);
OPTION_CALLBACK(list_files);
OPTION_CALLBACK(num_files);
OPTION_CALLBACK(get_file);
OPTION_CALLBACK(get_all_files);
OPTION_CALLBACK(get_thumbnail);
OPTION_CALLBACK(get_all_thumbnails);
OPTION_CALLBACK(get_raw_data);
OPTION_CALLBACK(get_all_raw_data);
OPTION_CALLBACK(get_audio_data);
OPTION_CALLBACK(get_all_audio_data);
OPTION_CALLBACK(delete_file);
OPTION_CALLBACK(delete_all_files);
OPTION_CALLBACK(upload_file);
OPTION_CALLBACK(capture_image);
OPTION_CALLBACK(capture_preview);
OPTION_CALLBACK(capture_movie);
OPTION_CALLBACK(capture_sound);
OPTION_CALLBACK(summary);
OPTION_CALLBACK(manual);
OPTION_CALLBACK(about);
OPTION_CALLBACK(make_dir);
OPTION_CALLBACK(remove_dir);

/* 2) Add an entry in the option table                          */
/*    ----------------------------------------------------------------- */
/*    Format for option is:                                             */
/*     {"short", "long", "argument", "description", callback_function, required}, */
/*    if it is just a flag, set the argument to "".                     */
/*    Order is important! Options are exec'd in the order in the table! */

Option option[] = {

/* Settings needed for formatting output */
{"",  "debug", "", N_("Turn on debugging"),              debug,         0},
{"q", "quiet", "", N_("Quiet output (default=verbose)"), quiet,         0},

/* Display and die actions */

{"v", "version",     "", N_("Display version and exit"),     version,        0},
{"h", "help",        "", N_("Displays this help screen"),    help,           0},
{"",  "list-cameras","", N_("List supported camera models"), list_cameras,   0},
{"",  "list-ports",  "", N_("List supported port devices"),  list_ports,     0},
{"",  "stdout",      "", N_("Send file to stdout"),          use_stdout,     0},
{"",  "stdout-size", "", N_("Print filesize before data"),   use_stdout_size,0},
{"",  "auto-detect", "", N_("List auto-detected cameras"),   auto_detect,    0},

/* Settings needed for camera functions */
{"" , "port",     "path",     N_("Specify port device"),            port,      0},
{"" , "speed",    "speed",    N_("Specify serial transfer speed"),  speed,     0},
{"" , "camera",   "model",    N_("Specify camera model"),           model,     0},
{"" , "filename", "filename", N_("Specify a filename"),             filename, 0},
{"" , "usbid",    "usbid",    N_("(expert only) Override USB IDs"), usbid,     0},

/* Actions that depend on settings */
{"a", "abilities", "",       N_("Display camera abilities"), abilities,    0},
{"f", "folder",    "folder", N_("Specify camera folder (default=\"/\")"),use_folder,0},
{"R", "recurse", "",  N_("Recursion (default for download)"), recurse, 0},
{"", "no-recurse", "",  N_("No recursion (default for deletion)"), no_recurse, 0},
{"l", "list-folders",   "", N_("List folders in folder"), list_folders,   0},
{"L", "list-files",     "", N_("List files in folder"),   list_files,     0},
{"m", "mkdir", N_("name"),  N_("Create a directory"),     make_dir,       0},
{"r", "rmdir", N_("name"),  N_("Remove a directory"),     remove_dir,     0},
{"n", "num-files", "",           N_("Display number of files"),     num_files,   0},
{"p", "get-file", "range",       N_("Get files given in range"),    get_file,    0},
{"P", "get-all-files","",        N_("Get all files from folder"),   get_all_files,0},
{"t", "get-thumbnail",  "range",  N_("Get thumbnails given in range"),  get_thumbnail,  0},
{"T", "get-all-thumbnails","",    N_("Get all thumbnails from folder"), get_all_thumbnails,0},
{"r", "get-raw-data", "range",    N_("Get raw data given in range"),    get_raw_data, 0},
{"", "get-all-raw-data", "",      N_("Get all raw data from folder"),   get_all_raw_data, 0},
{"", "get-audio-data", "range",   N_("Get audio data given in range"),  get_audio_data, 0},
{"", "get-all-audio-data", "",    N_("Get all audio data from folder"), get_all_audio_data, 0},
{"d", "delete-file", "range",  N_("Delete files given in range"), delete_file, 0},
{"D", "delete-all-files","",     N_("Delete all files in folder"), delete_all_files,0},
{"u", "upload-file", "filename", N_("Upload a file to camera"),    upload_file, 0},
#ifdef HAVE_CDK
{"" , "config",         "",  N_("Configure"),               config,          0},
#endif
{"" , "capture-preview", "", N_("Capture a quick preview"), capture_preview, 0},
{"" , "capture-image",  "",  N_("Capture an image"),        capture_image,   0},
{"" , "capture-movie",  "",  N_("Capture a movie "),        capture_movie,   0},
{"" , "capture-sound",  "",  N_("Capture an audio clip"),   capture_sound,   0},
#ifdef HAVE_EXIF
{"", "show-exif", "range",   N_("Show EXIF information"), show_exif, 0},
#endif
{"", "show-info", "range",   N_("Show info"), show_info, 0},
{"",  "summary",        "",  N_("Summary of camera status"), summary, 0},
{"",  "manual",         "",  N_("Camera driver manual"),     manual,  0},
{"",  "about",          "",  N_("About the camera driver"),  about,   0},
{"",  "shell",          "",  N_("gPhoto shell"),             shell,   0},

/* End of list                  */
{"" , "", "", "", NULL, 0}
};

/* 3) Add any necessary global variables                                */
/*    ----------------------------------------------------------------- */
/*    Most flags will set options, like choosing a port, camera model,  */
/*    etc...                                                            */

int  glob_option_count = 0;
#endif

int  glob_debug = -1;
int  glob_stdout=0;
int  glob_stdout_size=0;
char glob_cancel = 0;

GPParams p;

/* 4) Finally, add your callback function.                              */
/*    ----------------------------------------------------------------- */
/*    The callback function is passed "char *arg" to the argument of    */
/*    command-line option. It must return GP_OK or GP_ERROR.            */
/*    Again, use the OPTION_CALLBACK(function) macro.                   */

#ifndef HAVE_POPT

OPTION_CALLBACK(help)
{
        usage (&p);
        exit (EXIT_SUCCESS);

        return GP_OK;
}

OPTION_CALLBACK(version)
{
	CR (print_version_action (&p));

        exit (EXIT_SUCCESS);

        return GP_OK;
}

OPTION_CALLBACK(use_stdout) {

        p.quiet  = 1; /* implied */
        glob_stdout = 1;

        return GP_OK;
}

OPTION_CALLBACK (use_stdout_size)
{
        glob_stdout_size = 1;
        use_stdout(arg);

        return GP_OK;
}

OPTION_CALLBACK (auto_detect)
{
	CR (auto_detect_action (&p));

        return GP_OK;
}

OPTION_CALLBACK (abilities)
{
	CR (action_camera_show_abilities (&p));

        return (GP_OK);
}


OPTION_CALLBACK(list_cameras)
{
	CR (list_cameras_action (&p));

        return (GP_OK);
}

OPTION_CALLBACK(list_ports)
{
	CR (list_ports_action (&p));

        return (GP_OK);
}

OPTION_CALLBACK(filename)
{
	CR (set_filename_action (&p, arg));

        return (GP_OK);
}

OPTION_CALLBACK(port)
{
	CR (action_camera_set_port (&p, arg));

        return (GP_OK);
}

OPTION_CALLBACK(speed)
{
	CR (action_camera_set_speed (&p, atoi (arg)));

        return (GP_OK);
}

OPTION_CALLBACK(model)
{
	CR (action_camera_set_model (&p, arg));

        return (GP_OK);
}

OPTION_CALLBACK (usbid)
{
	int usb_product, usb_vendor, usb_product_modified, usb_vendor_modified;

	gp_log (GP_LOG_DEBUG, "main", "Overriding USB IDs to '%s'...", arg);

	if (sscanf (arg, "0x%x:0x%x=0x%x:0x%x", &usb_vendor_modified,
		    &usb_product_modified, &usb_vendor, &usb_product) != 4) {
		printf (_("Use the following syntax a:b=c:d to treat any "
			  "USB device detected as a:b as c:d instead. "
			  "a b c d should be hexadecimal numbers beginning "
			  "with '0x'.\n"));
		return (GP_ERROR_BAD_PARAMETERS);
	}
	CR (override_usbids_action (&p, usb_vendor, usb_product,
				usb_vendor_modified, usb_product_modified));

	return (GP_OK);
}

OPTION_CALLBACK (debug)
{
	CR (debug_action (&p));

	return (GP_OK);
}

OPTION_CALLBACK(quiet)
{
        p.quiet=1;

        return (GP_OK);
}

OPTION_CALLBACK(shell)
{
	CR (shell_prompt (&p));

	return (GP_OK);
}

OPTION_CALLBACK (use_folder)
{
	CR (set_folder_action (&p, arg));

        return (GP_OK);
}

OPTION_CALLBACK (recurse)
{
	p.flags |= FOR_EACH_FLAGS_RECURSE;

        return (GP_OK);
}

OPTION_CALLBACK (no_recurse)
{
	p.flags &= ~FOR_EACH_FLAGS_RECURSE;

        return (GP_OK);
}

OPTION_CALLBACK (list_folders)
{
	CR (for_each_folder (&p, list_folders_action));

	return (GP_OK);
}

#ifdef HAVE_CDK
OPTION_CALLBACK(config)
{
	CR (gp_cmd_config (p.camera, p.context));

	return (GP_OK);
}
#endif

OPTION_CALLBACK(show_info)
{
	/*
	 * If the user specified the file directly (and not a range),
	 * directly dump info.
	 */
	if (strchr (arg, '.'))
		return (print_info_action (&p, arg));

	return (for_each_file_in_range (&p, print_info_action, arg));
}

#ifdef HAVE_EXIF
OPTION_CALLBACK(show_exif)
{
	/*
	 * If the user specified the file directly (and not a range),
	 * directly dump EXIF information.
	 */
	if (strchr (arg, '.'))
		return (print_exif_action (&p, arg));

	return (for_each_file_in_range (&p, print_exif_action, arg));
}
#endif

OPTION_CALLBACK (list_files)
{
        return (for_each_folder (&p, list_files_action));
}

OPTION_CALLBACK (num_files)
{
	return (num_files_action (&p));
}

#endif

static struct {
	CameraFileType type;
	const char *prefix;
} PrefixTable[] = {
	{GP_FILE_TYPE_NORMAL, ""},
	{GP_FILE_TYPE_PREVIEW, "thumb_"},
	{GP_FILE_TYPE_RAW, "raw_"},
	{GP_FILE_TYPE_AUDIO, "audio_"},
	{0, NULL}
};

static struct {
	const char *s;
	const char *l;
} MonthTable[] = {
	{N_("Jan"), N_("January")},
	{N_("Feb"), N_("February")},
	{N_("Mar"), N_("March")},
	{N_("Apr"), N_("April")},
	{N_("May"), N_("May")},
	{N_("Jun"), N_("June")},
	{N_("Jul"), N_("July")},
	{N_("Aug"), N_("August")},
	{N_("Sep"), N_("September")},
	{N_("Oct"), N_("October")},
	{N_("Nov"), N_("November")},
	{N_("Dec"), N_("December")}
};

static struct {
	const char *s;
	const char *l;
} WeekdayTable[] = {
	{N_("Sun"), N_("Sunday")},
	{N_("Mon"), N_("Monday")},
	{N_("Tue"), N_("Tuesday")},
	{N_("Wed"), N_("Wednesday")},
	{N_("Thu"), N_("Thursday")},
	{N_("Fri"), N_("Friday")},
	{N_("Sat"), N_("Saturday")}
};

#undef  MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

static int
get_path_for_file (const char *folder, CameraFile *file, char **path)
{
	unsigned int i, l;
	int n;
	char *s, b[1024];
	const char *name, *prefix;
	CameraFileType type;
	time_t t;
	struct tm *tm;

	if (!file || !path)
		return (GP_ERROR_BAD_PARAMETERS);

	*path = NULL;
	CR (gp_file_get_name (file, &name));
	CR (gp_file_get_mtime (file, &t));
	tm = localtime (&t);

	/*
	 * If the user didn't specify a filename, use the original name 
	 * (and prefix).
	 */
	if (!p.filename || !strcmp (p.filename, "")) {
		CR (gp_file_get_type (file, &type));
		for (i = 0; PrefixTable[i].prefix; i++)
			if (PrefixTable[i].type == type)
				break;
		prefix = (PrefixTable[i].prefix ? PrefixTable[i].prefix :
						  "unknown_");
		*path = malloc (strlen (prefix) + strlen (name) + 1);
		if (!*path)
			return (GP_ERROR_NO_MEMORY);
		strcpy (*path, prefix);
		strcat (*path, name);
		return (GP_OK);
	}

	/* The user did specify a filename. Use it. */
	b[sizeof (b) - 1] = '\0';
	for (i = 0; i < strlen (p.filename); i++) {
		if (p.filename[i] == '%') {
			switch (p.filename[++i]) {
			case 'n':

				/*
				 * Get the number of the file. This can only
				 * be done with persistent files!
				 */
				if (!folder) {
					gp_context_error (p.context, 
						_("You cannot use '%n' "
						  "in combination with "
						  "non-persistent files!"));
					return (GP_ERROR_BAD_PARAMETERS);
				}
				n = gp_filesystem_number (p.camera->fs,
					folder, name, p.context);
				if (n < 0) {
					free (*path);
					*path = NULL;
					return (n);
				}
				snprintf (b, sizeof (b), "%i", n + 1);
				break;

			case 'C':

				/* Get the suffix of the original name */
				s = strrchr (name, '.');
				if (!s) {
					free (*path);
					*path = NULL;
					gp_context_error (p.context,
						_("The filename provided "
						  "by the camera ('%s') "
						  "does not contain a "
						  "suffix!"), name);
					return (GP_ERROR_BAD_PARAMETERS);
				}
				strncpy (b, s + 1, sizeof (b) - 1);
				break;

			case 'f':

				/* Get the file name without suffix */
				s = strrchr (name, '.');
				if (!s)
					strncpy (b, name, sizeof (b) - 1);
				else {
					l = MIN (sizeof (b) - 1, s - name);
					strncpy (b, name, l);
					b[l] = '\0';
				}
				break;

			case 'a':
				snprintf (b, sizeof (b), "%s",
					  WeekdayTable[tm->tm_wday].s);
				break;
			case 'A':
				snprintf (b, sizeof (b), "%s",
					  WeekdayTable[tm->tm_wday].l);
				break;
			case 'b':
				snprintf (b, sizeof (b), "%s",
					  MonthTable[tm->tm_mon].s);
				break;
			case 'B':
				snprintf (b, sizeof (b), "%s",
					  MonthTable[tm->tm_mon].l);
				break;
			case 'd':
				snprintf (b, sizeof (b), "%02i", tm->tm_mday);
				break;
			case 'H':
			case 'k':
				snprintf (b, sizeof (b), "%02i", tm->tm_hour);
				break;
			case 'I':
			case 'l':
				snprintf (b, sizeof (b), "%02i",
					  tm->tm_hour % 12);
				break;
			case 'j':
				snprintf (b, sizeof (b), "%03i", tm->tm_yday);
				break;
			case 'm':
				snprintf (b, sizeof (b), "%02i", tm->tm_mon);
				break;
			case 'M':
				snprintf (b, sizeof (b), "%02i", tm->tm_min);
				break;
			case 'S':
				snprintf (b, sizeof (b), "%02i", tm->tm_sec);
				break;
			case 'y':
				snprintf (b, sizeof (b), "%02i", tm->tm_year);
				break;
			case 'Y':
				snprintf (b, sizeof (b), "%i",
					  tm->tm_year + 1900);
				break;
			case '%':
				strcpy (b, "%");
				break;
			default:
				free (*path);
				*path = NULL;
				gp_context_error (p.context,
					_("Invalid format '%s' (error at "
					  "position %i)."), p.filename,
					i + 1);
				return (GP_ERROR_BAD_PARAMETERS);
			}
		} else {
			b[0] = p.filename[i];
			b[1] = '\0';
		}

		s = *path ? realloc (*path, strlen (*path) + strlen (b) + 1) :
			    malloc (strlen (b) + 1);
		if (!s) {
			free (*path);
			*path = NULL;
			return (GP_ERROR_NO_MEMORY);
		}
		if (*path) {
			*path = s;
			strcat (*path, b);
		} else {
			*path = s;
			strcpy (*path, b);
		}
	}

	return (GP_OK);
}

int
save_camera_file_to_file (const char *folder, CameraFile *file)
{
	char *path = NULL, s[1024], c[1024];
	CameraFileType type;

	CR (gp_file_get_type (file, &type));

	CR (get_path_for_file (folder, file, &path));
	strncpy (s, path, sizeof (s) - 1);
	s[sizeof (s) - 1] = '\0';
	free (path);
	path = NULL;

        if (!p.quiet) {
                while (!p.force_overwrite && GP_SYSTEM_IS_FILE (s)) {
			do {
				putchar ('\007');
				printf (_("File %s exists. Overwrite? [y|n] "),
					s);
				fflush (stdout);
				fgets (c, sizeof (c) - 1, stdin);
			} while ((c[0]!='y')&&(c[0]!='Y')&&
				 (c[0]!='n')&&(c[0]!='N'));

			if ((c[0]=='y') || (c[0]=='Y'))
				break;

			do { 
				printf (_("Specify new filename? [y|n] "));
				fflush (stdout); 
				fgets (c, sizeof (c) - 1, stdin);
			} while ((c[0]!='y')&&(c[0]!='Y')&&
				 (c[0]!='n')&&(c[0]!='N'));

			if (!((c[0]=='y') || (c[0]=='Y')))
				return (GP_OK);

			printf (_("Enter new filename: "));
			fflush (stdout);
			fgets (s, sizeof (s) - 1, stdin);
			s[strlen (s) - 1] = 0;
                }
                printf (_("Saving file as %s\n"), s);
        }
	CR (gp_file_save (file, s));

	return (GP_OK);
}

int
save_file_to_file (Camera *camera, GPContext *context, const char *folder,
		   const char *filename, CameraFileType type)
{
        int res;

        CameraFile *file;

        CR (gp_file_new (&file));
        CR (gp_camera_file_get (camera, folder, filename, type,
                                          file, context));

        if (glob_stdout) {
                const char *data;
                long int size;

                CR (gp_file_get_data_and_size (file, &data, &size));

                if (glob_stdout_size)
                        printf ("%li\n", size);
                fwrite (data, sizeof(char), size, stdout);
                gp_file_unref (file);
                return (GP_OK);
        }

        res = save_camera_file_to_file (folder, file);

        gp_file_unref (file);

        return (res);
}

/*
  get_file_common() - parse range, download specified files, or their
        thumbnails according to thumbnail argument, and save to files.
*/

static int
get_file_common (const char *arg, CameraFileType type )
{
        gp_log (GP_LOG_DEBUG, "main", "Getting '%s'...", arg);

	/*
	 * If the user specified the file directly (and not a number),
	 * get that file.
	 */
        if (strchr (arg, '.'))
                return (save_file_to_file (p.camera, p.context,
					   p.folder, arg, type));

        switch (type) {
        case GP_FILE_TYPE_PREVIEW:
		CR (for_each_file_in_range (&p, save_thumbnail_action,
					    arg));
		break;
        case GP_FILE_TYPE_NORMAL:
                CR (for_each_file_in_range (&p, save_file_action, arg));
		break;
        case GP_FILE_TYPE_RAW:
                CR (for_each_file_in_range (&p, save_raw_action, arg));
		break;
	case GP_FILE_TYPE_AUDIO:
		CR (for_each_file_in_range (&p, save_audio_action, arg));
		break;
	case GP_FILE_TYPE_EXIF:
		CR (for_each_file_in_range (&p, save_exif_action, arg));
		break;
        default:
                return (GP_ERROR_NOT_SUPPORTED);
        }

	return (GP_OK);
}

#ifndef HAVE_POPT
OPTION_CALLBACK (get_file)
{
        return get_file_common (arg, GP_FILE_TYPE_NORMAL);
}

OPTION_CALLBACK (get_all_files)
{
        CR (for_each_file (&p, save_file_action));

	return (GP_OK);
}

OPTION_CALLBACK (get_thumbnail)
{
        return (get_file_common (arg, GP_FILE_TYPE_PREVIEW));
}

OPTION_CALLBACK(get_all_thumbnails)
{
        CR (for_each_file (&p, save_thumbnail_action));

	return (GP_OK);
}

OPTION_CALLBACK (get_raw_data)
{
        return (get_file_common (arg, GP_FILE_TYPE_RAW));
}

OPTION_CALLBACK (get_all_raw_data)
{
        CR (for_each_file (&p, save_raw_action));

	return (GP_OK);
}

OPTION_CALLBACK (get_audio_data)
{
	return (get_file_common (arg, GP_FILE_TYPE_AUDIO));
}

OPTION_CALLBACK (get_all_audio_data)
{
	CR (for_each_file (&p, save_audio_action));

	return (GP_OK);
}

OPTION_CALLBACK (delete_file)
{
	if (strchr (arg, '.'))
		return (delete_file_action (&p, arg));

	return (for_each_file_in_range (&p, delete_file_action, arg));
}

OPTION_CALLBACK (delete_all_files)
{
	CR (for_each_folder (&p, delete_all_action));

	return (GP_OK);
}

OPTION_CALLBACK (upload_file)
{
        gp_log (GP_LOG_DEBUG, "main", "Uploading file...");

	CR (action_camera_upload_file (&p, p.folder, arg));

        return (GP_OK);
}

OPTION_CALLBACK (make_dir)
{
	CR (gp_camera_folder_make_dir (p.camera, p.folder, arg, 
						 p.context));

	return (GP_OK);
}

OPTION_CALLBACK (remove_dir)
{
	CR (gp_camera_folder_remove_dir (p.camera, p.folder, arg,
						   p.context));

	return (GP_OK);
}
#endif

static int
capture_generic (CameraCaptureType type, const char *name)
{
        CameraFilePath path;
        char *pathsep;
        int result;

	result =  gp_camera_capture (p.camera, type, &path, p.context);
	if (result != GP_OK) {
		cli_error_print("Could not capture.");
		return (result);
	}

	if (strcmp(path.folder, "/") == 0)
		pathsep = "";
	else
		pathsep = "/";

	if (p.quiet)
		printf ("%s%s%s\n", path.folder, pathsep, path.name);
	else
		printf (_("New file is in location %s%s%s on the camera\n"),
			path.folder, pathsep, path.name);

        return (GP_OK);
}

#ifndef HAVE_POPT
OPTION_CALLBACK (capture_image)
{
	return (capture_generic (GP_CAPTURE_IMAGE, arg));
}

OPTION_CALLBACK (capture_movie)
{
        return (capture_generic (GP_CAPTURE_MOVIE, arg));
}

OPTION_CALLBACK (capture_sound)
{
        return (capture_generic (GP_CAPTURE_SOUND, arg));
}

OPTION_CALLBACK (capture_preview)
{
	CR (action_camera_capture_preview (&p));

	return (GP_OK);
}

OPTION_CALLBACK(summary)
{
	CR (action_camera_summary (&p));

        return (GP_OK);
}

OPTION_CALLBACK (manual)
{
	CR (action_camera_manual (&p));

        return (GP_OK);
}

OPTION_CALLBACK (about)
{
	CR (action_camera_about (&p));

        return (GP_OK);
}

#endif

/* Set/init global variables                                    */
/* ------------------------------------------------------------ */

#ifdef HAVE_PTHREAD

typedef struct _ThreadData ThreadData;
struct _ThreadData {
	Camera *camera;
	unsigned int timeout;
	CameraTimeoutFunc func;
};

static void
thread_cleanup_func (void *data)
{
	ThreadData *td = data;

	free (td);
}

static void *
thread_func (void *data)
{
	ThreadData *td = data;
	time_t t, last;

	pthread_cleanup_push (thread_cleanup_func, td);

	last = time (NULL);
	while (1) {
		t = time (NULL);
		if (t - last > td->timeout) {
			td->func (td->camera, NULL);
			last = t;
		}
		pthread_testcancel ();
	}

	pthread_cleanup_pop (1);
}

static unsigned int
start_timeout_func (Camera *camera, unsigned int timeout,
		    CameraTimeoutFunc func, void *data)
{
	pthread_t tid;
	ThreadData *td;

	td = malloc (sizeof (ThreadData));
	if (!td)
		return 0;
	memset (td, 0, sizeof (ThreadData));
	td->camera = camera;
	td->timeout = timeout;
	td->func = func;

	pthread_create (&tid, NULL, thread_func, td);

	return (tid);
}

static void
stop_timeout_func (Camera *camera, unsigned int id, void *data)
{
	pthread_t tid = id;

	pthread_cancel (tid);
	pthread_join (tid, NULL);
}

#endif

/* Misc functions                                                       */
/* ------------------------------------------------------------------   */

void
cli_error_print (char *format, ...)
{
        va_list         pvar;

        fprintf(stderr, _("ERROR: "));
        va_start(pvar, format);
        vfprintf(stderr, format, pvar);
        va_end(pvar);
        fprintf(stderr, "\n");
}

static void
signal_resize (int signo)
{
	const char *columns;

	columns = getenv ("COLUMNS");
	if (columns && atoi (columns))
		p.cols = atoi (columns);
}

static void
signal_exit (int signo)
{
	/* If we already were told to cancel, abort. */
	if (glob_cancel) {
		if (!p.quiet)
			printf (_("\nAborting...\n"));
		if (p.camera)
			gp_camera_unref (p.camera);
		if (p.context)
			gp_context_unref (p.context);
		if (!p.quiet)
			printf (_("Aborted.\n"));
		exit (EXIT_FAILURE);
	}

        if (!p.quiet)
                printf (_("\nCancelling...\n"));

	glob_cancel = 1;
}

/* Main :)                                                              */
/* -------------------------------------------------------------------- */

#ifdef HAVE_POPT

typedef enum {
	ARG_ABILITIES,
	ARG_ABOUT,
	ARG_AUTO_DETECT,
	ARG_CAPTURE_IMAGE,
	ARG_CAPTURE_MOVIE,
	ARG_CAPTURE_PREVIEW,
	ARG_CAPTURE_SOUND,
	ARG_CONFIG,
	ARG_DEBUG,
	ARG_DELETE_ALL_FILES,
	ARG_DELETE_FILE,
	ARG_FILENAME,
	ARG_FOLDER,
	ARG_FORCE_OVERWRITE,
	ARG_GET_ALL_AUDIO_DATA,
	ARG_GET_ALL_FILES,
	ARG_GET_ALL_RAW_DATA,
	ARG_GET_ALL_THUMBNAILS,
	ARG_GET_AUDIO_DATA,
	ARG_GET_FILE,
	ARG_GET_RAW_DATA,
	ARG_GET_THUMBNAIL,
	ARG_LIST_CAMERAS,
	ARG_LIST_FILES,
	ARG_LIST_FOLDERS,
	ARG_LIST_PORTS,
	ARG_MANUAL,
	ARG_MKDIR,
	ARG_MODEL,
	ARG_NO_RECURSE,
	ARG_NUM_FILES,
	ARG_PORT,
	ARG_QUIET,
	ARG_RECURSE,
	ARG_RMDIR,
	ARG_SHELL,
	ARG_SHOW_EXIF,
	ARG_SHOW_INFO,
	ARG_SPEED,
	ARG_STDOUT,
	ARG_STDOUT_SIZE,
	ARG_SUMMARY,
	ARG_UPLOAD_FILE,
	ARG_USBID,
	ARG_VERSION
} Arg;

typedef enum {
	CALLBACK_PARAMS_TYPE_PREINITIALIZE,
	CALLBACK_PARAMS_TYPE_INITIALIZE,
	CALLBACK_PARAMS_TYPE_QUERY,
	CALLBACK_PARAMS_TYPE_RUN
} CallbackParamsType;

typedef struct _CallbackParams CallbackParams;
struct _CallbackParams {
	CallbackParamsType type;
	union {
		/*
		 * CALLBACK_PARAMS_TYPE_RUN,
		 * CALLBACK_PARAMS_TYPE_INITIALIZE,
		 * CALLBACK_PARAMS_TYPE_PREINITIALIZE,
		 */
		int r;

		/* CALLBACK_PARAMS_TYPE_QUERY */
		struct {
			Arg arg;
			char found;
		} q;
	} p;
};

static void
cb_arg (poptContext ctx, enum poptCallbackReason reason,
	const struct poptOption *opt, const char *arg, void *data)
{
	CallbackParams *params = (CallbackParams *) data;
	int usb_product, usb_vendor, usb_product_modified, usb_vendor_modified;

	/* Check if we are only to query. */
	if (params->type == CALLBACK_PARAMS_TYPE_QUERY) {
		if (opt->val == params->p.q.arg)
			params->p.q.found = 1;
		return;
	}

	/* Check if we are only to pre-initialize. */
	if (params->type == CALLBACK_PARAMS_TYPE_PREINITIALIZE) {
		switch (opt->val) {
		case ARG_USBID:
			gp_log (GP_LOG_DEBUG, "main", "Overriding USB "
				"IDs to '%s'...", arg);
			if (sscanf (arg, "0x%x:0x%x=0x%x:0x%x",
				    &usb_vendor_modified,
				    &usb_product_modified, &usb_vendor,
				    &usb_product) != 4) {
				fprintf (stdout, _("Use the following syntax "
						   "a:b=c:d to treat any "
						   "USB device detected as "
						   "a:b as c:d instead. "
						   "a b c d should be "
						   "hexadecimal numbers "
						   "beginning with '0x'.\n"));
				params->p.r = GP_ERROR_BAD_PARAMETERS;
				break;
			}
			params->p.r = override_usbids_action (&p, usb_vendor,
					usb_product, usb_vendor_modified,
					usb_product_modified);
			break;
		default:
			break;
		}
		return;
	}

	/* Check if we are only to initialize. */
	if (params->type == CALLBACK_PARAMS_TYPE_INITIALIZE) {
		switch (opt->val) {
		case ARG_FILENAME:
			params->p.r = set_filename_action (&p, arg);
			break;
		case ARG_FOLDER:
			params->p.r = set_folder_action (&p, arg);
			break;
		case ARG_FORCE_OVERWRITE:
			p.force_overwrite = 1;
			break;
		case ARG_MODEL:
			gp_log (GP_LOG_DEBUG, "main", "Processing 'model' "
				"option ('%s')...", arg);
			params->p.r = action_camera_set_model (&p, arg);
			break;
		case ARG_NO_RECURSE:
			p.flags &= ~FOR_EACH_FLAGS_RECURSE;
			break;
		case ARG_PORT:
			gp_log (GP_LOG_DEBUG, "main", "Processing 'port' "
				"option ('%s')...", arg);
			params->p.r = action_camera_set_port (&p, arg);
			break;
		case ARG_QUIET:
			p.quiet = 1;
			break;
		case ARG_RECURSE:
			p.flags |= FOR_EACH_FLAGS_RECURSE;
			break;
		case ARG_SPEED:
			params->p.r = action_camera_set_speed (&p, atoi (arg));
			break;
		case ARG_STDOUT:
			p.quiet = 1;
			glob_stdout = 1;
			break;
		case ARG_STDOUT_SIZE:
			glob_stdout_size = 1;
			p.quiet = 1;
			glob_stdout = 1;
			break;
		case ARG_VERSION:
			params->p.r = print_version_action (&p);
			break;
		default:
			break;
		}
		return;
	}

	switch (opt->val) {
	case ARG_ABILITIES:
		params->p.r = action_camera_show_abilities (&p);
		break;
	case ARG_ABOUT:
		params->p.r = action_camera_about (&p);
		break;
	case ARG_AUTO_DETECT:
		params->p.r = auto_detect_action (&p);
		break;
	case ARG_CAPTURE_IMAGE:
		params->p.r = capture_generic (GP_CAPTURE_IMAGE, arg);
		break;
	case ARG_CAPTURE_MOVIE:
		params->p.r = capture_generic (GP_CAPTURE_MOVIE, arg);
		break;
	case ARG_CAPTURE_PREVIEW:
		params->p.r = action_camera_capture_preview (&p);
		break;
	case ARG_CAPTURE_SOUND:
		params->p.r = capture_generic (GP_CAPTURE_SOUND, arg);
		break;
	case ARG_CONFIG:
#ifdef HAVE_CDK
		params->p.r = gp_cmd_config (p.camera, p.context);
#else
		gp_context_error (p.context,
				  _("gphoto2 has been compiled without "
				    "support for CDK."));
		params->p.r = GP_ERROR_NOT_SUPPORTED;
#endif
		break;
	case ARG_DELETE_ALL_FILES:
		params->p.r = for_each_folder (&p, delete_all_action);
		break;
	case ARG_DELETE_FILE:

		/* Did the user specify a file or a range? */
		if (strchr (arg, '.')) {
			params->p.r = delete_file_action (&p, arg);
			break;
		}
		params->p.r = for_each_file_in_range (&p,
					delete_file_action, arg);
		break;
	case ARG_GET_ALL_AUDIO_DATA:
		params->p.r = for_each_file (&p, save_audio_action);
		break;
	case ARG_GET_ALL_FILES:
		params->p.r = for_each_file (&p, save_file_action);
		break;
	case ARG_GET_ALL_RAW_DATA:
		params->p.r = for_each_file (&p, save_raw_action);
		break;
	case ARG_GET_ALL_THUMBNAILS:
		params->p.r = for_each_file (&p, save_thumbnail_action);
		break;
	case ARG_GET_AUDIO_DATA:
		params->p.r = get_file_common (arg, GP_FILE_TYPE_AUDIO);
		break;
	case ARG_GET_FILE:
		params->p.r = get_file_common (arg, GP_FILE_TYPE_NORMAL);
		break;
	case ARG_GET_THUMBNAIL:
		params->p.r = get_file_common (arg, GP_FILE_TYPE_PREVIEW);
		break;
	case ARG_GET_RAW_DATA:
		params->p.r = get_file_common (arg, GP_FILE_TYPE_RAW);
		break;
	case ARG_LIST_CAMERAS:
		params->p.r = list_cameras_action (&p);
		break;
	case ARG_LIST_FILES:
		params->p.r = for_each_folder (&p, list_files_action);
		break;
	case ARG_LIST_FOLDERS:
		params->p.r = for_each_folder (&p, list_folders_action);
		break;
	case ARG_LIST_PORTS:
		params->p.r = list_ports_action (&p);
		break;
	case ARG_MANUAL:
		params->p.r = action_camera_manual (&p);
		break;
	case ARG_MKDIR:
		params->p.r = gp_camera_folder_remove_dir (p.camera,
				p.folder, arg, p.context);
		break;
	case ARG_NUM_FILES:
		p.camera = p.camera;
		p.folder = p.folder;
		p.context = p.context;
		params->p.r = num_files_action (&p);
		break;
	case ARG_RMDIR:
		params->p.r = gp_camera_folder_make_dir (p.camera,
				p.folder, arg, p.context);
		break;
	case ARG_SHELL:
		params->p.r = shell_prompt (&p);
		break;
	case ARG_SHOW_EXIF:

		/* Did the user specify a file or a range? */
		if (strchr (arg, '.')) { 
			params->p.r = print_exif_action (&p, arg); 
			break; 
		} 
		params->p.r = for_each_file_in_range (&p, 
						 print_exif_action, arg); 
		break;
	case ARG_SHOW_INFO:

		/* Did the user specify a file or a range? */
		if (strchr (arg, '.')) {
			params->p.r = print_info_action (&p, arg);
			break;
		}
		params->p.r = for_each_file_in_range (&p,
						 print_info_action, arg);
		break;
	case ARG_SUMMARY:
		params->p.r = action_camera_summary (&p);
		break;
	case ARG_UPLOAD_FILE:
		params->p.r = action_camera_upload_file (&p, p.folder, arg);
	default:
		break;
	};
};

#endif

static void
report_failure (int result, int argc, char **argv)
{
	if (result >= 0)
		return;

	if (result == GP_ERROR_CANCEL) {
		fprintf (stderr, _("Operation cancelled.\n"));
		return;
	}

	fprintf (stderr, _("*** Error (%i: '%s') ***       \n\n"), result,
		gp_result_as_string (result));
	if (glob_debug == -1) {
		int n;
		printf (_("For debugging messages, please use "
			  "the --debug option.\n"
			  "Debugging messages may help finding a "
			  "solution to your problem.\n"
			  "If you intend to send any error or "
			  "debug messages to the gphoto\n"
			  "developer mailing list "
			  "<gphoto-devel@gphoto.org>, please run\n"
			  "gphoto2 as follows:\n\n"));

		/*
		 * Print the exact command line to assist
		 * l^Husers
		 */
		printf ("    env LANG=C gphoto2 --debug");
		for (n = 1; n < argc; n++) {
			if (argv[n][0] == '-')
				printf(" %s",argv[n]);
			else
				printf(" \"%s\"",argv[n]);
		}
		printf ("\n\n");
		printf ("Please make sure there is sufficient quoting "
			"around the arguments.\n\n");
	}
}

#define CR_MAIN(result)					\
{							\
	int r = (result);				\
							\
	if (r < 0) {					\
		report_failure (r, argc, argv);		\
		gp_params_exit (&p);			\
		return (EXIT_FAILURE);			\
	}						\
}

int
main (int argc, char **argv)
{
#ifdef HAVE_POPT
	CallbackParams params;
	poptContext ctx;
	const struct poptOption options[] = {
		POPT_AUTOHELP
		{NULL, '\0', POPT_ARG_CALLBACK,
		 (void *) &cb_arg, 0, (char *) &params, NULL},
		{"debug", '\0', POPT_ARG_NONE, NULL, ARG_DEBUG,
		 N_("Turn on debugging"), NULL},
		{"quiet", '\0', POPT_ARG_NONE, NULL, ARG_QUIET,
		 N_("Quiet output (default=verbose)"), NULL},
		{"force-overwrite", '\0', POPT_ARG_NONE, NULL,
		 ARG_FORCE_OVERWRITE, N_("Overwrite files without asking.")},
		{"version", 'v', POPT_ARG_NONE, NULL, ARG_VERSION,
		 N_("Display version and exit"), NULL},
		{"list-cameras", '\0', POPT_ARG_NONE, NULL, ARG_LIST_CAMERAS,
		 N_("List supported camera models"), NULL},
		{"list-ports", '\0', POPT_ARG_NONE, NULL, ARG_LIST_PORTS,
		 N_("List supported port devices"), NULL},
		{"stdout", '\0', POPT_ARG_NONE, NULL, ARG_STDOUT,
		 N_("Send file to stdout"), NULL},
		{"stdout-size", '\0', POPT_ARG_NONE, NULL, ARG_STDOUT_SIZE,
		 N_("Print filesize before data"), NULL},
		{"auto-detect", '\0', POPT_ARG_NONE, NULL, ARG_AUTO_DETECT,
		 N_("List auto-detected cameras"), NULL},
		{"port", '\0', POPT_ARG_STRING, NULL, ARG_PORT,
		 N_("Specify port device"), N_("path")},
		{"speed", '\0', POPT_ARG_INT, NULL, ARG_SPEED,
		 N_("Specify serial transfer speed"), N_("speed")},
		{"camera", '\0', POPT_ARG_STRING, NULL, ARG_MODEL,
		 N_("Specify camera model"), N_("model")},
		{"filename", '\0', POPT_ARG_STRING, NULL, ARG_FILENAME,
		 N_("Specify a filename"), N_("filename")},
		{"usbid", '\0', POPT_ARG_STRING, NULL, ARG_USBID,
		 N_("(expert only) Override USB IDs"), N_("usbid")},
		{"abilities", 'a', POPT_ARG_NONE, NULL, ARG_ABILITIES,
		 N_("Display camera abilities"), NULL},
		{"folder", 'f', POPT_ARG_STRING, NULL, ARG_FOLDER,
		 N_("Specify camera folder (default=\"/\")"), N_("folder")},
		{"recurse", 'R', POPT_ARG_NONE, NULL, ARG_RECURSE,
		 N_("Recursion (default for download)"), NULL},
		{"no-recurse", '\0', POPT_ARG_NONE, NULL, ARG_NO_RECURSE,
		 N_("No recursion (default for deletion)"), NULL},
		{"list-folders", 'l', POPT_ARG_NONE, NULL, ARG_LIST_FOLDERS,
		 N_("List folders in folder"), NULL},
		{"list-files", 'L', POPT_ARG_NONE, NULL, ARG_LIST_FILES,
		 N_("List files in folder"), NULL},
		{"mkdir", 'm', POPT_ARG_STRING, NULL, ARG_MKDIR,
		 N_("Create a directory"), NULL},
		{"rmdir", 'r', POPT_ARG_STRING, NULL, ARG_RMDIR,
		 N_("Remove a directory"), NULL},
		{"num-files", 'n', POPT_ARG_NONE, NULL, ARG_NUM_FILES,
		 N_("Display number of files"), NULL},
		{"get-file", 'p', POPT_ARG_STRING, NULL, ARG_GET_FILE,
		 N_("Get files given in range"), NULL},
		{"get-all-files", 'P', POPT_ARG_NONE, NULL, ARG_GET_ALL_FILES,
		 N_("Get all files from folder"), NULL},
		{"get-thumbnail", 't', POPT_ARG_STRING, NULL, ARG_GET_THUMBNAIL,
		 N_("Get thumbnails given in range"), NULL},
		{"get-all-thumbnails", 'T', POPT_ARG_NONE, 0,
		 ARG_GET_ALL_THUMBNAILS,
		 N_("Get all thumbnails from folder"), NULL},
		{"get-raw-data", '\0', POPT_ARG_STRING, NULL,
		 ARG_GET_RAW_DATA,
		 N_("Get raw data given in range"), NULL},
		{"get-all-raw-data", '\0', POPT_ARG_NONE, NULL,
		 ARG_GET_ALL_RAW_DATA,
		 N_("Get all raw data from folder"), NULL},
		{"get-audio-data", '\0', POPT_ARG_STRING, NULL,
		 ARG_GET_AUDIO_DATA,
		 N_("Get audio data given in range"), NULL},
		{"get-all-audio-data", '\0', POPT_ARG_NONE, NULL,
		 ARG_GET_ALL_AUDIO_DATA,
		 N_("Get all audio data from folder"), NULL},
		{"delete-file", 'd', POPT_ARG_STRING, NULL, ARG_DELETE_FILE,
		 N_("Delete files given in range"), NULL},
		{"delete-all-files", 'D', POPT_ARG_NONE, NULL,
		 ARG_DELETE_ALL_FILES, N_("Delete all files in folder"), NULL},
		{"upload-file", 'u', POPT_ARG_STRING, NULL, ARG_UPLOAD_FILE,
		 N_("Upload a file to camera"), NULL},
#ifdef HAVE_CDK
		{"config", '\0', POPT_ARG_NONE, NULL, ARG_CONFIG,
		 N_("Configure"), NULL},
#endif
		{"capture-preview", '\0', POPT_ARG_NONE, NULL,
		 ARG_CAPTURE_PREVIEW,
		 N_("Capture a quick preview"), NULL},
		{"capture-image", '\0', POPT_ARG_NONE, NULL,
		 ARG_CAPTURE_IMAGE, N_("Capture an image"), NULL},
		{"capture-movie", '\0', POPT_ARG_NONE, NULL,
		 ARG_CAPTURE_MOVIE, N_("Capture a movie"), NULL},
		{"capture-sound", '\0', POPT_ARG_NONE, NULL,
		 ARG_CAPTURE_SOUND, N_("Capture an audio clip"), NULL},
#ifdef HAVE_EXIF
		{"show-exif", '\0', POPT_ARG_STRING, NULL, ARG_SHOW_EXIF,
		 N_("Show EXIF information"), NULL},
#endif
		{"show-info", '\0', POPT_ARG_STRING, NULL, ARG_SHOW_INFO,
		 N_("Show info"), NULL},
		{"summary", '\0', POPT_ARG_NONE, NULL, ARG_SUMMARY,
		 N_("Show summary"), NULL},
		{"manual", '\0', POPT_ARG_NONE, NULL, ARG_MANUAL,
		 N_("Show camera driver manual"), NULL},
		{"about", '\0', POPT_ARG_NONE, NULL, ARG_ABOUT,
		 N_("About the camera driver manual"), NULL},
		{"shell", '\0', POPT_ARG_NONE, NULL, ARG_SHELL,
		 N_("gPhoto shell"), NULL},
		POPT_TABLEEND
	};
#endif
	CameraAbilities a;
	GPPortInfo info;
	int result = GP_OK;

	/* For translation */
	setlocale (LC_ALL, "");
        bindtextdomain (PACKAGE, GPHOTO2_LOCALEDIR);
        textdomain (PACKAGE);

	/* Create the global variables. */
	gp_params_init (&p);

	/* Figure out the width of the terminal and watch out for changes */
	signal_resize (0);
	signal (SIGWINCH, signal_resize);

	/* Prepare processing options. */
#ifdef HAVE_POPT
	ctx = poptGetContext (PACKAGE, argc, (const char **) argv, options, 0);
	if (argc <= 1) {
		poptPrintUsage (ctx, stdout, 0);
		return (0);
	}
#endif

	/*
	 * Do we need debugging output? While we are at it, scan the 
	 * options for bad ones.
	 */
#ifdef HAVE_POPT
	params.type = CALLBACK_PARAMS_TYPE_QUERY;
	params.p.q.found = 0;
	params.p.q.arg = ARG_DEBUG;
	poptResetContext (ctx);
	while ((result = poptGetNextOpt (ctx)) >= 0);
	if (result == POPT_ERROR_BADOPT) {
		poptPrintUsage (ctx, stderr, 0);
		return (EXIT_FAILURE);
	}
	if (params.p.q.found) {
		CR_MAIN (debug_action (&p));
	}
#endif

	/* Initialize. */
#ifdef HAVE_PTHREAD
	gp_camera_set_timeout_funcs (p.camera, start_timeout_func,
				     stop_timeout_func, NULL);
#endif
#ifdef HAVE_POPT
	params.type = CALLBACK_PARAMS_TYPE_PREINITIALIZE;
	params.p.r = GP_OK;
	poptResetContext (ctx);
	while ((params.p.r >= 0) && (poptGetNextOpt (ctx) >= 0));
	params.type = CALLBACK_PARAMS_TYPE_INITIALIZE;
	poptResetContext (ctx);
	while ((params.p.r >= 0) && (poptGetNextOpt (ctx) >= 0));
	CR_MAIN (params.p.r);
#endif

#ifdef HAVE_POPT
#define CHECK_OPT(o)					\
	if (!params.p.q.found) {			\
		params.p.q.arg = (o);			\
		poptResetContext (ctx);			\
		while (poptGetNextOpt (ctx) >= 0);	\
	}
#endif

	/* If we need a camera, make sure we've got one. */
	CR_MAIN (gp_camera_get_abilities (p.camera, &a));
	CR_MAIN (gp_camera_get_port_info (p.camera, &info));
#ifdef HAVE_POPT
	params.type = CALLBACK_PARAMS_TYPE_QUERY;
	params.p.q.found = 0;
	CHECK_OPT (ARG_ABILITIES);
	CHECK_OPT (ARG_CAPTURE_IMAGE);
	CHECK_OPT (ARG_CAPTURE_MOVIE);
	CHECK_OPT (ARG_CAPTURE_PREVIEW);
	CHECK_OPT (ARG_CAPTURE_SOUND);
	CHECK_OPT (ARG_CONFIG);
	CHECK_OPT (ARG_DELETE_ALL_FILES);
	CHECK_OPT (ARG_DELETE_FILE);
	CHECK_OPT (ARG_GET_ALL_AUDIO_DATA);
	CHECK_OPT (ARG_GET_ALL_FILES);
	CHECK_OPT (ARG_GET_ALL_RAW_DATA);
	CHECK_OPT (ARG_GET_ALL_THUMBNAILS);
	CHECK_OPT (ARG_GET_AUDIO_DATA);
	CHECK_OPT (ARG_GET_FILE);
	CHECK_OPT (ARG_GET_RAW_DATA);
	CHECK_OPT (ARG_GET_THUMBNAIL);
	CHECK_OPT (ARG_LIST_FILES);
	CHECK_OPT (ARG_LIST_FOLDERS);
	CHECK_OPT (ARG_MANUAL);
	CHECK_OPT (ARG_MKDIR);
	CHECK_OPT (ARG_NUM_FILES);
	CHECK_OPT (ARG_RMDIR);
	CHECK_OPT (ARG_SHELL);
	CHECK_OPT (ARG_SHOW_EXIF);
	CHECK_OPT (ARG_SHOW_INFO);
	CHECK_OPT (ARG_SUMMARY);
	CHECK_OPT (ARG_UPLOAD_FILE);
	if (params.p.q.found &&
	    (!strcmp (a.model, "") || (info.type == GP_PORT_NONE))) {
#else
	if (!strcmp (a.model, "") || (info.type == GP_PORT_NONE)) {
#endif
		int count;
		const char *model = NULL, *path = NULL;
		CameraList list;
		GPPortInfoList *il = NULL;
		char buf[1024];

		gp_log (GP_LOG_DEBUG, "main", "The user has not specified "
			"both a model and a port. Try to figure them out.");

		CR_MAIN (gp_port_info_list_new (&il));
		CR_MAIN (gp_port_info_list_load (il));
		CR_MAIN (gp_abilities_list_detect (p.abilities_list, il,
						   &list, p.context));
		CR_MAIN (count = gp_list_count (&list));
                if (count == 1) {

                        /* Exactly one camera detected */
			CR_MAIN (gp_list_get_name (&list, 0, &model));
			CR_MAIN (gp_list_get_value (&list, 0, &path));
			CR_MAIN (action_camera_set_model (&p, model));
			CR_MAIN (action_camera_set_port (&p, path));

                } else if (!count) {

			/*
			 * No camera detected. Have a look at the settings.
			 * Ignore errors here.
			 */
                        if (gp_setting_get ("gphoto2", "model", buf) >= 0)
				action_camera_set_model (&p, buf);
			if (gp_setting_get ("gphoto2", "port", buf) >= 0)
				action_camera_set_port (&p, buf);

		} else {

                        /* More than one camera detected */
                        /*FIXME: Let the user choose from the list!*/
			CR_MAIN (gp_list_get_name (&list, 0, &model));
			CR_MAIN (gp_list_get_value (&list, 0, &path));
			CR_MAIN (action_camera_set_model (&p, model));
			CR_MAIN (action_camera_set_port (&p, path));
                }

		CR_MAIN (gp_port_info_list_free (il));
        }

	/*
	 * Recursion is too dangerous for deletion. Only turn it on if
	 * explicitely specified.
	 */
#ifdef HAVE_POPT
	params.type = CALLBACK_PARAMS_TYPE_QUERY;
	params.p.q.found = 0;
	params.p.q.arg = ARG_DELETE_FILE;
	poptResetContext (ctx);
	while (poptGetNextOpt (ctx) >= 0);
	if (!params.p.q.found) {
		params.p.q.arg = ARG_DELETE_ALL_FILES;
		poptResetContext (ctx);
		while (poptGetNextOpt (ctx) >= 0);
	}
	if (params.p.q.found) {
		params.p.q.found = 0;
		params.p.q.arg = ARG_RECURSE;
		poptResetContext (ctx);
		while (poptGetNextOpt (ctx) >= 0);
		if (!params.p.q.found)
			p.flags &= ~FOR_EACH_FLAGS_RECURSE;
	}
#else
	if ((option_is_present ("delete-all-files", argc, argv) == GP_OK) ||
	    (option_is_present ("D", argc, argv) == GP_OK)) {
		if (option_is_present ("recurse", argc, argv) == GP_OK)
			p.flags |= FOR_EACH_FLAGS_RECURSE;
		else
			p.flags &= ~FOR_EACH_FLAGS_RECURSE;
	}
#endif

        signal (SIGINT, signal_exit);

	/* If we are told to be quiet, do so. */
#ifdef HAVE_POPT
	params.type = CALLBACK_PARAMS_TYPE_QUERY;
	params.p.q.found = 0;
	params.p.q.arg = ARG_QUIET;
	poptResetContext (ctx);
	while (poptGetNextOpt (ctx) >= 0);
	if (params.p.q.found)
		p.quiet = 1;
#else
	if (option_is_present ("q", argc, argv) == GP_OK)
		p.quiet = 1;
#endif

	/* Go! */
#ifdef HAVE_POPT
	params.type = CALLBACK_PARAMS_TYPE_RUN;
	poptResetContext (ctx);
	params.p.r = GP_OK;
	while ((params.p.r >= GP_OK) && (poptGetNextOpt (ctx) >= 0));
	CR_MAIN (params.p.r);
#else
        /* Count the number of command-line options we have */
        while ((strlen(option[glob_option_count].short_id) > 0) ||
               (strlen(option[glob_option_count].long_id)  > 0)) {
                glob_option_count++;
        }

#ifdef OS2 /*check if environment is set otherwise quit*/
        if(CAMLIBS==NULL)
        {
printf(_("gPhoto2 for OS/2 requires you to set the enviroment value CAMLIBS \
to the location of the camera libraries. \
e.g. SET CAMLIBS=C:\\GPHOTO2\\CAM\n"));
                exit(EXIT_FAILURE);
        }
#endif

#ifdef OS2 /*check if environment is set otherwise quit*/
        if(IOLIBS==NULL)
        {
printf(_("gPhoto2 for OS/2 requires you to set the enviroment value IOLIBS \
to the location of the io libraries. \
e.g. SET IOLIBS=C:\\GPHOTO2\\IOLIB\n"));
                exit(EXIT_FAILURE);
        }
#endif

        /* Peek ahead: Make sure we were called correctly */
        if ((argc == 1) || (verify_options (argc, argv) != GP_OK)) {
                if (!p.quiet)
                        usage (&p);
                exit (EXIT_FAILURE);
        }

	/* Port specified? If not, try to use the settings. */
	if (option_is_present ("port", argc, argv) < 0) {
		char port[1024];
		result = gp_setting_get ("gphoto2", "port", port);
		if (result >= 0)
			action_camera_set_port (&p, port);
	}

	/* Model specified? If not, try to use the settings. */
	if (option_is_present ("model", argc, argv) < 0) {
		char model[1024];
		result = gp_setting_get ("gphoto2", "model", model);
		if (result >= 0)
			CR_MAIN (action_camera_set_model (&p, model));
	}

	/* Now actually do something. */
	CR_MAIN (execute_options (argc, argv));

#endif

	gp_params_exit (&p);

        return (EXIT_SUCCESS);
}
