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

#include <fcntl.h>
#include <ctype.h>
#include <sys/time.h>
#include <arpa/telnet.h> /* for IAC, IP */
#include "jftpgw.h"


/* include the different command sets */

#include "cmds.h"
#include "std_cmds.h"

extern int timeout;
extern struct hostent_list* hostcache;

struct log_cmd_st lcs = { NULL, NULL, NULL, 0, 0 };
struct cmdhandlerstruct *cmdhandler;
struct conn_info_st conn_info = { 0, 0 };

int checkforabort(struct clientinfo*);

int handle_cmds(struct clientinfo *clntinfo) {
	char *buffer = 0;
	int ss, cs;
	int i;

	conn_info.lcs = &lcs;
	conn_info.clntinfo = clntinfo;
	clntinfo->cachefd = -1;
	log(9, "setting dataclientsock to -1 (initial)");
	clntinfo->dataclientsock = clntinfo->dataserversock = -1;
	clntinfo->dataport = socketinfo_get_local_port(clntinfo->clientsocket) - 1;
	log(9, "dataport is set to %d", clntinfo->dataport);
	clntinfo->portcmd = (char*) 0;
	ss = clntinfo->serversocket;
	cs = clntinfo->clientsocket;

	/*
            Server host name                                     svrname
            Server IP address                                    svrip
            Server host name as specified in the login           svrlogin
            Client host name                                     clntname
            Client host IP address                               clntip
            Interface IP to the client                           ifipclnt
            Interface IP to the server                           ifipsvr
	*/


	lcs.clntip = strdup(get_char_ip(GET_IP_CLIENT, clntinfo));
	enough_mem(lcs.clntip);
	lcs.clntname = hostent_get_name(&hostcache, inet_addr(lcs.clntip));

	lcs.ifipclnt = strdup(conv_ip_to_char(clntinfo->addr_to_client));
	enough_mem(lcs.ifipclnt);

	lcs.filename = (char*) 0;

	while (1) {
contin:
		lcs.service = "ftp";
		cmdhandler = &std_cmdhandler[0];

		ss = clntinfo->serversocket;

		if (timeout) {
			log(2, "Timeout in %s line %d\n", __FILE__ ,__LINE__);
			return -1;
		}
		if (buffer) {
			/* log the command */
			log_cmd(&lcs);
			free(buffer);
			free(lcs.method);
			buffer = 0;
			lcs.respcode = 0;
			lcs.transferred = 0;
		}

		errno = 0;
		buffer = readline(cs);

		if (!buffer && timeout) {
			log(2, "Timeout in %s line %d\n", __FILE__ ,__LINE__);
			err_time_readline(cs);
			return -1;
		}
		if (!buffer) {
			if (!errno) {
				set_errstr("The client has probably closed the connection without saying QUIT");
			} else {
				set_errstr(strerror(errno));
			}
			err_readline(cs);
			return -1;
		}

		if (buffer[0] == '\0') {
			/* empty line. Prevent logging of the command */
			free(buffer);
			buffer = 0;
			goto contin;
		}

		lcs.cmd = buffer;

		i = 0;
		lcs.method = quotstrtok(lcs.cmd, WHITESPACES, &i);

		/* check for passcmds and dontpasscmds options */
		if (passcmd_check(lcs.method) == 0) {
			say(clntinfo->clientsocket,
				"500 Command not implemented\r\n");
			log(8, "rejected command %s", lcs.method);
			goto contin;
		}

		/* Retrieve a file */
		clntinfo->mode = RETR;
		i = 0;

		while (cmdhandler[i].cmd) {
			if (checkbegin(buffer, cmdhandler[i].cmd)) {
				int ret = (cmdhandler[i].func)
						(buffer, &conn_info);
				if (checkbegin(buffer, "PASS ")) {
					memset(buffer, 0, strlen(buffer));
				}
				switch (ret) {
					case CMD_PASS:
						if (passcmd(buffer, clntinfo)
								< 0) {
							free(lcs.method);
							free(buffer);
							return -1;
						}
						break;
					case CMD_HANDLED:
						break;
					case CMD_QUIT:
						transfer_cleanup(
							conn_info.clntinfo);
						free(lcs.method);
						free(buffer);
						return 0;
					case CMD_ABORT:
						transfer_cleanup(
							conn_info.clntinfo);
						free(lcs.method);
						free(buffer);
						return -1;
					case CMD_ERROR:
						transfer_cleanup(
							conn_info.clntinfo);
						break;
				}
				/* found and called proper function */
				goto contin;
			}
			i++;
		}
		/* we didn't find a handler function for the command */
		/* pass it, if we are using the standard command set */

		if (cmdhandler == &std_cmdhandler[0]) {
			if (passcmd(buffer, clntinfo) < 0) {
				free(lcs.method);
				free(buffer);
				return -1;
			}
		} else {
			say(clntinfo->clientsocket, 
					"500 Command not implemented.\r\n");
		}
		/* end of while loop */
	}
	/* lcs.host and lcs.user  are freed at the termination of the programm */
	return 0;
}


