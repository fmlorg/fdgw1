#
# $FML: Makefile,v 1.12 2001/09/06 02:50:24 fukachan Exp $
#

MODEL?=         natbox

all:	build

build:
	(cd src;make MODEL=${MODEL} )

clean cleandir:
	(cd src; make clean )

allclean: clean
	-rm -fr image

mount:
	vnconfig -v -c /dev/vnd0d src/ramdisk-small.fs
	mount /dev/vnd0a /mnt

umount:
	umount /mnt
	vnconfig -u /dev/vnd0d
