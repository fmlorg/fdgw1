#!/bin/sh
#
# $FML: fix_ramdiskbin.conf.sh,v 1.1 2001/12/16 09:14:52 fukachan Exp $
#

cat |(
   if [ ! -d NetBSD/usr.sbin/dhcp/dst ];then
	echo '# FYI: fix to exclude usr.sbin/dhcp/dst/' 1>&2
	sed -e 's@ \${BSDSRCDIR}/usr.sbin/dhcp/dst/\*.o@@'
   else
	cat
   fi
)

exit 0
