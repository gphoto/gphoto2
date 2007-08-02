/* spawnve.c - implement spawnve() function, basically fork+execve+wait
 * Copyright © 2006 Hans Ulrich Niedermann <gp@n-dimensional.de>
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


#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "spawnve.h"


/** spawnve()
 * Run a program with a possibly modified environment and wait for it to end.
 * Based on fork(2), execve(2) and wait(2).
 */

int
spawnve(const char *filename, char *const argv[], char *const envp[])
{
  pid_t pid = fork();
  if (pid == 0) {
    /* child process */
    /* FIXME: Missing cleanup operations like closing files, stdin, etc */
    int fd;
    close(STDIN_FILENO);
    for (fd = 3; fd<200; fd++) { close(fd); }
    if (1) {
      const int retcode = execve(filename, argv, envp);
      const int s_errno = errno;
      fprintf(stderr, "execve(\"%s\") failed: %s\n", 	    
	      filename, strerror(s_errno));
      _exit(79);
      return retcode;
    }
  } else if (pid > 0) {
    /* parent process, child's PID is in pid */
    int status;
    const pid_t wait_pid = waitpid(pid, &status, 0);
    if (wait_pid == pid) {
      /* our child has exit()ed */
      /* fprintf(stderr, "child has exited, status: %d (0x%08x)\n", 
	 WEXITSTATUS(status), status); */
      if (status != 0) {
	return 1;
      }
      return 0;
    } else {
      /* some error occured */
      fprintf(stderr, "error waiting for child\n");
      return -1;
    }
  } else if (pid == -1) {
    /* parent process, fork() failed */
    /* int saved_errno = errno; */
    perror("fork() failed");
    return(-1);
  } else {
    /* Invalid return code */
    return(-1);
  }
}
