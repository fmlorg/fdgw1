#
# $FML: Makefile,v 1.14 2001/09/20 08:32:18 fukachan Exp $
#

MODEL?=         natbox
KERNEL_CONF?=	FDGW

all: build

dist:
	make MODEL=adslrouter KERNEL_CONF=FDGW
	make MODEL=natbox     KERNEL_CONF=FDGW6

build:
	(cd src;make MODEL=${MODEL} )

clean cleandir:
	(cd src; make clean )
	(cd src/gnu/rp-pppoe/src/;make distclean)

allclean: clean
	-rm -fr image src/work src/compile

mount:
	vnconfig -v -c /dev/vnd0d src/ramdisk-small.fs
	mount /dev/vnd0a /mnt

umount:
	umount /mnt
	vnconfig -u /dev/vnd0d
