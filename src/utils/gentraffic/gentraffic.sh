#!/bin/sh
#
# $FML$
#

ttcp_mode=-u
ttcp_target=127.0.0.1

echo ttcp -t $ttcp_mode $ttcp_target

(

   while true
   do
        cat /bin/cat
   done

) | ttcp -t $ttcp_mode $ttcp_target

exit 0
