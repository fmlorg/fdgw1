#
# $FML: Makefile,v 1.7 2001/08/15 10:03:35 fukachan Exp $
#

ARCH?=		`uname -p`
_TOP=		/usr/src/distrib/${ARCH}/floppies
WARNS=1

IMAGE=		boot.fs
MOUNT_POINT=	/mnt
MODEL?=         natbox.basic
_LISTS?=	conf/${MODEL}/list
_CBIN?=		conf/${MODEL}/ramdiskbin

all: ${IMAGE}

${IMAGE}: _prepare
	@ echo ""
	@ echo "1. make file system on md0a (ramdisk-small.fs)"
	@ echo ""
	-make -f Makefile.ramdisk MOUNT_POINT=${MOUNT_POINT} TOP=${_TOP}
	@ echo ""
	@ echo "2. make netbsd kernel and mdsetimage on it"
	@ echo ""
	-make -f Makefile.kernel netbsd MOUNT_POINT=${MOUNT_POINT} TOP=${_TOP}
	@ echo ""
	@ echo "3. make a bootable floppy and install netbsd to it"
	@ echo ""
	-make -f Makefile.bootfloppy MOUNT_POINT=${MOUNT_POINT} TOP=${_TOP}
	@ echo ""
	@ echo "done."

_prepare:
	@ echo "0. prepations ... "
	-rm -f disktab.preinstall termcap.mini
	cp ${_TOP}/ramdisk-small/disktab.preinstall disktab.preinstall
	cp ${_TOP}/ramdisk-small/termcap.mini       termcap.mini
	-rm -f ramdiskbin.conf
	ln -s ${_CBIN}.conf ramdiskbin.conf
	-rm -f list
	ln -s ${_LISTS} list
	if [ -x conf/${MODEL}/configure ]; then conf/${MODEL}/configure ; fi

clean cleandir:
	-make -f Makefile.ramdisk unconfig TOP=${_TOP}
	-make -f Makefile.ramdisk cleandir TOP=${_TOP}
	-rm -f disktab.preinstall termcap.mini
	-rm -f ramdiskbin.conf list
	-rm -f boot *.fs boot.fs* netbsd* *.tmp *~
