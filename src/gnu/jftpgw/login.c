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
#include "fw_auth_cmds.h"
#include <sys/mman.h>

#define ANON_USERS " anonymous ftp "

extern int timeout;
extern struct log_cmd_st lcs;
extern struct hostent_list* hostcache;
extern struct conn_info_st conn_info;
extern struct serverinfo srvinfo;

static int login_setforward_user(struct clientinfo*);
static int login_setforward_pass(struct clientinfo*);
static int login_connect(struct clientinfo*);
static int login_mayconnect(struct clientinfo*);
static int login_init_connection(struct clientinfo*);
static int login_connected_setup(struct clientinfo*);

static int login_auth(struct clientinfo*);
static int login_readwelcome(struct clientinfo*);
static int login_sendauth_user(struct clientinfo*);
static int login_sendauth_pass(struct clientinfo*);
static int login_finish_login(struct clientinfo*);
static int login_loggedin_setup(struct clientinfo*);

static int login_failed(struct clientinfo*);


int handle_login(struct clientinfo* clntinfo) {
	struct cmdhandlerstruct *cmdhandler;
	char *buffer = 0;
	int ss, cs;
	int i, expected;

	conn_info.lcs = &lcs;
	conn_info.clntinfo = clntinfo;
	ss = clntinfo->serversocket;
	cs = clntinfo->clientsocket;

	if (clntinfo->transparent == TRANSPARENT_YES
		/* we are connected */
		&&
	    config_compare_option("logintime", "connect")) {

		if (config_get_ioption("loginstyle", 0) != 0) {
			log(5, "A login at the connection time only works with loginstyle == 0, setting loginstyle = 0");

			config_option_list_delete("loginstyle");
			config_option_list_add("loginstyle", "0");
		}
	}

	cmdhandler = &login_auth_funcs
				[ config_get_ioption("loginstyle", 0) ][0];

	expected = QUITFUNC + 1;    /* skip reset and quit function */
	while (1) {
		if (timeout) {
			log(2, "Timeout in %s line %d\n", __FILE__ ,__LINE__);
			return -1;
		}
		if (buffer) {
			free(buffer);
			buffer = 0;
		}

		buffer = readline(cs);
		lcs.cmd = (char*) 0;

		if (buffer) {
			lcs.cmd = buffer;
			free(lcs.method);
			i = 0;
			lcs.method = quotstrtok(lcs.cmd, WHITESPACES, &i);

			if (buffer[0] == '\0') {
				/* empty line. Prevent logging of the
				 * command */
				free(buffer);
				buffer = 0;
				continue;
			}
			/* log the command */
			log_cmd(&lcs);
			free(lcs.method); lcs.method = (char*) 0;
			lcs.respcode = 0;
			lcs.transferred = 0;
		}

		if (!buffer) {
			if (timeout) {
				log(2, "Timeout in %s line %d\n", __FILE__
						,__LINE__);
				err_time_readline(cs);
			} else {
				err_readline(cs);
			}
			return -1;
		}

		log(8, "Expecting %s", cmdhandler[expected].cmd);
		if (strcasestr(buffer, "PASS ") == (char*) 0) {
			log(8, "Got: %s", buffer);
		} else {
			log(8, "Got the password");
		}

		/* check for QUIT */
		if (checkbegin(buffer, "QUIT")) {
			int ret = (cmdhandler[QUITFUNC].func)
						(buffer, &conn_info);
			ret = (cmdhandler[RESETFUNC].func)
						(buffer, &conn_info);
			free(buffer);
			/* return 1 to prevent the proxy from entering
			 * handle_cmds */
			return 1;
		}
		if (cmdhandler[expected].cmd 
			&& checkbegin(buffer, cmdhandler[expected].cmd)) {

			int ret = (cmdhandler[expected].func)
						(buffer, &conn_info);
			memset(buffer, 0, strlen(buffer));
			switch (ret) {
				case CMD_DONE:
					/* we're done - logged in */
					/* CMD_DONE is returned if the USER
					 * command got a 230 response back
					 * */
					break;
				case CMD_HANDLED:
					/* expecting the next */
					expected++;
					break;
				case CMD_ERROR:
					/* reset counter, skip reset and
					 * quit function */
					ret = (cmdhandler[RESETFUNC].func)
							(buffer, &conn_info);
					expected = QUITFUNC + 1;
					/* errors are handled by the
					 * command handler functions
					 * */
					break;
				case CMD_ABORT:
					return -1;
					break;
			}
			/* found and called proper function */
		} else {
			sayf(clntinfo->clientsocket,
				"530 Login incorrect. Expected %scommand\r\n",
				cmdhandler[expected].cmd);
			/* reset counter, skip reset and quit function */
			(cmdhandler[RESETFUNC].func)
						(buffer, &conn_info);
			expected = QUITFUNC + 1;
		}
		if (clntinfo->login.stage == LOGIN_ST_FULL) {
			/* we are done */
			free(buffer); buffer = (char*) 0;
			return 0;
		}
	}
	/* lcs.host and lcs.user  are freed at the termination of the
	 * programm */
}

