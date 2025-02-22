# gphoto2


## What is gphoto2?

gphoto2 is a command-line frontend to libgphoto2.


## Where can I find more information?

Visit the gphoto project web site. It should always be found at least
at one of the following URLs:

  * http://www.gphoto.com/
  * http://www.gphoto.org/
  * http://gphoto.sourceforge.net/
  * https://github.com/gphoto/
  * http://sf.net/projects/gphoto

The main page is in the file [gphoto2.1](doc/gphoto2.1).


## How do I build it?

If you have installed libgphoto2 into `$HOME/.local` and want to
install gphoto2 to `$HOME/.local` as well, keep the `PKG_CONFIG_PATH=`
and `--prefix=` arguments to `configure`. Otherwise adapt or remove
them.

```
autoreconf -is  # if using a git clone
./configure PKG_CONFIG_PATH="$HOME/.local/lib/pkgconfig${PKG_CONFIG_PATH+":${PKG_CONFIG_PATH}"}" --prefix="$HOME/.local"
make
make install
```

Out-of-tree builds are supported. `./configure --help` may help.

To build gphoto2, you will need the following (apart from the common build tools):

  * The libgphoto2 library.
  * The popt libraries (for commandline option handling),
    the system package may be called popt-devel or popt-dev or similar.

Optional:

  * The EXIF library. (libexif-devel, libexif-dev or similar)
  * The JPEG library. (libjpeg-devel, libjpeg-dev, or jpeg-dev or similar)
  * The CDK library (for ncurses based configuration UI). (cdk-devel or similar)
  * The AALIB library (for ascii art rendering of previews). (aalib-devel or similar)


## How do I test it?

```
make check
```

The test suite checks the installation and basic functionality of the gphoto2
program and the 'Directory Browse' libgphoto2 camera driver.
