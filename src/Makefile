#
# Copyright (C) 2001,2002 Ken'ichi Fukamachi <fukachan@fml.org>
#
# All rights reserved. This program is free software; you can
# redistribute it and/or modify it under the same terms as NetBSD itself.
#
#
# $FML: Makefile,v 1.63 2003/01/17 07:07:01 fukachan Exp $
#


#
# target architecture 
#
ARCH?=		`uname -p`

__VERSION != uname -r
NetBSD_VERSION?=	${__VERSION:C|^(...).*|\1|}

_CONF_DIR=	conf/${NetBSD_VERSION}

#
# directories for targets and sources
#

# destination to install
LOCALBASE?=	/usr/pkg

# NetBSD source
BSDSRCDIR?=	/usr/src

# dir to lndir
_BSDSRCDIR=	${PWD}/NetBSD

# utilities
_ENV= env PKG_DIR=${PKG_DIR}
_FETCH_PROGRAM=	  ${_ENV} ${SH} ${TOOL_DIR}/fetch.sh
_EXTRACT_PROGRAM= ${_ENV} ${SH} ${TOOL_DIR}/extract.sh

# packages we used for some models
_SQUID_SOURCE=	squid-2.4.STABLE6-src.tar.gz
_JFTPGW_SOURCE=	jftpgw-0.13.beta.j.tar.gz
_ZEBRA_SOURCE=	zebra-0.92a.tar.gz
_RACOON_SOURCE=	racoon-20020507a.tar.gz


# individual directories to link to
_DIR1=	etc
_DIR1+=	bin/cat
_DIR1+=	bin/cp
_DIR1+=	bin/chmod
_DIR1+=	bin/csh
_DIR1+=	bin/date
_DIR1+=	bin/df
_DIR1+=	bin/echo
_DIR1+=	bin/ed
_DIR1+=	bin/expr
_DIR1+=	bin/hostname
_DIR1+=	bin/kill
_DIR1+=	bin/ln
_DIR1+=	bin/ls
_DIR1+=	bin/mkdir
_DIR1+=	bin/mv
_DIR1+=	bin/ps
_DIR1+=	bin/pwd
_DIR1+=	bin/rm
_DIR1+=	bin/sh
_DIR1+=	bin/sleep
_DIR1+=	bin/stty
_DIR1+=	bin/sync
_DIR1+=	bin/test
_DIR1+=	distrib/utils
_DIR1+=	distrib/i386/floppies
_DIR1+=	sbin/ifconfig
_DIR1+=	sbin/init
_DIR1+=	sbin/mount
_DIR1+=	sbin/mount_ffs
_DIR1+=	sbin/mount_kernfs
_DIR1+=	sbin/mount_msdos
_DIR1+=	sbin/umount
_DIR1+=	sbin/ping
_DIR1+=	sbin/ping6
_DIR1+=	sbin/route
_DIR1+=	sbin/routed
_DIR1+=	sbin/reboot
.ifdef IPSEC
_DIR1+=	sbin/setkey
.endif
_DIR1+=	sbin/shutdown
_DIR1+=	sbin/sysctl
_DIR1+=	usr.bin/basename
_DIR1+=	usr.bin/id
_DIR1+=	usr.bin/less
_DIR1+=	usr.bin/logger
_DIR1+=	usr.bin/sed
_DIR1+=	usr.bin/tput
_DIR1+=	usr.bin/tty
_DIR1+=	usr.bin/uname
_DIR1+=	usr.sbin/chown
_DIR1+=	usr.sbin/chroot
_DIR1+=	usr.sbin/dhcp
_DIR1+=	usr.sbin/ipf
_DIR1+=	usr.sbin/pppd
_DIR1+=	usr.sbin/route6d
_DIR1+=	usr.sbin/rtadvd
_DIR1+=	usr.sbin/rtsold
_DIR1+=	usr.sbin/syslogd
_DIR1+=	usr.sbin/traceroute6

