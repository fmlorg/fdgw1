#
# $FML: Makefile,v 1.11 2001/09/06 00:23:00 fukachan Exp $
#

MODEL?=         natbox

all:	build

build:
	(cd src;make MODEL=${MODEL} )

clean cleandir:
	(cd src; make clean )

mount:
	vnconfig -v -c /dev/vnd0d src/ramdisk-small.fs
	mount /dev/vnd0a /mnt

umount:
	umount /mnt
	vnconfig -u /dev/vnd0d
