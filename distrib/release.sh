#!/bin/sh
#
# $FML$
#

VERSION=`date +%C%y%m%d`
ID=floppy_natbox-$VERSION
DIR=/var/tmp/$ID

test -d $DIR || mkdir -p $DIR

rsync --exclude distrib -av -C ./ $DIR

cd /var/tmp || exit 1

tar cvf $ID.tar $ID
gzip -9 $ID.tar

exit 0;

