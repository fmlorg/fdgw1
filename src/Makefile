#
# Copyright (C) 2001 Ken'ichi Fukamachi <fukachan@fml.org>
#
# All rights reserved. This program is free software; you can
# redistribute it and/or modify it under the same terms as NetBSD itself.
#
#
# $FML: Makefile,v 1.27 2001/12/16 14:52:09 fukachan Exp $
#


#
# target architecture 
#
ARCH?=		`uname -p`


#
# directories for targets and sources
#

# destination to install
LOCALBASE?=	/usr/pkg

# NetBSD source
BSDSRCDIR?=	/usr/src

# dir to lndir
_BSDSRCDIR=	${PWD}/NetBSD

_FLOPPY_DIR=	${_BSDSRCDIR}/distrib/${ARCH}/floppies

_SYS_DIR=	${BSDSRCDIR}/sys



#
# floppy image parameters
#
INO_BYTES=	40960

IMAGE=		boot.fs
MOUNT_POINT=	/mnt
MODEL?=         natbox
_LISTS?=	model/${MODEL}/list
_CBIN?=		model/${MODEL}/ramdiskbin

RAMDISK_S?=	ramdisk-small.fs


#
# compiler pragma / attributes / options
#
WARNS=1

ETC_DIR?=	conf/etc

_ARGS?=		MOUNT_POINT=${MOUNT_POINT} \
		TOP=${_FLOPPY_DIR} \
		_SYS_DIR=${_SYS_DIR} \
		BSDSRCDIR=${_BSDSRCDIR} \
		CURDIR=${PWD} _ETC_DIR=${ETC_DIR} \
		MODEL=${MODEL} ARCH=${ARCH} \
		INO_BYTES=${INO_BYTES} \
		RAMDISK_S=${RAMDISK_S}


#
# MAIN RULE
#

all: 
	@ echo "make build   (need NOT priviledge)"
	@ echo "make install (need root priviledge)"

build: _prepare_build_system
	@ echo ""
	@ echo "build stage 1. make ramdiskbin for ${MODEL} model"
	@ echo ""
	-make -f Makefile.ramdisk ${_ARGS} build
	@ echo ""
	@ echo "build stage 2. make netbsd kernel for ${MODEL} model"
	@ echo ""
	-make -f Makefile.kernel ${_ARGS} build
	@ echo ""
	@ echo "build stage for ${MODEL} model done."
	@ echo ""	

image:
	@ echo ""
	@ echo "image 1. make ramdisk-small.fs for ${MODEL} model"
	@ echo ""
	-make -f Makefile.ramdisk ${_ARGS} image
	@ echo ""
	@ echo "image 2. inject mdsetimage into NetBSD Kernel for ${MODEL} model"
	@ echo ""
	-make -f Makefile.kernel ${_ARGS} image
	@ echo ""
	@ echo "image 3. make a bootable floppy and inject netbsd into it for ${MODEL} model"
	@ echo ""
	-make -f Makefile.bootfloppy ${_ARGS} image
	@ echo ""
	@ echo "building image for ${MODEL} model stage done."
	@ echo ""	



######################################################
#
# initialize process before main build process
#
_prepare_build_system:	_init_mesg _init_dir  _lndir \
			_init_config _init_model \
			_init_pppoe _init_dhcp _init_resolver

_init_mesg:
	@ echo "
	@ echo "0. prepations ... "
	@ echo "

_lndir:
	@ test -d ${_BSDSRCDIR} || mkdir -p ${_BSDSRCDIR}
	@ echo lndir ${BSDSRCDIR} ${_BSDSRCDIR}
	@ if [ ! -f ${_BSDSRCDIR}/.lndir_done ]; then \
		lndir ${BSDSRCDIR} ${_BSDSRCDIR};\
	  fi
	@ touch ${_BSDSRCDIR}/.lndir_done

_init_dir:
	@ test -d work || mkdir -p work

_init_config:
	-rm -f dot.profile mtree.conf conf model
	ln ../conf/dot.profile .
	ln ../conf/mtree.conf  .
	-rm -f disktab.preinstall termcap.mini
	ln -s ../conf  conf
	ln -s ../model model
	cp ${_FLOPPY_DIR}/ramdisk-small/disktab.preinstall disktab.preinstall
	cp ${_FLOPPY_DIR}/ramdisk-small/termcap.mini       termcap.mini
	-rm -f ramdiskbin.conf
	sed "s/__ARCH__/${ARCH}/g" ${_CBIN}.conf |\
	sh fix_ramdiskbin.conf.sh > ramdiskbin.conf.new
	mv ramdiskbin.conf.new ramdiskbin.conf
	-rm -f list
	ln -s ${_LISTS} list

_init_model:
	if [ -x model/${MODEL}/configure ]; then \
		model/${MODEL}/configure ;\
	fi

_init_pppoe:
	@ echo "";
	@ echo "make rp-pppoe for ADSL router";
	@ echo "";
	@ if [ ! -f gnu/.pppoe_done ];then \
	  ( cd gnu/rp-pppoe/src;\
		./configure --prefix=${LOCALBASE};\
		make ;\
	  );\
	  fi
	@ touch gnu/.pppoe_done

_init_dhcp:
	(cd ${_BSDSRCDIR}/usr.sbin/dhcp;make)

_init_resolver:
	(cd utils/resolver/; make ${_ARGS})


#
# cleaning
#
clean cleandir:
	-make -f Makefile.ramdisk cleandir TOP=${_FLOPPY_DIR}
	-rm -f disktab.preinstall termcap.mini conf model
	-rm -f ramdiskbin.conf list
	-rm -f boot *.fs boot.fs* netbsd* *.tmp *~
	-rm -f dot.profile mtree.conf

allclean: clean
	-rm -fr work
	-make -f Makefile.ramdisk unconfig TOP=${_FLOPPY_DIR}
