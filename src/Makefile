#
# Copyright (C) 2001 Ken'ichi Fukamachi <fukachan@fml.org>
#
# All rights reserved. This program is free software; you can
# redistribute it and/or modify it under the same terms as NetBSD itself.
#
#
# $FML: Makefile,v 1.14 2001/09/29 08:15:41 fukachan Exp $
#

BSDSRCDIR?=	/usr/src
LOCALBASE?=	/usr/pkg

_BSDSRCDIR=	${PWD}/work/src

SRC_DIRS?=	bin sbin usr.bin usr.sbin distrib dist

ARCH?=		`uname -p`
_TOP=		${BSDSRCDIR}/distrib/${ARCH}/floppies
WARNS=1

# floppy
INO_BYTES=	40960

IMAGE=		boot.fs
MOUNT_POINT=	/mnt
MODEL?=         natbox
_LISTS?=	model/${MODEL}/list
_CBIN?=		model/${MODEL}/ramdiskbin

ETC_DIR?=	conf/etc

_ARGS?=		MOUNT_POINT=${MOUNT_POINT} TOP=${_TOP} \
		BSDSRCDIR=${_BSDSRCDIR} \
		CURDIR=${PWD} _ETC_DIR=${ETC_DIR} MODEL=${MODEL} ARCH=${ARCH} \
		INO_BYTES=${INO_BYTES}

all: ${IMAGE}

${IMAGE}:  _lndir _prepare
	@ echo ""
	@ echo "1. make file system on md0a (ramdisk-small.fs)"
	@ echo ""
	(cd utils/resolver/; make ${_ARGS})
	-make -f Makefile.ramdisk ${_ARGS} all
	@ echo ""
	@ echo "2. make netbsd kernel and mdsetimage on it"
	@ echo ""
	-make -f Makefile.kernel ${_ARGS} all

install:
	@ echo ""
	@ echo "3. make a bootable floppy and install netbsd to it"
	@ echo ""
	-make -f Makefile.bootfloppy ${_ARGS} install
	@ echo ""
	@ echo "done."

_lndir:
	@ test -d ${_BSDSRCDIR} || mkdir -p ${_BSDSRCDIR}
	@ echo ""
	@ echo lndir ${BSDSRCDIR} ${_BSDSRCDIR}
	@ echo ""
	@ if [ ! -f work/.lndir_done ]; then \
	  for dir in ${SRC_DIRS} ;\
	  do \
		test -d ${_BSDSRCDIR}/$$dir || mkdir -p ${_BSDSRCDIR}/$$dir ;\
		lndir ${BSDSRCDIR}/$$dir ${_BSDSRCDIR}/$$dir; \
	  done \
	  fi
	@ touch work/.lndir_done

_prepare:
	@ echo "0. prepations ... "
	-rm -f disktab.preinstall termcap.mini conf model
	test -d work || mkdir -p work
	ln ../conf/dot.profile .
	ln ../conf/mtree.conf  .
	ln -s ../conf  conf
	ln -s ../model model
	cp ${_TOP}/ramdisk-small/disktab.preinstall disktab.preinstall
	cp ${_TOP}/ramdisk-small/termcap.mini       termcap.mini
	-rm -f ramdiskbin.conf
	sed "s/__ARCH__/${ARCH}/g" ${_CBIN}.conf > ramdiskbin.conf
	-rm -f list
	ln -s ${_LISTS} list
	if [ -x model/${MODEL}/configure ]; then model/${MODEL}/configure ; fi
	@ echo "";echo "make rp-pppoe for ADSL router";echo "";
	( cd gnu/rp-pppoe/src;\
		./configure --prefix=${LOCALBASE};\
		make ;\
	)
	(cd ${_BSDSRCDIR}/usr.sbin/dhcp;make)

clean cleandir:
	-make -f Makefile.ramdisk unconfig TOP=${_TOP}
	-make -f Makefile.ramdisk cleandir TOP=${_TOP}
	-rm -f disktab.preinstall termcap.mini conf model
	-rm -f ramdiskbin.conf list
	-rm -f boot *.fs boot.fs* netbsd* *.tmp *~
	-rm -f dot.profile mtree.conf
