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

#ifndef __JFTPGW_H__
#define __JFTPGW_H__

#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STRINGS_H   /* SysV */
#include <strings.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <limits.h>  /* for UINT_MAX -> OSF does not accept inet_addr() == -1 */
#include "log.h"
#include "cache.h"


/* include the autoheader file config.h */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define MAX_VAL(a,b) ((a)>(b)?(a):(b))
#define MIN_VAL(a,b) ((a)<(b)?(a):(b))

#define DEFAULTCONFFILE		CONFPATH"/jftpgw.conf"
#define DEFAULTBINDPORT		2370
#define DEFAULTBINDADDRESS	"0.0.0.0:2370"
#define DEFAULTSERVERPORT	21
#define DEFAULTLOGFILE		"/var/log/jftpgw.log"

#define BROADCAST		"255.255.255.255"

#define RETR			1
#define STOR			2

#define UNSPEC			0
#define PASSIVE			1
#define ACTIVE			2
#define ASCLIENT		3

#define SERVERADDR		1
#define CLIENTADDR		2

#define TRANSPARENT_YES		1
#define TRANSPARENT_NO		2

#define UID			1
#define EUID			2
#define GID			3
#define EGID			4

#define PRIV			1
#define UNPRIV			2

#define CMD_HANDLED		0
#define CMD_DONE		1
#define CMD_PASS		2
#define CMD_QUIT		3
#define CMD_ABORT		(-1)
#define CMD_ERROR		(-2)

/* login stages must be sorted numerically */
#define LOGIN_ST_NOT_CONNECTED	0
#define LOGIN_ST_CONNECTED	1
#define LOGIN_ST_USER		2
#define LOGIN_ST_LOGGEDIN	3
#define LOGIN_ST_FULL		4

#define WHITESPACES		" \t"

#define CONV_NOTCONVERT		0
#define CONV_TOASCII		1
#define CONV_FRMASCII		2

#define MAX_LINE_SIZE		4096

#ifndef INPORT_ANY
#define INPORT_ANY		0
#endif

#define TRNSMT_SUCCESS		0
#define TRNSMT_ERROR		1
#define TRNSMT_ABORTED		2
#define TRNSMT_NOERRORMSG	4

#define SVR_LAUNCH_CMDLINE	0
#define SVR_LAUNCH_LOGFILES	1
#define SVR_LAUNCH_READY	2

#if !defined(HAVE_SNPRINTF) || !defined(HAVE_VSNPRINTF)
  int snprintf (char *str, size_t count, const char *fmt, ...);
  int vsnprintf (char *str, size_t count, const char *fmt, va_list arg);
#endif


extern FILE* logfile;
struct allowed_lst;
struct message {
	char* fullmsg;
	/* lastmsg points to an address somewhere in fullmsg, so you don't
	 * have to free() both */
	char* lastmsg;
};

struct clientinfo {
	int* boundsocket_list;
	int boundsocket_niface;  /* number of ifaces we bound to */
	int serversocket;
	int clientsocket;
	int dataserversock;
	int dataclientsock;
	int cachefd;
	int fromcache;
	int tocache;
	int *waitforconnect;
	int transparent;
	int mode;
	int servermode;
	int clientmode;
	unsigned long int addr_to_server;
	unsigned long int data_addr_to_server;
	unsigned long int addr_to_client;
	unsigned long int data_addr_to_client;
	unsigned long int server_ip;
	unsigned long int client_ip;
	unsigned long int proxy_ip;
	unsigned int proxy_port;
	unsigned int dataport;
	int havetoconvert;
	char* portcmd;
	char* destination;
	struct sockaddr_in transparent_destination;
	char* rev_hostname;
	char* user;
	char* pass;
	char* anon_user;
	unsigned int destinationport;
	float throughput;
	struct {
		struct message welcomemsg;
		struct message authresp;
		int stage;		/* Takes LOGIN_ST_* values */
		int auth_resp_sent;	/* Have we already sent the
					   authentication response ? */
	} login;
	struct {
		char* user;
		char* pass;
	} fw_auth;
	struct {
		char* user;
		unsigned long int dest_ip;
		char* destination;
		unsigned int destinationport;
	} before_forward;
	struct {
		char* accept_pw;
		char* send_pw;
		char* login;	/* the login name that is used on dest */
		int passauth;
	} forward;
};

struct serverinfo {
	int multithread;
	int tcp_wrapper;
	int servertype;
	int main_server_pid;
	int chrooted;
	int ready_to_serve;
	/* We need this to re-read the configuration file. If we destroy the
	 * configuration file, we don't know anymore what our chroot-path
	 * was and can't strip the chroot-path to reread the file. */
	char* chrootdir_saved;
	char *conffilename;
	char *binaryname;
};

struct ip_t {
	unsigned long int ip;
	unsigned long int netmask;
};

struct uidstruct {
	char* username;
	char* groupname;
	uid_t uid;
	gid_t gid;
};

struct connliststruct {
	pid_t pid;
	unsigned long int from_ip;
	unsigned long int proxy_ip;
	unsigned int      proxy_port;
	time_t            start_time;
	struct connliststruct* next;
};

struct limitstruct {
	struct ip_t ip;
	char* hostname;
	unsigned int connmax;
	unsigned int connected;
	struct connliststruct* connlist;
	struct limitstruct* next;
};

struct portrangestruct {
	unsigned int startport;
	unsigned int endport;
	struct portrangestruct* next;
};


