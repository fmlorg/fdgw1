#!/bin/sh
#
# $Id$
#

DIR=/var/tmp/netbsd

VERSION=`cat conf/etc/release_version`
VERSION=`date +%C%y%m%d`

test -d /var/tmp/netbsd && mv /var/tmp/netbsd /var/tmp/netbsd.$$
test -d /var/tmp/netbsd || mkdir /var/tmp/netbsd

rsync --exclude distrib -av -C ./ /var/tmp/netbsd/

cd /var/tmp

mv netbsd floppy_natbox-$VERSION

tar cvf floppy_natbox-$VERSION.tar floppy_natbox-$VERSION
gzip -9 floppy_natbox-$VERSION.tar

exit 0;