char* getftpwd(struct clientinfo *clntinfo) {
	char* answer, *dir;
	char* dirstart, *dirend;
	size_t dirsize;

	say(clntinfo->serversocket, "PWD\r\n");
	answer = ftp_readline(clntinfo->serversocket);
	/*  257 "/home/joe" is current directory.  */
	/*  257 "/home/joe/hi""ho lo di'da" is current directory. */

	if ( ! checkdigits(answer, 257)) {
		log(4, "PWD failed: %s", answer);
		free(answer);
		return (char*) 0;
	}
	dirstart = strchr(answer, '"');
	dirend = strrchr(answer, '"');

	if ( ! dirstart  ||  ! dirend ) {
		log(4, "Could not parse PWD command: %s", answer);
		free(answer);
		return (char*) 0;
	}

	dirstart++;
	dirend--;

	dirsize = dirend - dirstart + 1 + 1;

	dir = (char*) malloc(dirsize);
	enough_mem(dir);

	snprintf(dir, dirsize, "%s", dirstart);
	free(answer);
	char_squeeze(dir, '"');

	return dir;
}

time_t getftpmdtm(const char* filename, struct clientinfo *clntinfo) {
/*
 *	ftp> quote mdtm bla
 *	213 20010224102705
 */
	size_t cmdsize = strlen("MDTM ") + strlen(filename) + 2 + 1;
	struct tm tms;
	int i;
	char* answer;
	char* cmd = (char*) malloc(cmdsize);

	enough_mem(cmd);
	snprintf(cmd, cmdsize, "MDTM %s\r\n", filename);

	memset(&tms, 0, sizeof(tms));

	say(clntinfo->serversocket, cmd);
	free(cmd);
	answer = ftp_readline(clntinfo->serversocket);
	if ( ! checkdigits(answer, 213)) {
		log(4, "Error reading MDTM answer: %s", answer);
		free(answer);
		return (time_t)(-1);
	}

	/* 213 20010224102705 */
	i = sscanf(answer, "213 %4d%2d%2d%2d%2d%2d",
			&tms.tm_year, &tms.tm_mon, &tms.tm_mday,
			&tms.tm_hour, &tms.tm_min, &tms.tm_sec);

	if (i != 6) {
		log(4, "Error parsing MDTM answer: %s", answer);
		free(answer);
		return (time_t)(-1);
	}
	tms.tm_year -= 1900;  /* tm_year contains the number of years */
			      /* since 1900 */
	tms.tm_mon -= 1;      /* tm_mon starts with 0 */

	free(answer);
	return mktime( &tms );
}

