/* 
 * Copyright (C) 1999-2001 Joachim Wieland <joe@mcknight.de>
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111, USA.
 */

#include <sys/types.h>
#include <netdb.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include "jftpgw.h"
#ifdef HAVE_LINUX_NETFILTER_IPV4_H
#include <linux/netfilter_ipv4.h>
#endif
#ifdef HAVE_NETINET_IP_FIL_H
#include "support/ipfilter.c"
#endif

extern struct hostent_list* hostcache;


static
int getanylocalport(int sd, struct sockaddr_in* sin, int needroot) {
	int ret;

	sin->sin_port = INPORT_ANY;
	if (needroot
		&& changeid(PRIV, UID, "Changing ( bind() )") < 0) {
		return -1;
	}
	ret = bind(sd, (struct sockaddr*) sin, sizeof(struct sockaddr));
	if (ret < 0 && errno == EADDRNOTAVAIL
			&& sin->sin_addr.s_addr != INADDR_ANY) {
		sin->sin_addr.s_addr = INADDR_ANY;
		log(6, "Couldn't assign requested address, trying any address");
		ret = bind(sd, (struct sockaddr*) sin,
						sizeof(struct sockaddr));
	}
	if (needroot
		&& changeid(UNPRIV, EUID, "Changing back ( bind() )") < 0) {
		return -1;
	}
	if (ret < 0) {
		log(2, "Could not bind to a free port: %s",
				strerror(errno));
		return -1;
	} else {
		return sd;
	}
}

static
int getportinrange(int sd, struct sockaddr_in* sin,
					const struct portrangestruct* prs) {

	const struct portrangestruct *prscur;
	/* the following part is mostly from the proftpd patch by TJ
	 * Saunders <tj@digisle.net> - 10/14/00 */

	unsigned int found_pasv_port = 0;
	int pasv_range_len, pasv_port_index;
	int *pasv_range, *pasv_ports;
	int attempt, random_index;
	int tries = 0;
	int ret;

	/* hack up wu-ftpd's implementation of this to feature work here.  What
	 * can I say?  I'm a plagiarist and a hack to the Nth degree, and feel
	 * little shame about it, as long as it satisfies proftpd users'
	 * needs. Credits go to kinch, I think...that's the name in the wu-ftpd
	 * source code. -- TJ
	 */

	if (!prs) {
		return getanylocalport(sd, sin, 0);
	}

	pasv_range_len = config_count_portrange(prs);

	pasv_range = (int *) malloc(pasv_range_len * sizeof(int));
	pasv_ports = (int *) malloc((pasv_range_len + 1) * sizeof(int));

	/* populate the array with all the port numbers in the configured
	 * range.
	 */
	pasv_port_index = pasv_range_len;
	prscur = prs;
	do {
		unsigned int inner_index;
		if (!prscur) {
			log(2, "prscur was NIL in %s, %d", __FILE__, __LINE__);
			break;
		}
		inner_index = prscur->endport + 1;
		do {
			/* the first port that is registered is endport, the
			 * last one is startport */
			inner_index--;
			pasv_port_index--;
			pasv_range[pasv_port_index] = inner_index;
		} while (inner_index > prscur->startport);
		prscur = prscur->next;
	} while (pasv_port_index > 0);

	/* randomly choose a port from within the range, and call
	 * inet_create_connection().  If that call fails, try a different
	 * port, until all in the range have been tried.
	 */
	for (attempt = 3; attempt > 0 && (!found_pasv_port); attempt--) {
		for (pasv_port_index = pasv_range_len; pasv_port_index > 0 &&
			(!found_pasv_port); pasv_port_index--) {

			/* if this is the first attempt through the passive
			 * ports range, randomize the order of the port
			 * numbers used (eg no linear probing), and store
			 * this random order into the pasv_ports array, to
			 * be attempted again on the next two runs. -- TJ
			 */
			if (attempt == 3) {

				/* obtain a random index into the port range
				 * array
				 */
				random_index = (int) ((1.0 * pasv_port_index
					* rand()) / (RAND_MAX + 1.0));

				/* copy the port at that index into the
				 * array from which port numbers will be
				 * selected for passing into
				 * inet_create_connections()
				 */
				pasv_ports[pasv_port_index] =
					pasv_range[random_index];

				/* now, alter the order of the port numbers
				 * in the pasv_range array by moving the
				 * non-selected numbers down, so that the
				 * next randomly chosen port number will be
				 * from the range of as-yet unchosen ports.
				 * -- TJ
				 */
				while (++random_index < pasv_port_index) {
					pasv_range[random_index - 1] =
						pasv_range[random_index];
				}
			}
			sin->sin_port = htons(pasv_ports[pasv_port_index]);
			if (changeid(PRIV, UID,
					"Changing (bind to port)") < 0) {
				return -1;
			}
			ret = bind(sd, (struct sockaddr*) sin,
						sizeof(struct sockaddr));
			if (changeid(UNPRIV, EUID,
					"Changing back (bind to port)") < 0) {
				return -1;
			}
			if (ret < 0) {
				log(9, "Tried port %s:%d in vain: %s",
						inet_ntoa(sin->sin_addr),
						pasv_ports[pasv_port_index],
						strerror(errno));
				tries++;
			} else {
				found_pasv_port = 1;
				log(8, "Found free port %d after %d tries",
						pasv_ports[pasv_port_index],
						tries);
			}
		}
	}

	free(pasv_range);
	free(pasv_ports);

	if (!found_pasv_port) {
		/* if not able to find an open port in the given range,
		 * default to normal proftpd behavior (using INPORT_ANY),
		 * and log the failure -- symptom of a too-small port range
		 * configuration.  -- TJ
		 */

		log(4, "unable to find a free port in the port range"
		"; defaulting to INPORT_ANY");
		return getanylocalport(sd, sin, 1);
	}

	return sd;
}


