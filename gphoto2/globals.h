extern char             glob_cancel;

extern char *glob_folder;
extern char *glob_filename;

extern char		glob_cwd[];
extern int  		glob_quiet;
extern int		glob_option_count;
extern int  		glob_stdout;
extern int  		glob_stdout_size;

#ifndef HAVE_POPT
extern Option option[];
extern int    glob_option_count;
#endif

extern GPContext           *glob_context;
extern CameraAbilitiesList *glob_abilities_list;
