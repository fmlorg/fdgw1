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

#include <ctype.h>
#include <sys/stat.h>
#include "jftpgw.h"

#ifdef HAVE_LIBWRAP
#include <syslog.h>
#include <tcpd.h>
#ifndef LIBWRAP_ALLOW_FACILITY
#	define LIBWRAP_ALLOW_FACILITY LOG_AUTH
#endif
#ifndef LIBWRAP_ALLOW_SEVERITY
#	define LIBWRAP_ALLOW_SEVERITY LOG_INFO
#endif
#ifndef LIBWRAP_DENY_FACILITY
#	define LIBWRAP_DENY_FACILITY LOG_AUTH
#endif
#ifndef LIBWRAP_DENY_SEVERITY
#	define LIBWRAP_DENY_SEVERITY LOG_WARNING
#endif
int allow_severity = LIBWRAP_ALLOW_FACILITY | LIBWRAP_ALLOW_SEVERITY;
int deny_severity  = LIBWRAP_DENY_FACILITY  | LIBWRAP_DENY_SEVERITY;
#endif

extern struct hostent_list* hostcache;
extern struct serverinfo srvinfo;

int chlds_exited;
int should_read_config;

struct descriptor_set {
	fd_set set;
	int maxfd;
};

static int child_setup(int, struct clientinfo*);
static struct descriptor_set listen_on_ifaces(const char*, struct clientinfo*);
static int get_connecting_socket(struct descriptor_set);
static int say_welcome(int);
static char* prependcode(const char* s, int code);

/* bindport binds to the specified PORT on HOSTNAME (which may also be a
 * dot-notation IP and returns the socket descriptor
 *
 * Parameters: hostname & port: Where to bind
 *
 * Return value: The socket descriptor of the bound socket
 *
 * Called by: waitclient
 *
 * */

