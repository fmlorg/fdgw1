#
# Copyright (C) 2001 Ken'ichi Fukamachi <fukachan@fml.org>
#
# All rights reserved. This program is free software; you can
# redistribute it and/or modify it under the same terms as NetBSD itself.
#
# $FML: Makefile,v 1.22 2001/12/15 16:31:45 fukachan Exp $
#


MODEL?=         natbox
KERNEL_CONF?=	FDGW

ARCH?=		`uname -p`

SU_CMD?=	su - root -c


all: build image

dist:
	-make MODEL=adslrouter KERNEL_CONF=FDGW
	-make MODEL=natbox     KERNEL_CONF=FDGW6

build:
	${SH} ./src/prepare_workdir.sh ${ARCH}.${MODEL}
	(cd src.${ARCH}.${MODEL}; make MODEL=${MODEL} build )
		
image:
	(cd src.${ARCH}.${MODEL}; \
		${SU_CMD} "cd `pwd`; make MODEL=${MODEL} image" )

clean cleandir:
	- (cd src; make clean )
	- (cd src/gnu/rp-pppoe/src/;make distclean)
	@ for dir in src.* ; do \
		(cd $$dir ; make clean );\
	  done

allclean: clean
	-rm -fr src.* image.*

mount:
	vnconfig -v -c /dev/vnd0d src/ramdisk-small.fs
	mount /dev/vnd0a /mnt

mount-adslrouter:
	vnconfig -v -c /dev/vnd0d src/obj.${ARCH}/adslrouter.img
	mount /dev/vnd0a /mnt

mount-natbox:
	vnconfig -v -c /dev/vnd0d src/obj.${ARCH}/natbox.img
	mount /dev/vnd0a /mnt

umount:
	umount /mnt
	vnconfig -u /dev/vnd0d
