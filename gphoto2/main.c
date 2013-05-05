/*
 * Copyright 2002 Lutz Müller <lutz@users.sourceforge.net>
 * Copyright 2004-2010 Marcus Meissner <marcus@jet.franken.de>
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
#include <gphoto2/gphoto2-port-info-list.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-setting.h>
#include "gp-params.h"
#include "i18n.h"
#include "main.h"
#include "range.h"
#include "shell.h"

#ifdef HAVE_CDK
#  include "gphoto2-cmd-config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <locale.h>
#include <fcntl.h>
#include <utime.h>
#include <limits.h>
#include <errno.h>
#include <sys/time.h>

#ifdef HAVE_POPT
#  include <popt.h>
/* POPT_TABLEEND is only defined from popt 1.6.1 */
# ifndef POPT_TABLEEND
#  define POPT_TABLEEND { NULL, '\0', 0, 0, 0, NULL, NULL }
# endif
#else
# error gphoto2 now REQUIRES the popt library!
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

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define CR(result) {int r = (result); if (r < 0) return (r);}

#ifdef __GNUC__
#define __unused__ __attribute__((unused))
#else
#define __unused__
#endif

static int debug_option_given = 0;
char glob_cancel = 0;
static int glob_frames = 0;
static int glob_interval = 0;
static int glob_bulblength = 0;

GPParams gp_params;

/* flag for SIGUSR1 handler */
static volatile int capture_now = 0;

/* flag for SIGUSR2 handler */
static volatile int end_next = 0;

/*! \brief Copy string almost like strncpy, converting to lower case.
 *
 * This function behaves like strncpy, but
 *  - convert chars to lower case
 *  - ensures the dst buffer is terminated with a '\0'
 *    (even if that means cutting the string short)
 *  - limits the string copy at a reasonable size (32K)
 *
 * Relies on tolower() which may be locale specific, but cannot be
 * multibyte encoding safe.
 */

static size_t
strncpy_lower(char *dst, const char *src, size_t count)
{
	unsigned int i;
	if ((dst == NULL) || (src == NULL) 
	    || (((unsigned long)count)>= 0x7fff)) { 
		return -1; 
	}
	for (i=0; (i<count) && (src[i] != '\0'); i++) {
		dst[i] = (char) tolower((int)src[i]);
	}
	if (i<count) {
		dst[i] = '\0';
	} else {
		dst[count-1] = '\0';
	}
	return i;
}


#undef  MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))


/*! \brief Create local filename for CameraFile according to pattern in gp_params
 *
 * \param folder Name of the folder on the camera the CameraFile is stored in
 * \param file CameraFile to find a local name for.
 * \param path The pointer to the generated complete path name of the local filename.
 * \return GPError code
 *
 * \warning This function reads the static variable gp_params.
 */

