#!/bin/sh
#
# $FML: shrink.sh,v 1.1 2002/02/12 12:44:11 fukachan Exp $
#

perl convert.pl squid.conf.example |\
perl -nle 'print unless /EXAMPLE:|Note:|Default:|^\#\s{3,}|^\#\t|^\#\s*$/' |\
uniq > squid.conf
