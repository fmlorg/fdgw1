#
# Copyright (C) 2001,2002,2003 Ken'ichi Fukamachi <fukachan@fml.org>
#
# All rights reserved. This program is free software; you can
# redistribute it and/or modify it under the same terms as NetBSD itself.
#
# $FML: Makefile,v 1.52 2003/01/17 09:21:58 fukachan Exp $
#

# programs and directories
GMAKE?=		gmake
SH?=		/bin/sh

PKG_DIR?=	src/pkg
GNU_DIR?=	src/gnu
TOOL_DIR?=	src/utils
STATUS_DIR?=	${PKG_DIR}

#
# specify default image
#
ARCH?=		`uname -p`
MODEL?=         natbox
KERNEL_CONF?=	FDGW
BIOSBOOT?=	biosboot.sym

# root privilege control
SU_CMD?=	su - root -c

MAKE_PARAMS = 	MODEL=${MODEL} \
		KERNEL_CONF=${KERNEL_CONF} \
		BIOSBOOT=${BIOSBOOT} \
		SH=${SH}	\
		GNU_DIR=${GNU_DIR:S|^src/||} \
		PKG_DIR=${PKG_DIR:S|^src/||} \
		STATUS_DIR=${STATUS_DIR:S|^src/||} \
		TOOL_DIR=${TOOL_DIR:S|^src/||}

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
	${SH} ./${TOOL_DIR}/prepare_workdir.sh ${ARCH}.${MODEL}
	(cd obj.${ARCH}.${MODEL};${MAKE} ${MAKE_PARAMS} build )

image:
	@ echo ""	
	@ echo "\"make image\" needs root privilege"
	@ echo ""	
	(cd obj.${ARCH}.${MODEL};\
	   ${SU_CMD} "cd `pwd`; ${MAKE} ${MAKE_PARAMS} image" )


#
# clean up
#
clean cleandir: _clean_src _clean_fdgw_buildin_pkg _clean_pkg _clean_obj

_clean_src:
	@ echo "===> clearing src";
	- (cd src; ${MAKE} clean )

_clean_fdgw_buildin_pkg:
	@ echo "===> clearing rp-pppoe";
	- (cd ${GNU_DIR}/rp-pppoe/src ; ${GMAKE} distclean )
	- rm -f ${STATUS_DIR}/.pppoe_done
	@ echo "===> clearing stone";
	- (cd ${GNU_DIR}/stone ; ${GMAKE} distclean )
	- rm -f ${STATUS_DIR}/.stone_done
	@ echo "===> clearing transproxy";
	- (cd src/sbin/transproxy ; make clean RM="rm -f")
	- rm -f ${STATUS_DIR}/.transproxy_done
	@ echo "===> clearing pim6sd";
	- (cd src/usr.sbin/pim6sd/pim6sd ; make clean RM="rm -f")
	- rm -f ${STATUS_DIR}/.pim6sd_done

_clean_pkg:
	@ echo "===> clearing squid";
	- (cd ${PKG_DIR}/squid ; ${GMAKE} distclean )
	- rm -f ${STATUS_DIR}/.squid_done
	@ echo "===> clearing jftpgw";
	- (cd ${PKG_DIR}/jftpgw ; ${GMAKE} distclean )
	- rm -f ${STATUS_DIR}/.jftpgw_done

_clean_obj:
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
	- rm -fr obj.* image.*
	- rm -fr src/NetBSD
	- (rm -fr ${PKG_DIR} )

stat:	obj.*.*/log.*
	@ ${SH} ${TOOL_DIR}/stat.sh	

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
