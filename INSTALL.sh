#!/bin/sh

set -x

dd if=boot.fs of=/dev/rfd0a bs=18k

mount /dev/fd0a /mnt
test -d /mnt/etc || mkdir /mnt/etc
cp -pr config/[a-z0-9]*[a-z0-9] /mnt/etc
umount /mnt
