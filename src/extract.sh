#!/bin/sh
#
# $FML$
#

tmpdir=./trash/$$

cd gnu || exit 1

test -d $tmpdir || mkdir -p $tmpdir
mv squid* jftpgw* $tmpdir

for tgz in $*
do
	name=`basename $tgz .tar.gz`
	f=../distfiles/$tgz

	echo tar zxf $f
	eval tar zxf $f

	if expr $name : squid
	then
		mv squid* squid
	fi

	if expr $name : jftpgw
	then
		mv jftpgw* jftpgw
	fi
done

exit 0
