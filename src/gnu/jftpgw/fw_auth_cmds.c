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
#include "cmds.h"

/* static functions here */
static int cmds_after_user(struct conn_info_st*);
static int cmds_after_pass(struct conn_info_st*);

int login(struct clientinfo*, int);


struct destination_t {
	char* hostname;
	unsigned int port;
};


static
int fw_validate_user(const char* user,
		     const char* fwpass,
		     const char* method,
		     const char* pass) {

	if (cryptcmp(pass, fwpass) == 0) {
		return 0;
	}

	return 1;
}

static
int fw_validate(const char* fwuser, const char* fwpass) {
	struct slist_t* acct_list = config_get_option_array("account");
	struct slist_t* account, *acct_line;
	const char* user, *method, *pass;

	if ( ! acct_list ) {
		return 1;
	}

	/* reverse the list to keep the paradigma: if there are several
	 * similar options, the last one is taken */
	acct_list = slist_reverse(acct_list);

	account = acct_list;

	do {
		acct_line = config_split_line( account -> value, WHITESPACES );

		if (!acct_line) {
			goto error_line;
		}
		user = acct_line->value;
		if (!user || !strlen(user) || !acct_line->next) {
			goto error_line;
		}
		method = acct_line->next->value;
		if (!method || !strlen(method) || !acct_line->next->next) {
			goto error_line;
		}
		pass = acct_line->next->next->value;
		if (!pass || !strlen(pass)) {
			goto error_line;
		}

		/* parsing went fine down here */

		if (strcmp(user, fwuser) == 0) {
			/* jump out of the loop */
			int ret;
			slist_destroy(acct_list);
			ret = fw_validate_user(fwuser, fwpass, method, pass);
			slist_destroy(acct_line);
			return ret;
		}
		continue;
error_line:
		slist_destroy(acct_line);
		log(5, "Incorrect account specification: %s", account->value);
	} while ( (account = account -> next) );

	slist_destroy(acct_list);

	return 1;
}


int std_reset(const char* args, struct conn_info_st* conn_info) {
	/* set all values to (char*) 0 after free()ing */
	struct clientinfo* c = conn_info->clntinfo;

	log(8, "resetting login information");

	if (!c->transparent == TRANSPARENT_YES ||
	    !config_compare_option("logintime", "connect")) {

		c->login.stage = LOGIN_ST_NOT_CONNECTED;
		if (c->destination) {
			free(c->destination);
			c->destination = (char*) 0;
		}
		c->destinationport = 0;
	}
	if (c->user) {
		free(c->user);
		c->user = (char*) 0;
	}
	if (c->pass) {
		free(c->pass);
		c->pass = (char*) 0;
	}
	if (c->anon_user) {
		free(c->anon_user);
		c->anon_user = (char*) 0;
	}
	if (c->fw_auth.user) {
		free(c->fw_auth.user);
		c->fw_auth.user = (char*) 0;
	}
	if (c->fw_auth.pass) {
		free(c->fw_auth.pass);
		c->fw_auth.pass = (char*) 0;
	}
	if (c->before_forward.user) {
		free(c->before_forward.user);
		c->before_forward.user = (char*) 0;
	}
	if (c->before_forward.destination) {
		free(c->before_forward.destination);
		c->before_forward.destination = (char*) 0;
	}
	if (c->login.authresp.fullmsg) {
		free(c->login.authresp.fullmsg);
		c->login.authresp.fullmsg = (char*) 0;
		c->login.authresp.lastmsg = (char*) 0;
	}

	return CMD_HANDLED;
}


#define DELIMITERS "@,: \t"

static
struct destination_t fw_auth_parse_host_port(const char* cmd) {
	struct destination_t dest = { (char*) 0, 0 };
	char* portstr;
	int offset = 0;