static
int openport(struct sockaddr_in sin,
	     unsigned long int local_address,
	     const struct portrangestruct* localportrange) {

	int handle;
	int one = 1;
	struct sockaddr_in dp;

	/* Try to create the socket as root - Solaris can only bind to a
	 * privileged port if the socket belongs to root as well. We don't
	 * know if our port in the range will be a privileged one */
	if (localportrange
		&& changeid(PRIV, UID, "Changing (creating socket)") < 0) {
		return -1;
	}
	if ((handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		log(2, "Could not create socket in openport(): %s",
							strerror(errno));
		return -1;
	}
	if (setsockopt(handle, SOL_SOCKET, SO_REUSEADDR,
					(void*)&one, sizeof(one)) < 0) {
		log(3, "Could not set SO_REUSEADDR on socket in openport()"
				": %s", strerror(errno));
		/* do not return */
	}
	if (localportrange
		&& changeid(UNPRIV, EUID, "Changing back (creating socket)") < 0) {
		return -1;
	}
	if (handle < 0) {
		log(2, "Error opening the socket: %s", strerror(errno));
		return -1;
	}

	memset((void*)&dp, 0, sizeof(dp));
	dp.sin_family = AF_INET;
	dp.sin_addr.s_addr = local_address;

	/* will default to getanylocalport if localportrange is not defined
	 * */
	handle = getportinrange(handle, &dp, localportrange);

	if (connect(handle, (struct sockaddr*) &sin, sizeof(sin)) < 0) {
		int err = errno;
		log(1, "Error connecting to %s:%d: %s",
						inet_ntoa(sin.sin_addr),
						ntohs(sin.sin_port),
						strerror(err));
		errno = err;
		set_errstr(strerror(err));
		return -1;
	}

	return handle;
}

int openportiaddr(unsigned long addr,
		  unsigned int port,
		  unsigned long int local_address,
		  const struct portrangestruct* localportrange) {

	struct sockaddr_in sin;
	memset((void*)&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = addr;
	sin.sin_port = htons(port);
	return openport(sin, local_address, localportrange);
}

int openportname(const char* hostname,
		 unsigned int port,
		 unsigned long int local_address,
		 const struct portrangestruct* localportrange) {

	unsigned long int host_ip;
	struct sockaddr_in sin;

	if (local_address != INADDR_ANY) {
		log(8, "Using special address %s to connect to %s "
				"on port %d",
			inet_ntoa(*((struct in_addr*)&local_address)),
			hostname, port);
	}

	memset((void*)&sin, 0, sizeof(sin));
	host_ip = hostent_get_ip(&hostcache, hostname);
	if (host_ip == (unsigned long int) UINT_MAX) {
		log(3, "Could not look up %s", hostname);
		return -1;
	}
	sin.sin_family         = AF_INET;
	sin.sin_addr.s_addr    = host_ip;
	sin.sin_port           = htons(port);
	return openport(sin, local_address, localportrange);
}


/* openlocalport() binds to a free port on the own machine */

int openlocalport(struct sockaddr_in *sin,           /* return */
		  unsigned long int local_addr,      /* src address */
		  struct portrangestruct* prs) {     /* src ports */
#ifdef HAVE_SOCKLEN_T
	socklen_t slen;
#else
	int slen;
#endif
	int sd;
	int one = 1;

	if ((sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		log(1, "Could not create socket to bind to a free port: %s",
				strerror(errno));
		return -1;
	}
	if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,
				(void*)&one, sizeof(one)) < 0) {
		log(3, "Could not set SO_REUSEADDR in open_localport: %s",
							strerror(errno));
		/* do not return */
	}
	memset((void*)sin, 0, sizeof(*sin));

	sin->sin_addr.s_addr = local_addr;
	sin->sin_family = AF_INET;

	if(getportinrange(sd, sin, prs) < 0) {
		log(2, "Could not bind to a local port: %s", strerror(errno));
		return -1;
	}
	if (listen(sd, 1) < 0) {
		log(2, "Could not listen on a free port: %s", strerror(errno));
		return -1;
	}

	/* re-read the socket data to get the actual port */

	slen = sizeof(struct sockaddr);
	if (getsockname(sd, (struct sockaddr*) sin, &slen) < 0) {
		log(2, "getsockname failed after binding to a free port: %s",
				strerror(errno));
		return -1;
	}

	return sd;
}



/* nf_getsockname() - netfilter SO_ORIGINAL_DST variant of getsockopt()
 *
 * Within the new Linux netfilter framework, NAT functionality is cleanly
 * separated from the TCP/IP core processing. In old days, you could easily
 * retrieve the original destination (IP address and port) of a transparently
 * proxied connection by calling the normal getsockname() syscall.
 * With netfilter, getsockname() returns the real local IP address and port.
 * However, the netfilter code gives all TCP sockets a new socket option,
 * SO_ORIGINAL_DST, for retrieval of the original IP/port combination.
 *
 * This file implements a function nf_getsockname(), with the same calling
 * convention as getsockname() itself; it uses SO_ORIGINAL_DST, and if that
 * fails, falls back to using getsockname() itself.
 *
 * Public domain by Patrick Schaaf <bof@bof.de>
 */

int nf_getsockname(int fd, struct sockaddr *sa,
#ifdef HAVE_SOCKLEN_T
	socklen_t* salen
#else
	int* salen
#endif
	)
{
	if (*salen != sizeof(struct sockaddr_in)) {
		errno = EINVAL;
		return -1;
	}
#ifdef SO_ORIGINAL_DST
	if (0 == getsockopt(fd, SOL_IP, SO_ORIGINAL_DST, sa, salen)) {
		return 0;
	}
#endif
	return getsockname(fd, sa, salen);
}

#define SHOW_ORIGINAL_IP		0
#define SHOW_NOT_ORIGINAL_IP		1
static
struct sockaddr_in get_showaddr(int shandle, int type) {
	struct sockaddr_in sin;
	int i;
#ifdef HAVE_SOCKLEN_T
	socklen_t slen;
#else
	int slen;
#endif
	slen = sizeof (sin);
	if (type == SHOW_ORIGINAL_IP) {
#ifdef HAVE_NETINET_IP_FIL_H
		if (changeid(PRIV, UID, "ipfilter") < 0) {
			log(2, "failed to gain privileges (ipnat device)");
			i = -1;
		} else {
			i = ipfilter_get_real_dst(shandle, &sin) ? 0 : -1;
		}
		if (changeid(UNPRIV, EUID, "ipfilter") < 0) {
			log(2, "fail to drop privileges (ipnat device)");
			i = -1;
		}
#else
		i = nf_getsockname(shandle, (struct sockaddr*) &sin, &slen);
#endif
	} else {
		i = getsockname(shandle, (struct sockaddr*) &sin, &slen);
	}
	if (i != 0) {
		log(2, "getsockname failed. Can't get the IP of the interface");
		sin.sin_addr.s_addr = -1;
		sin.sin_port = 0;
		return sin;
	}
	return sin;
}


#ifdef HAVE_ICMP_SUPPORT
/* This is from the netkit-ping package */
static int in_cksum(u_short *addr, int len) {
	register int nleft = len;
	register u_short *w = addr;
	register int sum = 0;
	u_short answer = 0;

	/*
	 * Our algorithm is simple, using a 32 bit accumulator (sum), we add
	 * sequential 16 bit words to it, and at the end, fold back all the
	 * carry bits from the top 16 bits into the lower 16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1) {
		*(u_char *)(&answer) = *(u_char *)w ;
		sum += answer;
	}

	/* add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return(answer);
}
#endif


#define ICMP_SIZE 8
#define IP_SIZE (64 + 8)

static
unsigned long int get_local_addr_by_sending_icmp(unsigned long int to_addr) {
#ifdef HAVE_ICMP_SUPPORT
	char icmp_packet[ ICMP_SIZE ];
	char ip_packet[ IP_SIZE ];
	struct icmp* icp = (struct icmp*) icmp_packet;
	struct ip* ip = (struct ip*) ip_packet;
	struct protoent *proto;
	struct sockaddr_in sin;
	int i, s, len;
	unsigned int ident = getpid() & 0xFFFF;

	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_port = 0;
	sin.sin_addr.s_addr = to_addr;

	if (!(proto = getprotobyname("icmp"))) {
		log(4, "protocol icmp unknown");
		return ULONG_MAX;
	}
	if ((s = socket(AF_INET, SOCK_RAW, proto->p_proto)) < 0) {
		log(4, "Error creating the socket: %s", strerror(errno));
		return ULONG_MAX;
	}

	icp->icmp_type = ICMP_ECHO;
	icp->icmp_code = 0;
	icp->icmp_cksum = 0;

	icp->icmp_hun.ih_idseq.icd_id = ident;
	icp->icmp_hun.ih_idseq.icd_seq = 0;

	icp->icmp_cksum = in_cksum((u_short *) icp, ICMP_SIZE);

	i = sendto(s, (char *)icmp_packet, ICMP_SIZE,
			0, (struct sockaddr*) &sin, sizeof(struct sockaddr));
	if (i < 0) {
		log(4, "Error in sendto: %s", strerror(errno));
		close(s);
	}

	/* Give the packet a time of 5 seconds to come in */
	alarm(5);

	/* Now receive the packet again */
	do {
		i = sizeof(struct sockaddr);
		len = recvfrom(s, (char*) ip_packet, IP_SIZE,  0,
						(struct sockaddr*) &sin, &i);
		if (len < 0) {
			if (errno == EINTR) {
				/* the answer did not come back after 5
				 * seconds */
				log(4, "the ICMP Echo Reply did not come "
						"back within 5 seconds");
				close(s);
				return ULONG_MAX;
			}
			log(4, "Error in recvfrom: %s", strerror(errno));
		}

		/* multiply header length by 32 bits, i.e. 4 Bytes
		 * to reach the start of the ICMP header */
		icp = (struct icmp*) &ip_packet [(ip->ip_hl * 4)];
	} while (icp->icmp_hun.ih_idseq.icd_id != ident);

	alarm(0);
	close(s);
	return ip->ip_dst.s_addr;
#else
	return ULONG_MAX;
#endif
}

static
unsigned long int get_local_addr_by_sending_udp(unsigned long int to_addr,
							unsigned int port) {
	unsigned int i;
	int sd;
	struct sockaddr_in sin;

	if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		log(3, "Cannot create socket to send UDP packet to the transparent proxy client");
		return ULONG_MAX;
	}
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = to_addr;
	if (connect(sd, (struct sockaddr*) &sin, sizeof(sin)) < 0) {
		log(5, "Cannot connect to the transparent proxy client");
		close(sd);
		return ULONG_MAX;
	}
	if (send(sd, "jftpgw", 6, 0) < 0) {
		log(5, "Cannot send UDP packet to the transparent proxy client");
		close(sd);
		return ULONG_MAX;
	}
	i = sizeof(sin);
	if (getsockname(sd, (struct sockaddr*) &sin, &i) < 0) {
		log(2, "getsockname() failed to determine our IP (transparent proxy -> udp)");
		close(sd);
		return ULONG_MAX;
	}
	close(sd);
	return sin.sin_addr.s_addr;
}


