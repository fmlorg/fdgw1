#
# $FML: Makefile,v 1.16 2001/09/21 14:46:35 fukachan Exp $
#

MODEL?=         natbox
KERNEL_CONF?=	FDGW

all: build

dist:
	-make MODEL=adslrouter KERNEL_CONF=FDGW
	-make clean
	-make MODEL=natbox     KERNEL_CONF=FDGW6

build:
	(cd src;make MODEL=${MODEL} )

clean cleandir:
	(cd src; make clean )
	- (cd src/gnu/rp-pppoe/src/;make distclean)

allclean: clean
	-rm -fr image src/work src/compile

mount:
	vnconfig -v -c /dev/vnd0d src/ramdisk-small.fs
	mount /dev/vnd0a /mnt

mount-adslrouter:
	vnconfig -v -c /dev/vnd0d image/adslrouter.img
	mount /dev/vnd0a /mnt

mount-natbox:
	vnconfig -v -c /dev/vnd0d image/natbox.img
	mount /dev/vnd0a /mnt

umount:
	umount /mnt
	vnconfig -u /dev/vnd0d
