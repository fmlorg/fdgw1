#!/bin/sh
#
# $FML$
#

perl -nle 'print unless /EXAMPLE:|Note:|Default:|^\#\s{3,}|^\#\t|^\#\s*$/' squid.conf.example |\
uniq > squid.conf
