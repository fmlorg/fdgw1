#!/bin/sh
#
# $FML$
#

cat |(
   if [ ! -d NetBSD/usr.sbin/dhcp/dst ];then
	echo '# FYI: fix to exclude usr.sbin/dhcp/dst/' 1>&2
	sed -e 's@ \${BSDSRCDIR}/usr.sbin/dhcp/dst/\*.o@@'
   fi
)

exit 0
