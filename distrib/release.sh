#!/bin/sh
#
# $FML: release.sh,v 1.2 2001/08/14 21:52:56 fukachan Exp $
#

VERSION=`date +%C%y%m%d`
ID=fdgw-$VERSION
DIR=/var/tmp/$ID

test -d $DIR || mkdir -p $DIR

rsync --exclude distrib -av -C ./ $DIR

cd /var/tmp || exit 1

tar cvf $ID.tar $ID
gzip -9 $ID.tar

exit 0;

