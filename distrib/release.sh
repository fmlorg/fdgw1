#!/bin/sh
#
# $FML: release.sh,v 1.3 2001/09/07 00:27:05 fukachan Exp $
#

VERSION=`date +%C%y%m%d`
ID=fdgw-$VERSION
DIR=/var/tmp/$ID

test -d $DIR || mkdir -p $DIR

rsync --exclude distrib -av -C ./ $DIR

cd /var/tmp || exit 1

tar cvf $ID.tar $ID
rm -f $ID.tar.gz
gzip -9 $ID.tar

exit 0;

