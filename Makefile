#
# $FML: Makefile,v 1.10 2001/09/05 23:52:31 fukachan Exp $
#

MODEL?=         natbox

all:	build

build:
	(cd src;make MODEL=${MODEL} )

clean cleandir:
	(cd src; make clean )