unsigned long int getftpsize(char* filename, struct clientinfo *clntinfo) {
/*
 * ftp> quote size speak.ps
 * 213 146617
 */
	size_t cmdsize = strlen("SIZE ") + strlen(filename) + 2 + 1;
	unsigned long int size;
	int i;
	char* answer;
	char* cmd = (char*) malloc(cmdsize);

	enough_mem(cmd);
	snprintf(cmd, cmdsize, "SIZE %s\r\n", filename);

	say(clntinfo->serversocket, cmd);
	free(cmd);
	answer = ftp_readline(clntinfo->serversocket);
	if ( ! checkdigits(answer, 213)) {
		log(4, "Error reading SIZE answer: %s", answer);
		free(answer);
		return 0;
	}

	i = sscanf(answer, "213 %ld", &size);
	if (i != 1) {
		log(4, "Error parsing SIZE answer: %s", answer);
		free(answer);
		return 0;
	}

	free(answer);
	return size;
}

int passcmd(const char* buffer, struct clientinfo *clntinfo) {
	int cs = clntinfo->clientsocket;
	int ss = clntinfo->serversocket;
	char* sendbuf =0;
	char *last = 0;
	size_t sendbufsize;

	sendbufsize = strlen(buffer) + 3;
	sendbuf = (char*) malloc(sendbufsize);
	snprintf(sendbuf, sendbufsize, "%s\r\n", buffer);
	log(9, "Send (server - %d): %s", ss, sendbuf);
	say(ss, sendbuf);
	free(sendbuf);
	lcs.complete = 0;
	last = passall(ss, cs);
	if (last) {
		lcs.respcode = getcode(last);
		log(9, "Send (client - %d): %s", cs, last);
	}
	if (!last) {
		if (timeout) {
			log(2, "Timeout in %s line %d\n", __FILE__ ,__LINE__);
			err_time_readline(cs);
		} else {
			err_readline(cs);
		}
		return -1;
	}
	free(/*at*/ last);  /*  ;-)  */
	last                     =  (char*) 0;
	lcs.complete = lcs.respcode == 226;
	return 0;
}

int transfer_cleanup(struct clientinfo *clntinfo) {
	if (clntinfo->portcmd) {
		free(clntinfo->portcmd);
		clntinfo->portcmd = (char*) 0;
	}
	if (clntinfo->dataserversock != -1) {
		close(clntinfo->dataserversock);
	}
	if (clntinfo->dataclientsock != -1) {
		close(clntinfo->dataclientsock);
	}
	return 0;
}


/* transfer_negotiate:
 *
 * return values:
 *
 * 	-1    dramatic error that should cause the program to terminate
 * 	-2    non dramatic error
 */
