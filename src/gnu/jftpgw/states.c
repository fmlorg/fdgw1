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

#include <syslog.h>
#include <stdarg.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include "jftpgw.h"

extern struct serverinfo srvinfo;

struct hostent_list* hostcache;
struct uidstruct runasuser;
struct connliststruct* connected_clients = (struct connliststruct*) 0;

struct slist_t* passcmd_white_list, *passcmd_black_list;

int register_pid(pid_t pid,
		 unsigned long int from_ip,
		 unsigned long int proxy_ip,
		 unsigned int proxy_port,
		 time_t start_time) {

	struct connliststruct *cls, *tmp;

	cls = (struct connliststruct*) malloc(sizeof(struct connliststruct));
	enough_mem(cls);

	cls->next = (struct connliststruct*) 0;
	cls->pid = pid;
	cls->from_ip = from_ip;
	cls->proxy_ip = proxy_ip;
	cls->proxy_port = proxy_port;
	cls->start_time = start_time;
	log(9, "Adding pid %d", cls->pid);

	if (! connected_clients) {
		/* first entry */
		connected_clients = cls;
	} else {
		tmp = connected_clients;
		while (tmp->next) {
			tmp = tmp->next;
		}
		tmp->next = cls;
	}

	return 0;
}


int unregister_pid(pid_t pid) {

	struct connliststruct* cls, *prev = 0;

	cls = connected_clients;

	if (! cls) {
		/* empty! Error! */
		return 1;
	}

	while (cls) {
		log(8, "checking %d against %d", cls->pid, pid);
		if (cls->pid == pid) {
			/* found */
			/* delete it */
			/* case 1: prev is set */
			if (prev) {
				prev->next = cls->next;
			}

			/* case 2: prev not set */
			  else {
				/* first element */
				if (! cls->next) {
					/* only one element */
					connected_clients =
						(struct connliststruct*) 0;
				} else {
					/* nothing special, it's just the
					 * first element */
					connected_clients = cls->next;
				}
			}
			config_counter_decrease(cls->from_ip,
						cls->proxy_ip,
						cls->proxy_port,
						cls->start_time);
			/* remove that element */
			free(cls);
			return 0;
		}
		prev = cls;
		cls = cls->next;
	}
	/* not found */
	return 1;
}


int set_conffilename(const char* arg) {
	const int increment = 1;
	int pathsize = 1;
	char* ret;
	char* path;
	char* fname;

	if (!arg || strlen(arg) > PATH_MAX) {
		free(srvinfo.conffilename);
		srvinfo.conffilename = (char*) 0;
		return -1;
	}

	path = (char*) malloc(pathsize);
	enough_mem(path);
	free(srvinfo.conffilename);
	while ( ! (ret = getcwd(path, pathsize)) ) {
		pathsize += increment;
		if (pathsize > PATH_MAX) {
			srvinfo.conffilename = (char*) 0;
			free(path);
			return -1;
		}
		path = (char*) realloc(path, pathsize);
	}

	fname = (char*) malloc(PATH_MAX + 1);
	srvinfo.conffilename = rel2abs(arg, path, fname, PATH_MAX);
	if ( ! srvinfo.conffilename ) {
		free(path); free(fname);
		return -1;
	} else {
		/* reduce allocated memory */
		srvinfo.conffilename = realloc(fname, strlen(fname) + 1);
		enough_mem(srvinfo.conffilename);
		free(path);
	}
	log(9, "Conffile set to %s", srvinfo.conffilename);
	return 0;
}

void free_cmdlogentst(struct cmdlogent_t* cls) {
	if (!cls) {
		return;
	}
	free_cmdlogentst(cls->next);
	if (cls->logf_name) {
		free(cls->logf_name);
	}
	if (cls->specs) {
		free(cls->specs);
	}
	if (cls->logf) {
		fclose(cls->logf);
		cls->logf = (FILE*) 0;
	}
	if (cls->style) {
		free(cls->style);
	}
	free(cls);
}

void reset_loginfo(struct loginfo_st* ls) {
	if (!ls) {
		log(1, "loginfo_st* ls was NULL, this should NEVER happen!");
		return;
	}
	if (ls->logf_name) {
		free(ls->logf_name);
		ls->logf_name = (char*) 0;
	}
	if (ls->logf) {
		fclose(ls->logf);
		ls->logf = (FILE*) 0;
	}
	if (ls->syslog) {
		closelog();
	}
	free_cmdlogentst(ls->cmdlogfiles);
	free_cmdlogentst(ls->cmdlogdirs);
	ls->cmdlogfiles = (struct cmdlogent_t*) 0;
	ls->cmdlogdirs = (struct cmdlogent_t*) 0;
}


int getservermode() {
	int servmode = ASCLIENT;
	const char* opt = config_get_option("defaultmode");

	/* opt should be set in every case */
	if (opt) {
		if (0 == strcasecmp(opt, "passive")) {
			servmode = PASSIVE;
		}
		if (0 == strcasecmp(opt, "active")) {
			servmode = ACTIVE;
		}
		if (0 == strcasecmp(opt, "asclient")) {
			servmode = ASCLIENT;
		}
	}
	return servmode;
}



int save_runasuser_uid(void) {
	const char* option;
	struct passwd* pws;
	struct group* gs;

	option = config_get_option("runasuser");
	if (option) {
		if (runasuser.username) { free(runasuser.username); }
		runasuser.username = strdup(option);
		enough_mem(runasuser.username);
		runasuser.username = trim(runasuser.username);
		pws = getpwnam(runasuser.username);
		if (!pws) {
			log(3, "getpwnam(2) could not look up user %s",
				runasuser.username);
			perror("Could not look up the user name");
			return -1;
		}
		runasuser.uid = pws->pw_uid;
	} else {
		runasuser.uid = getuid();
	}

	option = config_get_option("runasgroup");
	if (option) {
		if (runasuser.groupname) { free(runasuser.groupname); }
		runasuser.groupname = strdup(option);
		enough_mem(runasuser.groupname);
		runasuser.groupname = trim(runasuser.groupname);
		gs = getgrnam(runasuser.groupname);
		if (!gs) {
			log(3, "getpwnam(2) could not look up group %s",
							runasuser.groupname);
			perror("Could not look up the group name");
			return -1;
		}
		runasuser.gid = gs->gr_gid;
	} else {
		runasuser.gid = getgid();
	}
	return 0;
}

struct slist_t* passcmd_create_list(const char* list_str) {
	if ( ! list_str ) {
		return (struct slist_t*) 0;
	}
	return config_split_line(list_str, WHITESPACES);
}

int passcmd_check(const char* cmd) {

	if (strcmp(config_get_option("passcmds"), "*") == 0) {
		/* this is a dummy and means "checking disabled". The "*"
		 * is the standard value if there is no such option in the
		 * configuration */
		return 1;
	}

	if ( !passcmd_white_list ) {
		passcmd_white_list
			= passcmd_create_list(config_get_option("passcmds"));
	}
	if ( !passcmd_black_list ) {
		passcmd_black_list
			= passcmd_create_list(config_get_option("dontpasscmds"));
	}

	if (slist_case_contains(passcmd_white_list, cmd)
			&&
	   !slist_case_contains(passcmd_black_list, cmd)) {

		return 1;
	}
	return 0;
}

