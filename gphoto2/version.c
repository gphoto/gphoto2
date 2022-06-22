/* version.c
 *
 * Copyright © 2002 Hans Ulrich Niedermann <gp@n-dimensional.de>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "version.h"

#include "mongoose.h"

#include <stdlib.h>


#ifdef __GNUC__
#define __unused__ __attribute__((unused))
#else
#define __unused__
#endif

#ifdef GPHOTO2_WEBAPI
static const char **gphoto2_frontend_mongoose_version(GPVersionVerbosity __unused__ verbose)
{
	static const char *both[] = {
		MG_VERSION,
		"webserver lib",
		NULL
	};
	return both;
}
#endif

static const char **gphoto2_frontend_version(GPVersionVerbosity verbose)
{
	/* we could also compute/parse the short strings from the long
	   ones, but the current method is easier for now :-) */
	static const char *verb[] = {
		VERSION,
#ifdef HAVE_CC
		HAVE_CC " (C compiler used)",
#else
		"unknown (C compiler used)",
#endif
		"popt (mandatory, for handling command-line parameters)",
#ifdef HAVE_LIBEXIF
		"exif (for displaying EXIF information)",
#else
		"no exif (for displaying EXIF information)",
#endif
#ifdef HAVE_CDK
		"cdk (for accessing configuration options)",
#else
		"no cdk (for accessing configuration options)",
#endif
#ifdef HAVE_AA
		"aa (for displaying live previews)",
#else
		"no aa (for displaying live previews)",
#endif
#ifdef HAVE_JPEG
		"jpeg (for displaying live previews in JPEG format)",
#else
		"no jpeg (for displaying live previews in JPEG format)",
#endif
#ifdef HAVE_RL
		"readline (for easy navigation in the shell)",
#else
		"no readline (for easy navigation in the shell)",
#endif
#ifdef GPHOTO2_WEBAPI
		"mongoose (for gphoto2-webapi)",
#endif
		NULL
	};
	static const char *shrt[] = {
		VERSION,
#ifdef HAVE_CC
		HAVE_CC,
#else
		"unknown cc",
#endif
		"popt(m)",
#ifdef HAVE_LIBEXIF
		"exif",
#else
		"no exif",
#endif
#ifdef HAVE_CDK
		"cdk",
#else
		"no cdk",
#endif
#ifdef HAVE_AA
		"aa",
#else
		"no aa",
#endif
#ifdef HAVE_JPEG
		"jpeg",
#else
		"no jpeg",
#endif
#ifdef HAVE_RL
		"readline",
#else
		"no readline",
#endif
#ifdef GPHOTO2_WEBAPI
		"mongoose",
#endif
		NULL
	};
	return((verbose == GP_VERSION_VERBOSE)?verb:shrt);
}

const module_version module_versions[] = {
	{ "gphoto2", gphoto2_frontend_version },
	{ "libgphoto2", gp_library_version },
	{ "libgphoto2_port", gp_port_library_version },
#ifdef GPHOTO2_WEBAPI
	{ "mongoose", gphoto2_frontend_mongoose_version },
#endif
	{ NULL, NULL }
};


/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
