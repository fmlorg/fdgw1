#!/usr/bin/env perl
#
# $FML$
#

while (<>) {

	s!^\# cache_dir.*!cache_dir ufs /var/squid/cache 1 4 4!;
	s!^\# cache_mem.*!cache_mem 1 MB!;
	s!^\# cache_swap_high.*!cache_swap_high 50!;
	s!^\# cache_swap_low.*!cache_swap_low  30!;
	s!^\# http_port.*!http_port 127.0.0.1:3128!;
	s!^\# httpd_accel_host.*!httpd_accel_host virtual!;
	s!^\# httpd_accel_port.*!httpd_accel_port 80!;
	s!^\# httpd_accel_uses_host_header.*!httpd_accel_uses_host_header on!;
	s!^\# httpd_accel_with_proxy.*!httpd_accel_with_proxy on!;
	s!^\# maximum_object_size.*!maximum_object_size 1 KB!;


	print $_;
}

exit 0;
