#!/usr/bin/env perl
#
# $FML$
#

use strict;
use FileHandle;

my $fh = new FileHandle "../../../../../sys/arch/i386/conf/INSTALL_LAPTOP";

if (defined $fh) {
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
		/file-system\s*(EXT2FS|MFS|NFS|NTFS|CD9660)/
		) {
		$_ = "\#". $_;
	    }
	}


	print;
    }
}

exit 0;