_DIR1+=	lib/libpcap
.ifdef IPSEC
_DIR1+=	lib/libipsec
.endif

_DIR1+=	sys/netinet
.ifdef IPSEC
_DIR1+=	sys/netkey
.endif

_DIR2=	dist/ipf
.if exists (${BSDSRCDIR}/dist/dhcp/Makefile)
_DIR2+=	dist/dhcp
.endif
_DIR2+=	sys/conf
_DIR2+=	sys/arch/i386/stand


_FLOPPY_DIR=	${_BSDSRCDIR}/distrib/${ARCH}/floppies

_SYS_DIR=	${BSDSRCDIR}/sys



#
# floppy image parameters
#
INO_BYTES=	40960

IMAGE=		boot.fs
MOUNT_POINT=	${.CURDIR}/mnt
MODEL?=         natbox
_LISTS?=	model/${MODEL}/list
_CBIN?=		model/${MODEL}/ramdiskbin
_RC_CONF?=	model/${MODEL}/rc.conf

RAMDISK_S?=	ramdisk-small.fs
BIOSBOOT?=	biosboot.sym


#
# compiler pragma / attributes / options
#
WARNS=1

# conf/etc not conf/1.5/etc in obj.*
ETC_DIR?=	conf/etc

_ARGS?=		MOUNT_POINT=${MOUNT_POINT} \
		TOP=${_FLOPPY_DIR} \
		_SYS_DIR=${_SYS_DIR} \
		_CONF_DIR=${_CONF_DIR} \
		BSDSRCDIR=${_BSDSRCDIR} \
		CURDIR=${PWD} _ETC_DIR=${ETC_DIR} \
		MODEL=${MODEL} ARCH=${ARCH} \
		INO_BYTES=${INO_BYTES} \
		RAMDISK_S=${RAMDISK_S} \
		BIOSBOOT=${BIOSBOOT}

# import from pkgsrc/www/squid/Makefile
SQUID_CONFIGURE_ARGS?=  --enable-ipf-transparent \
                        --enable-underscores


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
	make -f Makefile.ramdisk ${_ARGS} build
	@ echo ""
	@ echo "build stage 2. make netbsd kernel for ${MODEL} model"
	@ echo ""
	make -f Makefile.kernel ${_ARGS} build
	@ echo ""
	@ echo "build stage for ${MODEL} model done."
	@ echo ""	

image:
	@ echo ""
	@ echo "image 1. make ramdisk-small.fs for ${MODEL} model"
	@ echo ""
	make -f Makefile.ramdisk ${_ARGS} image ||\
		(make -f Makefile.ramdisk ${_ARGS} unconfig ; exit 1)
	@ echo ""
	@ echo "image 2. inject mdsetimage into NetBSD Kernel for ${MODEL} model"
	@ echo ""
	make -f Makefile.kernel ${_ARGS} image
	@ echo ""
	@ echo "image 3. make a bootable floppy and inject netbsd into it for ${MODEL} model"
	@ echo ""
	make -f Makefile.bootfloppy ${_ARGS} image ||\
		(make -f Makefile.bootfloppy ${_ARGS} unconfig; exit 1)
	@ echo ""
	@ echo "building image for ${MODEL} model stage done."
	@ echo ""	


#
# initialization rules
#
INIT_RULES =	_init_mesg _init_dir  _lndir \
		_init_config \
		_init_model_pre_configure \
		_init_dhcp \
		_init_resolver

.if ${MODEL} == "adslrouter"
INIT_RULES +=	_init_pppoe
.endif

.if ${MODEL} == "proxybox"
INIT_RULES +=	_init_squid \
		_init_jftpgw \
		_init_stone
.endif

.if ${MODEL} == "bgprouter"
INIT_RULES +=	_init_zebra
.endif

.if ${MODEL} == "mcastrouter6"
INIT_RULES +=	_init_pim6sd
.endif

.if ${MODEL} == "vpn-racoon"
INIT_RULES +=	_init_racoon
.endif

INIT_RULES +=	_init_model_post_configure


