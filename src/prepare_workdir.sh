#!/bin/sh
#
# $FML$
#

for model in $*
do
	test -d src.$model || mkdir src.$model

	cd src.$model || exit 1

	for x in ../src/M* ../src/[a-z]*
	do
	   if [ -f $x -o -d $x ];then
		   ln -s $x .
	   fi
	done
done

exit 0;
