/* gp-params.c
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "gp-params.h"
#include "i18n.h"

/* This needs to disappear. */
#include "globals.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "spawnve.h"

#ifndef MAX
#define MAX(x, y) (((x)>(y))?(x):(y))
#endif
#ifndef MIN
#define MIN(x, y) (((x)<(y))?(x):(y))
#endif


#ifdef __GNUC__
#define __unused__ __attribute__((unused))
#else
#define __unused__
#endif


static void
#ifdef __GNUC__
__attribute__((__format__(printf,2,0)))
#endif
ctx_status_func (GPContext __unused__ *context, const char *format, va_list args,
                 void __unused__ *data)
{
        vfprintf (stderr, format, args);
        fprintf  (stderr, "\n");
        fflush   (stderr);
}

static void
#ifdef __GNUC__
__attribute__((__format__(printf,2,0)))
#endif
ctx_error_func (GPContext __unused__ *context, const char *format, va_list args,
                void __unused__ *data)
{
        fprintf  (stderr, "\n");
        fprintf  (stderr, _("*** Error ***              \n"));
        vfprintf (stderr, format, args);
        fprintf  (stderr, "\n");
        fflush   (stderr);
}

#define MAX_PROGRESS_STATES 16
#define MAX_MSG_LEN 1024

static struct {
        char message[MAX_MSG_LEN + 1];
        float target;
        unsigned long int count;
        struct {
                float  current;
                time_t time, left;
        } last;
} progress_states[MAX_PROGRESS_STATES];

static unsigned int
#ifdef __GNUC__
__attribute__((__format__(printf,3,0)))
#endif
ctx_progress_start_func (GPContext __unused__ *context, float target,
                         const char *format, va_list args, void *data)
{
	GPParams *p = data;
        unsigned int id, len;
        static unsigned char initialized = 0;

        if (!initialized) {
                memset (progress_states, 0, sizeof (progress_states));
                initialized = 1;
        }

	if (p->flags & FLAGS_QUIET)
		return 0;

        /*
         * If the message is too long, we will shorten it. If we have less
         * than 4 cols available, we won't display any message.
         */
        len = (p->cols * 0.5 < 4) ? 0 : MIN (p->cols * 0.5, MAX_MSG_LEN);

        /* Remember message and target. */
        for (id = 0; id < MAX_PROGRESS_STATES; id++)
                if (!progress_states[id].target)
                        break;
        if (id == MAX_PROGRESS_STATES)
                id--;
        progress_states[id].target = target;
        vsnprintf (progress_states[id].message, len + 1, format, args);
        progress_states[id].count = 0;
        progress_states[id].last.time = time (NULL);
        progress_states[id].last.current = progress_states[id].last.left = 0.;

        /* If message is too long, shorten it. */
        if (progress_states[id].message[len - 1]) {
                progress_states[id].message[len - 1] = '\0';
                progress_states[id].message[len - 2] = '.';
                progress_states[id].message[len - 3] = '.';
                progress_states[id].message[len - 4] = '.';
        }

        return (id);
}

static void
ctx_progress_update_func (GPContext __unused__ *context, unsigned int id,
                          float current, void *data)
{
	GPParams *p = data;
        static const char spinner[] = "\\|/-";
        unsigned int i, width, pos;
        float rate;
        char remaining[10], buf[4];
        time_t sec = 0;

        /* Guard against buggy camera drivers */
        if (id >= MAX_PROGRESS_STATES || ((int)id) < 0)
                return;

	if (p->flags & FLAGS_QUIET)
		return;

        /* How much time until completion? */
        if ((time (NULL) - progress_states[id].last.time > 0) &&
            (current - progress_states[id].last.current > 0)) {
                rate = (time (NULL) - progress_states[id].last.time) /
                       (current - progress_states[id].last.current);
                sec = (MAX (0, progress_states[id].target - current)) * rate;
                if (progress_states[id].last.left) {
                        sec += progress_states[id].last.left;
                        sec /= 2.;
                }
                progress_states[id].last.time = time (NULL);
                progress_states[id].last.current = current;
                progress_states[id].last.left = sec;
        } else
                sec = progress_states[id].last.left;
        memset (remaining, 0, sizeof (remaining));
        if ((int) sec >= 3600) {
                snprintf (buf, sizeof (buf), "%2ih", (int) sec / 3600);
                sec -= ((int) (sec / 3600) * 3600);
                strncat (remaining, buf, sizeof (remaining) - strlen (remaining) - 1);
        }
        if ((int) sec >= 60) {
                snprintf (buf, sizeof (buf), "%2im", (int) sec / 60);
                sec -= ((int) (sec / 60) * 60);
                strncat (remaining, buf, sizeof (remaining) - strlen (remaining) - 1);
        }
        if ((int) sec) {
                snprintf (buf, sizeof (buf), "%2is", (int) sec);
                strncat (remaining, buf, sizeof (remaining) - strlen (remaining) - 1);
        }

        /* Determine the width of the progress bar and the current position */
        width = MAX (0, (int) (p->cols -
			       strlen (progress_states[id].message) - 20));
        pos = MIN (width, (MIN (current / progress_states[id].target, 100.) * width) + 0.5);

        /* Print the progress bar */
        printf ("%s |", progress_states[id].message);
        for (i = 0; i < width; i++)
                putchar ((i < pos) ? '-' : ' ');
        if (pos == width)
	        putchar ('|');
        else
                putchar (spinner[progress_states[id].count & 0x03]);
        progress_states[id].count++;

        printf (" %5.1f%% %9.9s\r",
		current / progress_states[id].target * 100., remaining);
        fflush (stdout);
}

