#
# $FML: Makefile,v 1.3 2001/08/14 21:42:51 fukachan Exp $
#

TOP=		${.CURDIR}/..
WARNS=1

.include "${TOP}/Makefile.inc"


IMAGE=		boot.fs
MOUNT_POINT=	/mnt


all: ${IMAGE}

${IMAGE}: _prepare
	@ echo ""
	@ echo "1. make file system on md0a (ramdisk-small.fs)"
	@ echo ""
	-make -f Makefile.ramdisk MOUNT_POINT=${MOUNT_POINT}
	@ echo ""
	@ echo "2. make netbsd kernel and mdsetimage on it"
	@ echo ""
	-make -f Makefile.kernel netbsd MOUNT_POINT=${MOUNT_POINT}
	@ echo ""
	@ echo "3. make a bootable floppy and install netbsd to it"
	@ echo ""
	-make -f Makefile.bootfloppy MOUNT_POINT=${MOUNT_POINT}
	@ echo ""
	@ echo "done."

_prepare:
	@ echo "0. prepations ... "
	rm -f disktab.preinstall termcap.mini
	ln ../ramdisk-small/disktab.preinstall disktab.preinstall
	ln ../ramdisk-small/termcap.mini       termcap.mini

clean cleandir:
	-make -f ../ramdisk-small/Makefile unconfig
	-make -f ../ramdisk-small/Makefile cleandir
	-rm -f disktab.preinstall termcap.mini
	-rm -f boot *.fs boot.fs* netbsd* *.tmp *~
