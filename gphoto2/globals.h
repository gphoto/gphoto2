#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#ifndef HAVE_POPT
#  include "main.h"
#endif

extern char	glob_cancel;

extern char	*glob_folder;

extern char	glob_cwd[];
extern int	glob_option_count;

#ifndef HAVE_POPT
extern Option option[];
extern int    glob_option_count;
#endif

#endif