	if (! cmd) {
		return dest;
	}
	dest.hostname = quotstrtok(cmd, DELIMITERS, &offset);
	if (!dest.hostname) {
		return dest;
	}
	if (strlen(dest.hostname) == 0) {
		free(dest.hostname);
		dest.hostname = (char*) 0;
		return dest;
	}
	portstr = quotstrtok(cmd, DELIMITERS, &offset);
	dest.port = conv_char2long(portstr,
			config_get_ioption(("serverport"), DEFAULTSERVERPORT));
	return dest;
}


static
int fw_forward(const char* user,
		    const char* dest,
		    struct clientinfo* clntinfo) {

	if ( ! user ) {
		say(clntinfo->clientsocket, "500 Expecting user name\r\n");
		log(5, "No username given");
		return CMD_ERROR;
	}

	if (clntinfo->forward.passauth && user && dest) {
		clntinfo->destination = strdup(dest);
		enough_mem(clntinfo->destination);
	}

	if (strcmp(user, "*") == 0) {
		/* keep the old user name */
		log(8, "no new user name - keeping old one: %s",
						clntinfo->user);
	} else {
		free(clntinfo->user);
		clntinfo->user = strdup(user);
		enough_mem(clntinfo->user);
	}

	if (!dest || strlen(dest) == 0 || strcmp(dest, "*") == 0) {
		/* keep the old one */
		log(8, "no new destination - keeping old one: %s",
					clntinfo->destination);
	} else {
		free(clntinfo->destination);
		clntinfo->destination = strdup(dest);
		enough_mem(clntinfo->destination);
	}
	return CMD_HANDLED;
}


static
int fw_transparent(struct clientinfo* clntinfo) {
	const char* transfor_opt = config_get_option("transparent-forward");
	/* check for transdest */
	/* opt => dest => forwardhost */
	struct sockaddr_in t_in;

	if (clntinfo->destination) {
		log(9, "fw_transparent: Destination already set");
		return CMD_HANDLED;
	}

	if (clntinfo->transparent == TRANSPARENT_NO) {
		/* not in transparent mode */
		log(9, "fw_transparent: not in transparent mode");
		return CMD_HANDLED;
	}

	if (transfor_opt) {
		/* transparent forward active */
		log(9, "fw_transparent: transparent forward active");
		return CMD_HANDLED;
	}

	if (config_get_bool("transparent-proxy") == 0) {
		log(4, "No destination set and transparent proxy support disabled");
		say(clntinfo->clientsocket, "500 No destination\r\n");
		return CMD_ERROR;
	}
	t_in = clntinfo->transparent_destination;
	clntinfo->destination = strdup(inet_ntoa(t_in.sin_addr));
	clntinfo->destinationport =
	     ntohs(clntinfo->transparent_destination.sin_port);

	/* Determine the IP the client sees of us */
	clntinfo->addr_to_client =
		socketinfo_get_local_addr_by_sending(clntinfo->clientsocket);

	log(8, "Using transparent proxy. Connecting to %s port %d. User: %s",
				clntinfo->destination,
				clntinfo->destinationport,
				clntinfo->user);

	return CMD_HANDLED;
}