int login(struct clientinfo* clntinfo, int stage) {
	int ret = 0;

	if (! clntinfo->user && stage >= LOGIN_ST_USER) {
		say(clntinfo->clientsocket, "500 Error logging in\r\n");
		log(4, "No user was set - Cannot proceed");
		return CMD_ERROR;
	}

	if ((ret = login_setforward_user(clntinfo)) < 0) {
		/* the error is logged and say()ed */
		return CMD_ERROR;
	}

	if (! clntinfo->destination) {
		say(clntinfo->clientsocket, "500 Error logging in\r\n");
		log(4, "No destination was set - Cannot proceed");
		if (config_compare_option("logintime", "connect")) {
			log(4, "This may be because of your logintime --> connect setting");
		}
		return CMD_ERROR;
	}

	if (stage >= LOGIN_ST_CONNECTED) {
		ret = login_connect(clntinfo);
		if (ret) { return ret; }
	}
	if (stage >= LOGIN_ST_USER) {
		ret = login_sendauth_user(clntinfo);
		if (ret) { return ret; }
	}
	if (stage >= LOGIN_ST_FULL) {
		ret = login_connect(clntinfo);
		if (ret) { return ret; }
		ret = login_setforward_pass(clntinfo);
		if (ret) { return ret; }
		ret = login_auth(clntinfo);
		if (ret) {
			int ret2;
			if ((ret2 = login_failed(clntinfo)) < 0) {
				return ret2;
			}
			return ret;
		} else {
			config_destroy_sectionconfig();
		}
		ret = login_loggedin_setup(clntinfo);
		if (ret) { return ret; }
	}
	return CMD_HANDLED;
}


static
int login_connect(struct clientinfo* clntinfo) {
	int ret;

	if (clntinfo->login.stage >= LOGIN_ST_CONNECTED) {
		return CMD_HANDLED;
	}

	if ((ret = login_init_connection(clntinfo)) < 0) {
		/* the error is logged and say()ed */
		return ret;
	}

	if ((ret = login_connected_setup(clntinfo)) < 0) {
		/* the error is logged and say()ed */
		return ret;
	}

	clntinfo->login.stage = LOGIN_ST_CONNECTED;
	return CMD_HANDLED;
}


static
int login_auth(struct clientinfo* clntinfo) {
	int ret;

	if (clntinfo->login.stage >= LOGIN_ST_LOGGEDIN) {
		return CMD_HANDLED;
	}

	if (clntinfo->login.stage < LOGIN_ST_USER) {
		if ((ret = login_sendauth_user(clntinfo)) < 0) {
			clntinfo->login.stage = LOGIN_ST_CONNECTED;
			return ret;
		}
		clntinfo->login.stage = LOGIN_ST_USER;
	}

	if ((ret = login_sendauth_pass(clntinfo)) < 0) {
		clntinfo->login.stage = LOGIN_ST_CONNECTED;
		return ret;
	}

	if ((ret = login_finish_login(clntinfo)) < 0) {
		clntinfo->login.stage = LOGIN_ST_CONNECTED;
		return ret;
	}

	return CMD_HANDLED;
}


