#
# Copyright (C) 2001 Ken'ichi Fukamachi <fukachan@fml.org>
#
# All rights reserved. This program is free software; you can
# redistribute it and/or modify it under the same terms as NetBSD itself.
#
# $FML: Makefile,v 1.23 2001/12/16 04:04:44 fukachan Exp $
#

#
# specify default image
#
ARCH?=		`uname -p`
MODEL?=         natbox
KERNEL_CONF?=	FDGW

# root privilege control
SU_CMD?=	su - root -c


all: build image

dist:
	-${MAKE} MODEL=adslrouter KERNEL_CONF=FDGW
	-${MAKE} MODEL=natbox     KERNEL_CONF=FDGW6

build:
	${SH} ./src/prepare_workdir.sh ${ARCH}.${MODEL}
	(cd src.${ARCH}.${MODEL}; ${MAKE} MODEL=${MODEL} build )
		
image:
	(cd src.${ARCH}.${MODEL}; \
		${SU_CMD} "cd `pwd`; ${MAKE} MODEL=${MODEL} image" )


#
# clean up
#
clean cleandir:
	- (cd src; ${MAKE} clean )
	- (cd src/gnu/rp-pppoe/src/; ${MAKE} distclean )
	- rm -f src/gnu/.pppoe_done
	@ for dir in src.* ; do \
		(cd $$dir ; ${MAKE} clean );\
	  done

allclean: clean
	-rm -fr src.* image.*


#
# utilities for debug
#
mount:
	if [ -f src.${ARCH}.${MODEL}/ramdisk-small.fs ];then \
	  vnconfig -v -c /dev/vnd0d src.${ARCH}.${MODEL}/ramdisk-small.fs;\
	  mount /dev/vnd0a /mnt;\
	fi

mount-adslrouter:
	vnconfig -v -c /dev/vnd0d image.${ARCH}/adslrouter.img
	mount /dev/vnd0a /mnt

mount-natbox:
	vnconfig -v -c /dev/vnd0d image.${ARCH}/natbox.img
	mount /dev/vnd0a /mnt

umount:
	umount /mnt
	vnconfig -u /dev/vnd0d
