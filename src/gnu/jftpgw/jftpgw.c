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
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <unistd.h>
#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#else
#include "support/getopt.h"
#include "support/getopt.c"
#include "support/getopt1.c"
#endif

#include "jftpgw.h"


void reset_loginfo(struct loginfo_st*);
void print_version(void);
void print_help(void);

struct clientinfo clntinfo;
struct serverinfo srvinfo;
extern struct loginfo_st loginfo;
extern struct log_cmd_st lcs;
extern struct uidstruct runasuser;
extern struct connliststruct* connected_clients;
extern struct slist_t* passcmd_white_list, *passcmd_black_list;
extern int should_read_config;
int timeout;


static struct option const long_option_arr[] =
{
	{ "single",     no_argument,       NULL, 's' },
	{ "inetd",      no_argument,       NULL, 'i' },
#ifdef HAVE_LIBWRAP
	{ "tcpwrap",    no_argument,       NULL, 't' },
#endif
	{ "encrypt",    no_argument,       NULL, 'e' },
	{ "version",    no_argument,       NULL, 'v' },
	{ "version",    no_argument,       NULL, 'V' },
	{ "help",       no_argument,       NULL, 'h' },
	{ "configfile", required_argument, NULL, 'f' },
	{ NULL,         0,                 NULL,  0  }
};

int main(int argc, char** argv) {

	/* We bind to 0.0.0.0, but our real IP is the one returned by the
	 * PASV command */

	struct sigaction sa, cf;
	int ret;

#ifdef RLIMIT_CORE
	/* set RLIMIT_CORE to 0 */
	struct rlimit rl = { 0, 0 };

	if (setrlimit(RLIMIT_CORE, &rl) < 0) {
		perror("setrlimit");
		return -1;
	}
#endif
	/* set the name of the config file to an initial value, it may be
	 * overwritten later
	 * */
	srvinfo.conffilename = strdup(DEFAULTCONFFILE);
	enough_mem(srvinfo.conffilename);

	/* No login done yet */
	clntinfo.login.stage = LOGIN_ST_NOT_CONNECTED;

	/* default: Multithread, i.e. fork for each connection */
	srvinfo.multithread = 1;
	srvinfo.tcp_wrapper = 0;
	srvinfo.servertype = SERVERTYPE_STANDALONE;
	srvinfo.main_server_pid = 0;
	srvinfo.chrooted = 0;

	if (strrchr(argv[0], '/')) {
		srvinfo.binaryname = strdup(strrchr(argv[0], '/') + 1);
	} else {
		srvinfo.binaryname = strdup(argv[0]);
	}
	enough_mem(srvinfo.binaryname);

	/* parse the command line */
	while ((ret=getopt_long(argc, argv, "isetvVhf:", long_option_arr, NULL)) != EOF) {
		switch (ret) {
			case 0: break;
			case 'i':
				srvinfo.servertype = SERVERTYPE_INETD;
				srvinfo.multithread = 0;
				break;
			case 's':
				srvinfo.multithread = 0;
				break;
#ifdef HAVE_LIBWRAP
			case 't':
				srvinfo.tcp_wrapper = 1;
				break;
#endif
			case 'e':
				encrypt_password();
				exit(0);
			case 'v':
			case 'V':
				print_version();
				exit(0);
			case 'h':
				print_help();
				exit(0);
			case 'f':
				if (set_conffilename(optarg) < 0) {
					log(2, "something is wrong with the name of the configuration file");
					return 1;
				}
				break;
			default:
				break;
		}
	}

	/* Read the configuration */
	memset(&loginfo, 0, sizeof(struct loginfo_st));
	loginfo.debuglevel = 6;
	srvinfo.ready_to_serve = SVR_LAUNCH_CMDLINE;
	srvinfo.chrootdir_saved = (char*) 0;

	ret = read_config(srvinfo.conffilename);
	if (ret) {
		return 1;
	}

	/* Drop privileges right after the start of the program. Right after
	 * reading the configuration file */

	if (stage_action("start") < 0) {
		return -1;
	}

	/* init the logfiles */
	if (changeid(PRIV, UID, "log_init()")    < 0) { return -1; }
	if (log_init()                           < 0) {
		changeid(UNPRIV, EUID, "log_init()");
		return -1;
	}
	if (changeid(UNPRIV, EUID, "log_init()") < 0) { return -1; }

	srvinfo.ready_to_serve = SVR_LAUNCH_LOGFILES;

	/* initiate a few values */
	clntinfo.transparent = TRANSPARENT_NO;
	clntinfo.destinationport = 0;

	/* Install the signal handlers */

	/* for SIGCHLD install just the reap function
	 *
	 * the register/unregister thing is installed after we've bound
	 * successfully */

	sa.sa_handler = reap_chld_info;
	sigemptyset (&sa.sa_mask);
#ifndef WINDOWS
	sa.sa_flags = SA_RESTART;
#endif
	sigaction (SIGCHLD, &sa, 0);

	cf.sa_handler = read_default_conf;
	should_read_config = 0;
	sigemptyset(&cf.sa_mask);
#ifndef WINDOWS
	cf.sa_flags = SA_RESTART;
#endif
	sigaction (SIGHUP, &cf, 0);

	cf.sa_handler = terminate;
	sigemptyset(&cf.sa_mask);
	sigaction (SIGTERM, &cf, 0);
	sigaction (SIGQUIT, &cf, 0);
	sigaction (SIGABRT, &cf, 0);

	/* Ignore SIGALRM */
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGALRM, &sa, 0);

	clntinfo.user = clntinfo.pass = clntinfo.destination = (char*) 0;
	clntinfo.before_forward.user = clntinfo.before_forward.destination
								= (char*) 0;
	clntinfo.anon_user = (char*) 0;
	clntinfo.throughput = 0;
	clntinfo.boundsocket_list = (int*) 0;
	clntinfo.server_ip = clntinfo.client_ip = clntinfo.addr_to_server
		= clntinfo.addr_to_client = (unsigned long int) UINT_MAX;

	atexit(closedescriptors);

	if (srvinfo.servertype == SERVERTYPE_STANDALONE &&
		(ret = waitclient(config_get_option("listen"),
							&clntinfo)) < 0) {
		/* An error occured */
		return ret;
	}
	if (srvinfo.servertype == SERVERTYPE_INETD) {
		close(1);
		dup2(0, 10);
		close(0);
		if (inetd_connected(10, &clntinfo) < 0) {
			return -1;
		}
	}
	ret = handle_login(&clntinfo);
	log(8, "Exited from handle_login, ret: %d", ret);
	if (!ret) {
		ret = handle_cmds(&clntinfo);
		log(8, "Exited from handle_cmds");
	}

	log(8, "Exiting");
	if (ret) {
		return 1;
	} else {
		return 0;
	}
}


