#!/bin/sh
#
# $FML: prepare_workdir.sh,v 1.2 2001/12/16 04:04:45 fukachan Exp $
#

# alloc netbsd source area, which is shared among src.$model
test -d src/NetBSD || mkdir -p src/NetBSD

# symlink(2)
for model in $*
do
	test -d src.$model || mkdir src.$model

	cd src.$model || exit 1

	for x in ../src/[MN]* ../src/[a-z]*
	do
	   if [ -f $x -o -d $x ];then
		if [ ! -h `basename $x` ];then
		   ln -s $x .
		fi
	   fi
	done
done

exit 0;
