#
# $FML: Makefile,v 1.9 2001/08/15 10:34:21 fukachan Exp $
#

MODEL?=         natbox.basic

all:	build

build:
	(cd src;make MODEL=${MODEL} )

clean cleandir:
	(cd src; make clean )