int transfer_negotiate(struct clientinfo *clntinfo) {
	int cs = clntinfo->clientsocket;
	int ret;
#ifdef HAVE_SOCKLEN_T
	socklen_t count;
#else
	int count;
#endif
	fd_set acc_set;
	int servacc = 0, clntacc = 0;
	struct timeval tmo;
	struct sockaddr_in sin;

	count = sizeof(sin);
	tmo.tv_sec = config_get_ioption("transfertimeout", 300);
	tmo.tv_usec = 0;

	if (clntinfo->portcmd) {
		ret = activeclient(clntinfo->portcmd, clntinfo);
		free(clntinfo->portcmd);
		clntinfo->portcmd = (char*) 0;
		if (ret) {
			log(2, "Error in activeclient()");
		}
	}

	if (clntinfo->mode == RETR && clntinfo->fromcache == 1) {
		/* the data comes from the cache */
		close(clntinfo->dataserversock);
		clntinfo->dataserversock = clntinfo->cachefd;
		servacc = 1;
	}

	/* if we're talking to the server in active ftp mode, the server
	 * connects to us. Since we're talking to the client in passive
	 * mode, the client connects to us, too! */
	if (clntinfo->servermode == ACTIVE 
	    && clntinfo->clientmode == PASSIVE) {
		/* accept connections from both sides */
		clntinfo->waitforconnect = (int*) 0;
		/* loop until both have connected */
		while (servacc == 0 || clntacc == 0) {
			FD_ZERO(&acc_set);
			if ( ! servacc ) {
				FD_SET(clntinfo->dataserversock, &acc_set);
			}
			if ( ! clntacc ) {
				FD_SET(clntinfo->dataclientsock, &acc_set);
			}
			/* select() and wait for both */
			ret = select(MAX_VAL(clntinfo->dataserversock,
					 clntinfo->dataclientsock) + 1,
					&acc_set, NULL, NULL, &tmo);
			if (ret < 0) {
				log(2, "Select() error1: %s", strerror(errno));
				return -1;
			}
			if (ret == 0) {
				log(2, "Connection timed out");
				lcs.respcode = 500;
				say(cs, "500 Connection timed out\r\n");
				return -1;
			}
			/* no timeout, no error, one of them must have
			 * connected, check if it was the server. */
			if (FD_ISSET(clntinfo->dataserversock, &acc_set)) {
				ret = accept(clntinfo->dataserversock,
					(struct sockaddr*) &sin, &count);
				if (ret < 0) {
					log(2, "Error in accept() #1: %s",
							strerror(errno));
					return -1;
				}
				close(clntinfo->dataserversock);
				clntinfo->dataserversock = ret;
				servacc = 1;
			}
			/* check if the client connected */
			if (FD_ISSET(clntinfo->dataclientsock, &acc_set)) {
				ret = accept(clntinfo->dataclientsock,
					(struct sockaddr*) &sin, &count);
				if (ret < 0) {
					log(2, "Error in accept() #2: %s",
							strerror(errno));
					return -1;
				}
				close(clntinfo->dataclientsock);
				clntinfo->dataclientsock = ret;
				clntacc = 1;
			}
		}
	}
	if (clntinfo->mode == RETR
		    && clntinfo->fromcache == 1
		    && clntinfo->waitforconnect == &clntinfo->dataserversock) {
		/* the data comes from the cache */
		clntinfo->waitforconnect = (int*) 0;
/*		close(clntinfo->dataserversock);
		clntinfo->dataserversock = clntinfo->cachefd;
*/
	}
	if (clntinfo->waitforconnect) {
		FD_ZERO(&acc_set);
		if (*clntinfo->waitforconnect < 0) {
			return -2;
		}
		FD_SET(*clntinfo->waitforconnect, &acc_set);
		ret = select(*clntinfo->waitforconnect + 1, &acc_set,
				NULL, NULL, &tmo);
		if (ret < 0) {
			log(2, "Select() error2: %s", strerror(errno));
			return -1;
		}
		if (ret == 0) {
			log(2, "Connection timed out");
			lcs.respcode = 500;
			say(cs, "500 Connection timed out\r\n");
			return -1;
		}
		/* accept connection from one side */
		ret = accept(*clntinfo->waitforconnect,
				(struct sockaddr*) &sin, &count);
		if (ret < 0) {
			log(2, "Error in accept() #3: %s",
					strerror(errno));
			return -1;
		}
		close(*clntinfo->waitforconnect);
		*clntinfo->waitforconnect = ret;
	}

	if (clntinfo->mode == STOR) {
		/* swap descriptors */
		ret = clntinfo->dataserversock;
		clntinfo->dataserversock = clntinfo->dataclientsock;
		clntinfo->dataclientsock = ret;
	}
	return 0;
}

