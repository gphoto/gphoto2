#!/bin/sh
# test-hook.sh - test script for gphoto2's --hook-script option
# Copyright © 2006 Hans Ulrich Niedermann <gp@n-dimensional.de>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the
# along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA


self=`basename $0`

case "$ACTION" in
    init)
	echo "$self: INIT"
	# exit 1 # non-null exit to make gphoto2 call fail
	;;
    start)
	echo "$self: START"
	;;
    download)
	echo "$self: DOWNLOAD to $ARGUMENT"
	;;
    stop)
	echo "$self: STOP"
	;;
    *)
	echo "$self: Unknown action: $ACTION"
	;;
esac

exit 0
