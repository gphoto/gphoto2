/* shell.c:
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
#include "globals.h"
#include "i18n.h"
#include "main.h"
#include "shell.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_RL
#  include <readline/readline.h>
#  include <readline/history.h>
#endif

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifdef __GNUC__
#define __unused__ __attribute__((unused))
#else
#define __unused__
#endif

#define CHECK(result) {int r=(result);if(r<0) return(r);}
#define CL(result,list) {int r=(result);if(r<0) {gp_list_free(list);return(r);}}
#define CHECK_CONT(result)					\
{								\
	int r = (result);					\
								\
	if (r < 0) {						\
		if (r == GP_ERROR_CANCEL) {			\
			glob_cancel = 0;			\
		} else {					\
			printf (_("*** Error (%i: '%s') ***"),	\
				r, gp_result_as_string (r));	\
			putchar ('\n');				\
		}						\
	}							\
}


static GPParams *p = NULL;
static char cwd[1024];

/* Forward declarations */
static int shell_cd            (Camera *, const char *);
static int shell_lcd           (Camera *, const char *);
static int shell_exit          (Camera *, const char *);
static int shell_get           (Camera *, const char *);
static int shell_put           (Camera *, const char *);
static int shell_get_thumbnail (Camera *, const char *);
static int shell_get_raw       (Camera *, const char *);
static int shell_del           (Camera *, const char *);
static int shell_help          (Camera *, const char *);
static int shell_ls            (Camera *, const char *);
static int shell_exit          (Camera *, const char *);
static int shell_show_info     (Camera *, const char *);
#ifdef HAVE_LIBEXIF
static int shell_show_exif     (Camera *, const char *);
#endif
static int shell_list_config   (Camera *, const char *);
static int shell_get_config    (Camera *, const char *);
static int shell_set_config    (Camera *, const char *);
static int shell_set_config_index    (Camera *, const char *);
static int shell_set_config_value    (Camera *, const char *);
static int shell_capture_image (Camera *, const char *);
static int shell_capture_tethered (Camera *, const char *);
static int shell_capture_image_and_download (Camera *, const char *);
static int shell_capture_preview (Camera *, const char *);
static int shell_mkdir         (Camera *, const char *);
static int shell_rmdir         (Camera *, const char *);
static int shell_wait_event    (Camera *, const char *);

#define MAX_FOLDER_LEN 1024
#define MAX_FILE_LEN 1024

static int shell_construct_path (const char *folder_orig, const char *rel_path,
				 char *dest_folder, char *dest_filename);

typedef int (* ShellFunction) (Camera *camera, const char *arg);