static
int fw_transparent_forward(struct clientinfo* clntinfo) {
	const char* transforward_opt = config_get_option("transparent-forward");
	size_t usersize;
	struct destination_t destination;
	char* transparent_target;

	if (! transforward_opt) {
		return CMD_HANDLED;
	}

	if (clntinfo->transparent == TRANSPARENT_NO) {
		/* not in transparent mode */
		return CMD_HANDLED;
	}

	if (config_compare_option("logintime", "connect")) {
		log(5, "Can't set transparent forward target - This does not work with logintime == connect");
		return CMD_HANDLED;
	}

	/* determine the intended destination */
	transparent_target = socketinfo_get_transparent_target_char(
					clntinfo->clientsocket);
	if (!transparent_target) {
		log(4, "Could not get transparent destination");
		say(clntinfo->clientsocket,
					"500 Error logging in\r\n");
		return CMD_ERROR;
	}

	/* resize the new USER buffer */
	usersize = strlen(clntinfo->user)
			+ 1 /* @ */
			+ strlen(transparent_target)
			+ 1  /* scnprintf seems to need it */
			+ 1; /* Terminate */

	/* write user@dest into user field. dest is the intended destination */
	clntinfo->user = realloc(clntinfo->user, usersize);
	enough_mem(clntinfo->user);
	scnprintf(clntinfo->user, usersize, "@");
	scnprintf(clntinfo->user, usersize, transparent_target);
	free(transparent_target);

	/* replace dest by the specified forward */
	destination = fw_auth_parse_host_port(transforward_opt);
	if (! destination.hostname) {
		say(clntinfo->clientsocket,
					"500 Error logging in\r\n");
		log(4, "Could not parse transparent forward target\r\n");
		return CMD_ERROR;
	}
	free(clntinfo->destination);
	clntinfo->destination = destination.hostname;
	clntinfo->destinationport = destination.port;
	destination.hostname = (char*) 0;

	/* Determine the IP the client sees of us */
	clntinfo->addr_to_client =
		socketinfo_get_local_addr_by_sending(clntinfo->clientsocket);
	clntinfo->transparent = TRANSPARENT_YES;

	log(8, "Using transparent proxy. Connecting to %s. User: %s",
					clntinfo->destination,
					clntinfo->user);

	return CMD_HANDLED;
}


static
int fw_port_mode(const char* portstr,
		 const char* modestr,
		 struct clientinfo* clntinfo) {
	long int pno;

	if (portstr) {
		pno = strtol(portstr, NULL, 10);
		if ((errno == ERANGE && (pno == LONG_MIN || pno == LONG_MAX))
			|| pno == 0) {
			/* it was not a number */
			clntinfo->destinationport = 0;
			/* maybe it is a mode */
			modestr = portstr;
			portstr = (char*) 0;
		} else {
			clntinfo->destinationport = pno;
		}
	}
	if (!clntinfo->destinationport) {
		clntinfo->destinationport
			= config_get_ioption("serverport",
						DEFAULTSERVERPORT);
	}
	clntinfo->servermode = UNSPEC;
	if (modestr) {
		if (modestr && (strchr("ap", *modestr))) {
			log(9, "mode specified: %s", modestr);
			switch (*modestr) {
				case 'a': 
					clntinfo->servermode = ACTIVE;
					log(9, "p-s: active");
					break;
				case 'p':
					clntinfo->servermode = PASSIVE;
					log(9, "p-s: passive");
					break;
			}
		}
	}
	return CMD_HANDLED;
}

int set_userdest(const char *buffer,
		 int offset,
		 struct clientinfo* clntinfo,
		 const char* delimiters) {

	char* user, *host, *port, *mode;

	/* joe@host,21,p */
	/* joe */
	/* joe@host */
	/* joe,host,21,p */

	if (buffer[0] == '@') {
		user = (char*) 0;
	} else {
		user = quotstrtok(buffer, delimiters, &offset);
	}
	host = quotstrtok(buffer, delimiters, &offset);
	port = quotstrtok(buffer, delimiters, &offset);
	mode = quotstrtok(buffer, delimiters, &offset);

	if (fw_forward(user, host, clntinfo) != CMD_HANDLED) {
		free(user); free(host); free(port); free(mode);
		return CMD_ERROR;
	}
	if (fw_transparent_forward(clntinfo) != CMD_HANDLED) {
		free(user); free(host); free(port); free(mode);
		return CMD_ERROR;
	}
	if (fw_transparent(clntinfo) != CMD_HANDLED) {
		free(user); free(host); free(port); free(mode);
		return CMD_ERROR;
	}
	if (fw_port_mode(port, mode, clntinfo) != CMD_HANDLED) {
		free(user); free(host); free(port); free(mode);
		return CMD_ERROR;
	}
	free(user); free(host); free(port); free(mode);

	return CMD_HANDLED;
}

