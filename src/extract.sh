#!/bin/sh
#
# $FML: extract.sh,v 1.2 2002/04/18 01:54:06 fukachan Exp $
#

tmpdir=./trash/$$

cd gnu || exit 1

test -d $tmpdir || mkdir -p $tmpdir

for tgz in $*
do
	name=`basename $tgz .tar.gz`
	f=../distfiles/$tgz

	if [ -f .extract_${name}_done ];then
		echo "ignore ${name} building (.extract_${name}_done)"
		sleep 3
		continue
	fi

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

	touch .extract_${name}_done
done

exit 0
