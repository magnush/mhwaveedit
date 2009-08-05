#! /bin/bash

# Simple script to create tar/bz2-ball from git repo
# Must have version number as argument

if test "x$1" = "x"
then
	echo "Syntax: package_mhwaveedit.sh <version>"
	exit
fi
VERSION=$1

if test ! -e git-release.sh
then
    echo "Run from source repo root dir please..."
    exit
fi

echo "Cloning git repository into temporary directory..."
rm -rf mhwaveedit-release
git clone . mhwaveedit-release
if test ! -d mhwaveedit-release
then
    echo " -- Error: directory wasn't created"
    exit
fi

echo "Checking out version $VERSION into temporary directory..."
cd mhwaveedit-release
if git checkout v$VERSION 
then    
    true
else
    echo " -- Error: Version tag v$VERSION doesn't seem to exist"
    cd ..
    rm -rf mhwaveedit-release
    exit
fi

echo "Generating ChangeLog.git..."
git log --abbrev-commit --date=short --pretty="format:%h %ad %an (%ae):%n%s%n" rel1-4-15..HEAD | fold -s > ChangeLog.git

echo "Compiling..."
sh cvscompile
make distclean
rm -rf autom4te.cache po/*~ docgen/*~
chmod 755 mkinstalldirs po/mkinstalldirs missing

cd ..
echo "Packaging version $VERSION ..."
ln -s mhwaveedit-release mhwaveedit-$VERSION
tar cz --exclude '*CVS*' --exclude '*#*' -f mhwaveedit-$VERSION.tar.gz mhwaveedit-$VERSION/*
tar cj --exclude '*CVS*' --exclude '*#*' -f mhwaveedit-$VERSION.tar.bz2 mhwaveedit-$VERSION/*

if test -L mhwaveedit-$VERSION
then
	rm mhwaveedit-$VERSION
fi
rm -rf mhwaveedit-release

ls -l mhwaveedit-$VERSION.tar.*