typedef struct _ShellFunctionTable ShellFunctionTable;
static const struct _ShellFunctionTable {
	const char *command;
	ShellFunction function;
	const char *description;
	const char *description_arg;
	unsigned char arg_required;
} func[] = {
	{"cd", shell_cd, N_("Change to a directory on the camera"),
	 N_("directory"), 1},
	{"lcd", shell_lcd, N_("Change to a directory on the local drive"),
	 N_("directory"), 1},
	{"exit", shell_exit, N_("Exit the gPhoto shell"), NULL, 0},
	{"get", shell_get, N_("Download a file"), N_("[directory/]filename"), 1},
	{"put", shell_put, N_("Upload a file"), N_("[directory/]filename"), 1},
	{"get-thumbnail", shell_get_thumbnail, N_("Download a thumbnail"),
	 N_("[directory/]filename"), 1},
	{"get-raw", shell_get_raw, N_("Download raw data"),
	 N_("[directory/]filename"), 1},
	{"show-info", shell_show_info, N_("Show info"),
	 N_("[directory/]filename"), 1},
	{"delete", shell_del, N_("Delete"), N_("[directory/]filename"), 1},
	{"mkdir", shell_mkdir, N_("Create Directory"), N_("directory"), 1},
	{"rmdir", shell_rmdir, N_("Remove Directory"), N_("directory"), 1},
#ifdef HAVE_LIBEXIF
	{"show-exif", shell_show_exif, N_("Show EXIF information"),
	 N_("[directory/]filename"), 1},
#endif
	{"help", shell_help, N_("Displays command usage"),
	 N_("[command]"), 0},
	{"ls", shell_ls, N_("List the contents of the current directory"),
	 N_("[directory/]"), 0},
	{"list-config", shell_list_config, N_("List configuration variables"), NULL, 0},
	{"get-config", shell_get_config, N_("Get configuration variable"), N_("name"), 1},
	{"set-config", shell_set_config, N_("Set configuration variable"), N_("name=value"), 1},
	{"set-config-index", shell_set_config_index, N_("Set configuration variable index"), N_("name=valueindex"), 1},
	{"set-config-value", shell_set_config_value, N_("Set configuration variable"), N_("name=value"), 1},
	{"capture-image", shell_capture_image, N_("Capture a single image"), NULL, 0},
	{"capture-image-and-download", shell_capture_image_and_download, N_("Capture a single image and download it"), NULL, 0},
	{"capture-preview", shell_capture_preview, N_("Capture a preview image"), NULL, 0},
	{"wait-event", shell_wait_event, N_("Wait for an event"), N_("count or seconds"), 0},
	{"capture-tethered", shell_capture_tethered, N_("Wait for images to be captured and download it"), N_("count or seconds"), 0},
	{"wait-event-and-download", shell_capture_tethered, N_("Wait for events and images to be captured and download it"), N_("count or seconds"), 0},
	{"q", shell_exit, N_("Exit the gPhoto shell"), NULL, 0},
	{"quit", shell_exit, N_("Exit the gPhoto shell"), NULL, 0},
	{"?", shell_help, N_("Displays command usage"), N_("[command]"), 0},
	{"", NULL, NULL, NULL, 0}
};

/* Local globals */
#define SHELL_PROMPT "gphoto2: {%s} %s> "
static int 	shell_done		= 0;

static unsigned int
shell_arg_count (const char *args)
{
	size_t x=0;
	int in_arg=0;
	unsigned int count=0;

	while (x < strlen (args)) {
		if ((!isspace((int)(args[x]))) && (!in_arg)) {
			in_arg = 1;
			count++;
		}
		if ((isspace((int)(args[x]))) && (in_arg))
			in_arg = 0;
		x++;
	}

	return (count);
}

static char *
shell_read_line (void)
{
	char prompt[70], buf[1024], *line;
#ifndef HAVE_RL
	char *tmp;
#endif

	if (p->flags & FLAGS_QUIET)
		snprintf (prompt, sizeof (prompt), SHELL_PROMPT, "\0", "\0");
	else {
		if (strlen (cwd) > 25) {
			strncpy (buf, "...", sizeof (buf));
			strncat (buf, &cwd[strlen (cwd) - 22],
				 sizeof (buf) - strlen(buf) - 1);
			snprintf (prompt, sizeof (prompt), SHELL_PROMPT, buf,
				  p->folder);
		} else
			snprintf (prompt, sizeof (prompt), SHELL_PROMPT,
				  cwd, p->folder);
	}
#ifdef HAVE_RL
	line = readline (prompt);
	if (line)
		add_history (line);
	else
		return (NULL);
#else
	line = malloc (1024);
	if (!line)
		return (NULL);
	printf (SHELL_PROMPT, prompt, p->folder);
	fflush(stdout);
	tmp = fgets (line, 1023, stdin);
	if (tmp == NULL)
		return (NULL);
	line[strlen (line) - 1] = '\0';
#endif
	return (line);
}

static int
shell_arg (const char *args, unsigned int arg_num, char *arg)
{
	size_t x=0, y=0;
	unsigned int count=0;
	int in_arg=0, copy=0;

	if (arg_num > shell_arg_count(args)-1)
		return (GP_ERROR);

	while (x < strlen(args)) {				/* Edge-triggered */
		if ((!isspace((int)(args[x]))) && (!in_arg)) {
			in_arg = 1;
			if (count == arg_num)
				copy = 1;
			count++;
		}
		if ((isspace((int)(args[x]))) && (in_arg)) {
			copy = 0;
			in_arg = 0;
		}

		if (copy)					/* Copy over the chars */
			arg[y++] = args[x];
		x++;
	}

	arg[y] = 0;						/* null-terminate the arg */

	return (GP_OK);
}

