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

#include "jftpgw.h"
#include <ctype.h>

extern int timeout;
static struct portrangestruct *pasvrangeclient = (struct portrangestruct*) 0;
static struct portrangestruct *pasvrangeserver = (struct portrangestruct*) 0;
static void saypasv(int, char*, unsigned short int);


/* pasvserver() handles nearly the whole passive connection, i.e., it
 * requests a port on the server, binds to one on the proxy host and sends
 * the address & port to the client
 * 
 * Parameters: clntinfo: connection information
 *
 * Return value: -1 on error,
 *                0 on success
 *
 * Called by: handlecmds (if the client issued a PORT or PASV command)
 *
 * */

int pasvserver(struct clientinfo* clntinfo) {
	int ss, cs, ret, pssock;
	char* brk;
	char* buffer;
	struct sockaddr_in pasvserv_sin;
	struct portrangestruct* prs;

	ss = clntinfo->serversocket;
	cs = clntinfo->clientsocket;

	say(ss, "PASV\r\n");
	buffer = ftp_readline(ss);
	if (!buffer) {
		if (timeout) {
			log(2, "Timeout in %s line %d\n", __FILE__ ,__LINE__);
			err_time_readline(cs);
		} else {
			err_readline(cs);
			log(2, "Error reading response to PASV from server: %s",
				strerror(errno));
		}
		return -1;
	}

	/* 
	 * An example of the answer would be:
	 * "227 Entering Passive Mode (127,0,0,1,7,123)"
	 *
	 * Roxen Challenger at ftp.mutt.org sends this reply
	 * "227 Entering Passive Mode. 217,69,76,44,156,68"
	 */
	/* Look for the beginning of the IP */
	brk = (char*) strchr(buffer, '(');

	if (!checkdigits(buffer, 227)) {
		/* invalid return string */
		say(cs, buffer);
		log(2, "Strange answer after sending PASV: %s", buffer);
		free(buffer);
		return -1;
	}
	if (!brk) {
		/* don`t give up, maybe it just didn`t send a bracket */
		/* skip over the numerical code */
		brk = strchr(buffer, ' ');
		brk++;
		/* increase until we found a numeric */
		while (*brk && !isdigit((int)*brk)) {
			brk++;
		}
		/* now brk should point to the first numeric */
	} else {
		/* skip over the bracket sign */
		brk++;
	}

	/* call parsesock to parse the answer string and get the values into
	 * PASVSERVERSOCK */
	ret = parsesock(brk, &pasvserv_sin, PASSIVE);
	if (ret) {
		say(cs, "500 Could not parse a valid address from the PASV response\r\n");
		log(2, "Could not parse a valid socket from the PASV "
		    " response: %s", buffer);
		free(buffer);
		return -1;
	}

	if ( ! pasvrangeserver ) {
		const char* range =config_get_option("passiveportrangeserver");
		if ( ! range ) {
			range = config_get_option("passiveportrange");
		}
		pasvrangeserver = config_parse_portranges(range);
	}
	if ( ! pasvrangeserver ) {
		prs = config_port2portrange(clntinfo->dataport);
	} else {
		prs = pasvrangeserver;
	}

	log(7, "Opening passive port %s:%d",
					inet_ntoa(pasvserv_sin.sin_addr),
					ntohs(pasvserv_sin.sin_port));
	/* open the port on the foreign machine specified by PASVSERVERSOCK */
	pssock = openportiaddr(pasvserv_sin.sin_addr.s_addr, /* dest ip   */
			ntohs(pasvserv_sin.sin_port),        /* dest port */
			clntinfo->data_addr_to_server,       /* source ip */
			prs);                             /* source ports */
	if (pssock < 0) {
		say(cs, "500 Could not connect to the specified PASV host\r\n");
		log(3, "Could not connect to the specified PASV host: (%s)", buffer);
		free(buffer);
		return -1;
	}
	clntinfo->dataserversock = pssock;

	free(buffer);
	return 0;
}


/* pasvclient() opens a port on the proxy machine and tells the addr and
 * port to the client
 *
 * Parameters: clntinfo: connection information
 *
 * Return values: -1 on error
 *                 0 on success
 *
 * Called by: handlecmds (if the client issued a PORT or PASV command)
 */


int pasvclient(struct clientinfo* clntinfo) {
	int pcsock;
	int cs = clntinfo->clientsocket;
	struct sockaddr_in pasvclientsin;
	struct in_addr in;

	clntinfo->clientmode = PASSIVE;

	if ( ! pasvrangeclient ) {
		const char* range =config_get_option("passiveportrangeclient");
		if (! range) {
			range = config_get_option("passiveportrange");
		}
		pasvrangeclient = config_parse_portranges(range);
	}
	if (clntinfo->dataclientsock >= 0) {
		/* we are still listening on another socket. Close it */
		close(clntinfo->dataclientsock);
		clntinfo->dataclientsock = -1;
	}
	errno = 0;
	/* open a port on the interface the client is connected to and store
	 * the values in PASVCLIENTSIN */
	pcsock = openlocalport(&pasvclientsin,
			       clntinfo->data_addr_to_client,
			       pasvrangeclient);
	if (pcsock < 0) {
		/*say(cs, "500 Error binding to a port\r\n");*/
		log(2, "Could not bind (%s)", strerror(errno));
		return -1;
	}
	clntinfo->dataclientsock = pcsock;
	clntinfo->waitforconnect = &clntinfo->dataclientsock;

	/* write the values to the client socket CS */
	in.s_addr = pasvclientsin.sin_addr.s_addr;
	saypasv(cs, inet_ntoa(in), htons(pasvclientsin.sin_port));

	return 0;
}


/* saypasv() writes the "227 Entering Passive Mode ..." to the client
 * descriptor CS
 * 
 * Parameters: cs: client descriptor
 *             addr: Our address of the interface the client connected
 *                   through
 *             port: The corresponding port on our side
 *
 * Return value: void
 *
 * Called by: pasvclient()
 *
 * */ 

static void saypasv(int cs, char* addr, unsigned short int port) {
	unsigned short int lo, hi;
	char *dot;

	hi = port % 256;
	lo = (port - hi) / 256;

	while ((dot = strchr(addr, '.'))) {
		*dot = ',';
	}

	sayf(cs, "227 Entering Passive Mode (%s,%d,%d)\r\n", addr, lo, hi);
}

void destroy_passive_portrange() {
	config_destroy_portrange(pasvrangeserver);
	config_destroy_portrange(pasvrangeclient);
	pasvrangeclient = (struct portrangestruct*) 0;
	pasvrangeserver = (struct portrangestruct*) 0;
}

