/* spawnve.h - interface for spawnve() function, basically fork+execve+wait
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


#ifndef __SPAWNVE_H__
#define __SPAWNVE_H__

int
spawnve(const char *filename, char *const argv[], char *const envp[]);

#endif /* __SPAWNVE_H__ */