#ifdef HAVE_RL

static char *
shell_command_generator (const char *text, int state)
{
	static int x, len;

	/* If this is a new command to complete, reinitialize */
	if (!state) {
		x = 0;
		len = strlen (text);
	}

	/* Search 'text' */
	for (; func[x].function; x++)
		if (!strncmp (func[x].command, text, len))
			break;
	if (func[x].function)
		return (strdup (func[x++].command));

	return (NULL);
}

static char *
shell_path_generator (const char *text, int state)
{
	static int x;
	const char *slash, *name;
	CameraList *list;
	int file_count, folder_count, r, len;
	char folder[MAX_FOLDER_LEN], basename[MAX_FILE_LEN], *path;

#if 0
	printf ("shell_path_generator ('%s', %i)\n", text, state);
#endif

	r = shell_construct_path (p->folder, text, folder, basename);
	if (r < 0)
		return (NULL);
	len = strlen (basename);

#if 0
	printf ("Searching for '%s' in '%s'...\n", basename, folder);
#endif

	/* If this is a new path to complete, reinitialize */
	if (!state)
		x = 0;

	r = gp_list_new (&list);
	if (r < 0)
		return (NULL);
	/* First search for matching file */
	r = gp_camera_folder_list_files (p->camera, folder, list, p->context);
	if (r < 0) {
		gp_list_free (list);
		return (NULL);
	}
	file_count = gp_list_count (list);
	if (file_count < 0) {
		gp_list_free (list);
		return (NULL);
	}
	if (x < file_count) {
		for (; x < file_count; x++) {
			r = gp_list_get_name (list, x, &name);
			if (r < 0)
				return (NULL);
			if (!strncmp (name, basename, len)) {
				x++;
				slash = strrchr (text, '/');
				if (!slash) {
					path = malloc (strlen (name) + 2);
					if (!path)
						return (NULL);
					strcpy (path, name);
					strcat (path, " ");
				} else {
					path = malloc (slash - text + 1 + strlen (name) + 2);
					if (!path)
						return (NULL);
					memset (path, 0, slash - text + 1 + strlen (name) + 2);
					strncpy (path, text, slash - text);
					strcat (path, "/");
					strcat (path, name);
					strcat (path, " ");
				}
				return (path);
			}
		}
	}

	/* Ok, we listed all matching files. Now, list matching folders. */
	r = gp_camera_folder_list_folders (p->camera, folder, list,
					   p->context);
	if (r < 0) {
		gp_list_free (list);
		return (NULL);
	}
	folder_count = gp_list_count (list);
	if (folder_count < 0) {
		gp_list_free (list);
		return (NULL);
	}
	if (x - file_count < folder_count) {
		for (; x - file_count < folder_count; x++) {
			r = gp_list_get_name (list, x - file_count, &name);
			if (r < 0) {
				gp_list_free (list);
				return (NULL);
			}
			if (!strncmp (name, basename, len)) {
				x++;
				slash = strrchr (text, '/');
				if (!slash) {
					path = malloc (strlen (name) + 2);
					if (!path)
						return (NULL);
					strcpy (path, name);
					strcat (path, "/");
				} else {
					path = malloc (slash - text + 1 + strlen (name) + 2);
					if (!path)
						return (NULL);
					memset (path, 0, slash - text + 1 + strlen (name) + 2);
					strncpy (path, text, slash - text);
					strcat (path, "/");
					strcat (path, name);
					strcat (path, "/");
				}
				gp_list_free (list);
				return (path);
			}
		}
		gp_list_free (list);
		return (NULL);
	}

	gp_list_free (list);
	return (NULL);
}

static char **
shell_completion_function (const char *text, int start, int end)
{
	char **matches = NULL;
	char *current;

	if (!text)
		return (NULL);

	if (!start) {
		/* Complete command */
		matches = rl_completion_matches (text, shell_command_generator);
	} else {
		current = strdup (rl_copy_text (0, end));

		/* Complete local path? */
		if (!strncmp (current, "lcd", strlen ("lcd"))) {
			free (current);
			return (NULL);
		}
		free (current);

		/* Complete remote path */
		matches = rl_completion_matches (text, shell_path_generator);
	}

	return (matches);
}
#endif /* HAVE_RL */