#ifdef WINDOWS
#define TRANSMITBUFSIZE  (10*1024)
#else
#define TRANSMITBUFSIZE  PIPE_BUF
#endif
int transfer_transmit(struct clientinfo *clntinfo)  {
	char* buffer = (char*) malloc(TRANSMITBUFSIZE);
	char* pbuf = 0;
	int count = 0, nwritten, cachewritten, totwritten, sret = 0, scret = 0;
	int cachefail = 0;
	int cs = clntinfo->clientsocket;
	int n, ret, error = 0, aborted = 0;
	int maxfd;
	int fdflags;
	fd_set readset, writeset, exceptset;
	struct timeval readtime;
	struct timeval writetime;
	time_t start, done, delay;
	sigset_t sigset, oldset;

	readtime.tv_sec = config_get_ioption("transfertimeout", 300);
	readtime.tv_usec = 0;
	writetime.tv_sec = config_get_ioption("transfertimeout", 300);
	writetime.tv_usec = 0;

	enough_mem(buffer);
	FD_ZERO(&readset);
	FD_ZERO(&writeset);
	FD_ZERO(&exceptset);

	if (clntinfo->dataclientsock < 0) {
		free(buffer);
		return 0;
	}

	if (clntinfo->fromcache == 0) {
		FD_SET(clntinfo->dataserversock, &readset);
	}
	FD_SET(cs, &exceptset);
	FD_SET(cs, &readset);
	FD_SET(clntinfo->dataclientsock, &writeset);

	log(7, "Throughputrate is %3.3f", clntinfo->throughput);

	/* set the descriptor nonblocking */
	if ((fdflags = fcntl(clntinfo->dataclientsock, F_GETFL)) < 0) {
		log(2, "Error getting fcntl() flags");
		fdflags = 0;
	}
	ret = fcntl(clntinfo->dataclientsock, F_SETFL, fdflags | O_NONBLOCK);
	if (ret < 0) {
		log(2, "Error setting fd to nonblocking");
	}

	totwritten = 0;
	start = time(NULL);

	sigemptyset(&sigset);
	sigemptyset(&oldset);
	sigaddset(&sigset, SIGCHLD);
	ret = sigprocmask(SIG_BLOCK, &sigset, &oldset);
	if (ret < 0) {
		log(3, "sigprocmask() error: %s", strerror(errno));
	}

	while(1) {
		/* Linux modifies the timeout struct */
		readtime.tv_sec = config_get_ioption("transfertimeout", 300);
		readtime.tv_usec = 0;
		writetime.tv_sec = config_get_ioption("transfertimeout", 300);
		writetime.tv_usec = 0;

		if (clntinfo->fromcache) {
			readtime.tv_sec = 0;
			readtime.tv_usec = 0;
			maxfd = cs;
		} else {
			maxfd = MAX_VAL(clntinfo->dataserversock, cs);
		}
		sret = select(maxfd+1, &readset, NULL, &exceptset, &readtime);

		if (sret == -1) {
			break;
		}

		/* save the errno value of select from sigprocmask() */
		n = errno;
		ret = sigprocmask(SIG_UNBLOCK, &sigset, &oldset);
		if (ret < 0) {
			log(3, "sigprocmask() error releasing the blocked"
				" signals: %s", strerror(errno));
		}
		errno = n;

		/* Can we read data from the client ? */
		if (clntinfo->fromcache
			|| FD_ISSET(clntinfo->dataserversock, &readset)) {

			count = read(clntinfo->dataserversock,
				buffer, TRANSMITBUFSIZE);
			if (count == 0) {
				log(8, "Read 0 bytes at %s (%d)", __FILE__, __LINE__);
				break;
			}
			if (count < 0) {
				int err = errno;
				log(3, "read error: %s", strerror(err));
				set_errstr(strerror(err));
				/* comm */
				sret = 1;
				error = TRNSMT_ERROR;
				break;
			}

			/* comm */
			pbuf = buffer;
			do {
				/* now write all the read data */
				FD_ZERO(&writeset);
				FD_SET(clntinfo->dataclientsock, &writeset);

				sigemptyset(&sigset);
				sigemptyset(&oldset);
				sigaddset(&sigset, SIGCHLD);
				ret = sigprocmask(SIG_BLOCK, &sigset, &oldset);
				if (ret < 0) {
					log(3, "sigprocmask() error: %s",
						strerror(errno));
				}

				/* selecting to write */
				scret = select(clntinfo->dataclientsock + 1,
					NULL, &writeset, NULL, &writetime);
				/* save the errno value of select from
				 * sigprocmask() */
				n = errno;
				ret = sigprocmask(SIG_UNBLOCK, &sigset,
						&oldset);
				if (ret < 0) {
					log(3, "sigprocmask() error releasing "
						"the blocked signals: %s",
						strerror(errno));
				}
				errno = n;

				if (scret <= 0) {
					break;
				}
				/* otherwise the descriptor must be ready */
				/* write to the cache first */
				if ( clntinfo->tocache &&  ! cachefail ) {
					cachewritten = write(clntinfo->cachefd,
						pbuf, count);
					if (cachewritten != count) {
						log(3, "Error writing to the "
							"cache: %s",
							strerror(errno));
						cachefail = 1;
					}
				}
				/* convert if we have to */
				if (clntinfo->havetoconvert != CONV_NOTCONVERT) {
					char* tmp;
					if (clntinfo->havetoconvert
							== CONV_TOASCII) {
						tmp = to_ascii(buffer, &count);
						if (count > TRANSMITBUFSIZE) {
							buffer =
							realloc(buffer, count);
						}
						memcpy((void*) buffer,
							(void*) tmp, count);
						free(tmp);
					}
					if (clntinfo->havetoconvert
							== CONV_FRMASCII) {
						tmp = from_ascii(buffer,
								&count);
						buffer = tmp;
					}
				}
				nwritten = write(clntinfo->dataclientsock,
						pbuf, count);
				if (nwritten < 0) {
					log(2, "Error writing (%s, %d): %s",
							__FILE__, __LINE__,
							strerror(errno));
					break;
				}
				if (nwritten == 0) {
					log(8, "Wrote 0 bytes (%s, %d)",
							__FILE__, __LINE__);
				}
				totwritten += nwritten;
				/* calculate the delay time */
				if (clntinfo->throughput >= 0) {
					done = time(NULL);
					delay = (totwritten / 1024.00) /
						clntinfo->throughput;
					delay += start;
					delay -= done;
					if (delay > 0) {
						sleep(delay);
					}
				}
				if (nwritten != count) {
					pbuf += nwritten;
					count -= nwritten;
				} else {
					pbuf = 0;
				}
			} while (pbuf);
		}
		if (FD_ISSET(cs, &exceptset) || FD_ISSET(cs, &readset)) {
			if (checkforabort(clntinfo)) {
				error = TRNSMT_NOERRORMSG;
				break;
			} else {
				aborted = 1;
				break;
			}
		}
		/* restore the sets */
		FD_ZERO(&readset);
		FD_ZERO(&writeset);
		FD_ZERO(&exceptset);
		FD_SET(clntinfo->dataserversock, &readset);
		FD_SET(clntinfo->dataclientsock, &writeset);
		FD_SET(cs, &exceptset);
		FD_SET(cs, &readset);
		/* Linux modifies the timeout struct */
		readtime.tv_sec = config_get_ioption("transfertimeout", 300);
		readtime.tv_usec = 0;
		writetime.tv_sec = config_get_ioption("transfertimeout", 300);
		writetime.tv_usec = 0;
	}

	/* Restore flags */
	if (!aborted) {
		ret = fcntl(clntinfo->dataclientsock, F_SETFL, fdflags);
		if (ret < 0) {
			log(2, "Error resetting fd flags");
		}
	}

	if (sret == -1 || scret == -1) {
		log(2, "Error in select() occured: %s", strerror(errno));
		lcs.respcode = 500;
		say(cs, "500 Error in select() occured\r\n");
		error = TRNSMT_NOERRORMSG;
	}
	/* count == 0 means correct end of transfer */
	if ((sret == 0 || scret == 0) && count != 0) {
		log(2, "Connection timed out in transfer_transmit(): %d, %d",
				readtime.tv_sec, writetime.tv_sec);
		log(9, "Sockets: dataserver: %d, dataclient: %d, client: %d",
				clntinfo->dataserversock,
				clntinfo->dataclientsock,
				clntinfo->clientsocket);
		if (scret == 0) {
			log(9, "scret == 0 (write)");
		}
		if (sret == 0) {
			log(9, "sret == 0 (read)");
		}
		error = TRNSMT_ERROR;
	}

	lcs.transferred = totwritten;
	log(7, "Transferred %d bytes", totwritten);

	free(buffer);

	close(clntinfo->dataclientsock);
	close(clntinfo->dataserversock);
	if (clntinfo->cachefd >= 0) {
		close(clntinfo->cachefd);
	}
	clntinfo->dataclientsock = -1;
	clntinfo->dataserversock = -1;
	clntinfo->cachefd        = -1;

	if (aborted) {
		error = TRNSMT_ABORTED;
	}
	return error;
}

