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

#ifndef MAX
#define MAX(x, y) (((x)>(y))?(x):(y))
#endif
#ifndef MIN
#define MIN(x, y) (((x)<(y))?(x):(y))
#endif

static void
ctx_status_func (GPContext *context, const char *format, va_list args,
                 void *data)
{
        vfprintf (stderr, format, args);
        fprintf  (stderr, "\n");
        fflush   (stderr);
}

static void
ctx_error_func (GPContext *context, const char *format, va_list args,
                void *data)
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
ctx_progress_start_func (GPContext *context, float target,
                         const char *format, va_list args, void *data)
{
	GPParams *p = data;
        unsigned int id, len;
        static unsigned char initialized = 0;

        if (!initialized) {
                memset (progress_states, 0, sizeof (progress_states));
                initialized = 1;
        }

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
ctx_progress_update_func (GPContext *context, unsigned int id,
                          float current, void *data)
{
	GPParams *p = data;
        static const char spinner[] = "\\|/-";
        unsigned int i, width, pos;
        float rate;
        char remaining[10], buf[4];
        time_t sec = 0;

        /* Guard against buggy camera drivers */
        if (id >= MAX_PROGRESS_STATES || id < 0)
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
                strncat (remaining, buf, sizeof (remaining) - 1);
        }
        if ((int) sec >= 60) {
                snprintf (buf, sizeof (buf), "%2im", (int) sec / 60);
                sec -= ((int) (sec / 60) * 60);
                strncat (remaining, buf, sizeof (remaining) - 1);
        }
        if ((int) sec) {
                snprintf (buf, sizeof (buf), "%2is", (int) sec);
                strncat (remaining, buf, sizeof (remaining) - 1);
        }

        /* Determine the width of the progress bar and the current position */
        width = MAX (0, (int) p->cols -
                                strlen (progress_states[id].message) - 20);
        pos = MIN (width, (MIN (current / progress_states[id].target, 100.) * width) + 0.5);

        /* Print the progress bar */
        fprintf (stdout, "%s |", progress_states[id].message);
        for (i = 0; i < width; i++)
                fprintf (stdout, (i < pos) ? "-" : " ");
        if (pos == width)
                fputc ('|', stdout);
        else
                fputc (spinner[progress_states[id].count & 0x03], stdout);
        progress_states[id].count++;

        fprintf (stdout, " %5.1f%% %9.9s\r",
                 current / progress_states[id].target * 100., remaining);
        fflush (stdout);
}

static void
ctx_progress_stop_func (GPContext *context, unsigned int id, void *data)
{
	GPParams *p = data;
        unsigned int i;

        /* Guard against buggy camera drivers */
        if (id >= MAX_PROGRESS_STATES || id < 0)
                return;

        /* Clear the progress bar. */
        for (i = 0; i < p->cols; i++)
                fprintf (stdout, " ");
        fprintf (stdout, "\r");
        fflush (stdout);

        progress_states[id].target = 0.;
}

static GPContextFeedback
ctx_cancel_func (GPContext *context, void *data)
{
        if (glob_cancel) {
                return (GP_CONTEXT_FEEDBACK_CANCEL);
                glob_cancel = 0;
        } else
                return (GP_CONTEXT_FEEDBACK_OK);
}

static void
ctx_message_func (GPContext *context, const char *format, va_list args,
                  void *data)
{
        int c;

        vprintf (format, args);
        fprintf (stdout, "\n");

        /* Only ask for confirmation if the user can give it. */
        if (isatty (STDOUT_FILENO) && isatty (STDIN_FILENO)) {
                fprintf (stdout, _("Press any key to continue.\n"));
                fflush (stdout);
                c = fgetc (stdin);
        } else
                fflush (stdout);
}

void
gp_params_init (GPParams *p)
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
	p->flags = FOR_EACH_FLAGS_RECURSE;

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

	gp_abilities_list_new (&p->abilities_list);
	gp_abilities_list_load (p->abilities_list, p->context);

	p->debug_func_id = -1;
}

void
gp_params_exit (GPParams *p)
{
	if (!p)
		return;

	if (p->abilities_list)
		gp_abilities_list_free (p->abilities_list);
	if (p->camera)
		gp_camera_unref (p->camera);
	if (p->folder)
		free (p->folder);
	if (p->filename)
		free (p->filename);
	if (p->context)
		gp_context_unref (p->context);
	memset (p, 0, sizeof (GPParams));
}
