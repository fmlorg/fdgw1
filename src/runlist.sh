#	$NetBSD: runlist.sh,v 1.2 1996/10/09 00:13:39 jtc Exp $
#	$FML$

if [ "X$1" = "X-d" ]; then
	SHELLCMD=cat
	shift
else
	SHELLCMD="sh -e"
fi

( while [ "X$1" != "X" ]; do
	cat $1
	shift
done ) | awk -f list2sh.awk | ${SHELLCMD}