static
int login_mayconnect(struct clientinfo* clntinfo) {
	int allowed = 0;
	unsigned long int target;
	unsigned long int host_ip;
	unsigned long int client_ip, server_ip;
	int tags;

	allowed = 0;

	/* we don't know yet if clntinfo->destination is a hostname like
	 * "somehost.foo.com" or if it is an IP in a char* like
	 * "212.117.232.20"
	 *
	 * So call inet_addr(), it should make the decision  :-)
	 *
	 * */
	target = inet_addr(clntinfo->destination);

	/* get the client_ip by asking information about the socket the
	 * client is connected */
	client_ip = get_uint_ip(GET_IP_CLIENT, clntinfo);
	server_ip = (unsigned long int) UINT_MAX;

	if (target == (unsigned long int) UINT_MAX) {
		/* clntinfo->destination may be a hostname, but it needn't.
		 * It may also be an invalid IP: "393.39.239.500" or
		 * anything else. Look it up.
		 * */
		host_ip = hostent_get_ip(&hostcache, clntinfo->destination);
		if (host_ip != (unsigned long int) UINT_MAX) {
			/* successful lookup */
			server_ip = host_ip;
		} else {
			/* it's not a valid IP and we could not look it up,
			 * so we could not get valid information about the
			 * destination host */
			log(7, "Nonsense destination (no IP and could not look up hostname): %s",
					clntinfo->destination);
			log(8, "Please check your nameserver configuration. This may also happen if your chroot-environment does not contain the necessary files which the libc needs for a lookup");
			allowed = 0;
		}
	} else {
		/* okay, it's an IP */
		server_ip = inet_addr(clntinfo->destination);
	}

	if (server_ip != (unsigned long) UINT_MAX) {
		/* we could get a valid IP, now we can evaluate if client
		 * is allowed to connect to the destination host */
		log(9, "Checking all tags");
		tags = TAG_ALL_NOT_FORWARDED;
		if (clntinfo->before_forward.user) {
			/* if we are treating a forward, add this value, it
			 * won't come up in the option list otherwise */
			tags |= TAG_FORWARDED;
		}
		/* checking of the configuration works such:
		 *
		 * no forward:   do not evaluate <forwarded>
		 *               always check the normal values
		 *
		 * forward:      evaluate <forwarded>
		 *               if (!in_forwarded_tag) {
		 *     (1)             check before_forward.* values
		 *               } else {
		 *     (2)             check normal values
		 *               }
		 */
		if (clntinfo->before_forward.user) {
			/* Yes, there was a forward applied */
			config_shrink_config(client_ip,
				/* this are the original values */
				/* see above (1) */
				clntinfo->before_forward.dest_ip,
				clntinfo->before_forward.destination,
				clntinfo->before_forward.destinationport,
				clntinfo->before_forward.user,
				/* these are the values set by the forward */
				/* pass them as well, if we are in the
				 * forwarded_tag, config_match_section()
				 * will overwrite the previous with the
				 * following values */
				server_ip,
				clntinfo->destination,
				clntinfo->destinationport,
				clntinfo->user,
				0,		/* set no specific time */
				clntinfo->proxy_ip,
				clntinfo->proxy_port,
				srvinfo.servertype,
				&hostcache,
				tags);
		} else {
			/* not a forward */
			config_shrink_config(client_ip,
				server_ip,
				clntinfo->destination,
				clntinfo->destinationport,
				clntinfo->user,
				clntinfo->before_forward.dest_ip,
				clntinfo->before_forward.destination,
				clntinfo->before_forward.destinationport,
				clntinfo->before_forward.user,
				0,		/* set no specific time */
				clntinfo->proxy_ip,
				clntinfo->proxy_port,
				srvinfo.servertype,
				&hostcache,
				tags);
		}

		allowed = strcmp(config_get_option("access"), "allow") == 0;
	}

	if (!allowed) {
		say(clntinfo->clientsocket, "531 You are not allowed to connect to that host.\r\n");
		log(8, "Not allowed to connect to %s", clntinfo->destination);
		lcs.respcode = 531;
		return CMD_ERROR;
	}
	/* if the client was allowed, save the clients original IP */
	if (!clntinfo->before_forward.user) {
		clntinfo->before_forward.dest_ip = server_ip;
	}
	return allowed;  /* a positive int */
}