static
unsigned long int get_local_addr_by_sending(unsigned long int to_addr) {
	const char* opt = config_get_option("getinternalip");
	unsigned long int addr;
	int changedid = 0;

	if (strcasecmp(opt, "icmp") == 0) {
		if (geteuid() != 0 && getuid() == 0) {
			if (changeid(PRIV, UID, "Sending ICMP") < 0) {
				log(4, "Trying a UDP packet");
				goto try_udp;
			} else {
				changedid = 1;
			}
		} else if (getuid() != 0) {
			log(4, "ICMP can only be sent by root, trying a UDP packet");
			goto try_udp;
		}
		addr = get_local_addr_by_sending_icmp(to_addr);
		if (addr == ULONG_MAX) {
			log(4, "Sending an ICMP packet failed, "
						"trying a UDP packet");
			goto try_udp;
		}
		if (changedid) {
			if (changeid(UNPRIV, EUID, "Sending ICMP") < 0) {
				return ULONG_MAX;
			}
		}
		log(9, "Getting IP by ICMP successful");
		return addr;
	}
try_udp:
	return get_local_addr_by_sending_udp(to_addr,
					config_get_ioption("udpporrt", 21));
}


unsigned long int socketinfo_get_local_addr_by_sending(int fd) {
	struct sockaddr_in sin;
	unsigned int ret;
#ifdef HAVE_SOCKLEN_T
	socklen_t namelen;
#else
	int namelen;
#endif
	namelen = sizeof(sin);

	/* Determine the IP the client sees of us */
	if (getpeername(fd, (struct sockaddr *) &sin, &namelen) != 0) {
		log(3, "Could not get peername for TP: %s", strerror(errno));
	}
	ret = get_local_addr_by_sending(sin.sin_addr.s_addr);


	/* debug */
	sin.sin_addr.s_addr = ret;
	log(8, "Our IP to the client is %s", inet_ntoa(sin.sin_addr));

	return ret;
}