static
int std_user(const char* args, struct conn_info_st* conn_info,
						const char* delimiters) {
	int ret;
	char* args_copy = strdup(args);
	enough_mem(args_copy);

	if (conn_info->clntinfo->forward.passauth == 0) {
		/* passallauth was not set */
	} else {
		delimiters = "";
	}
	if (set_userdest(args_copy, strlen("USER "),
		conn_info->clntinfo, delimiters) != CMD_HANDLED) {

		free(args_copy);
		return CMD_ERROR;
	}
	free(args_copy);

	ret = cmds_after_user(conn_info);
	if (ret != CMD_HANDLED && ret != CMD_DONE) {
		return CMD_ERROR;
	}

	log(7, "Client logged in: User: %s, Dest: %s:%d",
				conn_info->clntinfo->user,
				conn_info->clntinfo->destination,
				conn_info->clntinfo->destinationport);

	/* ret can be CMD_HANDLED or CMD_DONE */
	return ret;
}

int std_user_plain(const char* args, struct conn_info_st* conn_info) {
	return std_user(args, conn_info, "");
}

int std_user_split(const char* args, struct conn_info_st* conn_info) {
	return std_user(args, conn_info, DELIMITERS);
}

static
int cmds_after_user(struct conn_info_st* conn_info) {
	if (config_compare_option("logintime", "user")
		||
	    config_compare_option("logintime", "connect")) {
		/* connect if we should connect after having
		 * received the USER command. Do not connect, if we
		 * are already connected or should connect later on
		 * */
		int ret, code;
		char* buffer;

		ret = login(conn_info->clntinfo, LOGIN_ST_USER);
		if (ret) { return CMD_ERROR; }
		if (conn_info->clntinfo->login.welcomemsg.fullmsg) {
		   buffer = merge_responses(
			conn_info->clntinfo->login.welcomemsg.fullmsg,
			conn_info->clntinfo->login.authresp.fullmsg);
		} else {
			buffer = strdup(conn_info->clntinfo->login.authresp.fullmsg);
		}
		say(conn_info->clntinfo->clientsocket, buffer);
		conn_info->lcs->respcode = getcode(buffer);
		free(buffer);

		/* okay, there was no problem sending the user name and
		 * receiving the result, now check the result */
		code = getcode(conn_info->clntinfo->login.authresp.fullmsg);
		if (code != 331 && code != 230) {
			log(6, "Didn't get successful message after sending "
				"the user name: %s\n",
				conn_info->clntinfo->login.authresp.fullmsg);
			return CMD_ERROR;
		}
		/* Free the welcome message. The authentication
		 * response is always free'ed in login_send_auth */
		free(conn_info->clntinfo->login.welcomemsg.fullmsg);
		conn_info->clntinfo->login.welcomemsg.fullmsg = (char*) 0;
		conn_info->clntinfo->login.auth_resp_sent = 1;
		if (code == 230) {
			/* already logged in */
			free(conn_info->clntinfo->login.authresp.fullmsg);
			conn_info->clntinfo->login.authresp.fullmsg = (char*)0;
			conn_info->clntinfo->login.stage = LOGIN_ST_LOGGEDIN;
			if (login(conn_info->clntinfo, LOGIN_ST_FULL) < 0) {
				return CMD_ERROR;
			}
			/* this is the only case where we return CMD_DONE so
			 * that the login handler doesn't wait for the
			 * command that would follow but knows that his job
			 * is finished */
			return CMD_DONE;
		}

	} else {
		char* user;

		if (conn_info->clntinfo->before_forward.user) {
			user = conn_info->clntinfo->before_forward.user;
		} else {
			user = conn_info->clntinfo->user;
		}
		sayf(conn_info->clntinfo->clientsocket,
				"331 Password required for %s.\r\n", user);
		conn_info->lcs->respcode = 331;
	}
	return CMD_HANDLED;
}

