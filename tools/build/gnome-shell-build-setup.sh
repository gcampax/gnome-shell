#!/bin/sh
#
# Script that sets up jhbuild to build gnome-shell. Run this to
# checkout jhbuild and the required configuration.
#
# Copyright (C) 2008, Red Hat, Inc.
#
# Some ideas and code taken from gtk-osx-build
#
# Copyright (C) 2006, 2007, 2008 Imendio AB
#

release_file=

if which lsb_release > /dev/null 2>&1; then
  system=`lsb_release -is`
  version=`lsb_release -rs`
elif [ -f /etc/fedora-release ] ; then
  system=Fedora
  release_file=/etc/fedora-release
elif [ -f /etc/SuSE-release ] ; then
  system=SUSE
  release_file=/etc/SuSE-release
elif [ -f /etc/mandriva-release ]; then
  system=MandrivaLinux
  release_file=/etc/mandriva-release
fi

if [ x$release_file != x ] ; then
    version=`sed 's/[^0-9\.]*\([0-9\.]\+\).*/\1/' < $release_file`
fi

# This is the configuration of packages that we'll need to successfully jhbuild.
# Each line is of the form:
#
#  name_of_depenency: <distro_chars>:package [<distro_chars>:package...]
#
# The dependency name is purely informative and isn't otherwise used. distro_chars are:
#
#  f: Fedora
#  d: Debian/Ubuntu
#  s: SuSE
#  m: Mandriva
#
# Rather than have some complicated system here, when we have packages that depend
# on distribution version, we just tweak the package list in the code below.
# Where known, the module that requires a library is commented.

all_packages() {
cat <<EOF
# For this script:
curl: fdsm:curl
git: f:git d:git-core

# Build tools
build-essential: d:build-essential
automake: fd:automake
binutils: f:binutils
bison: fds:bison
cmake: fd:cmake # libproxy
docbook-style-xsl: f:docbook-style-xsl d:docbook-xsl # gtk-doc
flex: fds:flex
gettext: fd:gettext
gcc: f:gcc
g++: f:gcc-c++
gperf: f:gperf d:gperf # evolution-data-server gudev
intltool: f:intltool
libtool: f:libtool
make: f:make
perl-XML-Simple: f:perl-XML-Simple d:libxml-simple-perl # icon-naming-utils
pkgconfig: f:pkgconfig
python: f:python
texinfo: fd:texinfo # libgtop
xsltproc: f:libxslt d:xsltproc # gtk-doc

# Image handling libraries
freetype: f:freetype-devel d:libfreetype6-dev # fontconfig
jasper: f:jasper-devel d:libjasper-dev # gdk-pixbuf
libjpeg: f:libjpeg-devel d:libjpeg-dev # gdk-pixbuf
libpng: f:libpng-devel d:libpng-dev # gdk-pixbuf
libtiff: fs:libtiff-devel d:libtiff-dev # gdk-pixbuf

# X libraries
GL: f:mesa-libGL-devel d:mesa-common-dev d:libgl1-mesa-dev m:GL-devel # cogl
libX11: s:xorg-x11-proto-devel s:xorg-x11-devel # gtk+
libXcomposite: f:libXcomposite-devel d:libxcomposite-dev # cogl mutter
libXcursor: f:libXcursor-devel libxcursor-dev # mousetweaks
libXdamage: f:libXdamage-devel m:libxdamage-devel d:libxdamage-dev # cogl mutter
libXi: f:libXi-devel d:libxi-dev # gtk+
libXrandr: f:libXrandr-devel d:libxrandr-dev # gnome-desktop
libXrender: f:libXrender-devel d: libxrender-dev # cairo WebKit
libXt: f:libXt-devel d:libxt-dev # WebKit
libXtst: f:libXtst-devel d:libxtst-dev # caribou
xcb: f:xcb-util-devel d:libx11-xcb-dev # startup-notification

# Other libraries
cups: fs:cups-devel d:libcups2-dev # gnome-control-center
db4: f:db4-devel d:libdb-dev # evolution-data-server
icu: f:libicu-devel d:libicu-dev # WebKit
libacl: f:libacl-devel d:libacl1-dev # gudev
libcurl: f:libcurl-devel # liboauth. See below for Debian
libffi: fs:libffi-devel d:libffi-dev # gobject-introspection
libtool-ltdl: f:libtool-ltdl-devel d:libltdl-dev # libcanberra
libusb: f:libusb1-devel d:libusb-1.0-0-dev # upower
openssl: f:openssl-devel d:libssl-dev # liboauth
pam: f:pam-devel d:libpam-dev # polkit
ppp: f:ppp-devel d:ppp-dev # NetworkManager
python-devel: f:python-devel d:python-dev # pygobject py2cairo
readline: fsm:readline-devel d:libreadline-dev
sane: f:sane-backends-devel d:libsane-dev # colord
sqlite: d:libsqlite3-dev f:sqlite-devel # libsoup
udev: f:libudev-devel d:libudev-dev # gudev
uuid: f:libuuid-devel d:uuid-dev # Networkmanager
vorbis: f:libvorbis-devel d:libvorbis-dev # libcanberra

# python libraries used by gnome-shell wrapper script
# These are commented out because the gnome-shell wrapper script
# isn't built by default, and needs updating for running on
# a pure-GNOME 3 system, rather than recovering to GNOME 2.
# dbus-python: f:dbus-python d:python-dbus
# python-gobject: f:pygobject2 d:python-gobject
# python-gconf: f:gnome-python2-gconf d:python-gconf
EOF
}

