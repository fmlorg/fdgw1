#!/bin/sh

set -x

dd if=boot.fs of=/dev/rfd0a bs=512

sync;sync;sync; sleep 3

mount /dev/fd0a /mnt
cp -p config/* /mnt
umount /mnt