int daemonize()
{
	int childpid;

	if( (childpid = fork()) < 0) return(-1);
	else if(childpid > 0) exit(0);

	errno = 0;
	chdir("/");
	setsid();
	return(0);
}


void read_default_conf(int signo) {
	should_read_config = 1;
}

int reread_config() {
	int ret;
	sigset_t sigset, oldset;
	log(6, "SIGHUP received - Rereading config file");

	/* block other SIGHUP signals */

	sigemptyset(&sigset);
	sigemptyset(&oldset);
	sigaddset(&sigset, SIGCHLD);
	while ((ret = sigprocmask(SIG_BLOCK, &sigset,
		&oldset)) < 0 && errno == EINTR)  {}
	if (ret < 0) {
		log(2, "Error blocking signals: %s", strerror(errno));
	}
	config_delete_config();
	destroy_active_portrange();
	destroy_passive_portrange();
	ret = read_config(srvinfo.conffilename);
	if (ret) {
		/* the following line is a BADF if we had a SIGHUP */
		log(1, "Error rereading config file. Exiting.");
		exit(2);
	}
	/* re-register all connected clients */
	config_counter_add_connected(connected_clients);
	reset_loginfo(&loginfo);
	if (stage_action("reread") < 0) {
		return -1;
	}
	should_read_config = 0;
	while ((ret = sigprocmask(SIG_UNBLOCK, &sigset,
		&oldset)) < 0 && errno == EINTR) {}
	if (ret < 0) {
		log(2, "Error unblocking signal mask: %s", strerror(errno));
		return -1;
	}
	return 0;
}

void terminate (int signo) {
	/* exit is not POSIX-reentrant but in SVR4 SVID */
	exit(0);
}


int stage_action(const char* stage) {
	char* reason = char_append("stage_action() in stage ", stage);

	changeid(PRIV, UID, reason);
	free(reason);
	if (change_root(stage) < 0) {
		changeid(UNPRIV, EUID, "change_root() failed");
		return -1;
	}
	if (dropprivileges(stage) < 0) {
		changeid(UNPRIV, EUID, "dropprivileges() failed");
		return -1;
	}
	changeid(UNPRIV, EUID, "log_init()");
	return 0;
}


void print_version(void) {
	printf(PACKAGE" v"JFTPGW_VERSION);
#ifdef HAVE_CRYPT
	printf("  -  crypt support enabled");
#else
	printf("  -  without crypt support");
#endif


#ifdef HAVE_LINUX_NETFILTER_IPV4_H
	printf("  -  netfilter support enabled");
#else
	printf("  -  without netfilter support");
#endif
	printf("\n");

#ifdef HAVE_LIBWRAP
	printf("libwrap support enabled");
#else
	printf("without libwrap support");
#endif

#ifdef HAVE_SIOCGIFADDR
	printf(" - can get IPs from interfaces");
#else
	printf(" - can't get IPs from interfaces");
#endif

#ifdef HAVE_ICMP_SUPPORT
	printf(" - ICMP support");
#else
	printf(" - no ICMP support");
#endif
	printf("\n");


}

