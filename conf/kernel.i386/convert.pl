#!/usr/bin/env perl
#
# $FML: convert.pl,v 1.1.1.1 2001/09/06 03:30:25 fukachan Exp $
#

use strict;
use FileHandle;

my $fh = new FileHandle "../../../../../sys/arch/i386/conf/INSTALL_LAPTOP";

if (defined $fh) {
    print "#\n";
    print "# \$FML\$\n";
    print "#\n";
    print "# This file is derived from INSTALL_LAPTOP in NetBSD 1.5 stable branch.\n";
    print "#\n\n";

    while (<$fh>) {
	if (/^\#/) {	
	    if (/pseudo-device\s*ipfilter/ ||
		/options\s*GATEWAY/
		) {
		s/^\#\s*//;
	    }
	}
	else {
	    if (/COMPAT_1[0123]/ ||
		/DDB/            || 
		/atapibus/ 	 ||
		/[uo]hci/ 	 ||	# USB HUB
		/uhub./          ||	# USB HUB
		/ukbd./          ||	# USB HUB
		/umass./         ||	# USB HUB
		/isapnp./        ||
		/pseudo-device\s*sl\s*/ || # SLIP
		/file-system\s*(EXT2FS|MFS|NFS|NTFS|CD9660)/
		) {
		$_ = "\#". $_;
	    }
	}


	print;
    }
}

exit 0;