int
shell_prompt (GPParams *params)
{
	int x;
	char cmd[1024], arg[1024], *line;

	/* The stupid readline functions need that global variable. */
	p = params;

	if (!getcwd (cwd, 1023))
		strcpy (cwd, "./");

#ifdef HAVE_RL
	rl_attempted_completion_function = shell_completion_function;
	rl_completion_append_character = '\0';
#endif

	while (!shell_done && !glob_cancel) {
		line = shell_read_line ();
		if (line ==  NULL) {
			/* quit shell on EOF or input error */
			printf("\n");
			fflush(stdout);
			break;
		}
#ifdef HAVE_UNISTD_H
		if (!isatty(fileno(stdin))) {
			/* if non-interactive input, the command has not been
			 * printed yet, so we do that here */
			printf("%s\n", line);
			fflush(stdout);
		}
#endif

		/* If we don't have any command, start from the beginning */
		if (shell_arg_count (line) <= 0) {
			free (line);
			continue;
		}

		shell_arg (line, 0, cmd);
		strcpy (arg, &line[strlen (cmd)]);
		free (line);

		/* Search the command */
		for (x = 0; func[x].function; x++)
			if (!strcmp (cmd, func[x].command))
				break;
		if (!func[x].function) {
			cli_error_print (_("Invalid command."));
			continue;
		}

		/*
		 * If the command requires an argument, complain if this
		 * argument is not given.
		 */
		if (func[x].arg_required && !shell_arg_count (arg)) {
			printf (_("The command '%s' requires "
				  "an argument."), cmd);
			putchar ('\n');
			continue;
		}

		/* Execute the command */
		CHECK_CONT (func[x].function (p->camera, arg));
	}

	return (GP_OK);
}

static int
shell_construct_path (const char *folder_orig, const char *rel_path,
                      char *dest_folder, char *dest_filename)
{
        const char *slash;

        if (!folder_orig || !rel_path || !dest_folder)
                return (GP_ERROR);

        memset (dest_folder, 0, MAX_FOLDER_LEN);
	if (dest_filename)
	        memset (dest_filename, 0, MAX_FILE_LEN);

	/* Skip leading spaces */
	while (rel_path[0] == ' ')
		rel_path++;

        /*
         * Consider folder_orig only if we are really given a relative
         * path.
         */
        if (rel_path[0] != '/')
                strncpy (dest_folder, folder_orig, MAX_FOLDER_LEN);
	else {
		while (rel_path[0] == '/')
			rel_path++;
		strncpy (dest_folder, "/", MAX_FOLDER_LEN);
	}

        while (rel_path) {
		if (!strncmp (rel_path, "./", 2)) {
                        rel_path += MIN (strlen (rel_path), 2);
			continue;
		}
		if (!strncmp (rel_path, "../", 3) || !strcmp (rel_path, "..")) {
                        rel_path += MIN (3, strlen (rel_path));

                        /* Go up one folder */
                        slash = strrchr (dest_folder, '/');
                        if (!slash) {
                                cli_error_print (_("Invalid path."));
                                return (GP_ERROR);
                        }
			dest_folder[slash - dest_folder] = '\0';
			if (!strlen (dest_folder))
				strcpy (dest_folder, "/");
			continue;
                }

                slash = strchr (rel_path, '/');
		if (strcmp (rel_path, "") && (slash || !dest_filename)) {

			/*
			 * We need to go down one folder. Append a
			 * trailing slash
			 */
			if (dest_folder[strlen (dest_folder) - 1] != '/')
				strncat (dest_folder, "/", MAX_FOLDER_LEN - strlen(dest_folder) - 1);
		}
                if (slash) {
                        strncat (dest_folder, rel_path,
                                 MIN (MAX_FOLDER_LEN - strlen(dest_folder) - 1, slash - rel_path));
                        rel_path = slash + 1;
                } else {

                        /* Done */
                        if (dest_filename)
                                strncpy (dest_filename, rel_path,
                                         MAX_FILE_LEN);
                        else
				strncat (dest_folder, rel_path, MAX_FILE_LEN);
                        break;
                }
        }

        return (GP_OK);
}

