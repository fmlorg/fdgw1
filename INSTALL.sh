#!/bin/sh
#
# $FML$
#

set -x
dd if=boot.fs of=/dev/rfd0a bs=18k
exit 0;
