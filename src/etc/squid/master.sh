#!/bin/sh
#
#  $FML$
#

prefix=/usr/pkg
exec_prefix=${prefix}
logdir=/var/squid
PATH=${exec_prefix}/sbin:/bin:/usr/bin
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
touch ${logdir}/logs/access.log
touch ${logdir}/logs/cache.log
touch ${logdir}/logs/store.log
chown -R nobody ${logdir}


failcount=0
while : ; do
	echo "Running: squid -Y $conf >> $logdir/squid.out 2>&1"
	echo "Startup: `date`" | logger -t squid
	squid -z
	squid -NY $conf | logger -t squid
	sleep 10
done