static int
shell_lcd (Camera __unused__ *camera, const char *arg)
{
	char new_cwd[MAX_FOLDER_LEN];
	int arg_count = shell_arg_count (arg);

	if (!arg_count) {
		if (!getenv ("HOME")) {
			cli_error_print (_("Could not find home directory."));
			return (GP_OK);
		}
		strncpy (new_cwd, getenv ("HOME"), sizeof(new_cwd)-1);
		new_cwd[sizeof(new_cwd)-1] = '\0';
	} else
		shell_construct_path (cwd, arg, new_cwd, NULL);

	if (chdir (new_cwd) < 0) {
		cli_error_print (_("Could not change to "
				   "local directory '%s'."), new_cwd);
	} else {
		printf (_("Local directory now '%s'."), new_cwd);
		putchar ('\n');
		strcpy (cwd, new_cwd);
	}

	return (GP_OK);
}

static int
shell_cd (Camera __unused__ *camera, const char *arg)
{
	char folder[MAX_FOLDER_LEN];
	CameraList *list;
	int arg_count = shell_arg_count (arg);

	if (!arg_count)
		return (GP_OK);

	/* shell_arg(arg, 0, arg_dir); */

	if (strlen (arg) > 1023) {
		cli_error_print ("Folder value is too long");
		return (GP_ERROR);
	}

	/* Get the new folder value */
	shell_construct_path (p->folder, arg, folder, NULL);

	CHECK (gp_list_new (&list));

	CL (gp_camera_folder_list_folders (p->camera, folder, list,
					      p->context), list);
	gp_list_free (list);
	free (p->folder);
	p->folder = malloc (sizeof (char) * (strlen (folder) + 1));
	if (!p->folder)
		return (GP_ERROR_NO_MEMORY);
	strcpy (p->folder, folder);
	printf (_("Remote directory now '%s'."), p->folder);
	putchar ('\n');
	return (GP_OK);
}

static int
shell_ls (Camera __unused__ *camera, const char *arg)
{
	CameraList *list;
	char buf[1024], folder[MAX_FOLDER_LEN];
	int x, y=1;
	int arg_count = shell_arg_count(arg);
	const char *name;

	if (arg_count) {
		shell_construct_path (p->folder, arg, folder, NULL);
	} else {
		strcpy (folder, p->folder);
	}

	CHECK (gp_list_new (&list));
	CL (gp_camera_folder_list_folders (p->camera, folder, list,
					      p->context), list);

	if (p->flags & FLAGS_QUIET)
		printf ("%i\n", gp_list_count (list));

	for (x = 1; x <= gp_list_count (list); x++) {
		CL (gp_list_get_name (list, x - 1, &name), list);
		if (p->flags & FLAGS_QUIET)
			printf ("%s\n", name);
		else {
			sprintf (buf, "%s/", name);
			printf ("%-20s", buf);
			if (y++ % 4 == 0)
				putchar ('\n');
		}
	}

	CL (gp_camera_folder_list_files (p->camera, folder, list,
					    p->context), list);

	if (p->flags & FLAGS_QUIET)
		printf("%i\n", gp_list_count(list));

	for (x = 1; x <= gp_list_count (list); x++) {
		gp_list_get_name (list, x - 1, &name);
		if (p->flags & FLAGS_QUIET)
			printf ("%s\n", name);
		else {
			printf ("%-20s", name);
			if (y++ % 4 == 0)
				putchar ('\n');
		}
	}
	if ((p->flags & FLAGS_QUIET) == 0 && (y % 4 != 1))
		putchar ('\n');

	gp_list_free (list);
	return (GP_OK);
}