packages_for_distribution() {
    distribution_char=$1
    all_packages |
        sed -n 's/#.*//; /[^ 	]/p' | # Remove comments and blank lines
	while read dependency_name words ; do
	    for word in $words ; do
		# Word is <distribution-chars>:package
		IFS=:
		set $word
		IFS=' 	'
		case $1 in
		    *$distribution_char*) echo $2
                esac
	    done
       done
}

# We try to make it clear what we're doing via sudo so if a user gets prompted
# for their password, they have some idea why.
run_via_sudo() {
    echo "Running: sudo $@"
    if sudo "$@" ; then : ; else
	echo 1>&2 "Command failed."
	echo 1>&2 "Exiting gnome-shell-build-setup.sh. You can run it again safely."
	exit 1
    fi
}

if test "x$system" = xUbuntu -o "x$system" = xDebian -o "x$system" = xLinuxMint ; then
  reqd=`packages_for_distribution d`

  if apt-cache show libxcb-util0-dev > /dev/null 2> /dev/null; then
    reqd="$reqd libxcb-util0-dev"
  else
    reqd="$reqd libxcb-event1-dev libxcb-aux0-dev"
  fi

  if apt-cache show autopoint > /dev/null 2> /dev/null; then
    reqd="$reqd autopoint"
  fi

  if [ ! -x /usr/bin/dpkg-checkbuilddeps -o ! -x /usr/bin/apt-file ]; then
    echo "Installing base dependencies"
    run_via_sudo apt-get install dpkg-dev apt-file
  fi

  echo "Updating apt-file cache"
  run_via_sudo apt-file update

  # libcurl comes in both gnutls and openssl flavors. If the openssl
  # flavor of the runtime is installed, install the matching -dev
  # package, but default to the gnutls version. (the libcurl3 vs. libcurl4
  # mismatch is intentional and is how things are packaged.)

  if ! dpkg-checkbuilddeps -d libcurl-dev /dev/null 2> /dev/null; then
      if dpkg -s libcurl3 /dev/null 2> /dev/null; then
	  missing="libcurl4-openssl-dev $missing"
      elif dpkg -s libcurl3-nss /dev/null 2> /dev/null; then
	  missing="libcurl4-nss-dev $missing"
      else
	  missing="libcurl4-gnutls-dev $missing"
      fi
  fi

  for pkg in $reqd ; do
      if ! dpkg-checkbuilddeps -d $pkg /dev/null 2> /dev/null; then
        missing="$pkg $missing"
      fi
  done
  if test ! "x$missing" = x; then
    echo "Installing packages"
    run_via_sudo apt-get install $missing
  fi
