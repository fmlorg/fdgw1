#!/bin/sh
#
#  $FML: master.sh,v 1.3 2002/02/19 14:15:20 fukachan Exp $
#

prefix=/usr/pkg
exec_prefix=${prefix}
logdir=/var/squid
PATH=${exec_prefix}/sbin:/bin:/usr/bin:/sbin:/usr/sbin
export PATH

mode=$1

cd ${logdir}/logs || exit 1

while true
do
	/usr/pkg/sbin/squid -k rotate
	cat access.log.0 | logger -t squid.log
	rm -f *.log.?

	if [ "X$mode" = Xonce ];then
		exit 0;
	fi

	sleep 30
done

exit 0