static int
shell_file_action (Camera __unused__ *camera, GPContext __unused__ *context,
		   const char *folder,
		   const char *args, FileAction action)
{
	char arg[1024];
	unsigned int x;
	char dest_folder[MAX_FOLDER_LEN], dest_filename[MAX_FILE_LEN];

	for (x = 0; x < shell_arg_count (args); x++) {
		CHECK (shell_arg (args, x, arg));
		CHECK (shell_construct_path (folder, arg,
					     dest_folder, dest_filename));
		CHECK (action (p, dest_filename));
	}

	return (GP_OK);
}

static int
shell_get_thumbnail (Camera __unused__ *camera, const char *arg)
{
	CHECK (shell_file_action (p->camera, p->context, p->folder, arg,
				  save_thumbnail_action));

	return (GP_OK);
}

static int
shell_get (Camera __unused__ *camera, const char *arg)
{
	CHECK (shell_file_action (p->camera, p->context, p->folder, arg,
				  save_file_action));

	return (GP_OK);
}

static int
shell_get_raw (Camera __unused__ *camera, const char *arg)
{
	CHECK (shell_file_action (p->camera, p->context, p->folder, arg,
				  save_raw_action));

	return (GP_OK);
}

static int
shell_del (Camera __unused__ *camera, const char *arg)
{
	CHECK (shell_file_action (p->camera, p->context, p->folder, arg,
				  delete_file_action));

	return (GP_OK);
}

#ifdef HAVE_LIBEXIF
static int
shell_show_exif (Camera __unused__ *camera, const char *arg)
{
	CHECK (shell_file_action (p->camera, p->context, p->folder, arg,
				  print_exif_action));

	return (GP_OK);
}
#endif

static int
shell_show_info (Camera __unused__ *camera, const char *arg)
{
	CHECK (shell_file_action (p->camera, p->context, p->folder, arg,
				  print_info_action));

	return (GP_OK);
}

static int
shell_put (Camera __unused__ *camera, const char *args) {
	char arg[1024];
	unsigned int x;
	char dest_folder[MAX_FOLDER_LEN], dest_filename[MAX_FILE_LEN];

	for (x = 0; x < shell_arg_count (args); x++) {
		CHECK (shell_arg (args, x, arg));
		CHECK (shell_construct_path ("/", arg, dest_folder, dest_filename));
		CHECK (action_camera_upload_file (p, dest_folder, dest_filename));
	}

	return (GP_OK);
}

static int
shell_mkdir (Camera *camera, const char *args) {
	if (*args == ' ')
		args++;
	return gp_camera_folder_make_dir (camera, p->folder, args, p->context);
}

static int
shell_rmdir (Camera *camera, const char *args) {
	char *xarg;
	int xlen, ret;

	if (*args == ' ')
		args++;

	/* remove trailing / */
	xarg = strdup(args);
	xlen = strlen(xarg);
	while (xlen > 1) {
		if (xarg[xlen-1] != '/')
			break;
		xarg[xlen-1] = '\0';
		xlen--;
	}
	ret = gp_camera_folder_remove_dir (camera, p->folder, xarg, p->context);
	free (xarg);
	return ret;
}

static int
shell_list_config (Camera __unused__ *camera, const char __unused__ *args) {
	CHECK (list_config_action (p));
	return (GP_OK);
}

static int
shell_get_config (Camera __unused__ *camera, const char *args) {
	char arg[1024];
	unsigned int x;

	for (x = 0; x < shell_arg_count (args); x++) {
		CHECK (shell_arg (args, x, arg));
		CHECK (get_config_action (p, arg));
	}
	return (GP_OK);
}

static int
shell_set_config (Camera __unused__ *camera, const char *args) {
	char arg[1024];
	char *s,*x;

	strncpy (arg, args, sizeof(arg));
	arg[1023]='\0';
	/* need to skip spaces */
	x = arg; while (*x == ' ') x++;
	if ((s=strchr(x,'='))) {
		*s='\0';
		return set_config_action (p, x, s+1);
	}
	if ((s=strchr(x,' '))) {
		*s='\0';
		return set_config_action (p, x, s+1);
	}
	fprintf (stderr, _("set-config needs a second argument.\n"));
	return (GP_OK);
}

