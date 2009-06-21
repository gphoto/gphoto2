/* spawntest.c - test the spawnve() function
 * Copyright (C) 2006,2007 Hans Ulrich Niedermann <gp@n-dimensional.de>
 * C99ism fix Copyright (C) 2007 Dan Fandrich <dan@coneharvesters.com>
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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "spawnve.h"


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
  ASSERT(result_buf != NULL);
  strcpy(result_buf, varname);
  strcat(result_buf, "=");
  strcat(result_buf, value);
  return result_buf;
}


int
main(const int argc, const char *argv[], const char **envp)
{
  unsigned int i;

  FILE *out = stdout;
  int retcode;

  char *hook_env = getenv("GPHOTO_HOOK");
  char *filename = (hook_env!=NULL)?hook_env:"./test-hook.sh";
  
  /* We want this to be writable, so we explicitly define it as char[] */
  char params[7] = "params";
  char * const child_argv[] = {
    params,
    NULL,
  };
  
  /* envars not to copy */
  static const char * const varlist[] = {
    "ACTION", "ARGUMENT", NULL
  };

  unsigned int envi = 0;
  char **child_envp;

  /* count number of environment variables currently set */
  unsigned int envar_count;
  for (envar_count=0; envp[envar_count] != NULL; envar_count++) {
    /* printf("%3d: \"%s\"\n", envar_count, envp[envar_count]); */
  }
  
  fprintf(out, "Before spawn...\n");
  fflush(out);

  /* Initialize environment. Start with newly defined vars, then copy
   * all the existing ones.
   * Total amount of char* is
   *     number of existing envars (envar_count)
   *   + max number of new envars (2)
   *   + NULL list terminator (1)
   */
  child_envp = calloc(envar_count+2+1,sizeof(child_envp[0]));
  ASSERT(child_envp != NULL);

  /* own envars */
  child_envp[envi++] = alloc_envar("ARGUMENT", "/etc/shadow ;-P");
  child_envp[envi++] = alloc_envar("ACTION", "download");

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
      child_envp[envi] = strdup(envp[i]);
      ASSERT(child_envp[envi] != NULL);
      ++envi;
    }
  }

  /* Run the child program */
  retcode = spawnve(filename, child_argv, child_envp);

  fprintf(out, "After spawn, retcode=%d\n", retcode);
  fflush(out);

  /* Free memory */
  for (i=0; child_envp[i] != NULL; i++) {
    free(child_envp[i]);
  }
  free(child_envp);

  return retcode;
}