void print_help(void) {
	print_version();
	printf("usage: jftpgw [OPTION]\n\n");
	printf("Valid options:\n");
	printf("  -h, --help                Display this help text\n");
	printf("  -e, --encrypt             Use jftpgw to obtain an encrypted password\n");
	printf("  -f, --configfile file     Load file instead of default config file\n");
	printf("  -s, --single              Run jftpgw single threaded (do not fork)\n");
	printf("  -i, --inetd               Run jftpgw from inetd superserver\n");
#ifdef HAVE_LIBWRAP
	printf("  -t, --tcpwrap             Use libwrap for access control\n");
#endif
	printf("  -V, -v, --version         Display the version\n");
	printf("\nReport bugs to Joachim Wieland <joe@mcknight.de>\n");
}


void removepidfile(void) {
	const char* option;
	if (getpid() != srvinfo.main_server_pid) {
		/* the program has not become a daemon */
		return;
	}

	option = config_get_option("pidfile");
	if (option) {
		int i;
		if (changeid(PRIV, UID,
			"Changing ID to root (unlink pidfile)") < 0) {
			return;
		}
		i = unlink(option);
		if (i) {
			log(3, "Could not unlink the pidfile %s", option);
		}
		if (changeid(UNPRIV, EUID,
				"Changing id back (deleting pidfile)") < 0) {
			return;
		}
	}
}

void sayterminating(void) {
	log(6, "jftpgw terminating");
}

void closedescriptors(void) {
	int i;
	log(9, "In closedescriptors()");

	/* free the log info structure. Free the members, the structure for
	 * itself is on the stack */
	/* lcs.cmd must not be freed, it's   lcs.cmd = buffer;  */
	/* the same for lcs.filename */
	if (lcs.svrip)    { free(lcs.svrip);    }
	if (lcs.clntip)   { free(lcs.clntip);   }
	if (lcs.ifipclnt) { free(lcs.ifipclnt); }
	if (lcs.ifipsvr)  { free(lcs.ifipsvr);  }

	if (lcs.userlogin)      { free(lcs.userlogin);      }
	if (lcs.usereffective)  { free(lcs.usereffective);  }
	if (lcs.userforwarded)  { free(lcs.userforwarded);  }

	if (srvinfo.conffilename) { free(srvinfo.conffilename); }

	/* close the logfiles and delete the structures */
	reset_loginfo(&loginfo);
	free(srvinfo.chrootdir_saved);

	config_delete_config();
	config_delete_backup();

	/* maybe these variables are not yet allocated */
	if (clntinfo.user) {
		free(clntinfo.user);
	}
	if (clntinfo.pass) {
		free(clntinfo.pass);
	}
	if (clntinfo.destination) {
		free(clntinfo.destination);
	}
	if (runasuser.username) {
		free(runasuser.username);
		runasuser.username = 0;
	}
	if (runasuser.groupname) {
		free(runasuser.groupname);
		runasuser.groupname = 0;
	}
	if (clntinfo.anon_user) {
		free(clntinfo.anon_user);
		clntinfo.anon_user = (char*) 0;
	}
	if (clntinfo.before_forward.user) {
		free(clntinfo.before_forward.user);
		clntinfo.before_forward.user = (char*) 0;
	}
	if (clntinfo.before_forward.destination) {
		free(clntinfo.before_forward.destination);
		clntinfo.before_forward.destination = (char*) 0;
	}
	free_errstr();

	free(srvinfo.binaryname);

	slist_destroy(passcmd_white_list);
	slist_destroy(passcmd_black_list);

	destroy_active_portrange();
	destroy_passive_portrange();

	if (clntinfo.boundsocket_list) {
		for (i = 0; i < clntinfo.boundsocket_niface; i++) {
			close(clntinfo.boundsocket_list[i]);
		}
		free(clntinfo.boundsocket_list);
	}
	close(clntinfo.clientsocket);
	close(clntinfo.serversocket);
}


char* chrooted_path(const char* path) {
	char* newpath;
	const char* p, *start;
	const char* chrootdir = config_get_option("changerootdir");

	if (!chrootdir) {
		/* Try the saved path */
		chrootdir = srvinfo.chrootdir_saved;
	} else {
		if (!srvinfo.chrootdir_saved) {
			/* save it */
			srvinfo.chrootdir_saved = strdup(chrootdir);
			enough_mem(srvinfo.chrootdir_saved);
		}
	}
	/* okay, maybe we are chrooted, see if the logfile path lies in the
	 * chrooted dir */
	if (path && chrootdir && srvinfo.chrooted) {
		p = strstr(path, chrootdir);
		if (p == path) {
			/* Yeah, we're inside the chrooted dir, strip it
			 * off.
			 * Calculate where to split, make sure, the new path
			 * starts with a slash. */
			start = path + strlen(chrootdir);
			while (start > path && *start == '/') {
				start --;
			}
			start++;
			newpath = strdup(start);
			enough_mem(newpath);
			/* just for the beauty... */
			char_squeeze(newpath, '/');
			return newpath;
		}
	}
	/* maybe we are not chrooted or the chrootdir is not part of the
	 * path...*/
	newpath = strdup(path);
	enough_mem(newpath);
	/* just for the beauty... */
	char_squeeze(newpath, '/');

	return newpath;
}