static
int login_setforward_user(struct clientinfo* clntinfo) {

	const char* forward;
	struct slist_t* forward_list, *forward_list_cur;
	char* tmp;
	int newsize;
	int config_state = TAG_GLOBAL | TAG_FROM | TAG_PORT
			 | TAG_TO | TAG_TIME | TAG_SERVERTYPE
			 | TAG_PROXYIP | TAG_PROXYPORT;

	/* this is already a forward */
	if ( clntinfo->before_forward.user ) {
		return CMD_HANDLED;
	}

	if ( clntinfo->user ) {
		config_state |= TAG_USER;
	}

	config_shrink_config(get_uint_ip(GET_IP_CLIENT, clntinfo),
			get_uint_ip(GET_IP_SERVER, clntinfo),
			clntinfo->destination,
			clntinfo->destinationport,
			clntinfo->user,
			-1,          /* before_forward.dest_ip */
			(char*) 0,   /* before_forward.destination */
			0,           /* before_forward.destinationport */
			(char*) 0,   /* before_forward.user */
			0,		/* set no specific time */
			clntinfo->proxy_ip,
			clntinfo->proxy_port,
			srvinfo.servertype,
			&hostcache,
			config_state);

	if (config_compare_option("access", "allow") == 0) {
		return CMD_ERROR;
	}

	/* see if there is an option for the forward */
	forward = config_get_option("forward");

	if (! forward) {
		/* no forward */
		lcs.userlogin = strnulldup(clntinfo->user);
		lcs.usereffective = strnulldup(clntinfo->user);
		if (lcs.userforwarded) {
			free(lcs.userforwarded);
		}
		lcs.userforwarded = strdup("<no forward>");
		enough_mem(lcs.userforwarded);
		return CMD_HANDLED;
	}

	/*
	 * <user ftp>
	 *    forward johnfred@fooserver.com
	 *    forward johnfred@fooserver.com,3949,p
	 *    forward johnfred@fooserver.com     *     johnspass
	 *    forward johnfred@fooserver.com   JiKe94  johnspass
	 * </user>
	 *    forward *@fooserver.com:2378
	 */

	/* split the list into tokens */
	if ( ! (forward_list = config_split_line( forward, WHITESPACES )) ) {
		return CMD_HANDLED;
	}

	forward_list_cur = forward_list;

	/* read and save the values */
	clntinfo->forward.login = strdup(forward_list_cur->value);
	enough_mem(clntinfo->forward.login);

	forward_list_cur = forward_list_cur->next;

	if (forward_list_cur && forward_list_cur->value) {
		clntinfo->forward.accept_pw = strdup(forward_list_cur->value);
		enough_mem(clntinfo->forward.accept_pw);
	} else {
		clntinfo->forward.accept_pw = (char*) 0;
	}

	if (forward_list_cur) {
		forward_list_cur = forward_list_cur->next;
	}
	if (forward_list_cur && forward_list_cur->value) {
		clntinfo->forward.send_pw = strdup(forward_list_cur->value);
		enough_mem(clntinfo->forward.send_pw);
	} else {
		clntinfo->forward.send_pw = (char*) 0;
	}
	/* destroy the list again, we have saved the values into
	 * clntinfo->forward.<field>    */
	slist_destroy(forward_list);

	/* delete those configuration values that are evaluated by the
	 * parsing routine  -  they are not necessary anymore */
	config_option_list_delete("transparent-forward");
	config_option_list_delete("forward");

	/* back up */
	clntinfo->before_forward.user = strnulldup(clntinfo->user);
	clntinfo->before_forward.destination
			= strnulldup(clntinfo->destination);
	clntinfo->before_forward.destinationport = clntinfo->destinationport;
	clntinfo->before_forward.dest_ip = get_uint_ip(GET_IP_SERVER,clntinfo);

	/* special case: We don't have set a username. The client could have
	 * used logintime == connect and has a forward that already matches */
	if ( ! clntinfo->before_forward.user ) {
		clntinfo->before_forward.user = malloc(2);
		enough_mem(clntinfo->before_forward.user);
		clntinfo->before_forward.user[0] = '*';
		clntinfo->before_forward.user[1] = '\0';
	}

	/* if there was only a new destination but no new username given,
	 * prepend the old one */
	if ( ! strchr(clntinfo->forward.login, '@')
			&& clntinfo->before_forward.user) {
		newsize = strlen(clntinfo->before_forward.user)
			  + 1 /* @ */
			  + strlen(clntinfo->forward.login)
			  + 1 /* term */;

		tmp = (char*) malloc(newsize);
		enough_mem(tmp);
		snprintf( tmp, newsize, "%s@%s", clntinfo->before_forward.user,
						 clntinfo->forward.login);
		free(clntinfo->forward.login);
		clntinfo->forward.login = tmp;
	}

	if (clntinfo->forward.login[0] == '*' &&
	    clntinfo->forward.login[1] == '@') {
		/* passauth */
		clntinfo->forward.passauth = 1;
	}

	/* call the parsing routine and let it set the values */
	if (set_userdest(clntinfo->forward.login, 0, clntinfo, "@,: \t") < 0) {
		return CMD_ERROR;
	}

	/* set values for log info struct */
	lcs.userlogin = strnulldup(clntinfo->before_forward.user);
	lcs.usereffective = strnulldup(clntinfo->user);
	lcs.userforwarded = strnulldup(clntinfo->forward.login);

	/* shrink the configuration again - with TAG_FORWARDED this time */

	config_shrink_config(get_uint_ip(GET_IP_CLIENT, clntinfo),
			/* these are the original values */
			/* see above (1) */
			clntinfo->before_forward.dest_ip,
			clntinfo->before_forward.destination,
			clntinfo->before_forward.destinationport,
			clntinfo->before_forward.user,
			/* these are the values set by the forward */
			/* pass them as well, if we are in the
			 * forwarded_tag, config_match_section()
			 * will overwrite the previous with the
			 * following values */
			get_uint_ip(GET_IP_SERVER, clntinfo),
			clntinfo->destination,
			clntinfo->destinationport,
			clntinfo->user,

			0,		/* set no specific time */

			clntinfo->proxy_ip,
			clntinfo->proxy_port,

			srvinfo.servertype, &hostcache,
			TAG_ALL);

	return CMD_HANDLED;
}


