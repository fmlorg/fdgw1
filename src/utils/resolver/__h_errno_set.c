#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "minires/minires.h"
#include "arpa/nameser.h"

extern int h_errno;

void
__h_errno_set(struct __res_state *res, int err) {

	h_errno = res->res_h_errno = err;
}