/* the following header file depends on some struct definitions, that's why
 * it goes here */
#include "config_header.h"

/* signal handling functions */
void read_default_conf(int);
void childterm(int);
void reap_chld_info (int);
void terminate (int);

/* functions that handle flags set by the signal handlers */
int get_chld_pid(void);
int reread_config(void);

/* atexit functions */
void sayterminating(void);
void closedescriptors(void);
void conf_delete_config(void);
void removepidfile(void);

/* passive.c */
int pasvclient(struct clientinfo*);
int pasvserver(struct clientinfo*);
void destroy_passive_portrange();

/* active.c */
int portcommandcheck(const char*, struct sockaddr_in*, struct clientinfo*);
int activeclient(char*, struct clientinfo*);
int activeserver(char**, struct clientinfo*);
void destroy_active_portrange();


int parsesock(char*, struct sockaddr_in*, int mode);

int checkdigits(const char*, const int);
int waitclient(const char*, struct clientinfo*);
int inetd_connected(int, struct clientinfo*);
int handle_cmds(struct clientinfo*);
int handle_login(struct clientinfo*);
int set_userdest(const char*, int, struct clientinfo*, const char*);
int login(struct clientinfo*, int);
int say(int, const char*);
int sayf(int, const char*, ...);
char* getftpwd(struct clientinfo*);
unsigned long int getftpsize(char* filename, struct clientinfo*);
time_t getftpmdtm(const char* filename, struct clientinfo*);
int passcmd(const char*, struct clientinfo*);
int openlocalport(struct sockaddr_in *, unsigned long int local_addr,
		  struct portrangestruct *);
int openportiaddr(unsigned long, unsigned int,
		  unsigned long int, const struct portrangestruct*);
int openportname(const char*, unsigned int,
		 unsigned long int, const struct portrangestruct*);
unsigned long int  socketinfo_get_local_addr_by_sending(int);
struct sockaddr_in socketinfo_get_local_sin(int);
unsigned long int  socketinfo_get_local_ip(int);
unsigned int       socketinfo_get_local_port(int);
struct sockaddr_in socketinfo_get_transparent_target_sin(int);
char*              socketinfo_get_transparent_target_char(int);


int transfer_transmit(struct clientinfo *);
int transfer_negotiate(struct clientinfo *);
int transfer_cleanup(struct clientinfo *);

char* ftp_readline(int);
char* readline(int);
struct message readall(int);
int ftp_getrc(int, char**);
char* passall(int, int);

int checkbegin(const char*, const char*);
int getcode(const char*);
int getservermode(void);
void closedescriptors(void);
int daemonize(void);
void enough_mem(const void*);
void scnprintf (char *os, size_t len, const char *str, ...);
unsigned long int setlastbits(int);
int cmp_domains(const char* name, const char* pattern);
void err_time_readline(int fd);
void err_readline(int fd);
int changeid(int, int, const char*);
char* extract_username(const char*);
char* extract_userhost(const char*);
char* extract_path(const char*);
char* extract_file(const char*);
void toupstr(char*);
char* trim(char *const);
void char_squeeze(char *const, int);
int respcode(const char*);
int init_logdirs(const char*);
int init_logfiles(const char*);
char* to_ascii(char *, int *);
char* from_ascii(char *, int *);
const char* get_errstr(void);
void set_errstr(const char*);
const char* gethostentip(const char* iplist);
char* merge_responses(const char*, const char*);
int stage_action(const char* stage);
int change_root(const char* stage);
int dropprivileges(const char* stage);

const char* get_char_peer_ip(int);
unsigned long int get_uint_peer_ip(int);
int get_interface_ip(const char* iface, struct sockaddr_in*);
int get_interface_name(const struct sockaddr_in, char*);
#define GET_IP_SERVER	0
#define GET_IP_CLIENT	1
unsigned long int get_uint_ip(int, struct clientinfo*);
const char* get_char_ip(int, struct clientinfo*);
const char*  conv_ip_to_char(unsigned long int);
void replace_not_larger(char* s, char* replace_what, char* replace_with);
char* char_prepend(const char*, const char*);
char* char_append(const char*, const char*);
char* char_enclose(const char*, const char*, const char*);
char* strnulldup(const char*);
const char* strcasestr(const char* haystack, const char* needle);

/* from config.c */
int config_read_options(FILE*);
char* quotstrtok(const char*, const char*, int*);
char* quotstrtok_prepend(const char*, const char*, const char*, int*);
int add_to_options(char*);
FILE* open_file(const char*);
FILE* open_logfile(const char*);
int read_config(const char*);
int set_conffilename(const char*);
int is_below_limit(unsigned long int, struct limitstruct**);
int register_pid(pid_t, unsigned long int,		/* from ip */
			unsigned long int,		/* proxy ip */
			unsigned int,			/* proxy port */
			time_t);			/* start time */
int unregister_pid(pid_t);
int passcmd_check(const char*);

struct showaddr;
int add_showaddr(char*, struct showaddr**);

void encrypt_password(void);
int cryptcmp(const char*, const char*);
struct ip_t parse_ip(const char*);
int gethostaddr(const char*, struct in_addr*);

char* chrooted_path(const char* path);

void free_limitst(struct limitstruct*);
void free_errstr();

/* from rel2abs.c */
char* rel2abs(const char* path, const char* base,
			char* result, const size_t size);


#endif /* __JFTPGW_H__ */
