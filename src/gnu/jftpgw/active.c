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

extern int timeout;
static struct portrangestruct *actvrangeserver = (struct portrangestruct*) 0;
static struct portrangestruct *actvrangeclient = (struct portrangestruct*) 0;
static void sayact(int, char*, int);

int portcommandcheck(const char* buffer, struct sockaddr_in* sin,
					struct clientinfo* clntinfo) {
	int cs = clntinfo->clientsocket;
	char* beginning = 0;
	int ret;

	clntinfo->clientmode = ACTIVE;
	beginning = strchr(buffer, ' ');
	if (!beginning) {
		/* It should not fail, since we are testing for "PORT " in
		 * cmds.c */
		log(3, "Invalid PORT command (no space): %s", buffer);
		say(cs, "500 Invalid PORT command (no space char)\r\n");
		return -1;
	}
	beginning++;
	ret = parsesock(beginning, sin, ACTIVE);
	if (ret) {
		log(3, "Error parsing the PORT command: %s (%s)",
				buffer, beginning);
		say(cs, "501 Error parsing the PORT command\r\n");
		return -1;
	}
	if (config_get_bool("allowforeignaddress") == 0) {
		char* sin_addr = strdup(inet_ntoa(sin->sin_addr));
		enough_mem (sin_addr);
		if (0 != strcmp(sin_addr,
				get_char_ip(GET_IP_CLIENT, clntinfo))) {

			log(3, "Illegal foreign address %s in PORT command.",
					inet_ntoa(sin->sin_addr));
			log(5, "Expected address %s instead",
					get_char_ip(GET_IP_CLIENT, clntinfo));
			say(cs, "500 Illegal address in PORT command\r\n");
			free(sin_addr);
			return -1;
		}
		free(sin_addr);
	}
	if (config_get_bool("allowreservedports") == 0) {
		if (ntohs(sin->sin_port) < IPPORT_RESERVED) {
			log(3, "Illegal port %d in PORT command.",
					ntohs(sin->sin_port));
			say(cs, "500 Illegal port in PORT command\r\n");
			return -1;
		}
	}
	/* seems to be okay */
	return 0;
}


/* 
 * activeclient() reads the "PORT x,x,x,x,x,x" command from the client,
 * opens that port on the client's side and returns the socket descriptor
 * 
 * Parameters: buffer: contains the "PORT x,x,x,x,x,x" command
 *             clntinfo: the connection variables
 * 
 * Returns: -1 on error, 0 on success
 *
 * Called by: passcmd (if cmd starts with PORT)
 * 
 * */

int activeclient(char* buffer, struct clientinfo* clntinfo) {
	struct sockaddr_in sin;
	struct portrangestruct* prs;
	int ret;

	ret = portcommandcheck(buffer, &sin, clntinfo);
	if (ret < 0) {
		return ret;
	}
	log(8, "Opening the active FTP port %d on %s",
			ntohs(sin.sin_port), inet_ntoa(sin.sin_addr));
	if (clntinfo->waitforconnect == &clntinfo->dataclientsock) {
		clntinfo->waitforconnect = (int*) 0;
	}
	if ( ! actvrangeclient ) {
		const char* range = config_get_option("activeportrangeclient");
		if ( ! range ) {
			range = config_get_option("activeportrange");
		}
		actvrangeclient = config_parse_portranges(range);
	}
	if ( ! actvrangeclient ) {
		prs = config_port2portrange(clntinfo->dataport);
	} else {
		prs = actvrangeclient;
	}
	ret = openportiaddr(sin.sin_addr.s_addr,          /* dest ip   */
			    ntohs(sin.sin_port),          /* dest port */
			    clntinfo->data_addr_to_client,/* source ip, port */
			    prs);
	if (ret < 0) {
		log(8, "setting dataclientsock to -1 (openport error)");
		clntinfo->dataclientsock = -1;
		return -1;
	}
	clntinfo->dataclientsock = ret;

	return 0;
}


/* 
 * activeserver() opens a port on the proxy machine, determins the port
 * number and tells it to the server (along with the address of the interface
 * over which we connect to it).
 * 
 * Parameters: answer: A pointer to a pointer used to store the answer of the
 *                     server
 * 
 * Returns: -1 on error, 0 on success
 *
 * Called by: handlecmds (when client issued PORT or PASV)
 * 
 * */

int activeserver(char** answer, struct clientinfo *clntinfo) {

	int ret;
	struct sockaddr_in sin;
	int ss = clntinfo->serversocket;
	char* buffer;

	if ( ! actvrangeserver ) {
		const char* range = config_get_option("activeportrangeserver");
		if ( ! range ) {
			range = config_get_option("activeportrange");
		}
		actvrangeserver = config_parse_portranges(range);
	}
	/* open a port on our side use the interface address through which
	 * we are connected to the server */
	ret = openlocalport(&sin,                    /* for the return value */
			    clntinfo->data_addr_to_server,      /* source ip */
			    actvrangeserver);                /* source ports */
	if (clntinfo->waitforconnect == &clntinfo->dataserversock) {
		clntinfo->waitforconnect = (int*) 0;
	}
	if (ret < 0) {
		say(clntinfo->clientsocket,
			"425 Could not establish a connection endpoint\r\n");
		*answer = 0;
		return -1;
	}

	clntinfo->dataserversock = ret;
	clntinfo->waitforconnect = &clntinfo->dataserversock;

	/* tell the server about the addr + port it can connect to */
	sin.sin_addr.s_addr = clntinfo->data_addr_to_server;
	sayact(ss, inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));

	buffer = ftp_readline(ss);
	*answer = buffer;
	if (!buffer) {
		if (timeout) {
			log(2, "Timeout in %s line %d\n", __FILE__ ,__LINE__);
			log(2, "Timed out reading answer for the PORT command");
			say(clntinfo->clientsocket,
			"500 Timeout waiting for answer for the PORT command\r\n");
		}
		else {
			log(1, "Error in readline reading answer for the PORT command");
			say(clntinfo->clientsocket,
				"Error reading answer for PORT command\r\n");
		}
		return -1;
	}
	/* code 200 would be "PORT command successful" */
	if (!checkdigits(buffer, 200)) {
		log(1, "Failure sending PORT command (%s) to the server: %s",
				inet_ntoa(sin.sin_addr), buffer);
		say(clntinfo->clientsocket,
				"425 Can't establish connection\r\n");
		return -1;
	}
	return 0;

}

/* sayact() transforms the parameters ADDR and PORT to the 
 * "PORT x,x,x,x,x,x" notation and says it to the descriptor fd
 *
 * Paramters: fd: the file descriptor the command is written to
 *            addr: the address that should be included in dot notation
 *                  format
 *            port: the port that was opened on addr's side
 */

static void sayact(int fd, char* addr, int port) {
	unsigned short int lo, hi;
	char buffer[MAX_LINE_SIZE], *dot;

	hi = port % 256;
	lo = (port - hi) / 256;

	while ((dot = strchr(addr, '.'))) {
		*dot = ',';
	}
	snprintf(buffer, sizeof(buffer), "PORT %s,%d,%d\r\n",
		addr, lo, hi);
	
	say(fd, buffer);
}

void destroy_active_portrange() {
	config_destroy_portrange(actvrangeserver);
	config_destroy_portrange(actvrangeclient);
	actvrangeserver = (struct portrangestruct*) 0;
	actvrangeclient = (struct portrangestruct*) 0;
}

