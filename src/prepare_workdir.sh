#!/bin/sh
#
# $FML: prepare_workdir.sh,v 1.1 2001/12/15 16:29:41 fukachan Exp $
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
		   ln -s $x .
	   fi
	done
done

exit 0;