static void
ctx_progress_stop_func (GPContext __unused__ *context, unsigned int id, 
			void *data)
{
	GPParams *p = data;
        unsigned int i;

        /* Guard against buggy camera drivers */
        if (id >= MAX_PROGRESS_STATES || ((int)id) < 0)
                return;

	if (p->flags & FLAGS_QUIET)
		return;

        /* Clear the progress bar. */
        for (i = 0; i < p->cols; i++)
                putchar (' ');
        putchar ('\r');
        fflush (stdout);

        progress_states[id].target = 0.;
}

static GPContextFeedback
ctx_cancel_func (GPContext __unused__ *context, void __unused__ *data)
{
        if (glob_cancel) {
                return (GP_CONTEXT_FEEDBACK_CANCEL);
                glob_cancel = 0;
        } else
                return (GP_CONTEXT_FEEDBACK_OK);
}

static void
#ifdef __GNUC__
__attribute__((__format__(printf,2,0)))
#endif
ctx_message_func (GPContext __unused__ *context, const char *format, 
		  va_list args, void __unused__ *data)
{
        int c;

        vprintf (format, args);
        putchar ('\n');

        /* Only ask for confirmation if the user can give it. */
        if (isatty (STDOUT_FILENO) && isatty (STDIN_FILENO)) {
                printf (_("Press any key to continue.\n"));
                fflush (stdout);
                c = fgetc (stdin);
        } else
                fflush (stdout);
}


/** gp_params_init
 * @param envp: The third char ** parameter of the main() function
 */

void
gp_params_init (GPParams *p, char **envp)
{
	if (!p)
		return;

	memset (p, 0, sizeof (GPParams));

	p->folder = strdup ("/");
	if (!p->folder) {
		fprintf (stderr, _("Not enough memory."));
		fputc ('\n', stderr);
		exit (1);
	}

	gp_camera_new (&p->camera);

	p->cols = 79;
	p->flags = FLAGS_RECURSE;

	/* Create a context. Report progress only if users will see it. */
	p->context = gp_context_new ();
	gp_context_set_cancel_func    (p->context, ctx_cancel_func,  p);
	gp_context_set_error_func     (p->context, ctx_error_func,   p);
	gp_context_set_status_func    (p->context, ctx_status_func,  p);
	gp_context_set_message_func   (p->context, ctx_message_func, p);
	if (isatty (STDOUT_FILENO))
		gp_context_set_progress_funcs (p->context,
			ctx_progress_start_func, ctx_progress_update_func,
			ctx_progress_stop_func, p);

	p->_abilities_list = NULL;

	p->debug_func_id = -1;

	p->envp = envp;
}


CameraAbilitiesList *
gp_params_abilities_list (GPParams *p)
{
	/* If p == NULL, the behaviour of this function is as undefined as
	 * the expression p->abilities_list would have been. */
	if (p->_abilities_list == NULL) {
		gp_abilities_list_new (&p->_abilities_list);
		gp_abilities_list_load (p->_abilities_list, p->context);
	}
	return p->_abilities_list;
}


void
gp_params_exit (GPParams *p)
{
	if (!p)
		return;

	if (p->_abilities_list)
		gp_abilities_list_free (p->_abilities_list);
	if (p->camera)
		gp_camera_unref (p->camera);
	if (p->folder)
		free (p->folder);
	if (p->filename)
		free (p->filename);
	if (p->context)
		gp_context_unref (p->context);
	if (p->hook_script)
		free (p->hook_script);
	if (p->portinfo_list)
		gp_port_info_list_free (p->portinfo_list);
	memset (p, 0, sizeof (GPParams));
}


/* If CALL_VIA_SYSTEM is defined, use insecure system(3) instead of execve(2) */
/* #define CALL_VIA_SYSTEM */

static int
internal_run_hook(const char *const hook_script, 
		  const char *const action, const char *const argument,
		  char **envp);


int
gp_params_run_hook (GPParams *params, const char *action, const char *argument)
{
	/* printf("gp_params_run_hook(params, \"%s\", \"%s\")\n", 
	   action, argument);
	*/
	if (params->hook_script == NULL) {
		return 0;
	}
	return internal_run_hook(params->hook_script,
				 action, argument,
				 params->envp);
}