int std_pass(const char* args, struct conn_info_st* conn_info) {

	int offset;

	offset = strlen("PASS ");
	conn_info->clntinfo->pass = quotstrtok(args, WHITESPACES, &offset);

	return cmds_after_pass(conn_info);

}

static
int cmds_after_pass(struct conn_info_st* conn_info) {
	int ret;

	/* we are not yet connected to a server */
	ret = login(conn_info->clntinfo, LOGIN_ST_FULL);
	if (ret == CMD_ABORT) {
		return ret;
	}
	if (ret < 0) {
		/* login failed - do not print an error
		 * message */
		/* conn_info->clntinfo->serversocket = ss = -1; */
		return CMD_ERROR;
	}
	return CMD_HANDLED;
}


static
int cmds_after_fwpass(struct conn_info_st* conn_info) {
	if (fw_validate(conn_info->clntinfo->fw_auth.user,
			conn_info->clntinfo->fw_auth.pass) == 0) {

		say(conn_info->clntinfo->clientsocket,
				"230 Login to firewall successful\r\n");
	} else {
		say(conn_info->clntinfo->clientsocket,
				"530 Login failed.\r\n");
		return CMD_ERROR;
	}
	return CMD_HANDLED;
}


int fw_open(const char* args, struct conn_info_st* conn_info) {
	struct destination_t destination;
	char* space = (char*) 0;

	if (args) {
		space = strchr(args, ' ');
	}

	if ( !args || ! space || *(space + 1) == '\0') {
		say(conn_info->clntinfo->clientsocket,
					"530-Not a valid password\r\n"
					"530 Login failed.\r\n");
		return CMD_ERROR;
	}

	destination = fw_auth_parse_host_port(space + 1);

	if (destination.hostname == (char*) 0) {
		say(conn_info->clntinfo->clientsocket,
					"530-Not a valid destination\r\n"
					"530 Login failed.\r\n");
		return CMD_ERROR;
	}

	conn_info->clntinfo->destination = destination.hostname;
	conn_info->clntinfo->destinationport = destination.port;

	/* I don't know if 332 is the correct code */
	say(conn_info->clntinfo->clientsocket,
			"220 Welcome. Please proceed.\r\n");
	return CMD_HANDLED;
}

int fw_site(const char* args, struct conn_info_st* conn_info) {
	return fw_open(args, conn_info);
}

static
int fw_set_user(const char* args, struct conn_info_st* conn_info) {
	/* just chop off the command and put everything in the user name */
	if ( ! args || *(args + strlen("USER ")) == '\0') {
		say(conn_info->clntinfo->clientsocket,
				"530-Not a valid user name\r\n"
				"530 Login failed.\r\n");
		return CMD_ERROR;
	}

	conn_info->clntinfo->user = strdup(args + strlen("USER "));
	enough_mem(conn_info->clntinfo->user);
	return CMD_HANDLED;
}

int fw_user(const char* args, struct conn_info_st* conn_info) {
	if (fw_set_user(args, conn_info) != CMD_HANDLED) {
		return CMD_ERROR;
	}
	return cmds_after_user(conn_info);
}

static
int fw_set_pass(const char* args, struct conn_info_st* conn_info) {
	/* just chop off the command and put everything in the password */
	if ( ! args || *(args + strlen("PASS ")) == '\0') {
		say(conn_info->clntinfo->clientsocket,
				"530-Not a valid password\r\n"
				"530 Login failed.\r\n");
		return CMD_ERROR;
	}

	conn_info->clntinfo->pass = strdup(args + strlen("PASS "));
	enough_mem(conn_info->clntinfo->pass);
	return CMD_HANDLED;
}

int fw_pass(const char* args, struct conn_info_st* conn_info) {
	if (fw_set_pass(args, conn_info) != CMD_HANDLED) {
		return CMD_ERROR;
	}
	return cmds_after_pass(conn_info);
}

