#!/bin/sh
#
#  $FML: master.sh,v 1.2 2002/02/18 13:44:48 fukachan Exp $
#

prefix=/usr/pkg
exec_prefix=${prefix}
logdir=/var/squid
PATH=${exec_prefix}/sbin:/bin:/usr/bin:/sbin:/usr/sbin
export PATH

conf=""
if test "$1" ; then
	conf="-f $1"
	shift
fi


mkdir -p ${logdir}
mkdir -p ${logdir}/cache
mkdir -p ${logdir}/logs
mkdir -p ${logdir}/errors
cp /dev/null ${logdir}/logs/access.log
cp /dev/null ${logdir}/logs/cache.log
cp /dev/null ${logdir}/logs/store.log
chown -R nobody ${logdir}


failcount=0
while : ; do
	echo "Running: squid -Y $conf >> $logdir/squid.out 2>&1"
	echo "Startup: `date`" | logger -t squid
	chown -R nobody ${logdir}
	squid -z
	squid -NsY $conf | logger -t squid
	sleep 10
done