static int
shell_set_config_value (Camera __unused__ *camera, const char *args) {
	char arg[1024];
	char *s,*x;

	strncpy (arg, args, sizeof(arg));
	arg[1023]='\0';
	/* need to skip spaces */
	x = arg; while (*x == ' ') x++;
	if ((s=strchr(x,'='))) {
		*s='\0';
		return set_config_value_action (p, x, s+1);
	}
	if ((s=strchr(x,' '))) {
		*s='\0';
		return set_config_value_action (p, x, s+1);
	}
	fprintf (stderr, _("set-config-value needs a second argument.\n"));
	return (GP_OK);
}

static int
shell_set_config_index (Camera __unused__ *camera, const char *args) {
	char arg[1024];
	char *s,*x;

	strncpy (arg, args, sizeof(arg));
	arg[1023]='\0';
	/* need to skip spaces */
	x = arg; while (*x == ' ') x++;
	if ((s=strchr(x,'='))) {
		*s='\0';
		return set_config_index_action (p, x, s+1);
	}
	if ((s=strchr(x,' '))) {
		*s='\0';
		return set_config_index_action (p, x, s+1);
	}
	fprintf (stderr, _("set-config-index needs a second argument.\n"));
	return (GP_OK);
}


static int
shell_capture_image (Camera __unused__ *camera, const char __unused__ *args) {
	return capture_generic (GP_CAPTURE_IMAGE, NULL, 0);
}

static int
shell_capture_image_and_download (Camera __unused__ *camera, const char __unused__ *args) {
	return capture_generic (GP_CAPTURE_IMAGE, NULL, 1);
}

static int
shell_capture_preview (Camera __unused__ *camera, const char __unused__ *args) {
	return action_camera_capture_preview (p);
}


static int
shell_wait_event (Camera *camera, const char *args) {
	int evts = 1;
	if (args) {
		if (strchr(args,'s'))
			evts=-atoi(args);
		else
			evts=atoi(args);
	}
	return action_camera_wait_event (p, 0, evts);
}

static int
shell_capture_tethered (Camera *camera, const char *args) {
	int evts = 1;
	if (args) {
		if (strchr(args,'s'))
			evts=-atoi(args);
		else
			evts=atoi(args);
	}
	return action_camera_wait_event (p, 1, evts);
}


int
shell_exit (Camera __unused__ *camera, const char __unused__ *arg)
{
	shell_done = 1;
	return (GP_OK);
}

static int
shell_help_command (Camera __unused__ *camera, const char *arg)
{
	char arg_cmd[1024];
	unsigned int x;

	shell_arg (arg, 0, arg_cmd);

	/* Search this command */
	for (x = 0; func[x].function; x++)
		if (!strcmp (arg_cmd, func[x].command))
			break;
	if (!func[x].function) {
		printf (_("Command '%s' not found. Use 'help' to get a "
			"list of available commands."), arg_cmd);
		putchar ('\n');
		return (GP_OK);
	}

	/* Print the command's syntax. */
	printf (_("Help on \"%s\":"), func[x].command);
	printf ("\n\n");
	printf (_("Usage:"));
	printf (" %s %s\n", func[x].command,
		func[x].description_arg ? _(func[x].description_arg) : "");
	printf (_("Description:"));
	printf ("\n\t%s\n\n", _(func[x].description));
	printf (_("* Arguments in brackets [] are optional"));
	putchar ('\n');

	return (GP_OK);
}

static int
shell_help (Camera __unused__ *camera, const char *arg)
{
	unsigned int x;
	int arg_count = shell_arg_count (arg);

	/*
	 * If help on a command is requested, print the syntax of the command.
	 */
	if (arg_count > 0) {
		CHECK (shell_help_command (p->camera, arg));
		return (GP_OK);
	}

	/* No command specified. Print command listing. */
	printf (_("Available commands:"));
	putchar ('\n');
	for (x = 0; func[x].function; x++)
		printf ("\t%-16s%s\n", func[x].command, _(func[x].description));
	putchar ('\n');
	printf (_("To get help on a particular command, type in "
		"'help command-name'."));
	printf ("\n\n");

	return (GP_OK);
}


/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
