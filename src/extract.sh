#!/bin/sh
#
# $FML: extract.sh,v 1.1 2002/02/21 11:00:47 fukachan Exp $
#

tmpdir=./trash/$$

cd gnu || exit 1

test -d $tmpdir || mkdir -p $tmpdir
mv squid* jftpgw* $tmpdir >/dev/null 2>&1

for tgz in $*
do
	name=`basename $tgz .tar.gz`
	f=../distfiles/$tgz

	echo Extracting $tgz

	checksum0=`grep $tgz distinfo |awk '{print $4}'`
	checksum1=`digest sha1 $f |awk '{print $4}'`
	if [ "X$checksum0" = "X$checksum1" ];then
		echo "Checksum ok"
	else
		echo "Error: wrong checksum"
		exit 1		
	fi

	echo tar zxf $f
	eval tar zxf $f

	if expr $name : squid
	then
		mv squid* squid || exit 1
	fi

	if expr $name : jftpgw
	then
		mv jftpgw* jftpgw || exit 1
	fi
done

exit 0