static
int fw_set_fwpass(const char* args, struct conn_info_st* conn_info) {
	/* just chop off the command and put everything in the password */
	char* space = (char*) 0;
	if (args) {
		space = strchr(args, ' ');
	}
	if ( ! args || ! space || *(space + 1) == '\0') {
		say(conn_info->clntinfo->clientsocket,
					"530-Not a valid password\r\n"
					"530 Login failed.\r\n");
		return CMD_ERROR;
	}
	conn_info->clntinfo->fw_auth.pass = strdup(space + 1);
	enough_mem(conn_info->clntinfo->fw_auth.pass);
	return CMD_HANDLED;
}

int fw_fwpass(const char* args, struct conn_info_st* conn_info) {
	if (fw_set_fwpass(args, conn_info) != CMD_HANDLED) {
		return CMD_ERROR;
	}
	return cmds_after_fwpass(conn_info);
}

static
int fw_set_fwuser(const char* args, struct conn_info_st* conn_info) {
	/* just chop off the command and put everything in the user name */
	char* space = (char*) 0;
	if (args) {
		space = strchr(args, ' ');
	}
	if ( ! args || ! space || *(space + 1) == '\0') {
		say(conn_info->clntinfo->clientsocket,
					"530-Not a valid user name\r\n"
					"530 Login failed.\r\n");
		return CMD_ERROR;
	}
	conn_info->clntinfo->fw_auth.user = strdup(space + 1);
	enough_mem(conn_info->clntinfo->fw_auth.user);
	return CMD_HANDLED;
}

int fw_fwuser(const char* args, struct conn_info_st* conn_info) {
	if (fw_set_fwuser(args, conn_info) != CMD_HANDLED) {
		return CMD_ERROR;
	}
	say(conn_info->clntinfo->clientsocket,
			"331 User name okay, send password.\r\n");
	return CMD_HANDLED;
}


int fw_user_type8(const char* args, struct conn_info_st* conn_info) {
	/* expecting "USER fwuser@real.host.name" */
	char* cmd_copy = strdup(args);
	char* atsign;
	struct destination_t destination = { (char*) 0, 0 };
	enough_mem(cmd_copy);

	atsign = strchr(cmd_copy, '@');
	if (atsign) {
		destination = fw_auth_parse_host_port(atsign + 1);

		*atsign ='\0';
		if (fw_fwuser(cmd_copy, conn_info) != CMD_HANDLED) {
			free(cmd_copy);
			return CMD_ERROR;
		}
	}
	free(cmd_copy);
	if (! atsign || destination.hostname == (char*) 0) {
		say(conn_info->clntinfo->clientsocket,
				"530-Not a valid user name\r\n"
				"530 Login failed.\r\n");
		return CMD_ERROR;
	}
	conn_info->clntinfo->destination = destination.hostname;
	conn_info->clntinfo->destinationport = destination.port;
	return CMD_HANDLED;
}

int fw_user_type7(const char* args, struct conn_info_st* conn_info) {
	char* user, *fwuser, *destchar;
	int offset = strlen("USER ");
	struct destination_t destination;

	user = quotstrtok_prepend("USER ", args, "@", &offset);
	if ( fw_set_user(user, conn_info) != CMD_HANDLED) {
		free(user);
		return CMD_ERROR;
	}
	free(user);

	fwuser = quotstrtok_prepend("USER ", args, "@", &offset);
	if ( !fwuser || fw_set_fwuser(fwuser, conn_info) != CMD_HANDLED) {
		if (! fwuser) {
			say(conn_info->clntinfo->clientsocket,
					"550 USER not recognized\r\n");
		}
		free(fwuser);
		return CMD_ERROR;
	}
	free(fwuser);
	destchar = quotstrtok(args, "@\n", &offset);

	destination = fw_auth_parse_host_port(destchar);
	free(destchar);
	if ( ! destination.hostname ) {
		say(conn_info->clntinfo->clientsocket,
				"550 USER not recognized\r\n");
		return CMD_ERROR;
	}
	conn_info->clntinfo->destination = destination.hostname;
	conn_info->clntinfo->destinationport = destination.port;

	return cmds_after_user(conn_info);
}