######################################################
#
# initialize process before main build process
#
_prepare_build_system:	${INIT_RULES}

_init_mesg:
	@ echo "
	@ echo "0. prepations ... "
	@ echo "

_lndir:
	@ test -d ${_BSDSRCDIR} || mkdir -p ${_BSDSRCDIR}
	for DIR in ${_DIR1} ${_DIR2}; do \
	  test -d ${_BSDSRCDIR}/$$DIR || mkdir -p ${_BSDSRCDIR}/$$DIR; \
	  lndir ${BSDSRCDIR}/$$DIR ${_BSDSRCDIR}/$$DIR; \
	done
	(cd ${_BSDSRCDIR}/distrib; ln -s -f ${BSDSRCDIR}/distrib/Makefile.inc)
	(cd ${_BSDSRCDIR}/sbin; ln -s -f ${BSDSRCDIR}/sbin/Makefile.inc)
	(cd ${_BSDSRCDIR}; ln -s -f ${BSDSRCDIR}/Makefile.inc)
	for DIR in ${_DIR1} ; do \
	  (cd ${_BSDSRCDIR}/$$DIR; echo cleaning in $$DIR; make cleandir) \
	done
	@ touch ${_BSDSRCDIR}/.lndir_done

_init_dir:
	@ test -d work || mkdir -p work
	@ test -d work/etc || mkdir -p work/etc

_init_config:
	-rm -f dot.profile mtree.conf conf model
	ln ../${_CONF_DIR}/dot.profile .
	ln ../${_CONF_DIR}/mtree.conf  .
	-rm -f disktab.preinstall termcap.mini
	ln -s ../${_CONF_DIR} conf
	ln -s ../model model
	cp distrib/${ARCH}/floppies/disktab.preinstall disktab.preinstall
	cp distrib/${ARCH}/floppies/termcap.mini       termcap.mini
	-rm -f ramdiskbin.conf
	sed "s/__ARCH__/${ARCH}/g" ${_CBIN}.conf |\
	sh utils/fix_ramdiskbin.conf.sh > ramdiskbin.conf.new
	mv ramdiskbin.conf.new ramdiskbin.conf
	-rm -f list
	ln -s ${_LISTS} list
	-rm -f rc.conf
	ln -s ${_RC_CONF} rc.conf

_init_model_pre_configure:
	if [ -x model/${MODEL}/pre_configure ]; then \
		model/${MODEL}/pre_configure ;\
	fi

_init_model_post_configure:
	if [ -x model/${MODEL}/post_configure ]; then \
		model/${MODEL}/post_configure ;\
	fi

_init_pppoe:
	@ echo "";
	@ echo "make rp-pppoe for ADSL router";
	@ echo "";
	@ if [ ! -f ${STATUS_DIR}/.pppoe_done ];then \
	  ( cd ${GNU_DIR}/rp-pppoe/src;\
		./configure --prefix=${LOCALBASE} --sysconf=/etc/ppp ;\
		make ;\
	  );\
	  fi
	@ touch ${STATUS_DIR}/.pppoe_done

_init_squid:
	@ echo "";
	@ echo "make squid for proxyfw model";
	@ echo "";
	@ ${_FETCH_PROGRAM} ${_SQUID_SOURCE}
	@ ${_EXTRACT_PROGRAM} ${_SQUID_SOURCE}
	@ if [ ! -f ${STATUS_DIR}/.squid_done ];then \
	  ( cd ${PKG_DIR}/squid;\
		./configure --prefix=${LOCALBASE} \
		                --localstatedir=/var/squid \
		                ${SQUID_CONFIGURE_ARGS} ;\
		make ;\
	  );\
	  fi
	@ touch ${STATUS_DIR}/.squid_done

