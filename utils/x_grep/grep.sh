#!/bin/sh
#
# $FML$
#

pat=$1
shift

sed -n "/$pat/p" $*
