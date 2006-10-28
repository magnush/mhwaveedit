#! /bin/bash

# Simple script to create tar/bz2-ball
# Must have version number as argument

if test "x$1" = "x"
then
	echo "Syntax: package_mhwaveedit.sh <version>"
	exit
fi
VERSION=$1

echo "Compiling..."
sh cvscompile
make distclean
rm -rf autom4te.cache po/*~ docgen/*~
chmod 755 mkinstalldirs po/mkinstalldirs missing

cd ..
echo "Packaging version $VERSION ..."
ln -s mhwaveedit mhwaveedit-$VERSION
tar cz --exclude '*CVS*' --exclude '*#*' -f mhwaveedit-$VERSION.tar.gz mhwaveedit-$VERSION/*
tar cj --exclude '*CVS*' --exclude '*#*' -f mhwaveedit-$VERSION.tar.bz2 mhwaveedit-$VERSION/*

if test -L mhwaveedit-$VERSION
then
	rm mhwaveedit-$VERSION
fi

ls -l mhwaveedit-$VERSION.tar.*