static int
get_path_for_file (const char *folder, const char *name, CameraFileType type, CameraFile *file, char **path)
{
	unsigned int i, l;
	char *s, b[1024];
	time_t t;
	struct tm *tm;
	int hour12;
	static int filenr = 1;

	if (!file || !path)
		return (GP_ERROR_BAD_PARAMETERS);

	*path = NULL;
	CR (gp_file_get_mtime (file, &t));
	
	if (!t)	/* use the current time as fallback if the camera did not return it. */
		t = time(NULL);

	tm = localtime (&t);
	hour12 = tm->tm_hour % 12;
	if (hour12 == 0) {
		hour12 = 12;
	}

	/*
	 * If the user didn't specify a filename, use the original name 
	 * (and prefix).
	 */
	if (!gp_params.filename || !strcmp (gp_params.filename, ""))
		return gp_file_get_name_by_type (file, name, type, path);

	/* The user did specify a filename. Use it. */
	b[sizeof (b) - 1] = '\0';
	for (i = 0; i < strlen (gp_params.filename); i++) {
		if (gp_params.filename[i] == '%') {
			char padding = '0'; /* default padding character */
			int precision = 0;  /* default: no padding */
			i++;
			/* determine padding character */
			switch (gp_params.filename[i]) {
			  /* case ' ':
			   * spaces are not supported everywhere, so we
			   * restrict ourselves to padding with zeros.
			   */
			case '0':
				padding = gp_params.filename[i];
				precision = 1; /* do padding */
				i++;
				break;
			}
			/* determine padding width */
			if (isdigit((int)gp_params.filename[i])) {
				char *cp;
				long int _prec;
				_prec = strtol(&gp_params.filename[i],
					       &cp, 10);
				if (_prec < 1) 
					precision = 1;
				else if (_prec > 20)
					precision = 20;
				else
					precision = _prec;
				if (*cp != 'n') {
					/* make sure this is %n */
					gp_context_error (gp_params.context,
						  _("Zero padding numbers "
						    "in file names is only "
						    "possible with %%n."));
					return GP_ERROR_BAD_PARAMETERS;
				}
				/* go to first non-digit character */
				i += (cp - &gp_params.filename[i]);
			} else if (precision && ( gp_params.filename[i] != 'n')) {
				gp_context_error (gp_params.context,
					  _("You cannot use %%n "
					    "zero padding "
					    "without a "
					    "precision value!"
					    ));
				return GP_ERROR_BAD_PARAMETERS;
			}
			switch (gp_params.filename[i]) {
			case 'n':
				/*
 				 * Previously this used an folder index number.
				 * Now this uses a linear increasing number.
				 */
				if (precision > 1) {
					char padfmt[16];
					strcpy(padfmt, "%!.*i");
					padfmt[1] = padding;
					snprintf (b, sizeof (b), padfmt,
						  precision, filenr);
				} else {
					snprintf (b, sizeof (b), "%i",
						  filenr);
				}
				filenr++;
				break;

			case 'C':
				/* Get the suffix of the original name */
				s = strrchr (name, '.');
				if (!s) {
					free (*path);
					*path = NULL;
					gp_context_error (gp_params.context,
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
					l = MIN ((unsigned long)(sizeof(b) - 1),
						 (unsigned long)(s - name));
					strncpy (b, name, l);
					b[l] = '\0';
				}
				break;

			case 'a':
			case 'A':
			case 'b':
			case 'B':
			case 'd':
			case 'H':
			case 'k':
			case 'I':
			case 'l':
			case 'j':
			case 'm':
			case 'M':
			case 'S':
			case 'y':
			case 'Y':
				{
					char fmt[3] = { '%', '\0', '\0' };
					fmt[1] = gp_params.filename[i]; /* the letter of this 'case' */
					strftime(b, sizeof (b), fmt, tm);
					break;
				}
			case '%':
				strcpy (b, "%");
				break;
			case ':':
				strncpy_lower(b, name, sizeof(b));
				b[sizeof(b)-1] = '\0';
				break;
			default:
				free (*path);
				*path = NULL;
				gp_context_error (gp_params.context,
					_("Invalid format '%s' (error at "
					  "position %i)."), gp_params.filename,
					i + 1);
				return (GP_ERROR_BAD_PARAMETERS);
			}
		} else {
			b[0] = gp_params.filename[i];
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
save_camera_file_to_file (
	const char *folder, const char *name, CameraFileType type, CameraFile *file, const char *curname
) {
	char *path = NULL, s[1024], c[1024];
	int res;
	time_t mtime;
	struct utimbuf u;

	CR (get_path_for_file (folder, name, type, file, &path));
	strncpy (s, path, sizeof (s) - 1);
	s[sizeof (s) - 1] = '\0';
	free (path);
	path = NULL;

        if ((gp_params.flags & FLAGS_QUIET) == 0) {
                while ((gp_params.flags & FLAGS_FORCE_OVERWRITE) == 0 &&
		       gp_system_is_file (s)) {
			do {
				putchar ('\007');
				printf (_("File %s exists. Overwrite? [y|n] "),
					s);
				fflush (stdout);
				if (NULL == fgets (c, sizeof (c) - 1, stdin))
					return GP_ERROR;
			} while ((c[0]!='y')&&(c[0]!='Y')&&
				 (c[0]!='n')&&(c[0]!='N'));

			if ((c[0]=='y') || (c[0]=='Y'))
				break;

			do { 
				printf (_("Specify new filename? [y|n] "));
				fflush (stdout); 
				if (NULL == fgets (c, sizeof (c) - 1, stdin))
					return GP_ERROR;
			} while ((c[0]!='y')&&(c[0]!='Y')&&
				 (c[0]!='n')&&(c[0]!='N'));

			if (!((c[0]=='y') || (c[0]=='Y'))) {
				if (curname) unlink (curname);
				return (GP_OK);
			}

			printf (_("Enter new filename: "));
			fflush (stdout);
			if (NULL == fgets (s, sizeof (s) - 1, stdin))
				return GP_ERROR;
			s[strlen (s) - 1] = 0;
                }
                printf (_("Saving file as %s\n"), s);
		fflush (stdout);
        }
	path = s;
	while ((path = strchr (path, gp_system_dir_delim))){
		*path = '\0';
		if(!gp_system_is_dir (s))
			gp_system_mkdir (s);
		*path++ = gp_system_dir_delim;
	}
	if (curname) {
		int x;

		unlink(s);
		if (-1 == rename (curname, s))
			perror("rename");
		x = umask(0022); /* get umask */
		umask(x);/* set it back to the old value */
		chmod(s,0666 & ~x);
	}
	res = gp_file_get_mtime (file, &mtime);
        if ((res == GP_OK) && (mtime)) {
                u.actime = mtime;
                u.modtime = mtime;
                utime (s, &u);
        }
	gp_params_run_hook(&gp_params, "download", s);
	return (GP_OK);
}

int
camera_file_exists (Camera *camera, GPContext *context, const char *folder,
		    const char *filename, CameraFileType type)
{
	CameraFileInfo info;
	CR (gp_camera_file_get_info (camera, folder, filename, &info,
				     context));
	switch (type) {
	case GP_FILE_TYPE_METADATA:
		return TRUE;
	case GP_FILE_TYPE_AUDIO:
		return (info.audio.fields != 0);
	case GP_FILE_TYPE_PREVIEW:
		return (info.preview.fields != 0);
	case GP_FILE_TYPE_RAW:
	case GP_FILE_TYPE_NORMAL:
		return (info.file.fields != 0);
	default:
		gp_context_error (context, "Unknown file type in camera_file_exists: %d", type);
		return FALSE;
	}
}

struct privstr {
	int fd;
};

static int x_size(void*priv,uint64_t *size) {
	struct privstr *ps = priv;
	int fd = ps->fd;
	off_t res;

	gp_log (GP_LOG_DEBUG, "x_size","(%p,%u)", priv, (unsigned int)*size);
	res = lseek (fd, 0, SEEK_END);
	if (res == -1) {
		perror ("x_size: lseek SEEK_END");
		return GP_ERROR_IO;
	}
	res = lseek (fd, 0, SEEK_CUR);
	if (res == -1) {
		perror ("x_size: lseek SEEK_CUR");
		return GP_ERROR_IO;
	}
	*size = res;
	res = lseek (fd, 0, SEEK_SET);
	if (res == -1) {
		perror ("x_size: lseek SEEK_SET");
		return GP_ERROR_IO;
	}
	return GP_OK;
}

static int x_read(void*priv,unsigned char *data, uint64_t *size) {
	struct privstr 	*ps = priv;
	int 		fd = ps->fd;
	uint64_t	curread = 0, xsize, res;

	gp_log (GP_LOG_DEBUG, "x_read", "(%p,%p,%u)", priv, data, (unsigned int)*size);
	xsize = *size;
	while (curread < xsize) {
		res = read (fd, data+curread, xsize-curread);
		if (res == -1) return GP_ERROR_IO_READ;
		if (!res) break;
		curread += res;
	}
	*size = curread;
	return GP_OK;
}

static int x_write(void*priv,unsigned char *data, uint64_t *size) {
	struct privstr 	*ps = priv;
	int 		fd = ps->fd;
	uint64_t	curwritten = 0, xsize, res;

	gp_log (GP_LOG_DEBUG, "x_write","(%p,%p,%u)", priv, data, (unsigned int)*size);
	xsize = *size;
	while (curwritten < xsize) {
		res = write (fd, data+curwritten, xsize-curwritten);
		if (res == -1) return GP_ERROR_IO_WRITE;
		if (!res) break;
		curwritten += res;
	}
	*size = curwritten;
	return GP_OK;
}

static CameraFileHandler xhandler = { x_size, x_read, x_write };

int
save_file_to_file (Camera *camera, GPContext *context, Flags flags,
		   const char *folder, const char *filename,
		   CameraFileType type)
{
        int fd, res;
        CameraFile *file;
	char	tmpname[20], *tmpfilename;
	struct privstr *ps = NULL;

	if (flags & FLAGS_NEW) {
		CameraFileInfo info;
		
		CR (gp_camera_file_get_info (camera, folder, filename,
					     &info, context));
		switch (type) {
		case GP_FILE_TYPE_PREVIEW:
			if (info.preview.fields & GP_FILE_INFO_STATUS &&
			    info.preview.status == GP_FILE_STATUS_DOWNLOADED)
				return (GP_OK);
			break;
		case GP_FILE_TYPE_NORMAL:
		case GP_FILE_TYPE_RAW:
		case GP_FILE_TYPE_EXIF:
			if (info.file.fields & GP_FILE_INFO_STATUS &&
			    info.file.status == GP_FILE_STATUS_DOWNLOADED)
				return (GP_OK);
			break;
		case GP_FILE_TYPE_AUDIO:
			if (info.audio.fields & GP_FILE_INFO_STATUS &&
			    info.audio.status == GP_FILE_STATUS_DOWNLOADED)
				return (GP_OK);
			break;
		default:
			return (GP_ERROR_NOT_SUPPORTED);
		}
	}
	strcpy (tmpname, "tmpfileXXXXXX");
	fd = mkstemp(tmpname);
	if (fd == -1) {
	    if (errno == EACCES) {
	        gp_context_error (context, _("Permission denied"));
	        return GP_ERROR;
	    }
	    CR (gp_file_new (&file));
	    tmpfilename = NULL;
	} else {
		if (time(NULL) & 1) { /* to test both methods. */
			gp_log (GP_LOG_DEBUG, "save_file_to_file","using fd method");
			res = gp_file_new_from_fd (&file, fd);
			if (res < GP_OK) {
				close (fd);
				unlink (tmpname);
				return res;
			}
		} else {
			gp_log (GP_LOG_DEBUG, "save_file_to_file","using handler method");
			ps = malloc (sizeof(*ps));
			if (!ps) return GP_ERROR_NO_MEMORY;
			ps->fd = fd;
			/* just pass in the file pointer as private */
			res = gp_file_new_from_handler (&file, &xhandler, ps);
			if (res < GP_OK) {
				close (fd);
				unlink (tmpname);
				return res;
			}
		}
		tmpfilename = tmpname;
	}
        res = gp_camera_file_get (camera, folder, filename, type,
				  file, context);
	if (res < GP_OK) {
		free (ps);
		gp_file_unref (file);
		if (tmpfilename) unlink (tmpfilename);
		return res;
	}

	if (flags & FLAGS_STDOUT) {
                const char *data;
                unsigned long int size;

                CR (gp_file_get_data_and_size (file, &data, &size));

		if (flags & FLAGS_STDOUT_SIZE) /* this will be difficult in fd mode */
                        printf ("%li\n", size);
                if (1!=fwrite (data, size, 1, stdout))
			fprintf(stderr,"fwrite failed writing to stdout.\n");
		if (ps && ps->fd) close (ps->fd);
		free (ps);
		gp_file_unref (file);
		unlink (tmpname);
		return (GP_OK);
	}
	res = save_camera_file_to_file (folder, filename, type, file, tmpfilename);
	if (ps && ps->fd) close (ps->fd);
	free (ps);
	gp_file_unref (file);
	if ((res!=GP_OK) && tmpfilename)
		unlink (tmpfilename);
        return (res);
}

static void
dissolve_filename (
	const char *folder, const char *filename,
	char **newfolder, char **newfilename
) {
	char *nfolder, *s;

	s = strrchr (filename, '/');
	if (!s) {
		*newfolder = strdup (folder);
		*newfilename = strdup (filename);
		return;
	}
	while (filename[0] == '/')
		filename++;
	nfolder = malloc (strlen (folder) + 1 + (s-filename) + 1);
	strcpy (nfolder, folder);
	if (strcmp (nfolder, "/")) strcat (nfolder, "/"); /* if its not the root directory, append / */
	memcpy (nfolder+strlen(nfolder), filename, (s-filename));
	nfolder[strlen (folder) + 1 + (s-filename)-1] = '\0';
	*newfolder   = nfolder;
	*newfilename = strdup (s+1);
#if 0
	fprintf (stderr, "%s - %s dissolved to %s - %s\n", folder, filename, *newfolder, *newfilename);
#endif
}



/*! \brief parse range, download specified files, or their
 *         thumbnails according to thumbnail argument, and save to files.
 */

int
get_file_common (const char *arg, CameraFileType type )
{
        gp_log (GP_LOG_DEBUG, "main", "Getting '%s'...", arg);

	gp_params.download_type = type; /* remember for multi download */
	/*
	 * If the user specified the file directly (and not a number),
	 * get that file.
	 */
        if (strchr (arg, '.')) {
		int ret;
		char *newfolder, *newfilename;

		dissolve_filename (gp_params.folder, arg, &newfolder, &newfilename);
                ret = save_file_to_file (gp_params.camera, gp_params.context, gp_params.flags,
					   newfolder, newfilename, type);
		free (newfolder); free (newfilename);
		return ret;
	}

        switch (type) {
        case GP_FILE_TYPE_PREVIEW:
		CR (for_each_file_in_range (&gp_params, save_thumbnail_action, arg));
		break;
        case GP_FILE_TYPE_NORMAL:
                CR (for_each_file_in_range (&gp_params, save_file_action, arg));
		break;
        case GP_FILE_TYPE_RAW:
                CR (for_each_file_in_range (&gp_params, save_raw_action, arg));
		break;
	case GP_FILE_TYPE_AUDIO:
		CR (for_each_file_in_range (&gp_params, save_audio_action, arg));
		break;
	case GP_FILE_TYPE_EXIF:
		CR (for_each_file_in_range (&gp_params, save_exif_action, arg));
		break;
	case GP_FILE_TYPE_METADATA:
		CR (for_each_file_in_range (&gp_params, save_meta_action, arg));
		break;
        default:
                return (GP_ERROR_NOT_SUPPORTED);
        }

	return (GP_OK);
}

static void
sig_handler_capture_now (int sig_num)
{
	signal (SIGUSR1, sig_handler_capture_now);
	capture_now = 1;
}

static void
sig_handler_end_next (int sig_num)
{
        signal (SIGUSR2, sig_handler_end_next);
        end_next = 1;
}

/* temp test function */
int
trigger_capture (void) {
	int result =  gp_camera_trigger_capture (gp_params.camera, gp_params.context);
	if (result != GP_OK) {
		cli_error_print(_("Could not trigger capture."));
		return (result);
	}
	return GP_OK;
}

static long
timediff_now (struct timeval *target) {
	struct timeval now;

	gettimeofday (&now, NULL);
	return	(target->tv_sec-now.tv_sec)*1000+
		(target->tv_usec-now.tv_usec)/1000;
}

static int
save_captured_file (CameraFilePath *path, int download) {
	char *pathsep;
	static CameraFilePath last;
	int result;

	if (strcmp(path->folder, "/") == 0)
		pathsep = "";
	else
		pathsep = "/";

	if (gp_params.flags & FLAGS_QUIET) {
		if (!(gp_params.flags & (FLAGS_STDOUT|FLAGS_STDOUT_SIZE)))
			printf ("%s%s%s\n", path->folder, pathsep, path->name);
	} else {
		printf (_("New file is in location %s%s%s on the camera\n"),
			path->folder, pathsep, path->name);
	}
	if (download) {
		if (strcmp(path->folder, last.folder)) {
			memcpy(&last, path, sizeof(last));

			result = set_folder_action(&gp_params, path->folder);
			if (result != GP_OK) {
				cli_error_print(_("Could not set folder."));
				return (result);
			}
		}

		result = get_file_common (path->name, GP_FILE_TYPE_NORMAL);
		if (result != GP_OK) {
			cli_error_print (_("Could not get image."));
			if(result == GP_ERROR_FILE_NOT_FOUND) {
				/* Buggy libcanon.so?
				 * Can happen if this was the first capture after a
				 * CF card format, or during a directory roll-over,
				 * ie: CANON100 -> CANON101
				 */
				cli_error_print ( _("Buggy libcanon.so?"));
			}
			return (result);
		}

		if (!(gp_params.flags & FLAGS_KEEP)) {
			if (!(gp_params.flags & FLAGS_QUIET))
				printf (_("Deleting file %s%s%s on the camera\n"),
					path->folder, pathsep, path->name);

			result = delete_file_action (&gp_params, path->folder, path->name);
			if (result != GP_OK) {
				cli_error_print ( _("Could not delete image."));
				return (result);
			}
		} else {
			if (!(gp_params.flags & FLAGS_QUIET))
				printf (_("Keeping file %s%s%s on the camera\n"),
					path->folder, pathsep, path->name);
		}
	}
	return GP_OK;
}

static int
wait_and_handle_event (long waittime, CameraEventType *type, int download) {
	int 		result;
	CameraEventType	evtype;
	void		*data;
	CameraFilePath	*path;

	if (!type) type = &evtype;
	evtype = GP_EVENT_UNKNOWN;
	data = NULL;
	result = gp_camera_wait_for_event(gp_params.camera, waittime, type, &data, gp_params.context);
	if (result == GP_ERROR_NOT_SUPPORTED) {
		*type = GP_EVENT_TIMEOUT;
		usleep(waittime*1000);
		return GP_OK;
	}
	if (result != GP_OK)
		return result;
	path = data;
	switch (*type) {
	case GP_EVENT_TIMEOUT:
		break;
	case GP_EVENT_CAPTURE_COMPLETE:
		break;
	case GP_EVENT_FOLDER_ADDED:
		if (!(gp_params.flags & FLAGS_QUIET))
			printf (_("Event FOLDER_ADDED %s/%s during wait, ignoring.\n"), path->folder, path->name);
		free (data);
		break;
	case GP_EVENT_FILE_ADDED:
		result = save_captured_file (path, download);
		free (data);
		/* result will fall through to final return */
		break;
	case GP_EVENT_UNKNOWN:
#if 0 /* too much spam for the common usage */
		printf (_("Event UNKNOWN %s during wait, ignoring.\n"), (char*)data);
#endif
		free (data);
		break;
	default:
		if (!(gp_params.flags & FLAGS_QUIET))
			printf (_("Unknown event type %d during bulb wait, ignoring.\n"), *type);
		break;
	}
	return result;
}

int
capture_generic (CameraCaptureType type, const char __unused__ *name, int download)
{
	CameraFilePath path;
	int result, frames = 0;
	CameraAbilities	a;
	CameraEventType evtype;
	long waittime;
	struct timeval next_pic_time, expose_end_time;

	result = gp_camera_get_abilities (gp_params.camera, &a);
	if (result != GP_OK) {
		cli_error_print(_("Could not get capabilities?"));
		return (result);
	}
	gettimeofday (&next_pic_time, NULL);
	next_pic_time.tv_sec += glob_interval;
	if(glob_interval) {
		if (!(gp_params.flags & FLAGS_QUIET)) {
			if (glob_interval != -1)
				printf (_("Time-lapse mode enabled (interval: %ds).\n"),
					glob_interval);
			else
				printf (_("Standing by waiting for SIGUSR1 to capture.\n"));
		}
	}

	if(glob_bulblength) {
		if (!(gp_params.flags & FLAGS_QUIET)) {
			printf (_("Bulb mode enabled (exposure time: %ds).\n"),
				glob_bulblength);
		}
	}

	capture_now = 0;
	signal(SIGUSR1, sig_handler_capture_now);
	end_next = 0;
	signal(SIGUSR2, sig_handler_end_next);

	while(++frames) {
		if (!(gp_params.flags & FLAGS_QUIET) && glob_interval) {
			if(!glob_frames)
				printf (_("Capturing frame #%d...\n"), frames);
			else
				printf (_("Capturing frame #%d/%d...\n"), frames, glob_frames);
		}

		fflush(stdout);

		/* Now handle the different capture methods */
		if(glob_bulblength) {
			/* Bulb mode is special ... we enable it, wait disable it */
			result = set_config_action (&gp_params, "bulb", "1");
			if (result != GP_OK) {
				cli_error_print(_("Could not set bulb capture, result %d."), result);
				return (result);
			}
			gettimeofday (&expose_end_time, NULL);
			expose_end_time.tv_sec += glob_bulblength;
			waittime = timediff_now (&expose_end_time);
			while(waittime > 0) {
				result = wait_and_handle_event(waittime, &evtype, download);
				if (result != GP_OK)
					return result;
				waittime = timediff_now (&expose_end_time);
			}
			result = set_config_action (&gp_params, "bulb", "0");
			if (result != GP_OK) {
				cli_error_print(_("Could not end capture (bulb mode)."));
				return (result);
			}
			/* The actual download will happen down below in the interval wait
			 * or the loop exit.
			 */
		} else {
			result = GP_ERROR_NOT_SUPPORTED;
			if (a.operations & GP_OPERATION_TRIGGER_CAPTURE) {
				result = gp_camera_trigger_capture (gp_params.camera, gp_params.context);
				if ((result != GP_OK) && (result != GP_ERROR_NOT_SUPPORTED))
					cli_error_print(_("Could not trigger image capture."));
				/* The downloads will be handled by wait_event */
			}
			if (result == GP_ERROR_NOT_SUPPORTED) {
				result = gp_camera_capture (gp_params.camera, type, &path, gp_params.context);
				if (result != GP_OK) {
					cli_error_print(_("Could not capture image."));
				} else {
					/* If my Canon EOS 10D is set to auto-focus and it is unable to
					 * get focus lock - it will return with *UNKNOWN* as the filename.
					 */
					if (glob_interval && strcmp(path.name, "*UNKNOWN*") == 0) {
						if (!(gp_params.flags & FLAGS_QUIET)) {
							printf (_("Capture failed (auto-focus problem?)...\n"));
							sleep(1);
							continue;
						}
					}
					result = save_captured_file (&path, download);
					if (result != GP_OK)
						break;
				}
			}
			if (result != GP_OK) {
				cli_error_print(_("Could not capture."));
				if (	(result == GP_ERROR_NOT_SUPPORTED)	||
					(result == GP_ERROR_NO_MEMORY)		||
					(result == GP_ERROR_CANCEL)		||
					(result == GP_ERROR_NO_SPACE)		||
					(result == GP_ERROR_OS_FAILURE)
				)
					return (result);
			}
		}

		/* Break if we've reached the requested number of frames
		 * to capture.
		 */
		if(!glob_interval) break;

		if(glob_frames && frames == glob_frames) break;

		/* Break if we've been told to end before the next frame */
		if(end_next) break;

		/*
		 * Even if the interval is set to -1, it is better to take a
		 * picture first to prepare the camera driver for faster
		 * response when a signal is caught.
		 * [alesan]
		 */
		if (glob_interval != -1) {
			waittime = timediff_now (&next_pic_time);
			if (waittime > 0) {
				if (!(gp_params.flags & FLAGS_QUIET) && glob_interval)
					printf (_("Waiting for next capture slot %ld seconds...\n"), waittime/1000);
				/* We're not sure about sleep() semantics when catching a signal */
				do {
					/* granularity in which we can receive signals is 200 */
					if (waittime > 200) waittime = 200;
					result = wait_and_handle_event (waittime, NULL, download);
					if (result != GP_OK)
						break;
					if (capture_now && !(gp_params.flags & FLAGS_QUIET) && glob_interval) {
						printf (_("Awakened by SIGUSR1...\n"));
						break;
					}
					waittime = timediff_now (&next_pic_time);
				} while (waittime > 0);
			} else {
				/* drain the queue first though, even though there is no time. */
				while (1) {
					result = wait_and_handle_event (1, &evtype, download);
					if ((result != GP_OK) || (evtype == GP_EVENT_TIMEOUT))
						break;
				}
				if (!(gp_params.flags & FLAGS_QUIET) && glob_interval)
					printf (_("not sleeping (%ld seconds behind schedule)\n"), -waittime/1000);
			}
			if (capture_now && (gp_params.flags & FLAGS_RESET_CAPTURE_INTERVAL)) {
				gettimeofday (&next_pic_time, NULL);
				next_pic_time.tv_sec += glob_interval;
			}
			else if (!capture_now) {
				/*
				 * In the case of a (huge) time-sync while gphoto is running,
				 * gphoto could percieve an extremely large amount of time and
				 * stay "behind schedule" quite forever. That's why I reduce the
				 * difference of time with the following loop.
				 * [alesan]
				 */
				do {
					next_pic_time.tv_sec += glob_interval;
				} while (timediff_now (&next_pic_time) < 0);
			}
			capture_now = 0;
		} else {
			/* wait indefinitely for SIGUSR1 */
			do {
				result = wait_and_handle_event (200, &evtype, download);
			} while(!capture_now && (result == GP_OK));
			if (result != GP_OK)
				break;
			capture_now = 0;
			if (!(gp_params.flags & FLAGS_QUIET))
				printf (_("Awakened by SIGUSR1...\n"));
		}
	}
	/* The final capture will fall out of the loop into this case,
	 * so make sure we wait a bit for the the camera to finish stuff.
	 */
	gettimeofday (&expose_end_time, NULL);
	waittime = 100;
	if (glob_frames || end_next || !glob_interval || glob_bulblength) waittime = 2000;
	/* Drain the event queue at the end and download left over added images */
	while ((-timediff_now(&expose_end_time)) < waittime) {
		result = wait_and_handle_event(waittime - (-timediff_now(&expose_end_time)), &evtype, download);
		if ((result != GP_OK) || (evtype == GP_EVENT_TIMEOUT))
			break;
		if (evtype == GP_EVENT_CAPTURE_COMPLETE)
			waittime = 100;
	}

	signal(SIGUSR1, SIG_DFL);
	return (GP_OK);
}


/* Set/init global variables                                    */
/* ------------------------------------------------------------ */

#ifdef HAVE_PTHREAD

typedef struct _ThreadData ThreadData;
struct _ThreadData {
	Camera *camera;
	time_t timeout;
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
		    CameraTimeoutFunc func, void __unused__ *data)
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
stop_timeout_func (Camera __unused__ *camera, unsigned int id,
		   void __unused__ *data)
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
signal_resize (int __unused__ signo)
{
	const char *columns;

	columns = getenv ("COLUMNS");
	if (columns && atoi (columns))
		gp_params.cols = atoi (columns);
}

static void
signal_exit (int __unused__ signo)
{
	/* If we already were told to cancel, abort. */
	if (glob_cancel) {
		if ((gp_params.flags & FLAGS_QUIET) == 0)
			printf (_("\nAborting...\n"));
		if (gp_params.camera)
			gp_camera_unref (gp_params.camera);
		if (gp_params.context)
			gp_context_unref (gp_params.context);
		if ((gp_params.flags & FLAGS_QUIET) == 0)
			printf (_("Aborted.\n"));
		exit (EXIT_FAILURE);
	}

	if ((gp_params.flags & FLAGS_QUIET) == 0)
                printf (_("\nCancelling...\n"));

	glob_cancel = 1;
	glob_interval = 0;
}

/* Main :)                                                              */
/* -------------------------------------------------------------------- */

typedef enum {
	ARG_ABILITIES,
	ARG_ABOUT,
	ARG_AUTO_DETECT,
	ARG_CAPTURE_FRAMES,
	ARG_CAPTURE_INTERVAL,
	ARG_CAPTURE_BULB,
	ARG_TRIGGER_CAPTURE,
	ARG_CAPTURE_IMAGE,
	ARG_CAPTURE_IMAGE_AND_DOWNLOAD,
	ARG_CAPTURE_MOVIE,
	ARG_CAPTURE_PREVIEW,
	ARG_CAPTURE_SOUND,
	ARG_CAPTURE_TETHERED,
	ARG_CAPTURE_TETHERED_KEEP,
	ARG_CONFIG,
	ARG_DEBUG,
	ARG_DEBUG_LOGFILE,
	ARG_DELETE_ALL_FILES,
	ARG_DELETE_FILE,
	ARG_FILENAME,
	ARG_FOLDER,
	ARG_FORCE_OVERWRITE,
	ARG_GET_ALL_AUDIO_DATA,
	ARG_GET_ALL_FILES,
	ARG_GET_ALL_METADATA,
	ARG_GET_ALL_RAW_DATA,
	ARG_GET_ALL_THUMBNAILS,
	ARG_GET_AUDIO_DATA,
	ARG_GET_CONFIG,
	ARG_SET_CONFIG,
	ARG_SET_CONFIG_INDEX,
	ARG_SET_CONFIG_VALUE,
	ARG_GET_FILE,
	ARG_GET_METADATA,
	ARG_GET_RAW_DATA,
	ARG_GET_THUMBNAIL,
	ARG_HELP,
	ARG_HOOK_SCRIPT,
	ARG_KEEP,
	ARG_LIST_CAMERAS,
	ARG_LIST_ALL_CONFIG,
	ARG_LIST_CONFIG,
	ARG_LIST_FILES,
	ARG_LIST_FOLDERS,
	ARG_LIST_PORTS,
	ARG_MANUAL,
	ARG_MKDIR,
	ARG_MODEL,
	ARG_NEW,
	ARG_NO_KEEP,
	ARG_NO_RECURSE,
	ARG_NUM_FILES,
	ARG_PORT,
	ARG_QUIET,
	ARG_RECURSE,
	ARG_RESET,
	ARG_RESET_INTERVAL,
	ARG_RMDIR,
	ARG_SHELL,
	ARG_SHOW_EXIF,
	ARG_SHOW_INFO,
	ARG_SPEED,
	ARG_STDOUT,
	ARG_STDOUT_SIZE,
	ARG_STORAGE_INFO,
	ARG_SUMMARY,
	ARG_UPLOAD_FILE,
	ARG_UPLOAD_METADATA,
	ARG_USAGE,
	ARG_USBID,
	ARG_VERSION,
	ARG_WAIT_EVENT
} Arg;

typedef enum {
	CALLBACK_PARAMS_TYPE_NONE,
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

		/* CALLBACK_PARAMS_TYPE_NONE */
	} p;
};


/*! \brief popt callback with type CALLBACK_PARAMS_TYPE_QUERY
 */

static void
cb_arg_query (poptContext __unused__ ctx, 
	      enum poptCallbackReason __unused__ reason,
	      const struct poptOption *opt, const char __unused__ *arg,
	      CallbackParams *params)
{
	/* p.q.arg is an enum, but opt->val is an int */
	if (opt->val == (int)(params->p.q.arg))
		params->p.q.found = 1;
}


/*! \brief popt callback with type CALLBACK_PARAMS_TYPE_PREINITIALIZE
 */

static void
cb_arg_preinit (poptContext __unused__ ctx, 
		enum poptCallbackReason __unused__ reason,
		const struct poptOption *opt, const char *arg,
		CallbackParams *params)
{
	int usb_product, usb_vendor;
	int usb_product_modified, usb_vendor_modified;
	switch (opt->val) {
	case ARG_USBID:
		gp_log (GP_LOG_DEBUG, "main", "Overriding USB "
			"IDs to '%s'...", arg);
		if (sscanf (arg, "0x%x:0x%x=0x%x:0x%x",
			    &usb_vendor_modified,
			    &usb_product_modified, &usb_vendor,
			    &usb_product) != 4) {
			printf (_("Use the following syntax a:b=c:d "
				  "to treat any USB device detected "
				  "as a:b as c:d instead. "
				  "a b c d should be hexadecimal "
				  "numbers beginning with '0x'.\n"));
			params->p.r = GP_ERROR_BAD_PARAMETERS;
			break;
		}
		params->p.r = override_usbids_action (&gp_params, usb_vendor,
						      usb_product, usb_vendor_modified,
						      usb_product_modified);
		break;
	default:
		break;
	}
}


/*! \brief popt callback with type CALLBACK_PARAMS_TYPE_INITIALIZE
 */

static void
cb_arg_init (poptContext __unused__ ctx, 
	     enum poptCallbackReason __unused__ reason,
	     const struct poptOption *opt, const char *arg,
	     CallbackParams *params)
{
	switch (opt->val) {

	case ARG_FILENAME:
		params->p.r = set_filename_action (&gp_params, arg);
		break;
	case ARG_FOLDER:
		params->p.r = set_folder_action (&gp_params, arg);
		break;

	case ARG_FORCE_OVERWRITE:
		gp_params.flags |= FLAGS_FORCE_OVERWRITE;
		break;
	case ARG_NEW:
		gp_params.flags |= FLAGS_NEW;
		break;
	case ARG_KEEP:
		gp_params.flags |= FLAGS_KEEP;
		break;
	case ARG_NO_KEEP:
		gp_params.flags &= ~FLAGS_KEEP;
		break;

	case ARG_NO_RECURSE:
		gp_params.flags &= ~FLAGS_RECURSE;
		break;
	case ARG_RECURSE:
		gp_params.flags |= FLAGS_RECURSE;
		break;

	case ARG_MODEL:
		gp_log (GP_LOG_DEBUG, "main", "Processing 'model' "
			"option ('%s')...", arg);
		params->p.r = action_camera_set_model (&gp_params, arg);
		break;
	case ARG_PORT:
		gp_log (GP_LOG_DEBUG, "main", "Processing 'port' "
			"option ('%s')...", arg);
		params->p.r = action_camera_set_port (&gp_params, arg);
		break;
	case ARG_SPEED:
		params->p.r = action_camera_set_speed (&gp_params, atoi (arg));
		break;

	case ARG_QUIET:
		gp_params.flags |= FLAGS_QUIET;
		break;

	case ARG_RESET_INTERVAL:
		gp_params.flags |= FLAGS_RESET_CAPTURE_INTERVAL;
		break;

	case ARG_HOOK_SCRIPT:
		do {
			const size_t sz = strlen(arg);
			char *copy = malloc(sz+1);
			if (!copy) {
				perror("malloc error");
				exit (EXIT_FAILURE);
			}
			gp_params.hook_script = strcpy(copy, arg);
			/* Run init hook */
			if (0!=gp_params_run_hook(&gp_params, "init", NULL)) {
				fprintf(stderr,
					"Hook script \"%s\" init failed. Aborting.\n",
					gp_params.hook_script);
				exit(3);
			}
		} while (0);
		break;

	case ARG_STDOUT:
		gp_params.flags |= FLAGS_QUIET | FLAGS_STDOUT;
		break;
	case ARG_STDOUT_SIZE:
		gp_params.flags |= FLAGS_QUIET | FLAGS_STDOUT 
			| FLAGS_STDOUT_SIZE;
		break;

	case ARG_CAPTURE_FRAMES:
		glob_frames = atoi(arg);
		break;
	case ARG_CAPTURE_INTERVAL:
		glob_interval = atoi(arg);
		break;
	case ARG_CAPTURE_BULB:
		glob_bulblength = atoi(arg);
		break;

	case ARG_VERSION:
		params->p.r = print_version_action (&gp_params);
		break;

	default:
		break;
	}
}

/*! \brief popt callback with type CALLBACK_PARAMS_TYPE_RUN
 */

static void
cb_arg_run (poptContext __unused__ ctx, 
	    enum poptCallbackReason __unused__ reason,
	    const struct poptOption *opt, const char *arg,
	    CallbackParams *params)
{
	char *newfilename = NULL, *newfolder = NULL;

	switch (opt->val) {
	case ARG_ABILITIES:
		params->p.r = action_camera_show_abilities (&gp_params);
		break;
	case ARG_ABOUT:
		params->p.r = action_camera_about (&gp_params);
		break;
	case ARG_AUTO_DETECT:
		params->p.r = auto_detect_action (&gp_params);
		break;
	case ARG_TRIGGER_CAPTURE:
		params->p.r = trigger_capture ();
		break;
	case ARG_CAPTURE_IMAGE:
		params->p.r = capture_generic (GP_CAPTURE_IMAGE, arg, 0);
		break;
	case ARG_CAPTURE_IMAGE_AND_DOWNLOAD:
		params->p.r = capture_generic (GP_CAPTURE_IMAGE, arg, 1);
		break;
	case ARG_CAPTURE_MOVIE:
		params->p.r = action_camera_capture_movie (&gp_params, arg);
		break;
	case ARG_CAPTURE_PREVIEW:
		params->p.r = action_camera_capture_preview (&gp_params);
		break;
	case ARG_CAPTURE_SOUND:
		params->p.r = capture_generic (GP_CAPTURE_SOUND, arg, 0);
		break;
	case ARG_CONFIG:
#ifdef HAVE_CDK
		params->p.r = gp_cmd_config (gp_params.camera, gp_params.context);
#else
		gp_context_error (gp_params.context,
				  _("gphoto2 has been compiled without "
				    "support for CDK."));
		params->p.r = GP_ERROR_NOT_SUPPORTED;
#endif
		break;
	case ARG_DELETE_ALL_FILES:
		params->p.r = for_each_folder (&gp_params, delete_all_action);
		break;
	case ARG_DELETE_FILE:
		gp_params.multi_type = MULTI_DELETE;
		/* Did the user specify a file or a range? */
		if (strchr (arg, '.')) {
			dissolve_filename (gp_params.folder, arg, &newfolder, &newfilename);
			params->p.r = delete_file_action (&gp_params, newfolder, newfilename);
			free (newfolder); free (newfilename);
			break;
		}
		params->p.r = for_each_file_in_range (&gp_params,
						      delete_file_action, arg);
		break;
	case ARG_GET_ALL_AUDIO_DATA:
		params->p.r = for_each_file (&gp_params, save_all_audio_action);
		break;
	case ARG_GET_ALL_FILES:
		params->p.r = for_each_file (&gp_params, save_file_action);
		break;
	case ARG_GET_ALL_METADATA:
		params->p.r = for_each_file (&gp_params, save_meta_action);
		break;
	case ARG_GET_ALL_RAW_DATA:
		params->p.r = for_each_file (&gp_params, save_raw_action);
		break;
	case ARG_GET_ALL_THUMBNAILS:
		params->p.r = for_each_file (&gp_params, save_thumbnail_action);
		break;
	case ARG_GET_AUDIO_DATA:
		gp_params.multi_type = MULTI_DOWNLOAD;
		params->p.r = get_file_common (arg, GP_FILE_TYPE_AUDIO);
		break;
	case ARG_GET_METADATA:
		gp_params.multi_type = MULTI_DOWNLOAD;
		params->p.r = get_file_common (arg, GP_FILE_TYPE_METADATA);
		break;
	case ARG_GET_FILE:
		gp_params.multi_type = MULTI_DOWNLOAD;
		params->p.r = get_file_common (arg, GP_FILE_TYPE_NORMAL);
		break;
	case ARG_GET_THUMBNAIL:
		gp_params.multi_type = MULTI_DOWNLOAD;
		params->p.r = get_file_common (arg, GP_FILE_TYPE_PREVIEW);
		break;
	case ARG_GET_RAW_DATA:
		gp_params.multi_type = MULTI_DOWNLOAD;
		params->p.r = get_file_common (arg, GP_FILE_TYPE_RAW);
		break;
	case ARG_LIST_CAMERAS:
		params->p.r = list_cameras_action (&gp_params);
		break;
	case ARG_LIST_FILES:
		params->p.r = for_each_folder (&gp_params, list_files_action);
		break;
	case ARG_LIST_FOLDERS:
		params->p.r = for_each_folder (&gp_params, list_folders_action);
		break;
	case ARG_LIST_PORTS:
		params->p.r = list_ports_action (&gp_params);
		break;
	case ARG_RESET: {
		GPPort		*port;
		GPPortInfo	info;

		params->p.r = gp_port_new (&port);
		if (params->p.r != GP_OK) {
			gp_log(GP_LOG_ERROR,"port_reset", "new failed %d", params->p.r);
			break;
		}
		params->p.r = gp_camera_get_port_info (gp_params.camera, &info);
		if (params->p.r != GP_OK) {
			gp_log(GP_LOG_ERROR,"port_reset", "camera_get_port_info failed");
			break;
		}
		params->p.r = gp_port_set_info (port, info);
		if (params->p.r != GP_OK) {
			gp_log(GP_LOG_ERROR,"port_reset", "port_set_info failed");
			break;
		}
		params->p.r = gp_port_open (port);
		if (params->p.r != GP_OK) {
			gp_log(GP_LOG_ERROR,"port_reset", "open failed %d", params->p.r);
			break;
		}
		params->p.r = gp_port_reset (port);
		gp_port_close (port);
		gp_port_free (port);
		break;
	}
	case ARG_MANUAL:
		params->p.r = action_camera_manual (&gp_params);
		break;
	case ARG_RMDIR:
		dissolve_filename (gp_params.folder, arg, &newfolder, &newfilename);
		params->p.r = gp_camera_folder_remove_dir (gp_params.camera,
							   newfolder, newfilename, gp_params.context);
		free (newfolder); free (newfilename);
		break;
	case ARG_NUM_FILES:
		params->p.r = num_files_action (&gp_params);
		break;
	case ARG_MKDIR:
		dissolve_filename (gp_params.folder, arg, &newfolder, &newfilename);
		params->p.r = gp_camera_folder_make_dir (gp_params.camera,
							 newfolder, newfilename, gp_params.context);
		free (newfolder); free (newfilename);
		break;
	case ARG_SHELL:
		params->p.r = shell_prompt (&gp_params);
		break;
	case ARG_SHOW_EXIF:
		/* Did the user specify a file or a range? */
		if (strchr (arg, '.')) {
			dissolve_filename (gp_params.folder, arg, &newfolder, &newfilename);
			params->p.r = print_exif_action (&gp_params, newfolder, newfilename); 
			free (newfolder); free (newfilename);
			break; 
		} 
		params->p.r = for_each_file_in_range (&gp_params, 
						      print_exif_action, arg); 
		break;
	case ARG_SHOW_INFO:
		/* Did the user specify a file or a range? */
		if (strchr (arg, '.')) {
			dissolve_filename (gp_params.folder, arg, &newfolder, &newfilename);
			params->p.r = print_info_action (&gp_params, newfolder, newfilename);
			free (newfolder); free (newfilename);
			break;
		}
		params->p.r = for_each_file_in_range (&gp_params,
						      print_info_action, arg);
		break;
	case ARG_SUMMARY:
		params->p.r = action_camera_summary (&gp_params);
		break;
	case ARG_UPLOAD_FILE:
		gp_params.multi_type = MULTI_UPLOAD;
		/* Note: do not normalize folder/filename, as -u allows local filenames with paths */
		params->p.r = action_camera_upload_file (&gp_params, gp_params.folder, arg);
		break;
	case ARG_UPLOAD_METADATA:
		gp_params.multi_type = MULTI_UPLOAD_META;
		/* Note: do not normalize folder/filename, as -u-meta allows local filenames with paths */
		params->p.r = action_camera_upload_metadata (&gp_params, gp_params.folder, arg);
		break;
	case ARG_LIST_ALL_CONFIG:
		params->p.r = list_all_config_action (&gp_params);
		break;
	case ARG_LIST_CONFIG:
		params->p.r = list_config_action (&gp_params);
		break;
	case ARG_GET_CONFIG:
		params->p.r = get_config_action (&gp_params, arg);
		break;
	case ARG_SET_CONFIG: {
		char *name, *value;

		if (strchr (arg, '=') == NULL) {
			params->p.r = GP_ERROR_BAD_PARAMETERS;
			break;
		}
		name  = strdup (arg);
		value = strchr (name, '=');
		*value = '\0';
		value++;
		params->p.r = set_config_action (&gp_params, name, value);
		free (name);
		break;
	}
	case ARG_SET_CONFIG_INDEX: {
		char *name, *value;

		if (strchr (arg, '=') == NULL) {
			params->p.r = GP_ERROR_BAD_PARAMETERS;
			break;
		}
		name  = strdup (arg);
		value = strchr (name, '=');
		*value = '\0';
		value++;
		params->p.r = set_config_index_action (&gp_params, name, value);
		free (name);
		break;
	}
	case ARG_SET_CONFIG_VALUE: {
		char *name, *value;

		if (strchr (arg, '=') == NULL) {
			params->p.r = GP_ERROR_BAD_PARAMETERS;
			break;
		}
		name  = strdup (arg);
		value = strchr (name, '=');
		*value = '\0';
		value++;
		params->p.r = set_config_value_action (&gp_params, name, value);
		free (name);
		break;
	}
	case ARG_WAIT_EVENT:
		params->p.r = action_camera_wait_event (&gp_params, DT_NO_DOWNLOAD, arg);
		break;
	case ARG_CAPTURE_TETHERED:
		params->p.r = action_camera_wait_event (&gp_params, DT_DOWNLOAD, arg);
		break;
	case ARG_STORAGE_INFO:
		params->p.r = print_storage_info (&gp_params);
		break;
	default:
		break;
	};
}


/*! \brief Callback function called while parsing command line options.
 *
 * This callback function is called multiple times in multiple
 * phases. That should probably become separate functions.
 */

static void
cb_arg (poptContext __unused__ ctx, 
	enum poptCallbackReason __unused__ reason,
	const struct poptOption *opt, const char *arg,
	void *data)
{
	CallbackParams *params = (CallbackParams *) data;

	/* Check if we are only to query. */
	switch (params->type) {
	case CALLBACK_PARAMS_TYPE_NONE:
		/* do nothing */
		break;
	case CALLBACK_PARAMS_TYPE_QUERY:
		cb_arg_query (ctx, reason, opt, arg, params);
		break;
	case CALLBACK_PARAMS_TYPE_PREINITIALIZE:
		cb_arg_preinit (ctx, reason, opt, arg, params);
		break;
	case CALLBACK_PARAMS_TYPE_INITIALIZE:
		/* Check if we are only to initialize. */
		cb_arg_init (ctx, reason, opt, arg, params);
		break;
	case CALLBACK_PARAMS_TYPE_RUN:
		cb_arg_run (ctx, reason, opt, arg, params);
		break;
	}
};


static void
report_failure (int result, int argc, char **argv)
{
	if (result >= 0)
		return;

	if (result == GP_ERROR_CANCEL) {
		fprintf (stderr, _("Operation cancelled.\n"));
		return;
	}
	if (result == -2000) {
		fprintf (stderr, _("*** Error: No camera found. ***\n\n"));
	} else {
		fprintf (stderr, _("*** Error (%i: '%s') ***       \n\n"), result,
			gp_result_as_string (result));
	}
	if (!debug_option_given) {
		int n;
		printf (_("For debugging messages, please use "
			  "the --debug option.\n"
			  "Debugging messages may help finding a "
			  "solution to your problem.\n"
			  "If you intend to send any error or "
			  "debug messages to the gphoto\n"
			  "developer mailing list "
			  "<gphoto-devel@lists.sourceforge.net>, please run\n"
			  "gphoto2 as follows:\n\n"));

		/*
		 * Print the exact command line to assist bugreporters
		 */
		printf ("    env LANG=C gphoto2 --debug --debug-logfile=my-logfile.txt");
		for (n = 1; n < argc; n++) {
			if (strchr(argv[n], ' ') == NULL)
				printf(" %s",argv[n]);
			else
				printf(" \"%s\"",argv[n]);
		}
		printf ("\n\n");
		printf (_("Please make sure there is sufficient quoting "
			"around the arguments.\n\n"));
	}
}

#define CR_MAIN(result)							\
	do {								\
		int r = (result);					\
									\
		if (r < 0) {						\
			report_failure (r, argc, argv);			\
									\
			/* Run stop hook */				\
			gp_params_run_hook(&gp_params, "stop", NULL);	\
									\
			gp_params_exit (&gp_params);			\
                	poptFreeContext(ctx);				\
			return (EXIT_FAILURE);				\
		}							\
	} while (0)


#define GPHOTO2_POPT_CALLBACK \
	{NULL, '\0', POPT_ARG_CALLBACK, \
			(void *) &cb_arg, 0, (char *) &cb_params, NULL},

/*! main function: parse command line arguments and call actions
 *
 * Perhaps we should use the following code for parsing command line
 * options:

       poptGetContext(NULL, argc, argv, poptOptions, 0);
       while ((rc = poptGetNextOpt(poptcon)) > 0) {
            switch (rc) {
            ARG_FOO:
	         printf("foo = %s\n", poptGetOptArg(poptcon));
		 break;
            }
       }
       poptFreeContext(poptcon);
 *
 * Regardless of whether we do this or not, we should get rid of those
 * legions of poptResetContext() calls followed by lots of
 * poptGetNextOpt() calls.
 *
 * At least we should get rid of all those stages. Probably two stages
 * are sufficient:
 *  -# look for --help, --debug, --debug-logfile, --quiet
 *  -# repeat this until command line has been used up
 *     -# go through all command line options
 *     -# ignore those from above
 *     -# if setting for command, store its value
 *     -# if command, execute command
 */


int
main (int argc, char **argv, char **envp)
{
	CallbackParams cb_params;
	poptContext ctx;
	int help_option_given = 0;
	int usage_option_given = 0;
	char *debug_logfile_name = NULL;
	const struct poptOption generalOptions[] = {
		GPHOTO2_POPT_CALLBACK
		{"help", '?', POPT_ARG_NONE, (void *) &help_option_given, ARG_HELP,
		 N_("Print complete help message on program usage"), NULL},
		{"usage", '\0', POPT_ARG_NONE, (void *) &usage_option_given, ARG_USAGE,
		 N_("Print short message on program usage"), NULL},
		{"debug", '\0', POPT_ARG_NONE, (void *) &debug_option_given, ARG_DEBUG,
		 N_("Turn on debugging"), NULL},
		{"debug-logfile", '\0', POPT_ARG_STRING, (void *) &debug_logfile_name, ARG_DEBUG_LOGFILE,
		 N_("Name of file to write debug info to"), N_("FILENAME")},
		{"quiet", '\0', POPT_ARG_NONE, NULL, ARG_QUIET,
		 N_("Quiet output (default=verbose)"), NULL},
		{"hook-script", '\0', POPT_ARG_STRING, NULL, ARG_HOOK_SCRIPT,
		 N_("Hook script to call after downloads, captures, etc."),
		 N_("FILENAME")},
		POPT_TABLEEND
	};
	const struct poptOption cameraOptions[] = {
		GPHOTO2_POPT_CALLBACK
		{"port", '\0', POPT_ARG_STRING, NULL, ARG_PORT,
		 N_("Specify device port"), N_("FILENAME")},
		{"speed", '\0', POPT_ARG_INT, NULL, ARG_SPEED,
		 N_("Specify serial transfer speed"), N_("SPEED")},
		{"camera", '\0', POPT_ARG_STRING, NULL, ARG_MODEL,
		 N_("Specify camera model"), N_("MODEL")},
		{"usbid", '\0', POPT_ARG_STRING, NULL, ARG_USBID,
		 N_("(expert only) Override USB IDs"), N_("USBIDs")},
		POPT_TABLEEND
	};
	const struct poptOption infoOptions[] = {
		GPHOTO2_POPT_CALLBACK
		{"version", 'v', POPT_ARG_NONE, NULL, ARG_VERSION,
		 N_("Display version and exit"), NULL},
		{"list-cameras", '\0', POPT_ARG_NONE, NULL, ARG_LIST_CAMERAS,
		 N_("List supported camera models"), NULL},
		{"list-ports", '\0', POPT_ARG_NONE, NULL, ARG_LIST_PORTS,
		 N_("List supported port devices"), NULL},
		{"abilities", 'a', POPT_ARG_NONE, NULL, ARG_ABILITIES,
		 N_("Display camera/driver abilities"), NULL},
		POPT_TABLEEND
	};
	const struct poptOption configOptions[] = {
		GPHOTO2_POPT_CALLBACK
#ifdef HAVE_CDK
		{"config", '\0', POPT_ARG_NONE, NULL, ARG_CONFIG,
		 N_("Configure"), NULL},
#endif
		{"list-config", '\0', POPT_ARG_NONE, NULL, ARG_LIST_CONFIG,
		 N_("List configuration tree"), NULL},
		{"list-all-config", '\0', POPT_ARG_NONE, NULL, ARG_LIST_ALL_CONFIG,
		 N_("Dump full configuration tree"), NULL},
		{"get-config", '\0', POPT_ARG_STRING, NULL, ARG_GET_CONFIG,
		 N_("Get configuration value"), NULL},
		{"set-config", '\0', POPT_ARG_STRING, NULL, ARG_SET_CONFIG,
		 N_("Set configuration value or index in choices"), NULL},
		{"set-config-index", '\0', POPT_ARG_STRING, NULL, ARG_SET_CONFIG_INDEX,
		 N_("Set configuration value index in choices"), NULL},
		{"set-config-value", '\0', POPT_ARG_STRING, NULL, ARG_SET_CONFIG_VALUE,
		 N_("Set configuration value"), NULL},
		{"reset", '\0', POPT_ARG_NONE, NULL, ARG_RESET,
		 N_("Reset device port"), NULL},
		POPT_TABLEEND
	};
	const struct poptOption captureOptions[] = {
		GPHOTO2_POPT_CALLBACK
		{"keep", '\0', POPT_ARG_NONE, NULL, ARG_KEEP,
		 N_("Keep images on camera after capturing"), NULL},
		{"no-keep", '\0', POPT_ARG_NONE, NULL, ARG_NO_KEEP,
		 N_("Remove images from camera after capturing"), NULL},
		{"wait-event", '\0', POPT_ARG_STRING|POPT_ARGFLAG_OPTIONAL, NULL, ARG_WAIT_EVENT,
		 N_("Wait for event(s) from camera"), N_("COUNT")},
		{"wait-event-and-download", '\0', POPT_ARG_STRING|POPT_ARGFLAG_OPTIONAL, NULL,
		 ARG_CAPTURE_TETHERED, N_("Wait for event(s) from the camera and download new images"), N_("COUNT")},
		{"capture-preview", '\0', POPT_ARG_NONE, NULL,
		 ARG_CAPTURE_PREVIEW,
		 N_("Capture a quick preview"), NULL},
		{"bulb", 'B', POPT_ARG_INT, NULL, ARG_CAPTURE_BULB,
		 N_("Set bulb exposure time in seconds"), N_("SECONDS")},
		{"frames", 'F', POPT_ARG_INT, NULL, ARG_CAPTURE_FRAMES,
		 N_("Set number of frames to capture (default=infinite)"), N_("COUNT")},
		{"interval", 'I', POPT_ARG_INT, NULL, ARG_CAPTURE_INTERVAL,
		 N_("Set capture interval in seconds"), N_("SECONDS")},
		{"reset-interval", '\0', POPT_ARG_NONE, NULL, ARG_RESET_INTERVAL,
		 N_("Reset capture interval on signal (default=no)"), NULL},
		{"capture-image", '\0', POPT_ARG_NONE, NULL,
		 ARG_CAPTURE_IMAGE, N_("Capture an image"), NULL},
		{"trigger-capture", '\0', POPT_ARG_NONE, NULL,
		 ARG_TRIGGER_CAPTURE, N_("Trigger capture of an image"), NULL},
		{"capture-image-and-download", '\0', POPT_ARG_NONE, NULL,
		 ARG_CAPTURE_IMAGE_AND_DOWNLOAD, N_("Capture an image and download it"), NULL},
		{"capture-movie", '\0', POPT_ARG_STRING|POPT_ARGFLAG_OPTIONAL, NULL,
		 ARG_CAPTURE_MOVIE, N_("Capture a movie"), N_("COUNT or SECONDS")},
		{"capture-sound", '\0', POPT_ARG_NONE, NULL,
		 ARG_CAPTURE_SOUND, N_("Capture an audio clip"), NULL},
		{"capture-tethered", '\0', POPT_ARG_STRING|POPT_ARGFLAG_OPTIONAL, NULL,
		 ARG_CAPTURE_TETHERED, N_("Wait for shutter release on the camera and download"), N_("COUNT")},
		{"trigger-capture", '\0', POPT_ARG_NONE, NULL,
		 ARG_TRIGGER_CAPTURE, N_("Trigger image capture"), NULL},
		POPT_TABLEEND
	};
	const struct poptOption fileOptions[] = {
		GPHOTO2_POPT_CALLBACK
		{"list-folders", 'l', POPT_ARG_NONE, NULL, ARG_LIST_FOLDERS,
		 N_("List folders in folder"), NULL},
		{"list-files", 'L', POPT_ARG_NONE, NULL, ARG_LIST_FILES,
		 N_("List files in folder"), NULL},
		{"mkdir", 'm', POPT_ARG_STRING, NULL, ARG_MKDIR,
		 N_("Create a directory"), N_("DIRNAME")},
		{"rmdir", 'r', POPT_ARG_STRING, NULL, ARG_RMDIR,
		 N_("Remove a directory"), N_("DIRNAME")},
		{"num-files", 'n', POPT_ARG_NONE, NULL, ARG_NUM_FILES,
		 N_("Display number of files"), NULL},
		{"get-file", 'p', POPT_ARG_STRING, NULL, ARG_GET_FILE,
		 N_("Get files given in range"), N_("RANGE")},
		{"get-all-files", 'P', POPT_ARG_NONE, NULL, ARG_GET_ALL_FILES,
		 N_("Get all files from folder"), NULL},
		{"get-thumbnail", 't', POPT_ARG_STRING, NULL, ARG_GET_THUMBNAIL,
		 N_("Get thumbnails given in range"), N_("RANGE")},
		{"get-all-thumbnails", 'T', POPT_ARG_NONE, 0,
		 ARG_GET_ALL_THUMBNAILS,
		 N_("Get all thumbnails from folder"), NULL},
		{"get-metadata", '\0', POPT_ARG_STRING, NULL, ARG_GET_METADATA,
		 N_("Get metadata given in range"), N_("RANGE")},
		{"get-all-metadata", '\0', POPT_ARG_NONE, NULL, ARG_GET_ALL_METADATA,
		 N_("Get all metadata from folder"), NULL},
		{"upload-metadata", '\0', POPT_ARG_STRING, NULL, ARG_UPLOAD_METADATA,
		 N_("Upload metadata for file"), NULL},
		{"get-raw-data", '\0', POPT_ARG_STRING, NULL,
		 ARG_GET_RAW_DATA,
		 N_("Get raw data given in range"), N_("RANGE")},
		{"get-all-raw-data", '\0', POPT_ARG_NONE, NULL,
		 ARG_GET_ALL_RAW_DATA,
		 N_("Get all raw data from folder"), NULL},
		{"get-audio-data", '\0', POPT_ARG_STRING, NULL,
		 ARG_GET_AUDIO_DATA,
		 N_("Get audio data given in range"), N_("RANGE")},
		{"get-all-audio-data", '\0', POPT_ARG_NONE, NULL,
		 ARG_GET_ALL_AUDIO_DATA,
		 N_("Get all audio data from folder"), NULL},
		{"delete-file", 'd', POPT_ARG_STRING, NULL, ARG_DELETE_FILE,
		 N_("Delete files given in range"), N_("RANGE")},
		{"delete-all-files", 'D', POPT_ARG_NONE, NULL,
		 ARG_DELETE_ALL_FILES, N_("Delete all files in folder (--no-recurse by default)"), NULL},
		{"upload-file", 'u', POPT_ARG_STRING, NULL, ARG_UPLOAD_FILE,
		 N_("Upload a file to camera"), N_("FILENAME")},
		{"filename", '\0', POPT_ARG_STRING, NULL, ARG_FILENAME,
		 N_("Specify a filename or filename pattern"), N_("FILENAME_PATTERN")},
		{"folder", 'f', POPT_ARG_STRING, NULL, ARG_FOLDER,
		 N_("Specify camera folder (default=\"/\")"), N_("FOLDER")},
		{"recurse", 'R', POPT_ARG_NONE, NULL, ARG_RECURSE,
		 N_("Recursion (default for download)"), NULL},
		{"no-recurse", '\0', POPT_ARG_NONE, NULL, ARG_NO_RECURSE,
		 N_("No recursion (default for deletion)"), NULL},
		{"new", '\0', POPT_ARG_NONE, NULL, ARG_NEW,
		 N_("Process new files only"), NULL},
		{"force-overwrite", '\0', POPT_ARG_NONE, NULL,
		 ARG_FORCE_OVERWRITE, N_("Overwrite files without asking"), NULL},
		POPT_TABLEEND
	};
	const struct poptOption miscOptions[] = {
		GPHOTO2_POPT_CALLBACK
		{"stdout", '\0', POPT_ARG_NONE, NULL, ARG_STDOUT,
		 N_("Send file to stdout"), NULL},
		{"stdout-size", '\0', POPT_ARG_NONE, NULL, ARG_STDOUT_SIZE,
		 N_("Print filesize before data"), NULL},
		{"auto-detect", '\0', POPT_ARG_NONE, NULL, ARG_AUTO_DETECT,
		 N_("List auto-detected cameras"), NULL},

#ifdef HAVE_LIBEXIF
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
		{"storage-info", '\0', POPT_ARG_NONE, NULL, ARG_STORAGE_INFO,
		 N_("Show storage information"), NULL},
		{"shell", '\0', POPT_ARG_NONE, NULL, ARG_SHELL,
		 N_("gPhoto shell"), NULL},
		POPT_TABLEEND
	};
	const struct poptOption options[] = {
		GPHOTO2_POPT_CALLBACK
		{NULL, '\0', POPT_ARG_INCLUDE_TABLE, (void *) &generalOptions, 0,
		 N_("Common options"), NULL},		
		{NULL, '\0', POPT_ARG_INCLUDE_TABLE, (void *) &miscOptions, 0,
		 N_("Miscellaneous options (unsorted)"), NULL},		
		{NULL, '\0', POPT_ARG_INCLUDE_TABLE, (void *) &infoOptions, 0,
		 N_("Get information on software and host system (not from the camera)"), NULL},
		{NULL, '\0', POPT_ARG_INCLUDE_TABLE, (void *) &cameraOptions, 0,
		 N_("Specify the camera to use"), NULL},
		{NULL, '\0', POPT_ARG_INCLUDE_TABLE, (void *) &configOptions, 0,
		 N_("Camera and software configuration"), NULL},
		{NULL, '\0', POPT_ARG_INCLUDE_TABLE, (void *) &captureOptions, 0,
		 N_("Capture an image from or on the camera"), NULL},
		{NULL, '\0', POPT_ARG_INCLUDE_TABLE, (void *) &fileOptions, 0,
		 N_("Downloading, uploading and manipulating files"), NULL},
		POPT_TABLEEND
	};
	CameraAbilities a;
	GPPortInfo info;
	GPPortType type;
	int result = GP_OK;
	cb_params.type = CALLBACK_PARAMS_TYPE_NONE;

	/* For translation */
	setlocale (LC_ALL, "");
        bindtextdomain (PACKAGE, LOCALEDIR);
        textdomain (PACKAGE);

	/* Create/Initialize the global variables before we first use
	 * them. And the signal handlers and popt callback functions
	 * do use them. */
	gp_params_init (&gp_params, envp);

	/* Figure out the width of the terminal and watch out for changes */
	signal_resize (0);
#ifdef SIGWINCH
	signal (SIGWINCH, signal_resize);
#endif

	/* Prepare processing options. */
	ctx = poptGetContext (PACKAGE, argc, (const char **) argv, options, 0);
	if (argc <= 1) {
		poptPrintUsage (ctx, stdout, 0);
		gp_params_exit (&gp_params);
                poptFreeContext(ctx);
		return (0);
	}

	/*
	 * Do we need debugging output? While we are at it, scan the 
	 * options for bad ones.
	 */
	poptResetContext (ctx);
	while ((result = poptGetNextOpt (ctx)) >= 0);
	if (result == POPT_ERROR_BADOPT) {
		poptPrintUsage (ctx, stderr, 0);
		gp_params_exit (&gp_params);
                poptFreeContext(ctx);
		return (EXIT_FAILURE);
	}
	if (help_option_given) {
		poptPrintHelp(ctx, stdout, 0);
		gp_params_exit (&gp_params);
                poptFreeContext(ctx);
		return 0;
	}
	if (usage_option_given) {
		poptPrintUsage(ctx, stdout, 0);
		gp_params_exit (&gp_params);
                poptFreeContext(ctx);
		return 0;
	}
	if (debug_option_given) {
		CR_MAIN (debug_action (&gp_params, debug_logfile_name));
	}

	/* Initialize. */
#ifdef HAVE_PTHREAD
	gp_camera_set_timeout_funcs (gp_params.camera, start_timeout_func,
				     stop_timeout_func, NULL);
#endif
	cb_params.type = CALLBACK_PARAMS_TYPE_PREINITIALIZE;
	cb_params.p.r = GP_OK;
	poptResetContext (ctx);
	while ((cb_params.p.r >= 0) && (poptGetNextOpt (ctx) >= 0));
	cb_params.type = CALLBACK_PARAMS_TYPE_INITIALIZE;
	poptResetContext (ctx);
	while ((cb_params.p.r >= 0) && (poptGetNextOpt (ctx) >= 0));
	/* Load default values for --filename and --hook-script if not
	 * explicitely specified
	 */
	if (!gp_params.filename) {
		char buf[256];
		if (gp_setting_get("gphoto2","filename",buf)>=0) {
			set_filename_action(&gp_params,buf);
		}	
	}
	if (!gp_params.hook_script) {
		char buf[PATH_MAX];
		if (gp_setting_get("gphoto2","hook-script",buf)>=0) {
			gp_params.hook_script = strdup(buf);
			/* Run init hook */
			if (0!=gp_params_run_hook(&gp_params, "init", NULL)) {
				fprintf(stdout,
					"Hook script \"%s\" init failed. Aborting.\n",
					gp_params.hook_script);
				exit(3);
			}
		}	
	}
	CR_MAIN (cb_params.p.r);

#define CHECK_OPT(o)					\
	if (!cb_params.p.q.found) {			\
		cb_params.p.q.arg = (o);			\
		poptResetContext (ctx);			\
		while (poptGetNextOpt (ctx) >= 0);	\
	}

	/* If we need a camera, make sure we've got one. */
	CR_MAIN (gp_camera_get_abilities (gp_params.camera, &a));
	CR_MAIN (gp_camera_get_port_info (gp_params.camera, &info));

	/* Determine which command is given on command line */
	cb_params.type = CALLBACK_PARAMS_TYPE_QUERY;
	cb_params.p.q.found = 0;
	CHECK_OPT (ARG_ABILITIES);
	CHECK_OPT (ARG_CAPTURE_IMAGE);
	CHECK_OPT (ARG_CAPTURE_IMAGE_AND_DOWNLOAD);
	CHECK_OPT (ARG_CAPTURE_MOVIE);
	CHECK_OPT (ARG_CAPTURE_PREVIEW);
	CHECK_OPT (ARG_CAPTURE_SOUND);
	CHECK_OPT (ARG_CAPTURE_TETHERED);
	CHECK_OPT (ARG_CAPTURE_TETHERED_KEEP);
	CHECK_OPT (ARG_CONFIG);
	CHECK_OPT (ARG_DELETE_ALL_FILES);
	CHECK_OPT (ARG_DELETE_FILE);
	CHECK_OPT (ARG_GET_ALL_AUDIO_DATA);
	CHECK_OPT (ARG_GET_ALL_FILES);
	CHECK_OPT (ARG_GET_ALL_RAW_DATA);
	CHECK_OPT (ARG_GET_ALL_THUMBNAILS);
	CHECK_OPT (ARG_GET_AUDIO_DATA);
	CHECK_OPT (ARG_GET_CONFIG);
	CHECK_OPT (ARG_GET_FILE);
	CHECK_OPT (ARG_GET_RAW_DATA);
	CHECK_OPT (ARG_GET_THUMBNAIL);
	CHECK_OPT (ARG_LIST_CONFIG);
	CHECK_OPT (ARG_LIST_FILES);
	CHECK_OPT (ARG_LIST_FOLDERS);
	CHECK_OPT (ARG_MANUAL);
	CHECK_OPT (ARG_MKDIR);
	CHECK_OPT (ARG_NUM_FILES);
	CHECK_OPT (ARG_RESET);
	CHECK_OPT (ARG_RMDIR);
	CHECK_OPT (ARG_SET_CONFIG);
	CHECK_OPT (ARG_SET_CONFIG_INDEX);
	CHECK_OPT (ARG_SET_CONFIG_VALUE);
	CHECK_OPT (ARG_SHELL);
	CHECK_OPT (ARG_SHOW_EXIF);
	CHECK_OPT (ARG_SHOW_INFO);
	CHECK_OPT (ARG_STORAGE_INFO);
	CHECK_OPT (ARG_SUMMARY);
	CHECK_OPT (ARG_TRIGGER_CAPTURE);
	CHECK_OPT (ARG_UPLOAD_FILE);
	CHECK_OPT (ARG_UPLOAD_METADATA);
	CHECK_OPT (ARG_WAIT_EVENT);
	gp_port_info_get_type (info, &type);
	if (cb_params.p.q.found &&
	    (!strcmp (a.model, "") || (type == GP_PORT_NONE))) {
		int count;
		const char *model = NULL, *path = NULL;
		CameraList *list;
		char buf[1024];
		int use_auto = 1;

		gp_log (GP_LOG_DEBUG, "main", "The user has not specified "
			"both a model and a port. Try to figure them out.");


		_get_portinfo_list(&gp_params);
		CR_MAIN (gp_list_new (&list)); /* no freeing below */
		CR_MAIN (gp_abilities_list_detect (gp_params_abilities_list(&gp_params), 
						   gp_params.portinfo_list,
						   list, gp_params.context));
		CR_MAIN (count = gp_list_count (list));
                if (count == 1) {
                        /* Exactly one camera detected */
			CR_MAIN (gp_list_get_name (list, 0, &model));
			CR_MAIN (gp_list_get_value (list, 0, &path));
			if (a.model[0] && strcmp(a.model,model)) {
				CameraAbilities alt;
				int m;

				CR_MAIN (m = gp_abilities_list_lookup_model (
						 gp_params_abilities_list(&gp_params), 
						 model));
				CR_MAIN (gp_abilities_list_get_abilities (
						 gp_params_abilities_list(&gp_params),
						 m, &alt));

				if ((a.port == GP_PORT_USB) && (alt.port == GP_PORT_USB)) {
					if (	(a.usb_vendor  == alt.usb_vendor)  &&
						(a.usb_product == alt.usb_product)
					)
						use_auto = 0;
				}
			}

			if (use_auto) {
				CR_MAIN (action_camera_set_model (&gp_params, model));
			}
			CR_MAIN (action_camera_set_port (&gp_params, path));

                } else if (!count) {
			int ret;
			/*
			 * No camera detected. Have a look at the settings.
			 * Ignore errors here, it might be a serial one.
			 */
                        if (gp_setting_get ("gphoto2", "model", buf) >= 0)
				action_camera_set_model (&gp_params, buf);
			if (gp_setting_get ("gphoto2", "port", buf) >= 0)
				action_camera_set_port (&gp_params, buf);
			ret = gp_camera_init (gp_params.camera, gp_params.context);
			if (ret != GP_OK) {
				if (ret == GP_ERROR_BAD_PARAMETERS)
					ret = -2000;
				CR_MAIN (ret);
			}
		} else {
			/* If --port override, search the model with the same port. */
			if (type != GP_PORT_NONE) {
				int i;
				char *xpath, *xname;

				gp_port_info_get_path (info, &xpath);
				gp_port_info_get_name (info, &xname);
				gp_log (GP_LOG_DEBUG, "gphoto2", "Looking for port ...\n");
				gp_log (GP_LOG_DEBUG, "gphoto2", "info.type = %d\n", type);
				gp_log (GP_LOG_DEBUG, "gphoto2", "info.name = %s\n", xname);
				gp_log (GP_LOG_DEBUG, "gphoto2", "info.path = %s\n", xpath);

				for (i=0;i<count;i++) {
					const char *xport, *xmodel;
					gp_list_get_value (list, i, &xport);
					if (!strcmp(xport, xpath)) {
						gp_list_get_name (list, i, &xmodel);
						CR_MAIN (action_camera_set_model (&gp_params, xmodel));
						CR_MAIN (action_camera_set_port (&gp_params, xport));
						gp_log (GP_LOG_DEBUG, "gphoto2","found port, was entry %d\n", i);
						break;
					}
				}
				if (i != count)
					use_auto = 0;
			}
			/* If --camera override, search the model with the same USB ID. */
			if (use_auto && a.model[0]) {
				int i;
				const char *xmodel;

				gp_log (GP_LOG_DEBUG, "gphoto2","Looking for model %s\n", a.model);
				for (i=0;i<count;i++) {
					CameraAbilities alt;
					int m;

					gp_list_get_name (list, i, &xmodel);
					CR_MAIN (m = gp_abilities_list_lookup_model (
							 gp_params_abilities_list(&gp_params), xmodel));
					CR_MAIN (gp_abilities_list_get_abilities (
							 gp_params_abilities_list(&gp_params), m, &alt));

					if ((a.port == GP_PORT_USB) && (alt.port == GP_PORT_USB)) {
						if (	(a.usb_vendor  == alt.usb_vendor)  &&
							(a.usb_product == alt.usb_product)
						) {
							use_auto = 0;
							CR_MAIN (gp_list_get_value (list, i, &path));
							CR_MAIN (action_camera_set_port (&gp_params, path));
							break;
						}
					}
				}
			}
			if (use_auto) {
				/* More than one camera detected */
				/*FIXME: Let the user choose from the list!*/
				gp_log (GP_LOG_DEBUG,"gphoto2","Nothing specified, using first entry of autodetect list.\n");
				CR_MAIN (gp_list_get_name (list, 0, &model));
				CR_MAIN (gp_list_get_value (list, 0, &path));
				CR_MAIN (action_camera_set_model (&gp_params, model));
				CR_MAIN (action_camera_set_port (&gp_params, path));
			}
                }
		gp_list_free (list);
        }

	/*
	 * Recursion is too dangerous for deletion. Only turn it on if
	 * explicitely specified.
	 */
	cb_params.type = CALLBACK_PARAMS_TYPE_QUERY;
	cb_params.p.q.found = 0;
	cb_params.p.q.arg = ARG_DELETE_FILE;
	poptResetContext (ctx);
	while (poptGetNextOpt (ctx) >= 0);
	if (!cb_params.p.q.found) {
		cb_params.p.q.arg = ARG_DELETE_ALL_FILES;
		poptResetContext (ctx);
		while (poptGetNextOpt (ctx) >= 0);
	}
	if (cb_params.p.q.found) {
		cb_params.p.q.found = 0;
		cb_params.p.q.arg = ARG_RECURSE;
		poptResetContext (ctx);
		while (poptGetNextOpt (ctx) >= 0);
		if (!cb_params.p.q.found)
			gp_params.flags &= ~FLAGS_RECURSE;
	}

        signal (SIGINT, signal_exit);

	/* If we are told to be quiet, be so. *
	cb_params.type = CALLBACK_PARAMS_TYPE_QUERY;
	cb_params.p.q.found = 0;
	cb_params.p.q.arg = ARG_QUIET;
	poptResetContext (ctx);
	while (poptGetNextOpt (ctx) >= 0);
	if (cb_params.p.q.found) {
		gp_params.flags |= FLAGS_QUIET;
	}
	*/

	/* Run startup hook */
	gp_params_run_hook(&gp_params, "start", NULL);

	/* Go! */
	cb_params.type = CALLBACK_PARAMS_TYPE_RUN;
	poptResetContext (ctx);
	cb_params.p.r = GP_OK;
	while ((cb_params.p.r >= GP_OK) && (poptGetNextOpt (ctx) >= 0));

	switch (gp_params.multi_type) {
	case MULTI_UPLOAD: {
		const char *arg;

		while ((cb_params.p.r >= GP_OK) && (NULL != (arg = poptGetArg (ctx)))) {
			CR_MAIN (action_camera_upload_file (&gp_params, gp_params.folder, arg));
		}
		break;
	}
	case MULTI_UPLOAD_META: {
		const char *arg;

		while ((cb_params.p.r >= GP_OK) && (NULL != (arg = poptGetArg (ctx)))) {
			CR_MAIN (action_camera_upload_metadata (&gp_params, gp_params.folder, arg));
		}
		break;
	}
	case MULTI_DELETE: {
		const char *arg;

		while ((cb_params.p.r >= GP_OK) && (NULL != (arg = poptGetArg (ctx)))) {
			CR_MAIN (delete_file_action (&gp_params, gp_params.folder, arg));
		}
		break;
	}
	case MULTI_DOWNLOAD: {
		const char *arg;

		while ((cb_params.p.r >= GP_OK) && (NULL != (arg = poptGetArg (ctx)))) {
			CR_MAIN (get_file_common (arg, gp_params.download_type ));
		}
		break;
	}
	default:
		break;
	}

	CR_MAIN (cb_params.p.r);

	/* Run stop hook */
	gp_params_run_hook(&gp_params, "stop", NULL);

	/* FIXME: Env var checks (e.g. for Windows, OS/2) should happen before
	 *        we load the camlibs */

	gp_params_exit (&gp_params);
        poptFreeContext(ctx);
        return (EXIT_SUCCESS);
}


/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
