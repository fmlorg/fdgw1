#
# $FML$
#

TOP=		${.CURDIR}/..
WARNS=1

.include "${TOP}/Makefile.inc"


KERN=		${PWD}/netbsd 
IMAGE=		boot.fs
MOUNT_POINT=	/mnt


all: ${IMAGE}

${IMAGE}:
	ln ../ramdisk-small/disktab.preinstall disktab.preinstall
	ln ../ramdisk-small/termcap.mini       termcap.mini
	-make -f ../ramdisk-small/Makefile MOUNT_POINT=${MOUNT_POINT}
	-make -f Makefile.kernel netbsd MOUNT_POINT=${MOUNT_POINT}
	-make -f ../bootfloppy-common/Makefile.inc KERN=${KERN} IMAGE=${IMAGE} \
		MOUNT_POINT=${MOUNT_POINT}

clean cleandir:
	-make -f ../ramdisk-small/Makefile unconfig
	-make -f ../ramdisk-small/Makefile cleandir
	-rm -f disktab.preinstall termcap.mini
	-rm -f boot *.fs netbsd* *.tmp *~