fi

if test "x$system" = xFedora ; then
  reqd=`packages_for_distribution f`

  if expr $version = 14 > /dev/null ; then
      reqd="$reqd gettext-autopoint"
  elif expr $version \>= 15 > /dev/null ; then
      reqd="$reqd gettext-devel"
  fi

  echo -n "Computing packages to install ... "
  for pkg in $reqd ; do
      if ! rpm -q --whatprovides $pkg > /dev/null 2>&1; then
        missing="$pkg $missing"
      fi
  done
  echo "done"

  if test ! "x$missing" = x; then
      echo -n "Installing packages ... "
      gpk-install-package-name $missing
      echo "done"
  fi
fi

if test "x$system" = xSUSE -o "x$system" = "xSUSE LINUX" ; then
  reqd=`packages_for_distribution s`
  if test ! "x$reqd" = x; then
    echo "Please run 'su --command=\"zypper install $reqd\"' and try again."
    echo
    exit 1
  fi
fi

if test "x$system" = xMandrivaLinux ; then
  reqd=`packages_for_distribution m`
  if test ! "x$reqd" = x; then
	gurpmi --auto $reqd
  fi
fi

SOURCE=$HOME/Source
BASEURL=http://git.gnome.org/browse/gnome-shell/plain/tools/build

if [ -d $SOURCE ] ; then : ; else
    mkdir $SOURCE
    echo "Created $SOURCE"
fi

if [ -d $SOURCE/jhbuild ] ; then
    if [ -d $SOURCE/jhbuild/.git ] ; then
        echo -n "Updating jhbuild ... "
        ( cd $SOURCE/jhbuild && git pull --rebase > /dev/null ) || exit 1
        echo "done"
    else
        echo "$SOURCE/jhbuild is not a git repository"
        echo "You should remove it and rerun this script"
	exit 1
    fi
else
    echo -n "Checking out jhbuild into $SOURCE/jhbuild ... "
    cd $SOURCE
    git clone git://git.gnome.org/jhbuild > /dev/null || exit 1
    echo "done"
fi

echo -n "Installing jhbuild ... "
(cd $SOURCE/jhbuild &&
 ./autogen.sh --simple-install &&
 make -f Makefile.plain DISABLE_GETTEXT=1 bindir=$HOME/bin install) >/dev/null
echo "done"

if [ -e $HOME/.jhbuildrc ] ; then
    if grep JHBUILDRC_GNOME_SHELL $HOME/.jhbuildrc > /dev/null ; then : ; else
	mv $HOME/.jhbuildrc $HOME/.jhbuildrc.bak
	echo "Saved ~/.jhbuildrc as ~/.jhbuildrc.bak"
    fi
fi

echo -n "Writing ~/.jhbuildrc ... "
curl -L -s -o $HOME/.jhbuildrc $BASEURL/jhbuildrc-gnome-shell
echo "done"

if [ ! -f $HOME/.jhbuildrc-custom ]; then
    echo -n "Writing example ~/.jhbuildrc-custom ... "
    curl -L -s -o $HOME/.jhbuildrc-custom $BASEURL/jhbuildrc-custom-example
    echo "done"
fi

echo "Installing modules as system packages when possible"
$HOME/bin/jhbuild sysdeps --install

if test "x`echo $PATH | grep $HOME/bin`" = x; then
    echo "PATH does not contain $HOME/bin, it is recommended that you add that."
    echo
fi

echo "Done."
