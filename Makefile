#
# Copyright (C) 2001,2002 Ken'ichi Fukamachi <fukachan@fml.org>
#
# All rights reserved. This program is free software; you can
# redistribute it and/or modify it under the same terms as NetBSD itself.
#
# $FML: Makefile,v 1.29 2002/01/27 03:15:09 fukachan Exp $
#

#
# specify default image
#
ARCH?=		`uname -p`
MODEL?=         natbox
KERNEL_CONF?=	FDGW

# root privilege control
SU_CMD?=	su - root -c


all:
	@ echo "make build   (need NOT priviledge)"
	@ echo "make image   (need root priviledge)"

dist: dist-build dist-image

dist-build:
	${MAKE} MODEL=adslrouter KERNEL_CONF=FDGW build
	${MAKE} MODEL=natbox     KERNEL_CONF=FDGW6 build

dist-image:
	${MAKE} MODEL=adslrouter KERNEL_CONF=FDGW image
	${MAKE} MODEL=natbox     KERNEL_CONF=FDGW6 image

build:
	${SH} ./src/prepare_workdir.sh ${ARCH}.${MODEL}
	(cd src.${ARCH}.${MODEL};\
	   ${MAKE} MODEL=${MODEL} KERNEL_CONF=${KERNEL_CONF} build;\
	)
		
image:
	@ echo ""	
	@ echo "\"make image\" needs root privilege"
	@ echo ""	
	(cd src.${ARCH}.${MODEL};\
	   ${SU_CMD} \
	   "cd `pwd`; ${MAKE} MODEL=${MODEL} KERNEL_CONF=${KERNEL_CONF} image";\
	)

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