struct sockaddr_in socketinfo_get_transparent_target_sin(int fd) {
	struct sockaddr_in sin;
	sin = get_showaddr(fd, SHOW_ORIGINAL_IP);
	return sin;
}

char* socketinfo_get_transparent_target_char(int fd) {
	struct sockaddr_in sin;
	char *ipstr;
	const int IPBUFSIZE = 16;
	const int PORTSIZE = 5;

	sin = socketinfo_get_transparent_target_sin(fd);
	ipstr = (char*) malloc(IPBUFSIZE + 1 + PORTSIZE + 1);
	enough_mem(ipstr);

	snprintf(ipstr, IPBUFSIZE + 1 + PORTSIZE, "%s:%d", inet_ntoa(
					*(struct in_addr*) &sin.sin_addr),
					htons(sin.sin_port));
	return ipstr;
}


struct sockaddr_in socketinfo_get_local_sin(int fd) {
	struct sockaddr_in sin;

	sin = get_showaddr(fd, SHOW_NOT_ORIGINAL_IP);
	return sin;
}

unsigned long int socketinfo_get_local_ip(int fd) {
	struct sockaddr_in sin;

	sin = socketinfo_get_local_sin(fd);
	return sin.sin_addr.s_addr;
}

unsigned int socketinfo_get_local_port(int fd) {
	struct sockaddr_in sin;

	sin = socketinfo_get_local_sin(fd);
	return ntohs(sin.sin_port);
}