static
int login_setforward_pass(struct clientinfo* clntinfo) {
	/* check if the password matches */
	if (clntinfo->forward.accept_pw) {
		if (strcmp(clntinfo->forward.accept_pw, "*") == 0
			/* allow any pw but use destpw on dest */
			|| cryptcmp(clntinfo->forward.accept_pw, clntinfo->pass)==0) {
			/* check for the exact password */

			clntinfo->pass = realloc(clntinfo->pass,
					strlen(clntinfo->forward.send_pw) + 1);
			enough_mem(clntinfo->pass);
			strncpy(clntinfo->pass, clntinfo->forward.send_pw,
				strlen(clntinfo->forward.send_pw) + 1
					/* always true */);
		} else if (cryptcmp(clntinfo->forward.accept_pw,
						clntinfo->pass) != 0){
			say(clntinfo->clientsocket, "530 Login incorrect\r\n");
			return CMD_ERROR;
		}
	}
	return CMD_HANDLED;
}


#define ERR_STR_P2 "Error connecting to %s port %d: %s\r\n"

static
int login_init_connection(struct clientinfo* clntinfo) {
	int ss, cs = clntinfo->clientsocket;
	int ret, err;

	if ((ret = login_mayconnect(clntinfo)) < 0) {
		/* the error is logged and say()ed */
		return ret;
	}

	/* connect */
	ss = openportname(clntinfo->destination,     /* dest name */
			  clntinfo->destinationport, /* dest port */
			                             /* source address */
			  config_get_addroption("controlserveraddress",
								INADDR_ANY),
			  (struct portrangestruct*) 0); /* source port */
	if (ss < 0) {
		err = errno;
		sayf(cs, "500 "ERR_STR_P2,
				clntinfo->destination,
				clntinfo->destinationport,
				strerror(err));
		log(5, ERR_STR_P2, clntinfo->destination,
				   clntinfo->destinationport,
				   strerror(err));
		return CMD_ERROR;
	}

	/* connected */
	clntinfo->serversocket = ss;

	return CMD_HANDLED;
}
#undef ERRSTR


static
int login_connected_setup(struct clientinfo* clntinfo) {
	int ret;
	char* buffer;

	if ((ret = login_readwelcome(clntinfo)) < 0) {
		return ret;
	}

	if (/*clntinfo->transparent == TRANSPARENT_YES*/
		/* we are connected */
		/*&&*/
	    config_compare_option("logintime", "connect")) {
		say(clntinfo->clientsocket,clntinfo->login.welcomemsg.fullmsg);

		clntinfo->login.welcomemsg.fullmsg = (char*) 0;
		clntinfo->login.welcomemsg.lastmsg = (char*) 0;
	}

	say(clntinfo->serversocket, "SYST\r\n");
	buffer = ftp_readline(clntinfo->serversocket);
	if (!buffer) {
		if (timeout) {
			log(1, "Timeout in %s line %d\n", __FILE__ ,__LINE__);
			err_time_readline(clntinfo->clientsocket);
			return CMD_ABORT;
		} else {
			err_readline(clntinfo->clientsocket);
			return CMD_ABORT;
		}
	}
	if (!strlen(buffer)) {
		log(2, "The server did not respond to the initial SYST command");
		say(clntinfo->clientsocket,
				"The server did not respond correctly\r\n");
		free(buffer);
		return CMD_ERROR;
	}
	free(buffer);
	buffer = (char*) 0;

	return CMD_HANDLED;
}


