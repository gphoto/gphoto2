/* foreach.h
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

#ifndef __FOREACH_H__
#define __FOREACH_H__

#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-list.h>

#include <actions.h>
#include <gp-params.h>

int for_each_folder         (GPParams *, FolderAction action);
int for_each_file           (GPParams *, FileAction action);
int for_each_file_in_range  (GPParams *, FileAction action,
			     const char *range);

#endif /* __FOREACH_H__ */


/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