int fw_pass_type7(const char* args, struct conn_info_st* conn_info) {
	char* pass, *fwpass, *argscopy;
	const char* last_at;
	int offset;

	/* we have pass@fwpass but pass is likely to contain a "@" sign.
	 * So we find the last @ sign and everything before is the
	 * destination password, everything after is the firewall password */

	last_at = strrchr(args, '@');
	if (! last_at) {
		say(conn_info->clntinfo->clientsocket,
				"550 PASS not recognized\r\n");
		return CMD_ERROR;
	}

	offset = 0;
	fwpass = quotstrtok_prepend("PASS ", last_at, "\n", &offset);
	if ( !fwpass || fw_set_fwpass(fwpass, conn_info) != CMD_HANDLED) {
		if (! fwpass) {
			say(conn_info->clntinfo->clientsocket,
					"550 PASS not recognized\r\n");
		}
		free(fwpass);
		return CMD_ERROR;
	}
	free(fwpass);

	argscopy = strdup(args);
	enough_mem(argscopy);

	/* the at sign is at
	 * 	args [ last_at - args ]
	 * and, since argscopy is a copy of args, it is at
	 * 	argscopy [ last_at - args ]
	 * as well.
	 */
	argscopy[ last_at - args ] = '\0';

	/* But if we set a \0 at the location of the at-sign, the remaining
	 * part is "PASS destpass" and this is what we're looking for */
	pass = argscopy;
	if ( !pass || fw_set_pass(pass, conn_info) != CMD_HANDLED) {
		if (! pass) {
			say(conn_info->clntinfo->clientsocket,
					"550 PASS not recognized\r\n");
		}
		free(pass);
		return CMD_ERROR;
	}
	free(pass);

	if (fw_validate(conn_info->clntinfo->fw_auth.user,
			conn_info->clntinfo->fw_auth.pass) == 0) {
		say(conn_info->clntinfo->clientsocket,
				"550 Login incorrect\r\n");
		return CMD_ERROR;
	}
	return cmds_after_pass(conn_info);
}

int fw_login_type2(const char* args, struct conn_info_st* conn_info) {
	static int stage;

	if (stage == 0) {
		if (fw_user(args, conn_info) == CMD_HANDLED) {
			stage++;
		} else {
			stage = 0;
		}
	}
	if (stage == 1) {
		if (std_user_split(args, conn_info) == CMD_HANDLED) {
			stage++;
		}
	}

	return 0;
}


int fw_user_type9(const char* args, struct conn_info_st* conn_info) {
/*           "USER user@real.host.name fwuser"        */
	char* remoteuser;
	char* fwuser;
	int offset = strlen("USER ");

	remoteuser = quotstrtok_prepend("USER ", args, WHITESPACES, &offset);
	if (std_user_split(remoteuser, conn_info) != CMD_HANDLED) {
		free(remoteuser);
		return CMD_ERROR;
	}
	free(remoteuser);

	fwuser = quotstrtok_prepend("USER ", args, WHITESPACES, &offset);
	if (fw_set_fwuser(fwuser, conn_info) != CMD_HANDLED) {
		free(fwuser);
		return CMD_ERROR;
	}
	free(fwuser);

	return CMD_HANDLED;
}


int fw_pass_type9(const char* args, struct conn_info_st* conn_info) {
	/* Just register the password */

	if (fw_set_pass(args, conn_info) != CMD_HANDLED) {
		return CMD_ERROR;
	}
	say(conn_info->clntinfo->clientsocket,
					"332 Need account for login.\r\n");
	return CMD_HANDLED;
}

int fw_acct_type9(const char* args, struct conn_info_st* conn_info) {
	if (fw_set_fwpass(args, conn_info) != CMD_HANDLED) {
		return CMD_ERROR;
	}
	return cmds_after_pass(conn_info);
}