static
int login_readwelcome(struct clientinfo *clntinfo) {
	/* Read the welcome line */
	clntinfo->login.welcomemsg = readall(clntinfo->serversocket);
	if (!clntinfo->login.welcomemsg.fullmsg) {
		/* an error occurred */
		if (timeout) {
			log(2, "Timeout in %s line %d\n", __FILE__ ,__LINE__);
			err_time_readline(clntinfo->clientsocket);
		} else {
			set_errstr("Server closed the connection");
			err_readline(clntinfo->clientsocket);
		}
		return CMD_ABORT;
	}

	log(9, "Connected to %s, got \"%s\" as welcome message",
		clntinfo->destination, clntinfo->login.welcomemsg.fullmsg);

	if (!checkbegin(clntinfo->login.welcomemsg.lastmsg, "220 ")) {
		log(2, "Not a valid FTP server response (%s)",
				clntinfo->login.welcomemsg.fullmsg);
		say(clntinfo->clientsocket,clntinfo->login.welcomemsg.fullmsg);

		free(clntinfo->login.welcomemsg.fullmsg);
		clntinfo->login.welcomemsg.fullmsg = (char*) 0;
		clntinfo->login.welcomemsg.lastmsg = (char*) 0;
		return CMD_ERROR;
	}
	return CMD_HANDLED;
}


static
int login_sendauth_user(struct clientinfo* clntinfo) {
	size_t sendbufsize, ret;
	char* sendbuf;

	if (clntinfo->login.stage >= LOGIN_ST_USER) {
		return CMD_HANDLED;
	}

	sendbufsize = strlen("USER \r\n") + strlen(clntinfo->user) + 1;
	sendbuf = (char*) malloc(sendbufsize);
	enough_mem(sendbuf);

	snprintf(sendbuf, sendbufsize, "USER %s\r\n", clntinfo->user);
	ret = say(clntinfo->serversocket, sendbuf);
	free(sendbuf);
	sendbuf = 0;
	if (ret < 0) {
		log(2, "Error writing the user name to the server: %s",
			strerror(errno));
		return CMD_ABORT;
	}

	clntinfo->login.authresp = readall(clntinfo->serversocket);

	if (!clntinfo->login.authresp.fullmsg) {
		if (timeout) {
			log(2, "Timeout in %s line %d\n", __FILE__ ,__LINE__);
			err_time_readline(clntinfo->clientsocket);
		} else {
			err_readline(clntinfo->clientsocket);
		}
		free(clntinfo->login.welcomemsg.fullmsg);
		clntinfo->login.welcomemsg.fullmsg = (char*) 0;
		clntinfo->login.welcomemsg.lastmsg = (char*) 0;
		return CMD_ABORT;
	}

	clntinfo->login.stage = LOGIN_ST_USER;
	return CMD_HANDLED;
}


