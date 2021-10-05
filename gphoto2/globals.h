#ifndef GPHOTO2_GLOBALS_H
#define GPHOTO2_GLOBALS_H

extern char	glob_cancel;

extern char	*glob_folder;

extern char	glob_cwd[];
extern int	glob_option_count;

/* flag for SIGUSR1 handler */
extern volatile int capture_now;

/* flag for SIGUSR2 handler */
extern volatile int end_next;

#endif /* !defined(GPHOTO2_GLOBALS_H) */


/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