int checkforabort(struct clientinfo* clntinfo) {
	int clientfd = clntinfo->clientsocket;
	int serverfd = clntinfo->serversocket;
	char buffer[1024];
	char* buf =0;
	char rabseq[3] = { (char) IAC, (char) IP, (char) 0 };
	char sabseq[4] = { (char) IAC, (char) IP, (char) IAC, (char) 0 };
	char dmseq[1]  = { (char) DM };
	int ret;

	struct timeval tm;
	fd_set set, exset;

	tm.tv_sec = config_get_ioption("comamndtimeout", 300);
	tm.tv_usec = 0;

	/* this read won't block */
	ret = read(clientfd, buffer, sizeof(buffer) - 1);
	if (ret < 0) {
		/* return, if clientfd is negative for example. "Not a valid
		 * file descriptor". This is important since FD_SET might
		 * segfault with a negative filedescriptor id */
		return 0;
	}
	if (ret == 0) {
		log(8, "Read 0 bytes at %s, %d", __FILE__, __LINE__);
		log(8, "Read from clientfd, closing it! Should not happen!");
		close(clientfd);
		return 0;
	}
	buffer[ret] = '\0';

	if (strncmp(buffer, rabseq, 2) != 0) {
		return 0;
	}

	FD_ZERO(&set);
	FD_SET(clientfd, &set);
	ret = select(clientfd + 1, &set, NULL, &set, &tm);

	if (ret <= 0) {
		return 0;
	}

	ret = read(clientfd, buffer, sizeof(buffer) - 1);
	if (ret < 0) {
		return 0;
	}
	if (ret == 0) {
		log(8, "Read 0 bytes at %s, %d", __FILE__, __LINE__);
		log(8, "Read from clientfd, closing it! Should not happen!");
		close(clientfd);
		return 0;
	}
	buffer[ret] = '\0';

	if(buffer[0] != (char) DM ||
		strncasecmp(buffer + 1, "ABOR", 4) != 0) {
		return 0;
	}

	/* abort */

	if (clntinfo->dataclientsock == -1) {
		/* there is no open connection to the client */
		say(clientfd, "225 Abort successful\r\n");
	} else {
		/* Close the connection to the client */
		close(clntinfo->dataclientsock);
		clntinfo->dataclientsock = -1;
		say(clientfd, "426 Transfer aborted. "
				"Closed data connection.\r\n");
		say(clientfd, "226 Abort successful\r\n");
	}

	if (clntinfo->dataserversock == -1) {
		/* if we don't have a connection to the server we can
		 * quit here
		 */
		return 1;
	}

	if (send(serverfd, sabseq, 3, MSG_OOB) != 3
		|| send(serverfd, dmseq, 1, 0) != 1
		|| send(serverfd, "ABOR\r\n", 6, 0) != 6) {
		log(3, "send() error: %s", strerror(errno));
	}


	FD_ZERO(&set);
	FD_ZERO(&exset);
	FD_SET(serverfd, &set);
	FD_SET(serverfd, &exset);
	tm.tv_sec = 2;  /* grant the server 2 seconds to shut down */
	tm.tv_usec = 0;

	ret = select(serverfd + 1, &set, NULL, &exset, &tm);
	if (ret <= 0) {
		close(clntinfo->dataserversock);
		clntinfo->dataserversock = -1;
	}
	ret = ftp_getrc(serverfd, &buf);
	switch (ret) {
		case 425:
		case 451:
		case 426:
			free(buf);
			ret = ftp_getrc(serverfd, &buf);
			lcs.respcode = ret;
			/* fallthrough */
		case 226:
			log(6, "Client requested ABORT - Aborted successfully");
			free(buf);
			return 0;
		default:
			log(6, "Answer of server after ABORT unknown: (%s).",
					buf);
	}
	free(buf);
	return 1;
}