static
int login_sendauth_pass(struct clientinfo* clntinfo) {
	size_t sendbufsize;
	int ret;
	char* sendbuf, *buffer;

	buffer = clntinfo->login.authresp.lastmsg;
	if ( !checkdigits(buffer, 331) && !checkdigits(buffer, 230)) {
		/* prepend the response from the server with an error code
		 * and return it to the client */
		sendbufsize = strlen(buffer) + 4 + 1;
		sendbuf = (char*) malloc(sendbufsize);
		enough_mem(sendbuf);
		snprintf(sendbuf, sendbufsize, "500 %s", buffer);
		say(clntinfo->clientsocket, sendbuf);
		free(sendbuf);

		log(7, "Got \"%s\" after sending the username.", buffer);
		free(clntinfo->login.authresp.fullmsg);
		free(clntinfo->login.welcomemsg.fullmsg);
		clntinfo->login.welcomemsg.fullmsg = (char*) 0;
		clntinfo->login.welcomemsg.lastmsg = (char*) 0;
		clntinfo->login.authresp.fullmsg   = (char*) 0;
		clntinfo->login.authresp.lastmsg   = (char*) 0;
		return CMD_ERROR;
	}

	if (!checkdigits(buffer, 230)) {
		char* userdup;
		size_t size;

		sendbufsize = strlen("PASS \r\n") + strlen(clntinfo->pass) + 1;
		sendbuf = (char*) malloc(sendbufsize);
		enough_mem(sendbuf);
		snprintf(sendbuf, sendbufsize, "PASS %s\r\n", clntinfo->pass);
		ret = say(clntinfo->serversocket, sendbuf);

		size = strlen(clntinfo->user) + 3;
		userdup = (char*) malloc( size );
		snprintf(userdup, size, " %s ", clntinfo->user);

		if (strstr(ANON_USERS, userdup)) {
			clntinfo->anon_user = clntinfo->pass;
			clntinfo->pass = (char*) 0;
		} else {
			memset(sendbuf, (char) 0, sendbufsize);
			clntinfo->anon_user = strdup("UNKNOWN");
		}
		free(sendbuf);
		free(userdup);
		sendbuf = userdup = (char*) 0;
		free(clntinfo->login.authresp.fullmsg);
		clntinfo->login.authresp.fullmsg   = (char*) 0;
		clntinfo->login.authresp.lastmsg   = (char*) 0;

		if (ret < 0) {
			log(2, "Error writing the password to the server: %s",
				strerror(errno));
			free(clntinfo->login.welcomemsg.fullmsg);
			clntinfo->login.welcomemsg.fullmsg = (char*) 0;
			clntinfo->login.welcomemsg.lastmsg = (char*) 0;
			return CMD_ABORT;
		}
	}
	return CMD_HANDLED;
}


static
int login_loggedin_setup(struct clientinfo* clntinfo) {

	/* we seem to have a successful login */
	log(7, "Logged in to %s as %s!",
		clntinfo->destination, clntinfo->user);

	/* initialize the log_cmd_st structure with the values that have to
	 * be set only once after a successful login */
	lcs.anon_user = clntinfo->anon_user;
	lcs.svrip = strdup(get_char_ip(GET_IP_SERVER, clntinfo));
	lcs.svrname = hostent_get_name(&hostcache, inet_addr(lcs.svrip));
	lcs.ifipsvr = strdup(conv_ip_to_char(clntinfo->addr_to_server));
	enough_mem(lcs.ifipsvr);
	lcs.svrlogin = clntinfo->destination;

	/* we don't need the configuration sections and the backup anymore */

	config_delete_master();
	config_delete_backup();

	/* get the throughput rate */
	clntinfo->throughput = config_get_foption("throughput", -1.0);

	clntinfo->addr_to_server =
			socketinfo_get_local_ip(clntinfo->serversocket);

	clntinfo->data_addr_to_client
				= config_get_addroption("dataclientaddress",
						clntinfo->addr_to_client);
	log(8, "Got %s as data client address",
	       inet_ntoa(*((struct in_addr*) &clntinfo->data_addr_to_client)));

	clntinfo->data_addr_to_server =
				config_get_addroption("dataserveraddress",
						clntinfo->addr_to_server);
	log(8, "Got %s as data server address",
	       inet_ntoa(*((struct in_addr*) &clntinfo->data_addr_to_server)));

	if (clntinfo->servermode == UNSPEC) {
		clntinfo->servermode = getservermode();
	}

	clntinfo->login.stage = LOGIN_ST_FULL;

	return CMD_HANDLED;
}