#ifdef CALL_VIA_SYSTEM
static void
internal_putenv(const char *const varname, const char *const value)
{
	if (NULL != varname) {
		if (NULL != value) {
			const size_t varname_size = strlen(varname);
			const size_t value_size = strlen(value);
			/* '=' and '\0' */
			const size_t buffer_size = varname_size + value_size + 2;
			char buffer[buffer_size];
			strcpy(buffer, varname);
			strcat(buffer, "=");
			strcat(buffer, value);
			printf("putenv(\"%s\")\n", buffer);
			if (0 != putenv(buffer)) {
				printf("putenv error\n");
			}
		} else {
			/* clear variable */
			if (unsetenv(varname)) {
				int my_errno = errno;
				fprintf(stderr, "unsetenv(\"%s\"): %s", 
					varname, strerror(my_errno));
			}
		}
	}
	printf("%% %s=%s\n", varname, getenv(varname));
}
#endif


#ifndef CALL_VIA_SYSTEM

#define ASSERT(cond)					\
  do {							\
    if (!(cond)) {					\
      fprintf(stderr, "%s:%d: Assertion failed: %s\n",	\
	      __FILE__, __LINE__, #cond);		\
      exit(13);						\
    }							\
  } while(0)


static char *
alloc_envar(const char *varname, const char *value)
{
  const size_t varname_size = strlen(varname);
  const size_t value_size = strlen(value);
  const size_t buf_size = varname_size + 1 + value_size + 1;
  char *result_buf = malloc(buf_size);

  if (!result_buf) return NULL;
  strcpy(result_buf, varname);
  strcat(result_buf, "=");
  strcat(result_buf, value);
  return result_buf;
}
#endif


static int
internal_run_hook(const char *const hook_script, 
		  const char *const action, const char *const argument,
		  char **envp)
{
#ifdef CALL_VIA_SYSTEM
	int retcode;

	/* run hook using system(3) */
	internal_putenv("ACTION", action);
	internal_putenv("ARGUMENT", argument);
	
	retcode = system(hook_script);
	if (retcode != 0) {
		fprintf(stderr, "Hook script returned error code %d (0x%x)\n",
			retcode, retcode);
		return 1;
	}
	return 0;
#else
	/* spawnve() based implementation of internal_run_hook()
	 *
	 * Most of the code here creates and destructs the
	 * char *child_argv[] and char *child_envp[] to be passed to 
	 * spawnve() and thus execve().
	 *
	 * Error handling is simple:
	 *  * If malloc() or calloc() fail, abort the whole program.
	 */

	/* A note on program memory layout:
	 *
	 * child_argv and child_envp MUST be in writable memory, so we
	 * malloc() them.
	 */
	
	char *my_hook_script = strdup(hook_script);
	unsigned int i;

	/* run hook using execve(2) */
	char **child_argv = calloc(2, sizeof(child_argv[0]));

	/* envars not to copy */
	const char *const varlist[] = {
		"ACTION", "ARGUMENT", NULL
	};

	/* environment variables for child process, and index going through them */
	char **child_envp;
	unsigned int envi = 0;

	int retcode;
	
	/* count number of environment variables currently set */
	unsigned int envar_count;
	for (envar_count=0; envp[envar_count] != NULL; envar_count++) {
		/* printf("%3d: \"%s\"\n", envar_count, envp[envar_count]); */
	}

	ASSERT(my_hook_script != NULL);
	child_argv[0] = my_hook_script;

	/* Initialize environment. Start with newly defined vars, then copy
	 * all the existing ones. calloc() does the initialization with NULL.
	 * Total amount of char* is
	 *     number of existing envars (envar_count)
	 *   + max number of new envars (2)
	 *   + NULL list terminator (1)
	 */
	child_envp = calloc(envar_count+((sizeof(varlist)/sizeof(varlist[0]))-1)+1, 
			    sizeof(child_envp[0]));
	ASSERT(child_envp != NULL);

	/* own envars */
	if (NULL != action) {
		char *envar = alloc_envar("ACTION", action);
		ASSERT(envar != NULL);
		child_envp[envi++] = envar;
	}
	if (NULL != argument) {
		char *envar = alloc_envar("ARGUMENT", argument);
		ASSERT(envar != NULL);
		child_envp[envi++] = envar;
	}
	
	/* copy envars except for those in varlist */
	for (i=0; i<envar_count; i++) {
		int skip = 0;
		unsigned int n;
		for (n=0; varlist[n] != NULL; n++) {
			const char *varname = varlist[n];
			const char *start = strstr(envp[i], varname);
			if ((envp[i] == start) &&  (envp[i][strlen(varname)] == '=')) {
				skip = 1;
				break;
			}
		}
		if (!skip) {
			child_envp[envi++] = strdup(envp[i]);
		}
	}
	
	/* Actually run the hook script */
	retcode = spawnve(hook_script, child_argv, child_envp);
		
	/* Free all memory */
	for (i=0; child_envp[i] != NULL; i++) {
		free(child_envp[i]);
	}
	free(child_envp);
	for (i=0; child_argv[i] != NULL; i++) {
		free(child_argv[i]);
	}
	free(child_argv);

	/* And finally return to caller */
	if (retcode != 0) {
		fprintf(stderr, "Hook script returned error code %d (0x%x)\n",
			retcode, retcode);
		return 1;
	}
	return 0;
#endif
}


/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
