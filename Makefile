#
# Copyright (C) 2001,2002 Ken'ichi Fukamachi <fukachan@fml.org>
#
# All rights reserved. This program is free software; you can
# redistribute it and/or modify it under the same terms as NetBSD itself.
#
# $FML: Makefile,v 1.47 2002/06/20 09:22:01 fukachan Exp $
#

#
# specify default image
#
ARCH?=		`uname -p`
MODEL?=         natbox
KERNEL_CONF?=	FDGW
BIOSBOOT?=	biosboot.sym

# root privilege control
SU_CMD?=	su - root -c


all:
	@ echo "make build   (need NOT priviledge)"
	@ echo "make image   (need root priviledge)"

dist: dist-build dist-image

dist-build:
	${MAKE} MODEL=adslrouter KERNEL_CONF=FDGW  build
	${MAKE} MODEL=proxybox   KERNEL_CONF=FDGW  build
	${MAKE} MODEL=natbox     KERNEL_CONF=FDGW6 build
	${MAKE} MODEL=riprouter  KERNEL_CONF=FDGW6 build

dist-image:
	${MAKE} MODEL=adslrouter KERNEL_CONF=FDGW  image
	${MAKE} MODEL=proxybox   KERNEL_CONF=FDGW  image
	${MAKE} MODEL=natbox     KERNEL_CONF=FDGW6 image
	${MAKE} MODEL=riprouter  KERNEL_CONF=FDGW6 image

build:
	${SH} ./src/utils/prepare_workdir.sh ${ARCH}.${MODEL}
	(cd obj.${ARCH}.${MODEL};\
	   ${MAKE}	MODEL=${MODEL} \
			KERNEL_CONF=${KERNEL_CONF} \
			BIOSBOOT=${BIOSBOOT} \
			build;\
	)
		
image:
	@ echo ""	
	@ echo "\"make image\" needs root privilege"
	@ echo ""	
	(cd obj.${ARCH}.${MODEL};\
	   ${SU_CMD} \
	   "cd `pwd`; ${MAKE} \
			MODEL=${MODEL} \
			KERNEL_CONF=${KERNEL_CONF} \
			BIOSBOOT=${BIOSBOOT} \
			image";\
	)

#
# clean up
#
clean cleandir:
	@ echo "===> clearing src";
	- (cd src; ${MAKE} clean )
	- rm -f src/.extract_done
	@ echo "===> clearing rp-pppoe";
	- (cd src/gnu/rp-pppoe/src ; gmake distclean )
	- rm -f src/gnu/.pppoe_done
	@ echo "===> clearing squid";
	- (cd src/gnu/squid ; gmake distclean )
	- rm -f src/gnu/.squid_done
	@ echo "===> clearing jftpgw";
	- (cd src/gnu/jftpgw ; gmake distclean )
	- rm -f src/gnu/.jftpgw_done
	@ echo "===> clearing stone";
	- (cd src/gnu/stone ; gmake distclean )
	- rm -f src/gnu/.stone_done
	@ echo "===> clearing transproxy";
	- (cd src/sbin/transproxy ; make clean RM="rm -f")
	- rm -f src/sbin/.transproxy_done
	@ for dir in obj.* ; do \
		if [ -d $$dir ];then\
		(\
		  	echo "===> clearing $$dir" ;\
			cd $$dir ;\
			${MAKE} clean;\
		);\
		fi;\
	  done

allclean: clean
	-rm -fr obj.* image.*
	- (cd src/gnu ; make clean )

stat:	obj.*.*/log.*
	@ ${SH} src/utils/stat.sh	

#
# utilities for debug
#
mount-ramdisk:
	if [ -f obj.${ARCH}.${MODEL}/ramdisk-small.fs ];then \
	  vnconfig -v -c /dev/vnd0d obj.${ARCH}.${MODEL}/ramdisk-small.fs;\
	  mount /dev/vnd0a /mnt;\
	fi

mount-img:
	vnconfig -v -c /dev/vnd0d image.${ARCH}/${MODEL}.img
	mount /dev/vnd0a /mnt

umount-img: umount
umount-ramdisk: umount
umount:
	umount /mnt
	vnconfig -u /dev/vnd0d