static
int login_finish_login(struct clientinfo* clntinfo) {
	char* buffer;

	if (stage_action("loggedin") < 0) {
		say(clntinfo->clientsocket, "421 Error setting up (see logfile)\r\n");
		return CMD_ABORT;
	}

	clntinfo->login.authresp = readall(clntinfo->serversocket);
	if (!clntinfo->login.authresp.fullmsg) {
		if (timeout) {
			log(2, "Timeout in %s line %d\n", __FILE__,
					__LINE__);
			err_time_readline(clntinfo->clientsocket);
		}
		else {
			err_readline(clntinfo->clientsocket);
		}
		if (clntinfo->login.welcomemsg.fullmsg) {
			free(clntinfo->login.welcomemsg.fullmsg);
		}
		return CMD_ABORT;
	}

	if (clntinfo->login.welcomemsg.fullmsg) {
		/* we have not yet sent the welcome message to the client */
		buffer = merge_responses(clntinfo->login.welcomemsg.fullmsg,
					clntinfo->login.authresp.fullmsg);
		/* free the welcome message */
		free(clntinfo->login.welcomemsg.fullmsg);
		clntinfo->login.welcomemsg.fullmsg
			= clntinfo->login.welcomemsg.lastmsg = (char*) 0;
	} else {
		/* if the welcome message is sent, just copy the
		 * authentication response */
		buffer = strdup(clntinfo->login.authresp.fullmsg);
		enough_mem(buffer);
	}

	lcs.respcode = getcode(clntinfo->login.authresp.lastmsg);

	if (!checkdigits(clntinfo->login.authresp.lastmsg, 230)) {
		say(clntinfo->clientsocket,
				clntinfo->login.authresp.lastmsg);
		log(8, "Got \"%s\" after sending the password",
				clntinfo->login.authresp.fullmsg);
		free(clntinfo->login.authresp.fullmsg);
		free(buffer);
		clntinfo->login.authresp.fullmsg   = (char*) 0;
		clntinfo->login.authresp.lastmsg   = (char*) 0;
		return CMD_ERROR;
	}
	free(clntinfo->login.authresp.fullmsg);
	clntinfo->login.authresp.fullmsg   = (char*) 0;
	clntinfo->login.authresp.lastmsg   = (char*) 0;

	/* Now the client receives the welcome message as well as the
	 * response of the server from the authentication with the same code
	 * at the beginning */
	say(clntinfo->clientsocket, buffer);
	free(buffer);
	buffer =0;

	return CMD_HANDLED;
}


static
int login_failed(struct clientinfo* clntinfo) {

	static int failed_logins;

	/* if there was a forward, restore the clntinfo fields from the
	 * before_forward values */

	if (clntinfo->before_forward.user) {
		free(clntinfo->user);
		clntinfo->user = clntinfo->before_forward.user;
		clntinfo->before_forward.user = (char*) 0;
	}
	if (clntinfo->before_forward.destination) {
		free(clntinfo->destination);
		clntinfo->destination = clntinfo->before_forward.destination;
		clntinfo->before_forward.destination = (char*) 0;

		clntinfo->destinationport
				= clntinfo->before_forward.destinationport;
	}

	failed_logins ++;

	if (failed_logins >= config_get_ioption("failedlogins", 3)) {
		return CMD_ABORT;
	}

	/* switch configuration - remove the shrinked one,
	 * set the backup into place. If the login failed,
	 * the backup is used */

	if (config_activate_backup() < 0) {
		log(6, "Backup could not be activated - can't continue with login procedure");
		return CMD_ABORT;
	}

	/* shrink the configuration again - this will update the option list */

	config_shrink_config(get_uint_ip(GET_IP_CLIENT, clntinfo),
			(unsigned long int) UINT_MAX,
			(char*) 0,   /* destination */
			0,           /* destinationport */
			(char*) 0,   /* user */
			(unsigned long int) UINT_MAX,
			(char*) 0,   /* before_forward.destination */
			0,           /* before_forward.destinationport */
			(char*) 0,   /* before_forward.user */
			0,           /* set no specific time */
			clntinfo->proxy_ip,
			clntinfo->proxy_port,
			srvinfo.servertype,
			&hostcache,
			TAG_CONNECTED);

	return CMD_HANDLED;
}





/* login function flow

shrinks bindport.c: childsetup()
	handle_login {login.c 51}
	login {login.c 168}
shrinks		login_setforward_user {login.c 334}
		login_connect {login.c 214}
			login_init_connection {login.c 499}
shrinks				login_mayconnect {login.c 263}
			login_connected_setup {login.c 528}
				login_readwelcome {login.c 585}
		login_sendauth_user {login.c 618}
		login_setforward_pass {login.c 472}
		login_auth {login.c 237}
			login_sendauth_user ... {70}
			login_sendauth_pass {login.c 661}
			login_loggedin_setup {login.c 797}
				login_successfulp {login.c 731}
shrinks		login_failed {login.c 813}


We send USER, server sends 230:
	cmds_after_user 
		sets LOGIN_ST_LOGGEDIN and calls
		login(LOGIN_ST_FULL)
		sets CMD_DONE to tell the handler that the login
		procedure is done


*/