int bindport(const char *hostname, int port) {
	unsigned long inetaddr =1;
	int shandle;
	int one = 1;
	struct sockaddr_in sin;
	unsigned long host_ip;

	memset((void*)&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	inetaddr = inet_addr(hostname);
	if (inetaddr == (unsigned long int) UINT_MAX) {
		/* see if HOSTNAME is an interface */
		if (get_interface_ip(hostname, &sin) == 0) {
			/* found it */
			/* sin.sin_addr is already set */
			sin.sin_family = AF_INET;
		} else {
			/* HOSTNAME was probably a name since inet_addr
			 * returned an error.
			 * Look up the name to get the IP
			 */
			host_ip = hostent_get_ip(&hostcache, hostname);
			if (host_ip == (unsigned long int) UINT_MAX) {
				log(1, "Could not resolve %s: %s",
					hostname, strerror(errno));
				perror("Could not resolve the hostname");
				return -1;
			}
			sin.sin_addr.s_addr = host_ip;
		}
	}
	else {
		/* okay, HOSTNAME was a valid dot-notation IP */
		sin.sin_addr.s_addr = inetaddr;
	}
	sin.sin_port = htons(port);

	/* become root again - use our function instead of plain
	 * setuid() for the logging */
	if (changeid(PRIV, UID, "Changing ID to root (socket(), bind())")) {
		return -1;
	}

	shandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (shandle < 0) {
		log(1, "Error creating socket to bind: %s", strerror(errno));
		perror("Error creating socket to bind to");
		return -1;
	}
	if (setsockopt(shandle, SOL_SOCKET, SO_REUSEADDR,
				(void*) &one, sizeof(one)) < 0) {
		log(3, "Error setting socket to SO_REUSEADDR");
	}
	if (bind(shandle, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		log(1, "Error binding: %s", strerror(errno));
		perror("Error binding");
		return -1;
	}
	if (listen(shandle, 5) < 0) {
		log(1, "Error listening on the socket: %s", strerror(errno));
		perror("Error listening on a bound socket");
		return -1;
	}

	log(6, "Listening on %s, port %d, fd %d", hostname, port, shandle);

	if (changeid(UNPRIV, EUID,
				"Changing id back (socket(), bind())") < 0) {
		return -1;
	}

	return shandle;
}


/* waitclient() waits for a client to connect. It binds to the ports and
 * listens to them. If a connection comes it, jftpgw forks. The parent process
 * keeps on listening whereas the child process handles the connection
 *
 * Parameters: hostnames:      Where to bind to
 *             clntinfo:       connection information
 *
 * Return value: -1 on error (if error message has been sent to the client)
 *               -2 on error (if error message should be created by
 *                            strerror()
 *               socket handle: on success (by the child process
 *
 *               Note: The parent process never returns from this function,
 *                     it is terminated by a signal
 */


int waitclient(const char* hostnames, struct clientinfo* clntinfo) {

	int chldpid =0;
	const char* option = 0;
	int ahandle, i;
	struct sigaction sa;
	struct descriptor_set d_set;
	unsigned int peer_ip;
	struct sockaddr_in c_in;
	time_t now;

	if (srvinfo.multithread) {
		daemonize();
	}

	/* become root again - use our function instead of plain
	 * setuid() for the logging */
	if (changeid(PRIV, UID, "Changing ID to root (pidfile)") < 0) {
		return -1;
	}

	option = config_get_option("pidfile");
	if (option) {
		FILE* pidf;
		umask(022);
		pidf = fopen(option, "w");
		if (pidf) {
			fprintf(pidf, "%ld\n", (long) getpid());
			fclose(pidf);
			/* if successful register function to remove the
			 * pidfile */
			atexit(removepidfile);
		} else {
			log(2, "Error creating pidfile %s", option);
		}
		umask(0);
	}

	if (changeid(UNPRIV, EUID,
				"Changing id back (socket(), bind())") < 0) {
		return -1;
	}

	/* this has to be done for the daemonization. We do it now after
	 * the pidfile has been created */
	umask(0);

	srvinfo.ready_to_serve = SVR_LAUNCH_READY;

	if (stage_action("startsetup") < 0) {
		return -1;
	}

	d_set = listen_on_ifaces(hostnames, clntinfo);
	if (d_set.maxfd < 0) {
		log(8, "d_set.maxfd was negative: %d", d_set.maxfd);
		return -1;
	}

	/* we have successfully bound */

	sa.sa_handler = childterm;
	chlds_exited = 0;
	sigemptyset (&sa.sa_mask);
#ifndef WINDOWS
	sa.sa_flags = SA_RESTART;
#endif
	sigaction (SIGCHLD, &sa, 0);


	/* Close stdin,stdout,stderr */
	for(i = 0; i <= 2 && srvinfo.multithread; i++) {
		close(i);
	}
	srvinfo.main_server_pid = getpid();
	atexit(sayterminating);

	while(1) {
		ahandle = get_connecting_socket(d_set);
		if (ahandle == -1) {
			/* either select() or accept() failed */
			/* I don't try resume here because we are in an
			 * endless loop. The danger of the programm falling
			 * into an infinite loop consuming all cpu time is
			 * too big... */
			log(8, "get_connecting_socket() returned error code");
			return -1;
		}

		c_in = socketinfo_get_local_sin(ahandle);
		peer_ip = get_uint_peer_ip(ahandle);
		now = time(NULL);
		config_counter_increase(peer_ip,               /* from ip */
					c_in.sin_addr.s_addr,  /* proxy_ip */
					ntohs(c_in.sin_port),  /* proxy_port */
					now);               /* specific_time */
		if (config_check_limit_violation()) {
			say(ahandle, "500 Too many connections, sorry\r\n");
			close(ahandle);
			config_counter_decrease(peer_ip,       /* from ip */
					c_in.sin_addr.s_addr,  /* proxy_ip */
					ntohs(c_in.sin_port),  /* proxy_port */
					now);               /* specific_time */
			continue;
		}
		if (srvinfo.multithread) {
			if ((chldpid = fork()) < 0) {
				log(1, "Error forking: %s", strerror(errno));
				close(ahandle);
				return -1;
			}
			if (chldpid > 0) {
				/* parent process */
				/* register the PID */
				register_pid(chldpid, peer_ip,
					c_in.sin_addr.s_addr,  /* proxy_ip */
					ntohs(c_in.sin_port),  /* proxy_port */
					now);               /* specific_time */
				close(ahandle);
			}
			if (chldpid == 0) {
				/* child process */
				log(8, "forked to pid %d", getpid());
			}
		}
		if (!srvinfo.multithread || chldpid == 0) {
			return child_setup(ahandle, clntinfo);
		}
	}
}


static
int get_connecting_socket(struct descriptor_set d_set) {
#ifdef HAVE_SOCKLEN_T
	socklen_t size;
#else
	int size;
#endif
	static int nfd;
	int shandle, ahandle;
	fd_set backupset;
	struct sockaddr_in sin;

	size = sizeof(sin);

	memcpy(&backupset, &d_set.set, sizeof(fd_set));

	/* is there no remaining ready fd from the last select() ? */
	if (nfd == 0) {
		while (1) {
			/* eternal select() */
			/* nfd returns the number of fds that are ready */
			nfd = select(d_set.maxfd + 1, &d_set.set, 0, 0, 0);
			if (nfd > 0) {
				break;
			}
			if (errno == EINTR) {
				memcpy(&d_set.set, &backupset, sizeof(fd_set));
				if (chlds_exited > 0) {
					get_chld_pid();
				}
				if (should_read_config) {
					log(9, "Re-reading configuration");
					reread_config();
				}
				continue;
			}
			log(1, "select() failed: %s, nfd: %d", strerror(errno), nfd);
			return -1;
		}
	}
	/* a descriptor is ready */
	shandle = 0;
	while (!FD_ISSET(shandle, &d_set.set)) {
		shandle++;
	}
	/* one descriptor less is ready in the set */
	nfd--;

	while(1) {
		ahandle = accept(shandle, (struct sockaddr *) &sin, &size);
		if (ahandle < 0) {
			if (errno == EINTR) {
				continue;
			}
			log(1, "accept() failed: %s", strerror(errno));
			return -1;
		}
		break;
	}
	return ahandle;
}


static
int child_setup(int sock_fd, struct clientinfo *clntinfo) {
	struct sockaddr_in t_in;
	struct sockaddr_in c_in;
	unsigned long int peer_ip = get_uint_peer_ip(sock_fd);
	struct sigaction sa;
	int i, ret;
#ifdef HAVE_LIBWRAP
	struct request_info req;
	int libwrap_allow = 0;
#endif

	if (peer_ip == (unsigned long int) UINT_MAX) {
		return -1;
	}

	if (stage_action("connect") < 0) {
		say(sock_fd, "421 Error setting up (see logfile)\r\n");
		return -1;
	}

	/* The clients ignore the SIGHUP signal. Thus
	 * the user can issue a killall -HUP jftpgw
	 * and the master jftpgw process rereads its
	 * configuration file without affecting the
	 * child servers */

	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGHUP, &sa, 0);

	/* And they just reap the status of exited children
	 */
	sa.sa_handler = reap_chld_info;
	sigemptyset(&sa.sa_mask);
#ifndef WINDOWS
	sa.sa_flags = SA_RESTART;
#endif
	sigaction(SIGCHLD, &sa, 0);

	for (i = 0; i < clntinfo->boundsocket_niface; i++) {
		close(clntinfo->boundsocket_list[i]);
	}
	free(clntinfo->boundsocket_list);
	clntinfo->boundsocket_list = (int*) 0;

	clntinfo->forward.passauth = 0;
	clntinfo->serversocket = -1;

	clntinfo->clientsocket = sock_fd;
	log(7, "Connection from %s", get_char_peer_ip(sock_fd));
	c_in = socketinfo_get_local_sin(sock_fd);
	log(7, "Client tried to connect to %s on port %d",
					inet_ntoa(c_in.sin_addr),
					ntohs(c_in.sin_port));

	clntinfo->addr_to_client = c_in.sin_addr.s_addr;
	clntinfo->proxy_ip = c_in.sin_addr.s_addr;
	clntinfo->proxy_port = ntohs(c_in.sin_port);

	/* see if we are in transparent mode */
	t_in = socketinfo_get_transparent_target_sin(sock_fd);
	log(7, "Transparent target seems to be %s on port %d",
					inet_ntoa(t_in.sin_addr),
					ntohs(t_in.sin_port));

	log(9, "Checking TAG_GLOBAL | TAG_FROM | TAG_PROXYIP | TAG_PROXYPORT | TAG_TIME | TAG_SERVERTYPE");
	ret = config_shrink_config(peer_ip,	/* source IP */
				-1,		/* dest IP */
				(char*) 0,	/* dest name */
				0,		/* dest port */
				(char*) 0,	/* dest user */
				-1,		/* forwarded IP */
				(char*) 0,	/* forwarded destination */
				0,		/* forwarded destinationport */
				(char*) 0,	/* forwarded username */
				0,		/* set no specific time */
				clntinfo->proxy_ip,   /* ip of proxy if   */
				clntinfo->proxy_port, /* port of proxy if */
				srvinfo.servertype,   /* global variable */
				&hostcache,
				TAG_CONNECTED
				);
	if (ret != 0) {
		log(2, "Error shrinking config data");
		return ret;
	}

#ifndef HAVE_LIBWRAP
	srvinfo.tcp_wrapper = 0;
#endif

	if (srvinfo.tcp_wrapper) {
#ifdef HAVE_LIBWRAP
		/* use libwrap(tcp_wrapper) for access control
		 *   patch by <fukachan@fml.org>.
		 * It is useful to check both keywords "jftpgw" and "ftp-gw"
		 * for TIS Gauntlet flabour.  
		 */
		request_init(&req, 
			     RQ_DAEMON, "ftp-gw", 
			     RQ_CLIENT_ADDR, conv_ip_to_char(peer_ip),
			     NULL);
		libwrap_allow = hosts_access(&req);

		request_init(&req, 
			     RQ_DAEMON, "jftpgw", 
			     RQ_CLIENT_ADDR, conv_ip_to_char(peer_ip),
			     NULL);

		if (libwrap_allow || hosts_access(&req)) {
			log(5, "%s is allowed by libwrap to connect.", 
					conv_ip_to_char(peer_ip));
		}
		else {
			say(sock_fd, "500 You are not allowed by libwrap to "
					"connect. Goodbye.\r\n");
			log(5, "%s was not allowed to connect.",
					conv_ip_to_char(peer_ip));
			close(sock_fd);
			return -2;
		}
#endif
	} else {
		if ( ! config_get_option("access")
			||
			! strcmp(config_get_option("access"), "allow") == 0) {

			say(sock_fd, "500 You are not allowed to "
				"connect. Goodbye.\r\n");
			log(5, "%s was not allowed to connect.",
						conv_ip_to_char(peer_ip));
			close(sock_fd);
			return -2;
		} else {
			log(6, "%s is allowed to connect.",
					conv_ip_to_char(peer_ip));
		}
	}

	if (config_get_bool("transparent-proxy") && 
			get_uint_peer_ip(sock_fd) == t_in.sin_addr.s_addr) {
		log(4, "proxy loop detected - machine connects to itself, disabling transparent proxy support");
	}
	if (config_get_bool("transparent-proxy")
			&&
		/* see if the proxy loops. It loops when the source has the
		 * same IP as the destination. There is no need for such a
		 * configuration "in the wild". Is there? */
		(!(get_uint_peer_ip(sock_fd) == t_in.sin_addr.s_addr)
#ifdef HAVE_LINUX_NETFILTER_IPV4_H
			&& !(t_in.sin_addr.s_addr == c_in.sin_addr.s_addr
				&&
			    t_in.sin_port == c_in.sin_port)
#endif
		)) {

		log(9, "Enabling transparent proxy");
		clntinfo->transparent = TRANSPARENT_YES;
		clntinfo->transparent_destination = t_in;
	} else {
		log(9, "No transparent proxy support");
		clntinfo->transparent = TRANSPARENT_NO;
		clntinfo->transparent_destination.sin_port = 0;
		clntinfo->transparent_destination.sin_addr.s_addr = 0;
	}

	if (stage_action("connectsetup") < 0) {
		say(sock_fd, "421 Error setting up (see logfile)\r\n");
		return -1;
	}

	if (clntinfo->transparent == TRANSPARENT_YES) {
		char* colon;
		long int dport;

		clntinfo->destination =
			socketinfo_get_transparent_target_char(sock_fd);
		colon = strchr(clntinfo->destination, ':');
		if (!colon) {
			/* abort, shouldnt happen */
			log(9, "Could not get transparent target properly");
			clntinfo->transparent = TRANSPARENT_NO;
			return say_welcome(sock_fd);
		}
		*colon = '\0';  /* terminate destination */
		colon++;        /* skip to port number   */
		dport = strtol(colon, NULL, 10);
		if (errno == ERANGE 
				&& (dport == LONG_MIN || dport == LONG_MAX)) {
			clntinfo->destinationport =
				config_get_ioption("serverport", DEFAULTSERVERPORT);
			/* reverse for logging */
			colon--; *colon = ':';
			log(6, "Could not parse dest/port to connect to: %s",
					clntinfo->destination);
			return -1;
		} else {
			clntinfo->destinationport = dport;
		}
	}
	if ( ! config_compare_option("logintime", "connect") ) {
		ret = say_welcome(sock_fd);
	} else {
		ret = login(clntinfo, LOGIN_ST_CONNECTED);
	}

	/* seed the random number generator
	 */
	srand(time(NULL));

	return ret;
}

int inetd_connected(int sock, struct clientinfo* clntinfo) {
	if (stage_action("startsetup") < 0) {
		return -1;
	}
	return child_setup(sock, clntinfo);
}

static
int say_welcome(int sock_fd) {
	const char* welcome = config_get_option("welcomeline");

	if (config_get_option("welcomeline")) {
		char* welcmsg;

		welcmsg = prependcode(welcome, 220);
		enough_mem(welcmsg);

		log(9, "Saying this text as welcomeline: %s", welcmsg);
		say(sock_fd, welcmsg);
		free(welcmsg);
	} else {
		say(sock_fd, "220 Joe FTP Proxy Server/Gateway (v"JFTPGW_VERSION") ready\r\n");
	}
	return 0;
}


static
struct descriptor_set listen_on_ifaces(const char* hostnames,
				struct clientinfo* clntinfo) {
	char* part =0, *portdel =0;
	char* hostnames2;
	size_t hostnames2size;
	int offset, port, shandle, i;
	struct descriptor_set d_set;

	offset = 0;
	clntinfo->boundsocket_niface = 0;
	FD_ZERO(&d_set.set);
	d_set.maxfd = 0;

	/* we have to make the string suitable for quotstrtok by appending a
	 * WHITESPACE character */
	hostnames2size = strlen(hostnames) + 2;
	hostnames2 = (char*) malloc(hostnames2size);
	enough_mem(hostnames2);
	snprintf(hostnames2, hostnames2size, "%s ", hostnames);

	while ((part = quotstrtok(hostnames2, WHITESPACES, &offset))) {
		/* do some counting */
		clntinfo->boundsocket_niface++;
		free(part);
	}
	clntinfo->boundsocket_list =
		(int*) malloc(sizeof(int) * clntinfo->boundsocket_niface);
	enough_mem(clntinfo->boundsocket_list);

	offset = 0; i = 0;
	while ((part = quotstrtok(hostnames2, WHITESPACES, &offset))) {
		portdel = strchr(part, ':');
		if (!portdel) {
			log(3, "Invalid IP/Port specification: %s", part);
			free(part);
			continue;
		}
		*portdel = (char) 0;
		errno = 0;
		port = strtol(portdel+1, (char**) 0, 10);
		if (errno || port > 65535 || port <= 0) {
			log(4, "Invalid port specification: %s. "
			   "Using default value %d", portdel, DEFAULTBINDPORT);
			port = DEFAULTBINDPORT;
		}
		portdel = (char*) 0;

		log(9, "binding to %s, port %d", part, port);

		shandle = bindport(part, port);
		free(part);
		part = (char*) 0;
		if (shandle < 0) {
			log(8, "Could not bind: %s", strerror(errno));
			free(hostnames2);
			FD_ZERO(&d_set.set);
			d_set.maxfd = -1;
			return d_set;
		}
		FD_SET(shandle, &d_set.set);
		d_set.maxfd = MAX_VAL(d_set.maxfd, shandle);
		clntinfo->boundsocket_list[ i++ ] = shandle;
	}
	free(hostnames2);
	hostnames2 = (char*) 0;

	if (clntinfo->boundsocket_niface == 0) {
		log(2, "No interfaces found to bind to");
		FD_ZERO(&d_set.set);
		d_set.maxfd = -1;
		return d_set;
	}

	return d_set;
}


static
char* prependcode(const char* s, int code) {
	char* sdup = strdup(s);
	char* nextline = sdup;
	char* buffer;
	char* end, *replaceend, *bufferend, *bufferstart;
	char dashp = '-';
	const char* const linefeed = "\r\n";
	const char* const empty = "";
	const char* append = linefeed;
	int count = 0;
	int i, length;
	size_t bufsize;

	enough_mem(sdup);
	replace_not_larger(sdup, "\\r", "\r");
	replace_not_larger(sdup, "\\n", "\n");

	if (code < 0) {
		code = -code;
	}
	if (code > 999) {
		code = code % 1000;
	}
	if (code < 100) {
		code += 100;
	}

	/* count the newlines */
	while ((nextline = strstr(nextline, "\r\n"))) {
		count++;
		nextline++;  /* thus this position does not match again */
	}

	/* memory: (count + 1) * strlen("xxx ") + 4 (to terminate for sure
	 * with "\r\n") + 1 (Terminator)
	 *
	 * memory = (count + 1) * 4  + 1
	 */

	bufsize = strlen(sdup) + (count + 1) * 4  + 4  + 1;
	bufferstart = buffer = (char*) malloc(bufsize);
	enough_mem(buffer);
	bufferend = buffer + bufsize - 1;

	end = nextline = sdup;
	while (strlen(end)) {
		end = strstr(nextline, "\r\n");
		if (!end) {
			end = nextline + strlen(nextline);
			replaceend = end;
			dashp = ' ';
			append = linefeed;
		} else {
			/* don't replace the first '\' */
			replaceend = end - 1; 
			end += strlen("\r\n");
			if (end == nextline + strlen(nextline)) {
				/* at the very end */
				dashp = ' ';
			} else {
				dashp = '-';
			}
			append = empty;
		}
		i = 0;
		while ((nextline + i) < replaceend) {
			if (nextline[i] == '%') {
				/* Remove the '%' */
				nextline[i] = '/';
			}
			/* remove any special characters between nextline
			 * and end */
			if ((unsigned char) nextline[i] < 32) {
				nextline[i] = '_';
			}
			i++;
		}
		length = MIN_VAL( bufferend - bufferstart,
				  strlen("xxx ")
				  + (end - nextline)
				  + strlen(append)
				  + 1
				);
		snprintf(bufferstart, length, "%d%c%s%s",
				code, dashp, nextline, append);
		bufferstart += strlen(bufferstart);
		if (!*nextline) { /* end */
			nextline = 0;
		}
		nextline = strstr(nextline, "\r\n") + strlen("\r\n");
	}
	free(sdup);
	return buffer;
}