_init_jftpgw:
	@ echo "";
	@ echo "make jftpgw for proxyfw model";
	@ echo "";
	@ ${_FETCH_PROGRAM} ${_JFTPGW_SOURCE}
	@ ${_EXTRACT_PROGRAM} ${_JFTPGW_SOURCE}
	@ if [ ! -f ${STATUS_DIR}/.jftpgw_done ];then \
	  ( cd ${PKG_DIR}/jftpgw/support;\
		rm -f ipfilter.? ;\
		ln -s ../../../lib/libtransparent/ipfilter.? . ;\
	  );\
	  ( cd ${PKG_DIR}/jftpgw;\
		./configure --prefix=${LOCALBASE} --sysconf=/etc/jftpgw \
			--disable-libwrap ;\
		make ;\
	  );\
	  fi
	@ touch ${STATUS_DIR}/.jftpgw_done

_init_zebra:
	@ echo "";
	@ echo "make zebra for proxyfw model";
	@ echo "";
	@ ${_FETCH_PROGRAM} ${_ZEBRA_SOURCE}
	@ ${_EXTRACT_PROGRAM} ${_ZEBRA_SOURCE}
	@ if [ ! -f ${STATUS_DIR}/.zebra_done ];then \
	  ( cd ${PKG_DIR}/zebra;\
		./configure --prefix=${LOCALBASE} --sysconf=/etc/zebra ;\
		make ;\
	  );\
	  fi
	@ touch ${STATUS_DIR}/.zebra_done

_init_transproxy:
	@ echo "";
	@ echo "make transproxy for proxyfw model";
	@ echo "";
	@ if [ ! -f sbin/.transproxy_done ];then \
	  ( cd sbin/transproxy;\
		make ;\
	  );\
	  fi
	@ touch sbin/.transproxy_done

_init_stone:
	@ echo "";
	@ echo "make stone for proxyfw model";
	@ echo "";
	@ if [ ! -f ${STATUS_DIR}/.stone_done ];then \
	  ( cd ${GNU_DIR}/stone;\
		rm -f ipfilter.? ;\
		ln -s ../../lib/libtransparent/ipfilter.? . ;\
		cc  -DUSE_IPF -c -o stone.o stone.c ;\
		cc  -DUSE_IPF -c -o ipfilter.o ipfilter.c ;\
	  );\
	  fi
	@ touch ${STATUS_DIR}/.stone_done

_init_dhcp:
	(cd ${_BSDSRCDIR}/usr.sbin/dhcp;make)

_init_resolver:
	(cd utils/resolver/; make ${_ARGS})

_init_pim6sd:
	@ echo "";
	@ echo "make pim6sd for pim6sd model";
	@ echo "";
	@ if [ ! -f ${STATUS_DIR}/.pim6sd_done ];then \
		(cd usr.sbin/pim6sd/pim6sd/; make );\
	fi
	@ touch ${STATUS_DIR}/.pim6sd_done

_init_racoon:
	@ echo "";
	@ echo "make racoon for vpn-racoon model";
	@ echo "";
	@ ${_FETCH_PROGRAM} ${_RACOON_SOURCE}
	@ ${_EXTRACT_PROGRAM} ${_RACOON_SOURCE}
	@ if [ ! -f ${STATUS_DIR}/.racoon_done ];then \
	  ( cd ${PKG_DIR}/racoon/libipsec;\
		make OPSYS=NetBSD ;\
	  );\
	  ( cd ${PKG_DIR}/racoon/racoon;\
		./configure --prefix=${LOCALBASE} --sysconf=/etc/racoon ;\
		make ;\
	  );\
	  fi
	@ touch ${STATUS_DIR}/.racoon_done


#
# cleaning
#
clean cleandir:
	-make -f Makefile.ramdisk cleandir TOP=${_FLOPPY_DIR}
	-rm -f disktab.preinstall termcap.mini conf model
	-rm -f ramdiskbin.conf list rc.conf
	-rm -f boot *.fs boot.fs* netbsd* *.tmp *~
	-rm -f dot.profile mtree.conf

allclean: clean
	-rm -fr work
	-make -f Makefile.ramdisk unconfig TOP=${_FLOPPY_DIR}
