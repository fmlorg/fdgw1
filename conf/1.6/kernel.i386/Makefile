#
# Copyright (C) 2004 Ken'ichi Fukamachi <fukachan@fml.org>
#
# All rights reserved. This program is free software; you can
# redistribute it and/or modify it under the same terms as NetBSD itself.
#
# $FML: Makefile,v 1.6 2004/02/09 12:22:46 fukachan Exp $
#

all: GENERIC FDGW FDGW6 BRIDGE

GENERIC: /sys/arch/i386/conf/GENERIC
	cp -p /sys/arch/i386/conf/GENERIC .

FDGW:	convert.pl /sys/arch/i386/conf/INSTALL_LAPTOP
	perl convert.pl /sys/arch/i386/conf/INSTALL_LAPTOP |\
	sed 's/__FML__/\$$FML\$$/' > FDGW.new
	mv FDGW.new FDGW

FDGW6: FDGW
	perl -nple '/options\s*INET6/ && s/^\#//' FDGW > FDGW6.new
	mv FDGW6.new FDGW6

BRIDGE: FDGW
	cp FDGW BRIDGE
	echo ""                       >> BRIDGE
	echo "# enable bridge"        >> BRIDGE
	echo "pseudo-device   bridge" >> BRIDGE
