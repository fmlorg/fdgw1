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

#include "jftpgw.h"     /* this also includes config_header.h */
#include <ctype.h>
#include <time.h>
#include <net/if.h>     /* IF_NAMESIZE */
#define WHITESPACE " \r\n\t"


/* for testing */
void config_debug_outputsections();
const struct option_t* config_get_option_list();
void config_debug_output_options ( const struct option_t*, char* );

/* from jftpgw.c */
extern char* conffilename;
extern struct serverinfo srvinfo;

/* from states.c */
extern struct hostent_list* hostcache;
extern struct uidstruct runasuser;
int save_runasuser_uid(void);

/* variables with file scope */
static struct section_t* base_section;
static struct section_t* backup_base_section;
static struct option_t* option_list;

static int debug;
static int config_error;

/* Table of configuration options (identifiers, validity, defaultvalues) */
struct conf_dat {
	char* name;
	int tag_type;
	char* defaultvalue;
	int match_type;
	int read_limit;
};

/* forward declarations */
const struct hostent_list* config_forward_lookup(struct hostent_list** hl,
					const char* name);

const struct hostent_list* config_reverse_lookup(struct hostent_list** hl,
					unsigned long int ip);
struct option_t* config_generate_option_list(struct section_t* section,
						int config_state);

#define OPTION_EXACT_MATCH     1
#define READ_UP_TO_WS          1
#define READ_FULL_LINE         2
#define EM                     OPTION_EXACT_MATCH
#define WSP                    READ_UP_TO_WS
#define FL                     READ_FULL_LINE
struct conf_dat configuration_data[] = {
	{"listen",		TAG_STARTUP, "0.0.0.0:2370", EM, FL },
	{"runasuser",		TAG_STARTUP, (char*) 0, EM, WSP },
	{"runasgroup",		TAG_STARTUP, (char*) 0, EM, WSP },
	{"pidfile",		TAG_STARTUP, "/var/run/jftpgw.pid", EM, WSP },
	{"changeroot",		TAG_STARTUP, "never", EM, WSP },
	{"changerootdir",	TAG_STARTUP, (char*) 0, EM, WSP },
	{"dropprivileges",	TAG_STARTUP, "start", EM, WSP },
	{"welcomeline",		TAG_CONNECTED,
			"FTP proxy (v"JFTPGW_VERSION") ready", EM, FL },
	{"transparent-forward",	TAG_CONNECTED, (char*) 0, EM, WSP },
	{"transparent-proxy",	TAG_CONNECTED, "off", EM, WSP },
	{"logintime",		TAG_TO | TAG_CONNECTED, "user", EM, WSP },
	{"forward",		TAG_ALL, (char*) 0, EM, FL },
	{"udpport",		TAG_CONNECTED, "2370", EM, FL },
	{"getinternalip",	TAG_CONNECTED, "udp", EM, FL },
	{"controlserveraddress",TAG_ALL, (char*) 0, EM, WSP },
	{"dataserveraddress",	TAG_ALL, (char*) 0, EM, WSP },
	{"dataclientaddress",	TAG_ALL, (char*) 0, EM, WSP },
	{"access",		TAG_ALL, "deny"   , EM, WSP },
	{"debuglevel",		TAG_ALL, "7"      , EM, WSP },
	{"logstyle",		TAG_ALL, "files"  , EM, WSP },
	{"logfile",		TAG_ALL, "/var/log/jftpgw.log", EM, WSP },
	{"syslogfacility",	TAG_ALL, "daemon", EM, WSP },
	{"cmdlogfile",		TAG_ALL, (char*) 0, EM, WSP },
	{"cmdlogfile-style",	TAG_ALL, (char*) 0, EM, WSP },
	{"cmdlogfile-specs",	TAG_ALL, (char*) 0, EM, WSP },
	{"connectionlogdir",	TAG_ALL, (char*) 0, EM, WSP },
	{"connectionlogdir-filesuffix",	TAG_ALL, (char*) 0, EM, WSP },
	{"connectionlogdir-fileprefix",	TAG_ALL, (char*) 0, EM, WSP },
	{"connectionlogdir-style",	TAG_ALL, (char*) 0, EM, WSP },
	{"connectionlogdir-specs",	TAG_ALL, (char*) 0, EM, WSP },
	{"cacheprefix",			TAG_ALL, (char*) 0, EM, WSP },
	{"cache",			TAG_ALL,  "off"   , EM, WSP },
	{"cachemaxsize",		TAG_ALL, "unlimited", EM, WSP },
	{"cacheminsize",		TAG_ALL,       "0", EM, WSP },
	{"failedlogins",		TAG_ALL,       "3", EM, WSP },
	{"throughput",			TAG_ALL, (char*) 0, EM, WSP },
	{"limit",			TAG_CONNECTED, (char*) 0, EM, WSP },
	{"passcmds",			TAG_ALL, "*"      , EM, FL },
	{"dontpasscmds",		TAG_ALL, (char*) 0, EM, FL },
	{"activeportrange",		TAG_ALL, (char*) 0, EM, FL },
	{"passiveportrange",		TAG_ALL, (char*) 0, EM, FL },
	{"activeportrangeclient",	TAG_ALL, (char*) 0, EM, FL },
	{"passiveportrangeclient",	TAG_ALL, (char*) 0, EM, FL },
	{"activeportrangeserver",	TAG_ALL, (char*) 0, EM, FL },
	{"passiveportrangeserver",	TAG_ALL, (char*) 0, EM, FL },
	{"defaultmode",			TAG_ALL, "asclient", EM, WSP },
	{"allowreservedports",		TAG_ALL, "no", EM, WSP },
	{"allowforeignaddress",		TAG_ALL, "no", EM, WSP },
	{"serverport",			TAG_ALL, "21", EM, WSP },
	{"loginstyle",			TAG_CONNECTED, "1", EM, WSP },
	{"account",			TAG_CONNECTED, (char*) 0, EM, FL },
	{"commandtimeout",		TAG_ALL, "300", EM, WSP },
	{"transfertimeout",		TAG_ALL, "300", EM, WSP },
	{"reverselookups",		TAG_ALL, "yes", EM, WSP },
	{"forwardlookups",		TAG_ALL, "yes", EM, WSP },
	{"dnslookups",			TAG_ALL, "yes", EM, WSP },
						/* 8 hours */
	{"hostcachetimeout",		TAG_ALL, "28800", EM, WSP },
	{ (char*) 0,                  0, (char*) 0, 0, 0 }

};

#define MAXCONFOPTVALS			23	/* facility */
struct conf_opt {
	char* name;
	char* values[ MAXCONFOPTVALS ];
};

#define TERM ((char*) 0)
#define TRUEFALSE "on", "off", "yes", "no", "true", "false", "1", "0"
struct conf_opt configuration_options[] = {
	{"logstyle",            {"files", "syslog", TERM} },
	/*                           <---- better                          */
	{"dropprivileges",      {"start", "startsetup", "connect",
				  "connectsetup", "loggedin", "never", TERM} },
	/*                           <---- better                          */
	{"changeroot",          {"start", "startsetup", "connect",
				  "connectsetup", "loggedin", "never", TERM} },
	{"logintime",           {"connect", "user", "pass", TERM} },
	{"access",              {"allow", "deny", TERM} },
	{"defaultmode",         {"asclient", "active", "passive", TERM} },
	{"getinternalip",       {"udp", "icmp", TERM} },
	{"transparent-proxy",   { TRUEFALSE, TERM } },
	{"cache",               { TRUEFALSE, TERM } },
	{"allowreservedports",  { TRUEFALSE, TERM } },
	{"allowforeignaddress", { TRUEFALSE, TERM } },
	{"reverselookups",      { TRUEFALSE, TERM } },
	{"forwardlookups",      { TRUEFALSE, TERM } },
	{"dnslookups",          { TRUEFALSE, TERM } },
	{"syslogfacility", {
#ifdef HAVE_LOG_FACILITY_LOG_AUTH
				"auth",
#endif
#ifdef HAVE_LOG_FACILITY_LOG_AUTHPRIV
				"authpriv",
#endif
#ifdef HAVE_LOG_FACILITY_LOG_CRON
				"cron",
#endif
#ifdef HAVE_LOG_FACILITY_LOG_DAEMON
				"daemon",
#endif
#ifdef HAVE_LOG_FACILITY_LOG_FTP
				"ftp",
#endif
#ifdef HAVE_LOG_FACILITY_LOG_KERN
				"kern",
#endif
#ifdef HAVE_LOG_FACILITY_LOG_LOCAL0
				"local0",
#endif
#ifdef HAVE_LOG_FACILITY_LOG_LOCAL1
				"local1",
#endif
#ifdef HAVE_LOG_FACILITY_LOG_LOCAL2
				"local2",
#endif
#ifdef HAVE_LOG_FACILITY_LOG_LOCAL3
				"local3",
#endif
#ifdef HAVE_LOG_FACILITY_LOG_LOCAL4
				"local4",
#endif
#ifdef HAVE_LOG_FACILITY_LOG_LOCAL5
				"local5",
#endif
#ifdef HAVE_LOG_FACILITY_LOG_LOCAL6
				"local6",
#endif
#ifdef HAVE_LOG_FACILITY_LOG_LOCAL7
				"local7",
#endif
#ifdef HAVE_LOG_FACILITY_LOG_LPR
				"lpr",
#endif
#ifdef HAVE_LOG_FACILITY_LOG_MAIL
				"mail",
#endif
#ifdef HAVE_LOG_FACILITY_LOG_NEWS
				"news",
#endif
#ifdef HAVE_LOG_FACILITY_LOG_SYSLOG
				"syslog",
#endif
#ifdef HAVE_LOG_FACILITY_LOG_USER
				"user",
#endif
#ifdef HAVE_LOG_FACILITY_LOG_UUCP
				"uucp",
#endif
#ifdef HAVE_LOG_FACILITY_LOG_CONSOLE
				"console",
#endif
#ifdef HAVE_LOG_FACILITY_LOG_SECURITY
				"security",
#endif
				TERM } },
	{ TERM, { TERM } }
};

/* --------begin struct ilist-------------- */

struct ilist_t* ilist_init(int val) {
	struct ilist_t* i;

	i = (struct ilist_t*) malloc(sizeof(struct ilist_t));
	enough_mem(i);

	i -> value = val;
	i -> next = (struct ilist_t*) 0;

	return i;
}

void ilist_destroy(struct ilist_t* il) {
	if (! il) { return; }
	ilist_destroy(il->next);
	free(il);
}

struct ilist_t* ilist_push(struct ilist_t* il, int i) {
	if (!il) {
		return il;
	}
	while(il -> next) {
		il = il -> next;
	}
	il -> next = ilist_init(i);
	return il->next;
}

int ilist_pop(struct ilist_t* il) {
	struct ilist_t* prev = (struct ilist_t*) 0;
	int i;

	while(il -> next) {
		prev = il;
		il = il -> next;
	}
	i = il->value;
	if (prev) {
		free(il);
		prev->next = (struct ilist_t*) 0;
	}
	return i;
}

int ilist_empty(struct ilist_t* il) {
	if (!il) {
		return 1;
	}
	return (il->next == (struct ilist_t*) 0);
}

struct ilist_t* ilist_clone(const struct ilist_t* il) {
	struct ilist_t* new;

	if (!il) {
		return (struct ilist_t*) 0;
	}
	new = (struct ilist_t*) malloc(sizeof(struct ilist_t));
	new->value = il->value;
	new->next = ilist_clone(il->next);

	return new;
}

/* --------end struct ilist-------------- */



/* --------begin struct ullist-------------- */

struct ullist_t* ullist_init(unsigned long int val) {
	struct ullist_t* ul;

	ul = (struct ullist_t*) malloc(sizeof(struct ullist_t));
	enough_mem(ul);

	ul -> value = val;
	ul -> next = (struct ullist_t*) 0;

	return ul;
}

void ullist_destroy(struct ullist_t* ul) {
	if (! ul) { return; }
	ullist_destroy(ul->next);
	free(ul);
}

struct ullist_t* ullist_push(struct ullist_t* ul, unsigned long int ulval) {
	if (!ul) {
		return ul;
	}
	while(ul -> next) {
		ul = ul -> next;
	}
	ul -> next = ullist_init(ulval);
	return ul->next;
}

int ullist_pop(struct ullist_t* ul) {
	struct ullist_t* prev = (struct ullist_t*) 0;
	unsigned long int val;

	while(ul -> next) {
		prev = ul;
		ul = ul -> next;
	}
	val = ul->value;
	if (prev) {
		free(ul);
		prev->next = (struct ullist_t*) 0;
	}
	return val;
}

int ullist_empty(struct ullist_t* ul) {
	if (!ul) {
		return 1;
	}
	return (ul->next == (struct ullist_t*) 0);
}

struct ullist_t* ullist_conv_ips(char** ip_list) {
	int i = 0;
	struct ullist_t* base = (struct ullist_t*) 0,
			*cur = (struct ullist_t*) 0;
	unsigned long int ip;

	while (ip_list[i]) {
		ip = inet_addr(gethostentip(ip_list[i]));
		if (i == 0) {
			base = cur = ullist_init(ip);
		} else {
			cur = ullist_push(cur, ip);
		}
		i++;
	}
	return base;
}

/* --------end struct ullist-------------- */


/* --------begin struct slist-------------- */

struct slist_t* slist_init(char* val) {
	struct slist_t* s;

	s = (struct slist_t*) malloc(sizeof(struct slist_t));
	enough_mem(s);

	s -> value = val;
	s -> next = (struct slist_t*) 0;

	return s;
}


/* slist_cinit  copies */
struct slist_t* slist_cinit(const char* val) {
	struct slist_t* s;

	s = slist_init((char*) 0);
	s -> value = strdup(val);
	enough_mem(s->value);
	return s;
}

void slist_destroy(struct slist_t* sl) {
	if (! sl) { return; }
	slist_destroy(sl->next);
	free(sl->value);
	free(sl);
}

struct slist_t* slist_push(struct slist_t* sl, char* s) {
	if (!sl) {
		return sl;
	}
	while(sl -> next) {
		sl = sl -> next;
	}
	sl -> next = slist_init(s);
	return sl->next;
}

/* copy before pushing */
struct slist_t* slist_cpush(struct slist_t* sl, const char* s) {
	char* a = strdup(s);
	enough_mem(a);

	return slist_push(sl, a);
}

struct slist_t* slist_append(struct slist_t* a, struct slist_t* b) {
	struct slist_t* orig = a;

	if (!a) {
		return b;
	}
	while (a->next) {
		a = a->next;
	}
	a->next = b;

	return orig;
}

char* slist_get(struct slist_t* sl) {
	return sl->value;
}

char* slist_pop(struct slist_t* sl) {
	struct slist_t* prev = (struct slist_t*) 0;
	char *s;

	while(sl -> next) {
		prev = sl;
		sl = sl -> next;
	}
	s = sl->value;
	if (prev) {
		/* don't free value */
		free(sl);
		prev->next = (struct slist_t*) 0;
	}
	return s;
}


int slist_empty(struct slist_t* sl) {
	if (!sl) {
		return 1;
	}
	return (sl->next == (struct slist_t*) 0);
}

struct slist_t* slist_conv_dpointer(char** dpointer) {
	int i = 0;
	struct slist_t* sl;
	char* s;

	if (dpointer[0]) {
		s = strdup(dpointer[0]);
		enough_mem(s);
		sl = slist_init(s);
	} else {
		return (struct slist_t*) 0;
	}

	i = 1;
	while (dpointer[i]) {
		s = strdup(dpointer[i]);
		enough_mem(s);
		slist_push(sl, s);
		i++;
	}
	return sl;
}

struct slist_t* slist_clone(const struct slist_t* sl) {
	struct slist_t* slret, *slcur;

	if ( ! sl ) {
		return (struct slist_t*) 0;
	}
	slret = slcur = slist_cinit(sl->value);
	sl = sl->next;
	while (sl) {
		slist_cpush(slcur, sl->value);
		slcur = slcur->next;
		sl    = sl->next;
	}
	return slret;
}

struct slist_t* slist_reverse(struct slist_t* sl) {
	/*   a -> b -> c
	 *
	 * becomes
	 *
	 *   c -> b -> a
	 *
	 **/

	struct slist_t* head, *curr, *change;

	if (! sl) {
		return (struct slist_t*) 0;
	}

	if (! sl->next) {
		/* only one element */
		return sl;
	}

	curr = sl;
	change = sl->next;
	head = sl->next;

	curr->next = (struct slist_t*) 0;

	while ( head->next ) {

		head = head->next;
		change->next = curr;

		curr = change;
		change = head;
	};

	/* head has reached the end */
	head->next = curr;

	return head;
}

int slist_case_contains(const struct slist_t* haystack, const char* needle) {
	while (haystack) {
		if (strcasecmp(haystack->value, needle) == 0) {
			return 1;
		}
		haystack = haystack->next;
	}
	return 0;
}

int slist_count(const struct slist_t* haystack) {
	int counter = 0;
	while (haystack) {
		counter++;
		haystack = haystack->next;
	}
	return counter;
}

/* --------end struct ilist-------------- */

/* --------begin struct optionlist-------------- */

struct option_t* optionlist_init(const char* key, const char* value) {
	struct option_t* o;

	o = (struct option_t*) malloc(sizeof(struct option_t));
	enough_mem(o);

	o->key  = strdup(key);
	enough_mem(o->key);
	o->value = strdup(value);
	enough_mem(o->value);
	o->next = (struct option_t*) 0;

	return o;
}

struct option_t* optionlist_push(struct option_t* o,
					const char* key,
					const char* value) {

	if (! o) {
		return (struct option_t*) 0;
	}
	while (o->next) {
		o = o->next;
	}
	o->next  = optionlist_init(key, value);

	return o;
}

struct option_t* optionlist_clone(struct option_t* orig) {
	struct option_t* ret, *trav;
	if ( ! orig ) {
		return (struct option_t*) 0;
	}

	ret = optionlist_init(orig->key, orig->value);
	trav = ret;

	while ((orig = orig->next)) {
		trav->next = optionlist_init(orig->key, orig->value);
		trav = trav->next;
	}
	return ret;
}

struct option_t* optionlist_append(struct option_t* a, struct option_t* b) {
	struct option_t* orig = a;

	if (!a) {
		return b;
	}
	while (a->next) {
		a = a->next;
	}
	a->next = b;

	return orig;
}

void optionlist_destroy(struct option_t* olist) {
	if ( !olist ) { return; }
	optionlist_destroy( olist->next );
	free( olist->key );
	free( olist->value );
	free( olist );
}

void optionlist_delete_key(struct option_t* olist, const char* key) {
	struct option_t* deleted;

	while (olist) {
		if (olist->next) {
			if (strcasecmp(olist->next->key, key) == 0) {
				/* found */
				deleted = olist->next;
				olist->next = olist->next->next;
				deleted->next = (struct option_t*) 0;
				optionlist_destroy(deleted);
			}
		}
		olist = olist->next;
	}
}

/* --------end struct optionlist-------------- */


/* --------begin struct hostent_list-------------- */

struct hostent_list* hostent_init(struct hostent* e,
					unsigned long int ip,
					const char* name) {

	struct hostent_list* h;

	h = (struct hostent_list*) malloc(sizeof(struct hostent_list));
	enough_mem(h);

	if (name) {
		/* log(9, "adding %s to host cache", name); */
	} else {
		/* log(9, "adding %s to host cache",
				inet_ntoa(*((struct in_addr*) &ip))); */
	}

	h->next = (struct hostent_list*) 0;
	if (name) {
		h->name = strdup(name);
	} else {
		h->name = (char*) 0;
	}
	h->ip = ip;

	if (e) {
		h->aliases_list = slist_conv_dpointer(e->h_aliases);
		h->addr_list = ullist_conv_ips(e->h_addr_list);
		if (e->h_name) {
			if (h->aliases_list) {
				slist_cpush(h->aliases_list, e->h_name);
			} else {
				h->aliases_list = slist_cinit(e->h_name);
			}
		} else {
			h->aliases_list = (struct slist_t*) 0;
			h->addr_list    = (struct ullist_t*) 0;
		}
	} else {
		h->aliases_list = (struct slist_t*) 0;
		h->addr_list    = (struct ullist_t*) 0;
	}
	h->lookup_time = time(NULL);
	return h;
}

struct hostent_list* hostent_push(struct hostent_list* h,
					struct hostent* e,
					unsigned long int ip,
					const char* name) {
	if (!h) {
		return (struct hostent_list*) 0;
	}
	while (h->next) {
		h = h->next;
	}
	h->next = hostent_init(e, ip, name);
	return h;
}

const struct hostent_list* hostent_get(struct hostent_list* h,
				unsigned long int ip,
				const char* name) {

	while (h) {
		if (name && h->name) {
			if (strcasecmp(name, h->name) == 0) {
				return h;
			}
		} else {
			if (ip == h->ip) {
				return h;
			}
		}
		h = h->next;
	}
	return (struct hostent_list*) 0;
}

const struct slist_t* hostent_get_aliases(struct hostent_list** h,
					unsigned long int ip) {
	const struct hostent_list* hl;

	hl = config_reverse_lookup(h, ip);
	if (!hl) {
		return (struct slist_t*) 0;
	}
	return hl->aliases_list;
}

const struct ullist_t* hostent_get_addr(struct hostent_list** h,
					const char* name) {
	const struct hostent_list* hl;

	hl = config_forward_lookup(h, name);
	if (!hl) {
		return (struct ullist_t*) 0;
	}
	return hl->addr_list;
}

const char* hostent_get_name(struct hostent_list** h,
					unsigned long int ip) {

	const struct slist_t* l = hostent_get_aliases(h, ip);
	if ( ! l ) {
		return (char*) 0;
	}
	return l->value;
}

unsigned long int hostent_get_ip(struct hostent_list** h,
					const char* name) {
	const struct ullist_t* l = hostent_get_addr(h, name);
	if ( ! name ) {
		return UINT_MAX;
	}
	if ( ! l ) {
		/* Maybe name is already an IP ? */
		unsigned long int iptest;
		iptest = inet_addr(name);
		if (iptest != (unsigned long int) UINT_MAX) {
			/* it was an IP */
			return iptest;
		}
		return UINT_MAX;
	}
	return l->value;
}

void hostent_destroy(struct hostent_list* h) {
	if (!h) { return; }
	hostent_destroy(h->next);
	ullist_destroy(h->addr_list);
	slist_destroy(h->aliases_list);
	free(h->name);
	free(h);
}

void hostent_delete(struct hostent_list* hl, const struct hostent_list* entry){
	struct hostent_list* delete;
	if (!hl || !entry) {
		return;
	}
	while (hl) {
		if (hl->next == entry) {
			delete = hl->next;
			hl->next = hl->next->next;
			delete->next = (struct hostent_list*) 0;
			hostent_destroy(delete); 
			return;
		} else {
			hl = hl->next;
		}
	}
}


/* --------end struct hostent_list-------------- */


/* --------begin struct hostlist_t-------------- */

struct hostlist_t* hostlist_init() {
	struct hostlist_t* hl;
	hl = (struct hostlist_t*) malloc(sizeof(struct hostlist_t));
	enough_mem(hl);

	hl->next                     = (struct hostlist_t*) 0;
	hl->host.ip.ip               = -1;
	hl->host.ip.netmask          = -1;
	hl->host.name                = (char*) 0;

	return hl;
}


struct hostlist_t* hostlist_push(struct hostlist_t** hl,
				 char* name,
				 unsigned long int ip,
				 unsigned long int netmask) {

	struct hostlist_t* h;
	if (! hl) {
		/* this should NOT happen */
		log(3, "hostlist was NULL in hostlist_push()");
		return (struct hostlist_t*) 0;
	}
	h = *hl;
	if (! h) {
		/* new */
		h = hostlist_init();
		*hl = h;
	} else {
		while (h->next) {
			h = h->next;
		}
		h->next = hostlist_init();
		h = h->next;
	}

	/* fill with content */
	h->next = (struct hostlist_t*) 0;
	h->host.ip.ip           = ip;
	h->host.ip.netmask      = netmask;
	h->host.name            = name;

	/* return base address */
	return *hl;
}

struct hostlist_t* hostlist_clone(struct hostlist_t* hl) {
	struct hostlist_t* new;

	if ( ! hl ) {
		return (struct hostlist_t*) 0;
	}

	new = (struct hostlist_t*) malloc(sizeof(struct hostlist_t));

	new->host.ip = hl->host.ip;
	if (hl->host.name) {
		new->host.name = strdup(hl->host.name);
		enough_mem(new->host.name);
	} else {
		new->host.name = (char*) 0;
	}
	new->next = hostlist_clone(hl->next);
	return new;
}

struct hostlist_t* hostlist_ip_push(struct hostlist_t** hl,
				    unsigned long int ip,
				    unsigned long int netmask) {

	return hostlist_push(hl, (char*) 0, ip, netmask);
}

struct hostlist_t* hostlist_name_push(struct hostlist_t** hl,
				      char* name) {

	return hostlist_push(hl, name, -1, -1);
}

/* --------end struct hostlist_t---------------- */


/* --------begin struct portrangestruct-------------- */

struct portrangestruct* config_port2portrange(unsigned int port) {
	static struct portrangestruct prs;
	prs.next = (struct portrangestruct*) 0;
	prs.startport = prs.endport = port;
	return &prs;
}

unsigned int config_count_portrange(const struct portrangestruct* prs) {
	if ( ! prs ) {
		return 0;
	}
	return (prs->endport - prs->startport + 1) +
		config_count_portrange(prs->next);
}

struct portrangestruct* portrangestruct_clone(struct portrangestruct* prs) {
	struct portrangestruct* new;
	if ( ! prs ) {
		return (struct portrangestruct*) 0;
	}

	new = (struct portrangestruct*) malloc(sizeof(struct portrangestruct));
	enough_mem(new);

	new->startport = prs->startport;
	new->endport = prs->endport;

	new->next = portrangestruct_clone(prs->next);

	return new;
}

/* --------end struct portrangestruct---------------- */



char* config_read_line_basic(FILE* file) {
	/* read 255 bytes at first */
	const int startsize = 5;
	/* therafter increase the buffer again by size bytes */
	const int increase = startsize;
	int times;
	unsigned int size = startsize;
	static char* line;
	char* ret;
	if (line) {
		free(line);
		line = (char*) 0;
	}
	size = startsize;
	times = 0;
	do {
		if (size > INT_MAX - increase) {
			log(1, "input in config file too long");
			free(line);
			line = (char*) 0;
			return (char*) 0;
		}
		if (!line) {
			line = (char*) malloc(size + 1);
			enough_mem(line);
			line[0] = '\0';
		} else {
			line = (char*) realloc(line, size + 1);
			enough_mem(line);
		}
		/* append the newly read characters to the old ones */
		ret = fgets(line + times*increase, increase + 1, file);
		if (!ret) {
			if (feof(file)) {
				break;
			}
			perror("Error reading a line from the "
			       "configuration file");
			log(1, "Error reading a line from the "
			       "configuration file: %s", strerror(errno));
		}
		size += increase;
		times++;
	} while (ret && ret[strlen(ret)-1] != '\n');

	if (feof(file)) {
		if (line && strlen(line)) {
			return line;
		}
		free(line);
		line = (char*) 0;
		return (char*) 0;
	}
	return line;
}


char* config_read_line(FILE* file) {
	char* line = (char*) 0;

	do {
		line = config_read_line_basic(file);
		if (line) {
			line = trim(line);
		}
	} while (line && ( !strlen(line) || line[0] == '#' ));

	return line;
}


char* trim(char* s) {
	size_t start = 0;
	size_t end;
	size_t i;
	char c;

	if ( !s || !strlen(s) ) {
		return s;
	}

	end = strlen(s);

	while (isspace((int) s[ start ])) {
		start++;
		if (start >= end) {
			s[ 0 ] = '\0';
			return s;
		}
	}

	while (isspace((int) s[ end - 1 ])) {
		end--;
		if (start >= end) {
			return s;
		}
	}

	s[ end ] = '\0';

	if (start == 0) {
		return s;
	}

	i = 0;
	do {
		c = s[ start++ ];
		s[ i++ ] = c;
	} while ( c );

	return s;

}

/* quotstrtok parses *s and returns a malloc'ed pointer to the tokens. It
 * also respects quotation marks:
 *
 * bla "foo bar" 	bar     bla
 *
 * returns bla, "foo bar", bar and bla
 */

char* quotstrtok(const char* s, const char* delim, int *past_offset) {

	int i = *past_offset;
	const char* q = 0;
	char *ret =0, *r =0;
	const char* ubound, *lbound;
	int length;
	const int MAXLENGTH = 65536;

	/*while(s[i] && isspace((int)s[i])) {*/
	/* move forth if s[i] is one of the delimiters */
	while(s[i] && strchr(delim, (int)s[i])) {
		i++;
	}
	if (s[i] == 0)
		return 0;
	if (s[i] == '"') {
		q = strchr(&s[i+1], '"');
		q = strpbrk(++q, delim);
	} else {
		q = strpbrk(&s[i], delim);
	}
	if (!q || !*q) {
		if (strlen(&s[i]) == 0) {
			return 0;
		} else {
			q = s + i + strlen(&s[i]);
		}
	}
	/* chop quotation marks on both sides */
	if (s[i] == '"' && *(q-1) == '"') {
		lbound = &s[i+1];
		ubound = q-1;
	} else {
		lbound = &s[i];
		ubound = q;
	}
	length = MIN_VAL(ubound - lbound, MAXLENGTH);
	ret = (char*) malloc( length + 1);
	enough_mem(ret);
	strncpy(ret, lbound, length);
	ret[length] = '\0';
	*past_offset = i + (q - &s[i]);  /* q - &s[i] is ubound - lbound,
					    regardless of quotation marks */

	if (strlen(ret)) {
		r = ret + strlen(ret) - 1;
		while (iscntrl((int)*r) && strlen(ret)) {
			*r = '\0';
		r--;
		}
	}

	return ret;
}

/* the same as quotstrtok with an additional parameter that is prepended to
 * the quotstrtok output */
char* quotstrtok_prepend(const char* prefix,
			 const char* s, const char* delim, int *past_offset) {
	char* qstr = quotstrtok(s, delim, past_offset);
	char* nstr;
	int newsize;

	if ( ! qstr ) {
		return qstr;
	}
	newsize = strlen(qstr) + strlen(prefix) + 1;
	nstr = (char*) malloc( newsize );
	enough_mem(nstr);
	snprintf(nstr, newsize, "%s%s", prefix, qstr);

	return nstr;
}

struct slist_t* config_split_line(const char *line, const char* delim) {
	int offset = 0;
	char* buf;
	struct slist_t *slistbase =0, *slistcur =0, *ptr =0;

	while ((buf = quotstrtok(line, delim, &offset))) {
		ptr = (struct slist_t*) malloc (sizeof(struct slist_t));
		enough_mem(ptr);
		if (!slistbase) {
			slistbase = ptr;
		}
		if (slistcur) {
			slistcur->next = ptr;
		}
		slistcur = ptr;

		slistcur->value = buf;
		slistcur->next = (struct slist_t*) 0;
	}

	return slistbase;
}

void config_section_init(struct section_t* section) {
	section->hosts         = (struct hostlist_t*) 0;
	section->hosts_exclude = (struct hostlist_t*) 0;

	section->users         = (struct slist_t*) 0;
	section->users_exclude = (struct slist_t*) 0;

	section->forwarded         = (struct slist_t*) 0;
	section->forwarded_exclude = (struct slist_t*) 0;

	section->ports         = (struct portrangestruct*) 0;
	section->ports_exclude = (struct portrangestruct*) 0;

	section->time         = (struct timestruct*) 0;
	section->time_exclude = (struct timestruct*) 0;

	section->options       = (struct option_t*) 0;

	section->nested        = (struct section_t*) 0;
	section->next          = (struct section_t*) 0;

	section->servertype = SERVERTYPE_STANDALONE;
	section->connection_counter = 0;
}

/* ---------------------- begin parse functions -------------------- */

void config_append_string(char** s, char* appstr) {
	if (! appstr || ! s) {
		return;
	}
	if ( ! *s ) {
		*s = (char*) malloc(strlen(appstr) + 1);
		enough_mem(s);
		*s[0] = '\0';
	} else {
		*s = (char*) realloc(*s, strlen(*s) + strlen(appstr) + 1);
		enough_mem(s);
	}
	strcat(*s, appstr);
}

static
char* config_get_valid_tags(const char* key) {
	int i = 0;
	char* retstr = (char*) 0;

	while (configuration_data[i].name) {
		if (strcasecmp(key, configuration_data[i].name) == 0) {
			/* found */
			if (TAG_GLOBAL & configuration_data[i].tag_type) {
				config_append_string(&retstr, " global");
			}
			if (TAG_FROM & configuration_data[i].tag_type) {
				config_append_string(&retstr, " from");
			}
			if (TAG_TO & configuration_data[i].tag_type) {
				config_append_string(&retstr, " to");
			}
			if (TAG_PORT & configuration_data[i].tag_type) {
				config_append_string(&retstr, " port");
			}
			if (TAG_USER & configuration_data[i].tag_type) {
				config_append_string(&retstr, " user");
			}
			if (TAG_FORWARDED & configuration_data[i].tag_type) {
				config_append_string(&retstr, " forwarded");
			}
			if (TAG_TIME & configuration_data[i].tag_type) {
				config_append_string(&retstr, " time");
			}
			if (TAG_SERVERTYPE & configuration_data[i].tag_type) {
				config_append_string(&retstr, " servertype");
			}
			if (TAG_PROXYIP & configuration_data[i].tag_type) {
				config_append_string(&retstr, " proxyip");
			}
			if (TAG_PROXYPORT & configuration_data[i].tag_type) {
				config_append_string(&retstr, " proxyport");
			}
			break;
		}
		i++;
	}
	return retstr;
}

static
int config_is_option_key_valid(const char* key, int tag_num) {
	int i = 0;
	int valid = 0;

	while (configuration_data[i].name) {
		if ((configuration_data[i].match_type == OPTION_EXACT_MATCH
			&& strcasecmp(key, configuration_data[i].name) == 0)) {

			/* found */
			if (tag_num & configuration_data[i].tag_type) {
				valid = 1;
			}
			break;
		}
		i++;
	}
	return valid;
}

static
char* config_get_valid_values(const char* key) {
	int i = 0, j = 0;
	char* retstr = (char*) 0;

	while (configuration_options[i].name) {
		if (strcasecmp(key, configuration_options[i].name) == 0) {
			/* there are limitations for the value */
			while(configuration_options[i].values[j]) {
				config_append_string(&retstr,
					configuration_options[i].values[j]);
				config_append_string(&retstr, " ");
				j++;
			}
			break;
		}
		i++;
	}
	return retstr;
}

static
int config_is_option_value_valid(const char* key, const char* value) {
	int i = 0, j = 0;

	while (configuration_options[i].name) {
		if (strcasecmp(key, configuration_options[i].name) == 0) {
			/* there are limitations for the value */
			while(configuration_options[i].values[j]) {
				/* option values have to match in case */
				if (strcmp(configuration_options[i].values[j],
								value) == 0) {
					/* found it. it's valid */
					return 1;
				}
				j++;
			}
			/* didn't find the value */
			return 0;
		}
		i++;
	}
	/* not found in conf_dat. There are no limitations for the value */
	return 1;
}

static
const char* config_get_delimiter(const char* key) {
	int i = 0;
	while (configuration_data[i].name) {
		if (strncasecmp(key, configuration_data[i].name,
				strlen(configuration_data[i].name)) == 0) {
			/* found */
			if (configuration_data[i].read_limit == FL) {
				return "\n";
			} else {
				return WHITESPACE;
			}
		}
		i++;
	}
	return WHITESPACE;
}

static
int config_parse_option(const char* line, int tag_num,
					struct option_t** options) {
	int pos = 0;
	struct option_t* o;
	char* key, *value, *tmp;

	key   = quotstrtok(line, WHITESPACES, &pos);
	tmp   = strdup(line + pos);
	enough_mem(tmp);
	tmp   = trim(tmp);
	pos   = 0;
	value = quotstrtok(tmp, config_get_delimiter(key), &pos);
	value = trim(value);
	free(tmp);

	if ( ! key || ! value) {
		log(4, "Could not parse option %s", line);
		free(key); free(value);
		return -1;
	}

	if ( ! strlen(key) || ! strlen(value) ) {
		log(4, "Could not parse option %s", line);
		free(key); free(value);
		return -1;
	}

	if ( ! config_is_option_key_valid(key, tag_num) ) {
		char* tags = config_get_valid_tags(key);
		if (tags) {
			log(5, "%s only valid in those tags:%s", key, tags);
			free(tags);
		} else {
			log(5, "unknown option: %s", key);
		}
		free(key); free(value);
		return -1;
	}

	if ( ! config_is_option_value_valid(key, value) ) {
		char* values = config_get_valid_values(key);
		if (values) {
			log(5, "%s may only take those values: %s",
								key, values);
			free(values);
			return -1;
		}
	}

	if (*options) {
		o = optionlist_push(*options, key, value);
	} else {
		o = optionlist_init(key, value);
		*options = o;
	}
	free(key); /* = freaky ! */
	free(value);
	return o != (struct option_t*) 0;
}

static
int config_parse_users(char* line, struct slist_t** sl) {
	int offset = 0;
	char* entry;

	if (!line || !strlen(line)) {
		*sl = (struct slist_t*) 0;
		return 0;
	}

	while ((entry = quotstrtok(line, WHITESPACES, &offset))) {
		if (! *sl) {
			*sl = slist_init(entry);
		} else {
			slist_push(*sl, entry);
		}
	}
	return 0;
}

struct portrangestruct* config_parse_portranges(const char* line) {
	int offset = 0;
	struct portrangestruct* prsbase, *prscur, *prs;
	char* startportstr, *endportstr;
	long int startport, endport;

	struct slist_t* slistbase, *slistcur, *prevtmp;

	if ( ! line ) {
		return (struct portrangestruct*) 0;
	}
	slistcur = slistbase = config_split_line(line, WHITESPACES"\n"); 
	prsbase = prs = prscur = (struct portrangestruct*) 0;

	while (slistcur) {
		offset = 0;
		startportstr = quotstrtok(slistcur->value, ":", &offset);
		if (!startportstr) {
			log(3, "invalid port range: %s", slistcur->value);
			slist_destroy(slistbase);
			return (struct portrangestruct*) 0;
		}
		endportstr = quotstrtok(slistcur->value, ":", &offset);
		if (!endportstr) {
			size_t size = strlen(slistcur->value) * 2 + 1 + 1;
			char* newentry = (char*) malloc(size);
			snprintf(newentry, size, "%s:%s",
					slistcur->value, slistcur->value);
			free(slistcur->value);
			slistcur->value = newentry;
			offset = 0;
			if (startportstr) { free(startportstr); }
			startportstr = quotstrtok(slistcur->value, ":",
					&offset);
			endportstr = quotstrtok(slistcur->value, ":",
					&offset);
		}
		if (!*startportstr || !*endportstr) {
			log(3, "invalid port range: %s", slistcur->value);
			if (startportstr) { free(startportstr); }
			if (endportstr)   { free(endportstr);   }
			slist_destroy(slistbase);
			return (struct portrangestruct*) 0;
		}
		startport = strtol(startportstr, NULL, 10);
		if (errno == ERANGE
				&& (startport == LONG_MIN || startport == LONG_MAX)) {
			log(3, "Error reading the starting port number in %s",
				startportstr);
			slist_destroy(slistbase);
			free(startportstr);
			if (endportstr) { free(endportstr); }
			return (struct portrangestruct*) 0;
		}
		free(startportstr);
		startportstr = (char*) 0;

		endport = strtol(endportstr, NULL, 10);
		if (errno == ERANGE
				&& (endport == LONG_MIN || endport == LONG_MAX)) {
			log(3, "Error reading the ending port number in %s",
				endportstr);
			slist_destroy(slistbase);
			free(endportstr);
			return (struct portrangestruct*) 0;
		}
		free(endportstr);
		endportstr = (char*) 0;

		/* startport really below or equal to endport ? */
		if (startport > endport) {
			log(4, "Port range %d:%d invalid (starting port number"
				" %d is _above_ endport %d)",
					startport, endport,
					startport, endport);
			slist_destroy(slistbase);
			return (struct portrangestruct*) 0;
		}

		/* I want only to allow clear ranges. This is better for the
		 * checking, for the selection of a port and the admin is
		 * forced to specify his ports in a clear manner  :-)
		 *
		 * That means that a portrange like:
		 *
		 * 1000:2000   1500:1600   is invalid, since 1500:1600 is
		 * already in 1000:2000
		 *
		 * Also invalid:
		 *
		 * 1000:2000   1800:2200   (200 portnumbers are the same)
		 * 1000:2000   2000:3000   (1 portnumber is the same)
		 * 1000:2000   500:1000    (1 portnumber is the same)
		 * 1000:2000   799:1200    (200 portnumbers are the same)
		 *
		 * criteria: startport and endport => let's say just pno
		 *
		 * If pno is between (or equal to) the range of another port
		 * specification, state that is invalid.
		 * ------
		 * What happens if we first specify
		 *
		 * 300:400
		 *
		 * and then
		 *
		 * 200:1000 ?
		 *
		 * We have to do the checking for _each_ new range to _all_
		 * ranges. Compare every range to every other range
		 *
		 */

		prs = prsbase;
		while (prs) {
			if (
			    (startport >= prs->startport &&
			     startport <= prs->endport)
			    ||
			    (endport >= prs->startport &&
			     endport <= prs->endport)
			   ) {
				log(4, "Port range %d:%d invalid (parts "
					"covered by another range (%d:%d))",
						startport, endport,
						prs->startport, prs->endport);
				slist_destroy(slistbase);
				return (struct portrangestruct*) 0;
			}

			if (
			    (prs->startport >= startport &&
			     prs->startport <= endport)
			    ||
			    (prs->endport >= startport &&
			     prs->endport <= endport)
			    ) {
				log(4, "Port range %d:%d invalid (parts "
					"covered by another range (%d:%d))",
						prs->startport, prs->endport,
						startport, endport);
				slist_destroy(slistbase);
				return (struct portrangestruct*) 0;
			}
			prs = prs->next;
		}

		prs = (struct portrangestruct*)
				malloc(sizeof(struct portrangestruct));
		if (prscur) {
			prscur->next = prs;
		}

		prscur = prs;

		if (!prsbase) {
			/* the first one */
			prsbase = prscur;
		}
		prscur->startport = startport;
		prscur->endport = endport;
		prscur->next = (struct portrangestruct*) 0;

		/* iterate through and always chop off the first element */
		prevtmp = slistcur;
		slistcur = slistcur->next;
		slistbase = slistcur;
		free(prevtmp->value);
		free(prevtmp);
		prevtmp = 0;
	}

	return prsbase;
}

static
int config_parse_ports(char* line, struct portrangestruct** pr) {
	*pr = config_parse_portranges(line);
	return 0;
}


static
struct hostlist_t* config_parse_ips(struct hostlist_t** hl,
							const char* line) {
	const char* p = line;
	int invalid = 0;
	char ipbuf[16];

	struct hostlist_t* h_list = (struct hostlist_t*) 0;

	unsigned long int ip, netmask;

	ip = -1;
	netmask = -1;

	if ( !line ) { *hl = (struct hostlist_t*) '\0'; };

	for (;;) {
		int i =0;
		while (p && *p) {
			/* set p to the next part */
			if (isdigit((int)*p)) {
				if (p == line) {
					break;
				}
				if (isspace((int)*(p-1))) {
					break;
				}
			}
			p++;
		}
		if (!p || !*p) {
			break;
		}
		/* p theoretically points to an ip */
		if (isdigit((int)*p)) {
			while ((isdigit((int)*p) || *p == '.') && *p != '/') {
				if (i < sizeof(ipbuf)-1) {
					ipbuf[i++] = *p;
				}
				p++;
			}
			ipbuf[i] = '\0';
			/* log(9, "Found ip %s", ipbuf); */
			ip = inet_addr(ipbuf);
			if (ip == (unsigned long int) UINT_MAX
					&& strcmp(ipbuf, BROADCAST)) {
				log(4, "Invalid IP: %s", ipbuf);
				return (struct hostlist_t *) 0;
			}
			if (*p == '/') {
				p++; i =0;
				while (!isspace((int)*p) && *p) {
					if (i < sizeof(ipbuf)-1) {
						ipbuf[i++] = *p;
					}
					p++;
				}
				ipbuf[i] = '\0';
			/*	log(8, "With netmask %s", ipbuf); */
				invalid = 0;

				if (strlen(ipbuf) < 3 && !strchr(ipbuf, '.')) {
					netmask = atoi(ipbuf);
					if (netmask < 0 || netmask > 32) {
						invalid = 1;
					}
					else {
						netmask = setlastbits(netmask);
					}
				} else {
					netmask = inet_addr(ipbuf);
					if (netmask == (unsigned long int)
								UINT_MAX
						&& strcmp(ipbuf, BROADCAST)) {
						invalid = 1;
					}
				}
				if (invalid) {
					log(4, "Invalid netmask: %s", ipbuf);

					/* Set the netmask to
					 * 255.255.255.255 */

					netmask = -1;
				}
			} else {
				/* no netmask specified */
				netmask = -1;
			}
			h_list = hostlist_ip_push(hl, ip, netmask);
		}
	}
	return h_list;
}


#define NAMESIZE  3
static
struct hostlist_t* config_parse_names(struct hostlist_t** hl,
							const char* line) {
	const char* p = line;
	char* buf, *tmp;
	char n[NAMESIZE];
	int i;
	size_t bufsize;

	struct hostlist_t* h_list = (struct hostlist_t*) 0;

	if ( !line ) { *hl = (struct hostlist_t*) '\0'; };

	for (;;) {
		while( p && *p && !isalpha((int)*p)) {
			/* see if it is a domain like .ibm.com */
			if (*p == '.') {
				if (p == line) {
					/* at the beginning of the line */
					break;
				}
				if (isspace((int)*(p-1))) {
					/* the dot is following a space chr */
					break;
				}
			}
			p++;
		}
		if ( !p || !*p ) {
			break;
		}
		buf = (char*) malloc(1);
		buf[0] = '\0';
		while(*p && !isspace((int)*p)) {
			i =0;
			while (i < NAMESIZE - 1 && *p && !isspace((int)*p)) {
				n[i++] = *p++;
			}
			n[i] = '\0';
			tmp = buf;
			bufsize = strlen(buf) + strlen(n) + 1;
			buf = (char*) malloc(bufsize);
			snprintf(buf, bufsize, "%s%s", tmp, n);
			free(tmp);
			tmp = 0;
		}
		/* log(9, "Read name %s", buf); */

		h_list = hostlist_name_push(hl, buf);
	}
	return h_list;
}


static
int config_parse_hosts(char* line, struct hostlist_t** hl) {
	config_parse_ips(hl, line);
	config_parse_names(hl, line);

	return 0;
}

static
int day_number(char* day) {
	if (! day) {
		return -1;
	}
	if (strcasecmp(day, "sun") == 0) {
		return 0;
	}
	if (strcasecmp(day, "mon") == 0) {
		return 1;
	}
	if (strcasecmp(day, "tue") == 0) {
		return 2;
	}
	if (strcasecmp(day, "wed") == 0) {
		return 3;
	}
	if (strcasecmp(day, "thu") == 0) {
		return 4;
	}
	if (strcasecmp(day, "fri") == 0) {
		return 5;
	}
	if (strcasecmp(day, "sat") == 0) {
		return 6;
	}
	return -1;
}

static
int config_parse_time(char* line, struct timestruct** ts) {
	int offset = 0, offset2, offset3;
	struct timestruct* ts_end = (struct timestruct*) 0;
	struct timestruct* t = (struct timestruct*) 0;
	char* entry;
	char* dat_str;
	char* tm_str;
	int i;

	*ts = (struct timestruct*) 0;
	if (!line || !strlen(line)) {
		return 0;
	}

	while ((entry = quotstrtok(line, ";,", &offset))) {
		if (!t) {
			t = (struct timestruct*)
					malloc(sizeof(struct timestruct));
		} else {
			/* there was an illegal time and the alllocated t
			 * was not used. */
		}
		enough_mem(t);
		t->next = (struct timestruct*) 0;
		t->days = (struct ilist_t*) 0;
		t->start_day = t->end_day = -1;

		/* Mon/Tue/Sun 17.00 - 19.00
		 * Wed 19.00 - Fri 20.00
		 */

		offset2 = 0;
		dat_str = quotstrtok(entry, WHITESPACE, &offset2);
		/* is dat_str a range ? */
		if (strchr(dat_str, '/')) {
			/* yes */
			char* wday;
			offset3 = 0;
			while ((wday = quotstrtok(dat_str, "/", &offset3))) {
				if (day_number(wday) < 0) {
					log(5, "Invalid date (day %s not known): %s", wday, entry);
					free(wday); free(dat_str); free(entry);
					/* set dat_str to 0 to indicate that
					 * we this date is invalid */
					dat_str = (char*) 0;
					break;
				}
				if ( ! t->days ) {
					t->days = ilist_init(day_number(wday));
				} else {
					ilist_push(t->days, day_number(wday));
				}
				free(wday);
			}
		} else {
			/* no */
			t->start_day = day_number(dat_str);
		}
		if (!dat_str) {
			break;
		}
		free(dat_str);

		tm_str = quotstrtok(entry, WHITESPACE"-", &offset2);
		i = sscanf(tm_str, "%d.%d", &t->start_hour, &t->start_minute);
		if (i != 2) {
			i = sscanf(tm_str, "%d:%d",
					&t->start_hour, &t->start_minute);
		}
		if (i != 2) {
			log(5, "Illegal start time: %s", entry);
			free(entry); free(tm_str);
			break;
		}
		if (t->start_hour < 0 || t->start_hour > 23) {
			log(5, "Illegal start time: %s", entry);
			free(entry); free(tm_str);
			break;
		}
		if (t->start_minute < 0 || t->start_minute > 59) {
			log(5, "Illegal start time: %s", entry);
			free(entry); free(tm_str);
			break;
		}
		free(tm_str);

		while(strchr(WHITESPACE"-", entry[offset2])) {
			offset2++;
		}

		/* still have
		 *
		 * 19.00
                 * Fri 20.00
		 */

		dat_str = quotstrtok(entry, WHITESPACE, &offset2);
		if ((t->end_day = day_number(dat_str)) >= 0) {
			free(dat_str);
			/* was a date */
			if (t->days) {
				log(5, "You can't specify a day for the end time if you specified a range of days for the start time: %s", entry);
				free(dat_str); free(entry);
				break;
			}
			dat_str = quotstrtok(entry, "\n", &offset2);
		} else {
			/* may be a time */
			if (strchr(dat_str, '/')) {
				log(5, "Ranges not allowed for the end time: %s", entry);
				free(dat_str); free(entry);
				break;
			}
		}
		i = sscanf(dat_str, "%d.%d", &t->end_hour, &t->end_minute);
		if (i != 2) {
			i = sscanf(dat_str, "%d:%d",
						&t->end_hour, &t->end_minute);
		}
		if (i != 2) {
			log(5, "Illegal end time: %s", entry);
			free(dat_str); free(entry);
			break;
		}
		free(dat_str);

		if (!t->days && t->end_day == -1) {
			t->end_day = t->start_day;
		}
		if (t->days || t->start_day == t->end_day) {
			/* start and stop time on the same day, check if
			 * times make sense */
			if (t->end_hour*100 + t->end_minute <
			    t->start_hour*100 + t->start_minute) {
				log(5, "end time before start time on the same day: %s", entry);
				free(entry);
				break;
			}
		}
		if (! *ts) {
			*ts = t;
			ts_end = t;
		} else {
			ts_end->next = t;
			ts_end = t;
		}
		free(entry);
		t = (struct timestruct*) 0;
	}
	if (t) {
		free(t);
	}
	return 0;
}


/* ---------------------- end parse functions -------------------- */


/* ------------------- begin config_parse_tag_* ------------------ */

static
int config_parse_tag_global(char* line, struct section_t* section) {
	section->hosts = (struct hostlist_t*)malloc(sizeof(struct hostlist_t));
	enough_mem(section->hosts);

	section->hosts->host.ip.ip      = 0;
	section->hosts->host.ip.netmask = 0;
	section->hosts->host.name       = (char*) 0;
	section->hosts->next            = (struct hostlist_t*) 0;

	section->hosts_exclude          = (struct hostlist_t*) 0;
	return 0;
}

static
struct tag_options_t config_parse_tag_options(char* line) {
	struct tag_options_t to = { (char*) 0, (char*) 0 };
	char* space = strpbrk(line, WHITESPACE);
	char* exclude;
	char* end;

	if (! space) { return to; };
	if (! (exclude = strstr(space, "exclude"))) {
		end = strchr(space, '>');
		if (! end) {
			return to;
		}
		*end = '\0';
		to.list_str = strdup(space);
		enough_mem(to.list_str);
		to.list_exclude_str = (char*) 0;
	} else {
		*exclude = '\0';
		exclude = strpbrk(++exclude, WHITESPACE);
		to.list_str = strdup(space);
		enough_mem(to.list_str);

		if (! exclude) {
			log(4, "malformed line starting with %s", space);
			free(to.list_str);
			to.list_str = (char*) 0;
			return to;
		}

		end = strchr(exclude, '>');
		if (! end) {
			free(to.list_str);
			to.list_str = (char*) 0;
			return to;
		}
		*end = '\0';
		to.list_exclude_str = strdup(exclude);
		enough_mem(to.list_exclude_str);
	}
	if (to.list_exclude_str) {
		to.list_exclude_str = trim(to.list_exclude_str);
	}
	to.list_str = trim(to.list_str);
	return to;
}

static
int config_parse_tag_hostlist(char* line, struct section_t* section) {
	struct tag_options_t to = config_parse_tag_options(line);
	int ret;

	if (to.list_str == (char*) 0) {
		return -1;
	}
	if ((ret = config_parse_hosts(to.list_str, &section->hosts)) < 0) {
		free(to.list_str);
		free(to.list_exclude_str);
		return ret;
	}
	free(to.list_str);

	if ((ret = config_parse_hosts(to.list_exclude_str,
					&section->hosts_exclude)) < 0) {
		free(to.list_exclude_str);
		return ret;
	}
	if (to.list_exclude_str) {
		free(to.list_exclude_str);
	}

	return 0;
}

static
int config_parse_tag_from(char* line, struct section_t* section) {
	return config_parse_tag_hostlist(line, section);
}

static
int config_parse_tag_to(char* line, struct section_t* section) {
	return config_parse_tag_hostlist(line, section);
}

static
int config_parse_tag_proxyip(char* line, struct section_t* section) {
	return config_parse_tag_hostlist(line, section);
}

static
int config_parse_tag_user(char* line, struct section_t* section) {
	int ret;
	struct tag_options_t to = config_parse_tag_options(line);

	if (to.list_str == (char*) 0) {
		return -1;
	}

	if ((ret = config_parse_users(to.list_str, &section->users)) < 0) {
		free(to.list_str);
		free(to.list_exclude_str);
		return ret;
	}
	free(to.list_str);

	if ((ret = config_parse_users(to.list_exclude_str,
					&section->users_exclude)) < 0) {
		free(to.list_exclude_str);
		return ret;
	}
	if (to.list_exclude_str) {
		free(to.list_exclude_str);
	}

	return 0;
}

static
int config_parse_tag_port(char* line, struct section_t* section) {
	struct tag_options_t to = config_parse_tag_options(line);
	int ret;

	if (to.list_str == (char*) 0) {
		return -1;
	}

	if ((ret = config_parse_ports(to.list_str, &section->ports)) < 0) {
		free(to.list_str);
		free(to.list_exclude_str);
		return ret;
	}
	free(to.list_str);

	if ((ret = config_parse_ports(to.list_exclude_str,
					&section->ports_exclude)) < 0) {
		free(to.list_exclude_str);
		return ret;
	}
	if (to.list_exclude_str) {
		free(to.list_exclude_str);
	}

	return 0;
}

static
int config_parse_tag_time(char* line, struct section_t* section) {
	struct tag_options_t to = config_parse_tag_options(line);
	int ret;

	if (to.list_str == (char*) 0) {
		return -1;
	}

	if ((ret = config_parse_time(to.list_str, &section->time)) < 0) {
		free(to.list_str);
		free(to.list_exclude_str);
		return ret;
	}
	free(to.list_str);

	if ((ret = config_parse_time(to.list_exclude_str,
					&section->time_exclude)) < 0) {
		free(to.list_exclude_str);
		return ret;
	}
	if (to.list_exclude_str) {
		free(to.list_exclude_str);
	}

	return 0;
}

static
int config_parse_tag_servertype(char* line, struct section_t* section) {
	section->servertype = SERVERTYPE_STANDALONE;
	if (!line || !strlen(line)) {
		return 0;
	}
	if (strstr(line, "inetd")) {
		section->servertype = SERVERTYPE_INETD;
	}
	return 0;
}

/* ------- end of config_parse_tag_* ----------- */

static
int config_get_tag_name(const char* line, int begin) {
	int tag_name;

	if ( !line || strlen(line) < 2 ) {
		return TAG_UNKNOWN;
	}

	if (begin == TAG_CLOSING) {
		begin = 2;
	} else {
		begin = 1;
	}

	if (checkbegin(&line[ begin ], TAG_GLOBAL_STR)) {
		tag_name = TAG_GLOBAL;
	} else if (checkbegin(&line[ begin ], TAG_FROM_STR)) {
		tag_name = TAG_FROM;
	} else if (checkbegin(&line[ begin ], TAG_TO_STR)) {
		tag_name = TAG_TO;
	} else if (checkbegin(&line[ begin ], TAG_USER_STR)) {
		tag_name = TAG_USER;
	} else if (checkbegin(&line[ begin ], TAG_PORT_STR)) {
		tag_name = TAG_PORT;
	} else if (checkbegin(&line[ begin ], TAG_FORWARDED_STR)) {
		tag_name = TAG_FORWARDED;
	} else if (checkbegin(&line[ begin ], TAG_TIME_STR)) {
		tag_name = TAG_TIME;
	} else if (checkbegin(&line[ begin ], TAG_SERVERTYPE_STR)) {
		tag_name = TAG_SERVERTYPE;
	} else if (checkbegin(&line[ begin ], TAG_PROXYIP_STR)) {
		tag_name = TAG_PROXYIP;
	} else if (checkbegin(&line[ begin ], TAG_PROXYPORT_STR)) {
		tag_name = TAG_PROXYPORT;
	} else {
		tag_name = TAG_UNKNOWN;
	}
	return tag_name;
}

static
int config_get_tag_num(struct ilist_t* tag_list) {
	int ret = 0;

	if (tag_list) {
		tag_list = tag_list->next;
	}

	while (tag_list) {
		ret = ret | tag_list->value;
		tag_list = tag_list->next;
	}
	return ret;
}

static
int ld(int a) {
	/* calculate logarithmus dualis */
	int i;
	int exp;

	if (a == 0) {
		return 0;
	}
	i = 1;
	exp = 0;

	while (a > i) {
		exp++;
		i *= 2;
	}
	return exp;
}

struct section_t* config_parse_section(FILE* file,
					char* line,
					int tag_name,
					int* id,
					struct ilist_t* tag_list) {
	int tag_type;
	int tag_num = config_get_tag_num(tag_list);
	int ret;
	char* recline;
	int (*handler_funcs[ TAG_NUMBER + 1 ])(char*, struct section_t*);
	struct section_t *section = (struct section_t*)
					malloc(sizeof(struct section_t));
	enough_mem(section);
	config_section_init(section);

	handler_funcs[ ld(TAG_GLOBAL)     ] = config_parse_tag_global;
	handler_funcs[ ld(TAG_FROM)       ] = config_parse_tag_from;
	handler_funcs[ ld(TAG_TO)         ] = config_parse_tag_to;
	handler_funcs[ ld(TAG_USER)       ] = config_parse_tag_user;
	/* set the handler to anything */
	handler_funcs[ ld(TAG_FORWARDED)  ] = config_parse_tag_user;
	handler_funcs[ ld(TAG_PORT)       ] = config_parse_tag_port;
	handler_funcs[ ld(TAG_TIME)       ] = config_parse_tag_time;
	handler_funcs[ ld(TAG_SERVERTYPE) ] = config_parse_tag_servertype;
	handler_funcs[ ld(TAG_PROXYIP)    ] = config_parse_tag_proxyip;
	handler_funcs[ ld(TAG_PROXYPORT)  ] = config_parse_tag_port;

	ret = (*handler_funcs[ ld(tag_name) ])(line, section);
	(*id)++;
	section->id = *id;

	while ((line = config_read_line(file))) {
		if (line[0] == '<') {
			/* probably a tag */
			if (line[1] == '/') {
				tag_type = TAG_CLOSING;
			} else {
				tag_type = TAG_OPENING;
			}
			tag_name = config_get_tag_name(line, tag_type);
			if (tag_name == TAG_UNKNOWN) {
				log(3, "invalid tag: %s", line);
				config_error = 1;
				return (struct section_t*) 0;
			}
			section->tag_name = tag_name;
			if (tag_type == TAG_OPENING) {
				struct section_t* s, *t;
				ilist_push(tag_list, tag_name);

				recline = strdup(line);
				enough_mem(recline);

				s = config_parse_section(file, recline,
						tag_name, id, tag_list);

				free(recline);

				if (! section->nested) {
					section->nested = s;
				} else {
					/* already a nested */
					t = section->nested;
					while (t->next) {
						t = t->next;
					}
					t->next = s;
					s->next = (struct section_t*) 0;
				}
			} else {
				/* tag_type = TAG_CLOSING */
				int last_opened;
				if (ilist_empty(tag_list)) {
					log(3, "Closed a tag where no one "
							"was open: %s", line);
					config_error = 1;
					return (struct section_t*) 0;
				}
				last_opened = ilist_pop(tag_list);
				if (last_opened != tag_name) {
					log(3, "Closed a tag that was not "
						"opened before: %s", line);
					config_error = 1;
					return (struct section_t*) 0;
				}
				break; /* go out of while */
			}
		} else {
			/* not a tag: an option */

			recline = strdup(line);
			enough_mem(recline);

			ret = config_parse_option(recline, tag_num,
					&section->options);

			free(recline);

			if (ret < 0) {
				/* don't throw an error  - no, do it  :-) */
				/* return (struct section_t*) 0; */
				config_error = 1;
				return (struct section_t*) 0;
			}
		}
/* continue goes here */
	}
	return section;
}


void config_set_limits(struct section_t* section) {
	struct option_t* opt;

	if (! section) {
		return;
	}

	config_set_limits(section->next);
	config_set_limits(section->nested);

	section->limit = LONG_MAX;

	opt = section->options;
	while (opt) {
		if (strcasecmp(opt->key, "limit") == 0) {
			section->limit = conv_char2long(opt->value, LONG_MAX);
		}
		opt = opt->next;
	}
}

/* check for all sections if connection_counter is less than or equal to
 * limit
 *
 * return 0 if everything is okay and
 * return 1 if somewhere a limit has been exceeded
 * */
static
int config_check_limits(const struct section_t* section) {
	if ( ! section ) {
		return 0;
	}
	if (section->connection_counter > section->limit) {
		return 1;
	}

	if (config_check_limits(section->nested)) {
		return 1;
	}
	return config_check_limits(section->next);
}

struct timestruct* timestruct_clone(const struct timestruct* orig) {
	struct timestruct* new;

	if (! orig) {
		return (struct timestruct*) 0;
	}

	new = (struct timestruct*) malloc(sizeof(struct timestruct));
	new->days = ilist_clone(orig->days);
	new->start_day = orig->start_day;
	new->start_hour = orig->start_hour;
	new->start_minute = orig->start_minute;

	new->end_day = orig->end_day;
	new->end_hour = orig->end_hour;
	new->end_minute = orig->end_minute;

	new->next = timestruct_clone(orig->next);
	return new;
}

struct section_t* section_clone(const struct section_t* orig) {
	struct section_t* new;

	if (! orig) {
		return (struct section_t*) 0;
	}

	new = (struct section_t*) malloc(sizeof(struct section_t));

	new->tag_name = orig->tag_name;
	new->hosts = hostlist_clone(orig->hosts);
	new->hosts_exclude = hostlist_clone(orig->hosts_exclude);
	new->users = slist_clone(orig->users);
	new->users_exclude = slist_clone(orig->users_exclude);
	new->forwarded = slist_clone(orig->forwarded);
	new->forwarded_exclude = slist_clone(orig->forwarded_exclude);
	new->ports = portrangestruct_clone(orig->ports);
	new->ports_exclude = portrangestruct_clone(orig->ports_exclude);
	new->time = timestruct_clone(orig->time);
	new->time_exclude = timestruct_clone(orig->time_exclude);
	new->options = optionlist_clone(orig->options);
	new->nested = section_clone(orig->nested);
	new->next = section_clone(orig->next);

	new->connection_counter = orig->connection_counter;
	new->id = orig->id;
	new->servertype = orig->servertype;
	new->limit = orig->limit;

	return new;
}


FILE* open_file(const char* fname) {
	FILE* conf;

	conf = fopen(fname, "r");
	if (!conf) {
		perror("Couldn't open the configuration file");
		log(1, "Couldn't open the configuration file %s: %s", fname,
		strerror(errno));
		return 0;
	}
	return conf;
}


int config_read_sections(FILE* file) {
	char* line, *recline;
	struct ilist_t* tag_list = ilist_init(-1);
	struct section_t* section;
	int tag_name;
	int id = 0;
	int counter = 0;

	base_section = (struct section_t*) 0;
	section  = (struct section_t*) 0;

	/* init recursion */

	do {
		do {
			line = config_read_line(file);
			if ( line && line[0] != '<' ) {
				log(6, "Garbage in config file: %s", line);
			}
		} while (line && (line[0] != '<' || line[1] == '/'));

		if ( !line ) {
			break;
		}

		counter++;
		if ( counter == 1 && ! strncasecmp(&line[1],
				"global", strlen("global")) == 0) {

			log(4, "global section is not the first one. "
				"Exiting.");
			return -1;
		}
		while (section && section->next) {
			section = section->next;
		}
		tag_name = config_get_tag_name(line, TAG_OPENING);
		if (tag_name == TAG_UNKNOWN) {
			log(5, "Invalid tag: %s", line);
			continue;
		}
		ilist_push(tag_list, tag_name);
		if (section) {
			recline = strdup(line);
			enough_mem(recline);

			section->next = config_parse_section(file,
					recline, tag_name, &id, tag_list);

			free(recline);

			if (!section) {
				break;
			}
		} else {
			recline = strdup(line);
			enough_mem(recline);

			section = config_parse_section(file,
					recline, tag_name, &id, tag_list);

			free(recline);
			if (!section) {
				break;
			}

			section->next = (struct section_t*) 0;
			base_section = section;
		}
	} while (line && !config_error);

	if (config_error) {
		return -1;
	}

	if (! base_section) {
		log(3, "No global section in configuration file");
		return -1;
	}

	if ( ! ilist_empty(tag_list)) {
		log(3, "Not all tags have been closed");
		return -1;
	}

	ilist_destroy(tag_list);
	tag_list = (struct ilist_t*) 0;

	/* set limit values */
	config_set_limits(base_section);

	if (debug) {
		config_debug_outputsections();
	}
	return 0;
}


int read_config(const char* fname) {
	int ret;
	char* filename = chrooted_path(fname);
	FILE* conffile = open_file(filename);
	if (!conffile) {
		free(filename);
		return -1;
	}
	log(9, "opened file %s", filename);
	free(filename);

	ret = config_read_sections(conffile);
	fclose(conffile);

	if (ret == -1) {
		/* an error - try to activate backup */
		if (config_activate_backup() < 0) {
			/* failed */
			return -1;
		}
		/* backup successfully activated */
		log(2, "New configuration NOT active - backup configuration"
				" has been re-enabled");
	} else {
		/* destroy the backup and create a new copy, we've read a
		 * fresh configuration */
		config_create_backup();
	}

	optionlist_destroy( option_list );
	option_list = (struct option_t*) 0;
	if (base_section) {
		ret = config_shrink_config(-1,	/* source IP */
				-1,		/* dest IP */
				(char*) 0,	/* dest name */
				0,		/* dest port */
				(char*) 0,	/* dest user */
				-1,		/* forwarded IP */
				(char*) 0,	/* forwarded destination */
				0,		/* forwarded destinationport */
				(char*) 0,	/* forwarded username */
				0,		/* set no specific time */
				-1,		/* proxy ip */
				0,		/* proxy port */
				srvinfo.servertype,	/* global variable */
				&hostcache,
				TAG_GLOBAL | TAG_SERVERTYPE);
	}

	/* switch debug on if there is just one process but leave it if
	 * we are running from inetd */
	debug = srvinfo.servertype == SERVERTYPE_STANDALONE 
			&& !srvinfo.multithread
			&& config_get_ioption("debuglevel", 6) > 8;

	if (ret != -1) {
		/* cache the UIDs */
		ret = save_runasuser_uid();
	}

	if (ret == 0) {
		/* dump configuration */
		if (debug) {
			config_debug_outputsections();

			/* option_list */
			printf("\n\nOptionlist:\n");
			config_debug_output_options( config_get_option_list(), "" );
		}
	}

	return ret;
}


/* --------- begin destruction of structures --------- */
void config_destroy_hostlist(struct hostlist_t* hlist) {
	if (! hlist) { return; }
	config_destroy_hostlist( hlist->next );
	if (hlist->host.name) { free(hlist->host.name); }
	free(hlist);
}

void config_destroy_slist(struct slist_t* slist) {
	slist_destroy(slist);
}

void config_destroy_portrange(struct portrangestruct* plist) {
	if ( !plist ) { return; }
	config_destroy_portrange( plist->next );
	free(plist);
}

void config_destroy_optionlist(struct option_t* olist) {
	optionlist_destroy(olist);
}

void config_destroy_section(struct section_t* sectlist) {
	if ( ! sectlist ) { return; }
	config_destroy_section( sectlist->nested );
	config_destroy_section( sectlist->next );

	config_destroy_hostlist( sectlist->hosts );
	config_destroy_hostlist( sectlist->hosts_exclude );

	config_destroy_slist( sectlist->users );
	config_destroy_slist( sectlist->users_exclude );

	config_destroy_portrange( sectlist->ports );
	config_destroy_portrange( sectlist->ports_exclude );

	config_destroy_optionlist( sectlist->options );

	/* finally destroy the section itself */
	free(sectlist);
}

void config_destroy_sectionconfig() {
	config_destroy_section(base_section);
	base_section = (struct section_t*) 0;
}

/* --------- end destruction of structures --------- */


/* this function is called after each final shrink of the configuration. If
 * the login fails for some reason, the user will issue another
 * authentification and the new shrink operations will deal with a fresh
 * configuration.
 *
 * If the user succeeds instead with login, the optionlist of the former
 * configuration is active and the backup will be deleted (some functions
 * above  :-) */

int config_activate_backup() {
	/* move backup into life */

	if ( ! backup_base_section ) {
		return -1;
	}

	/* free shrinked config */
	config_destroy_section(base_section);

	/* the shrinked configuration is still active through the option
	 * list but it will be regenerated upon a new shrink, i.e. a new
	 * login */

	/* create a new clone of the backup */
	base_section = backup_base_section;
	backup_base_section = (struct section_t*) 0;
	config_create_backup();

	log(8, "Backup configuration activated");

	return 0;
}


/* -------------------- begin lookup functions ------------------ */

int config_host_valid(const struct hostent_list* h) {
	long lookup_diff = config_get_loption("hostcachetimeout", 28800);
	time_t now = time(NULL);

	if (!h) {
		return 0;
	}

	return (now - h->lookup_time) <= (lookup_diff);
}

const struct hostent_list* config_forward_lookup(struct hostent_list** hl,
							const char* name) {

	const struct hostent_list* h;
	struct hostent* he;
	unsigned long int addr;

	if (config_get_bool("forwardlookups") == 0
			||
	    config_get_bool("dnslookups") == 0) {
		return (struct hostent_list*) 0;
	}

	if (! name || ! hl) {
		return (struct hostent_list*) 0;
	}

	/* log(9, "looking up %s", name); */
	if (*hl) {
		h = hostent_get(*hl, 0, name);
		if (config_host_valid(h)) {
		/*	log(9, "Could use cache for forward lookup of %s",
					name); */
			return h;
		} else {
		/*	log(9, "Could not use cache for forward lookup of %s",
					name); */
			hostent_delete(*hl, h);
		}
	}
	addr = inet_addr(name);
	if (addr == UINT_MAX) {
		/* it is really a name */
		he = gethostbyname(name);
	} else {
		he = gethostbyaddr((char*) &addr, sizeof(addr), AF_INET);
	}
	if (*hl) {
		hostent_push(*hl, he, 0, name);
	} else {
		*hl = hostent_init(he, 0, name);
	}
	return hostent_get(*hl, 0, name);
}

const struct hostent_list* config_reverse_lookup(struct hostent_list** hl,
							unsigned long int ip) {
	const struct hostent_list* h;
	struct hostent* he;

	if (config_get_bool("reverselookups") == 0
			||
	    config_get_bool("dnslookups") == 0) {
		return (struct hostent_list*) 0;
	}
	if (! hl) {
		return (struct hostent_list*) 0;
	}

	/* log(9, "looking up %s", inet_ntoa(*((struct in_addr*) &ip))); */
	if (*hl) {
		h = hostent_get(*hl, ip, (char*) 0);
		if (config_host_valid(h)) {
		/*	log(9, "Could use cache for reverse lookup of %s",
					inet_ntoa(*((struct in_addr*) &ip)));
		*/
			return h;
		} else {
			hostent_delete(*hl, h);
		/*	log(9, "Could not use cache for reverse lookup of %s",
					inet_ntoa(*((struct in_addr*) &ip)));
		*/
		}
	}
	he = gethostbyaddr((char*) &ip, sizeof(ip), AF_INET);
	if (*hl) {
		hostent_push(*hl, he, ip, (char*) 0);
	} else {
		*hl = hostent_init(he, ip, (char*) 0);
	}
	return hostent_get(*hl, ip, (char*) 0);
}

/* -------------------- end lookup functions ------------------ */



/* ------------------ begin matching functions ------------------- */

static
int config_match_domain(const char* name, const char* pattern) {
	const char* start;
	/* the hostname may not be shorter than the pattern
	 *
	 * pattern: .foobar.mit.edu
	 * name:	 bla.mit.edu
	 *
	 * => won't match
	 */

	if (strlen(name) < strlen(pattern)) {
		return 0;
	}
	/* pattern has to start with a dot */
	if (pattern[0] != '.') {
		return 0;
	}
	start = name + strlen(name) - strlen(pattern);
	return !strcasecmp(start, pattern);
}

static
int config_search_ip(struct ullist_t* ul,
			unsigned long int ip, unsigned long int netmask) {
	while (ul) {
		if ((ul->value & netmask) == (ip & netmask)) {
			return 1;
		}
		ul = ul->next;
	}
	return 0;
}

int config_search_name(struct slist_t* sl, const char* name) {
	if ( ! name ) {
		return 0;
	}
	while (sl) {
		if(config_match_domain(name, sl->value)) {
			return 1;
		}
		sl = sl->next;
	}
	return 0;
}

static
int config_search_namepattern(struct slist_t* sl, const char* namepat) {
	if ( ! namepat ) {
		return 0;
	}
	while (sl) {
		if (config_match_domain(sl->value, namepat)) {
			return 1;
		}
		sl = sl->next;
	}
	return 0;
}

static
int match_addrs(unsigned long int ip, const char* name,
		unsigned long int ipwnet, unsigned long int netmask,
		const char* namepat, struct hostent_list** hl) {

	/* ip, name want to connect *
	 * ipwnet/netmask, namepat are allowed to connect */

	const struct hostent_list* host;
	struct in_addr iaddr;
	struct sockaddr_in sin;

	if (name) {
		if (namepat) {
			/* try without lookup */
			/* check for exact match */
			if (strcasecmp(name, namepat) == 0) {
				return 1;
			}
			/* check for domain equivalence */
			if (config_match_domain(name, namepat)) {
				return 1;
			}
			/* see if name is a device */
			if (get_interface_ip(name, &sin) == 0) {
				struct ullist_t* ul;
				int ret;
				ul = ullist_init(sin.sin_addr.s_addr);
				ret = config_search_ip(ul, ipwnet, netmask);
				ullist_destroy(ul);
				if (ret == 1) {
					return 1;
				}
			}
			/* look up name and try to match the aliases of the
			 * name's host against the allowed namepattern and
			 * ipwnet/netmask */
			host = config_forward_lookup(hl, name);
			if (!host) {
				log(5, "Could not look up (1) name %s", name);
			} else {
				/* see if one alias of the host matches the
				 * allowed pattern */
				if (config_search_namepattern(
						host->aliases_list, namepat)) {
					return 1;
				}
				/* see if one other IP of the host matches
				 * the IP/netmask values */
				if (config_search_ip(host->addr_list,
							ipwnet, netmask)) {
					return 1;
				}
			}
		}
	}
	if (ip != -1) {
#ifndef IF_NAMESIZE
#       define IF_NAMESIZE IFNAMSIZ
#endif
		char iface[IF_NAMESIZE + 1];
		/* try without lookup */
		if ((ip & netmask) == (ipwnet & netmask)) {
			return 1;
		}
		/* see if the IP belongs to a device */
		memset(&sin, 0, sizeof(struct sockaddr_in));
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = ip;
		if (get_interface_name(sin, iface) == 0) {
			if (namepat && strcasecmp(iface, namepat) == 0) {
				return 1;
			}
		}
		/* look up the IP and try to match */
		host = config_reverse_lookup(hl, ip);
		if (!host) {
			iaddr.s_addr = ip;
			log(5, "Could not look up IP %s", inet_ntoa(iaddr));
		} else {
			/* see if one IP of the host is allowed by the
			 * IP/netmask fields */
			if (config_search_ip(host->addr_list,
							ipwnet, netmask)) {
				return 1;
			}
			/* see if one alias hostname of the host is in the
			 * allowed pattern */
			if (config_search_namepattern(host->aliases_list,
							namepat)) {
				return 1;
			}
		}
		/* look up the name and try to match */
		if (namepat) {
			host = config_forward_lookup(hl, namepat);
			if (!host) {
				log(5, "Could not look up (2) name %s",
						namepat);
			} else {
				/* see if one IP of the host is allowed by the
				 * IP/netmask fields */
				if (config_search_ip(host->addr_list,
							ip, netmask)) {
					return 1;
				}
				/* see if one alias hostname of the host is
				 * in the allowed pattern */
				if (config_search_namepattern(
						host->aliases_list, name)) {
					return 1;
				}
			}
		}
	}

	/* didn't find anything */

	return 0;

}

/* ------------------ end matching functions ------------------- */



/* ------------------ begin keep_matching functions ---------------- */

static
struct section_t*
config_process_section_increase_conn_counter(struct section_t* section) {
	section->connection_counter++;
	return section;
}

static
struct section_t*
config_process_section_decrease_conn_counter(struct section_t* section) {
	if (section->connection_counter > 0) {
		section->connection_counter--;
	}
	return section;
}

static
struct section_t*
config_process_section_nothing(struct section_t* section) {
	return section;
}

static
struct section_t*
config_process_section_shrink(struct section_t* section) {
	struct section_t* pRet;

	pRet = section->next;
	section->next = (struct section_t*) 0;
	config_destroy_section(section);

	return pRet;
}

static
int config_matches_hosts(unsigned int ip, const char* name,
				struct hostlist_t* hlist,
				struct hostent_list** hostc_list) {
	while (hlist) {
		if (match_addrs(ip, name,
			hlist->host.ip.ip, hlist->host.ip.netmask,
			hlist->host.name,
			hostc_list)

		||

		match_addrs(hlist->host.ip.ip, hlist->host.name,
			ip, hlist->host.ip.netmask,
			name,
			hostc_list)) {

			return 1;
		}
		hlist = hlist->next;
	}
	return 0;
}

static
int config_matches_user(const char* name, struct slist_t* ulist) {
	while(ulist) {
		if (0 == strcmp(name, ulist->value)
				|| 0 == strcmp(ulist->value, "*")) {
			return 1;
		}
		ulist = ulist->next;
	}
	return 0;
}

static
int config_matches_port(int port, struct portrangestruct* plist) {
	while(plist) {
		if (port >= plist->startport && port <= plist->endport) {
			return 1;
		}
		plist = plist->next;
	}
	return 0;
}

static
int config_matches_time(struct timestruct* tlist, time_t specific_time) {
	time_t nowseconds;
	struct tm* now;
	int starttime, endtime, nowtime;
	int startmatch, endmatch;
	struct ilist_t* il;
	if (specific_time == 0) {
		nowseconds = time(NULL);
	} else {
		nowseconds = specific_time;
	}
	now = localtime(&nowseconds);

	while(tlist) {
		starttime = tlist->start_hour * 100;
		starttime += tlist->start_minute;

		endtime = tlist->end_hour * 100;
		endtime += tlist->end_minute;

		nowtime = now->tm_hour * 100;
		nowtime += now->tm_min;

		/* These combinations match in a range:
		 *
		 * Mon  Tue  Wed  Thu  Fri  Sat  Sun
		 *       s                   e              (1)
		 *       e         s                        (2)
		 *
		 * (1) means: start < day && day < end
		 * (2) means: start < day && end < start
		 */

		if ( ! tlist->days &&
			((tlist->start_day <= now->tm_wday
				&& now->tm_wday <= tlist->end_day)
			||
			 (tlist->start_day <= now->tm_wday
				&& tlist->end_day <= tlist->start_day))) {

			/* the day range matches */
			startmatch = 0;
			if (tlist->start_day == now->tm_wday) {
				/* check for start time */
				if (starttime <= nowtime) {
					startmatch = 1;
				}
			} else {
				/* we are on a day after the start */
				startmatch = 1;
			}
			endmatch = 0;
			if (tlist->end_day == now->tm_wday) {
				/* check for end time */
				if (nowtime <= endtime) {
					endmatch = 1;
				}
			} else {
				/* we are on a day before the end */
				endmatch = 1;
			}
			if (startmatch && endmatch) {
				return 1;
			}
		}

		/* check for a date list */
		il = tlist->days;
		while (il) {
			if (il->value == now->tm_wday) {
				/* the day list matches */
				if (tlist->days && starttime <= nowtime
						     && nowtime <= endtime) {
					return 1;
				}
			}
			il = il->next;
		}
		tlist = tlist->next;
	}
	return 0;
}

static
struct section_t* config_match_section(struct section_t* section,
				unsigned long int from_ip,

				unsigned long int to_ip,
				const char* to_name,
				unsigned int to_port,
				const char* to_user,

				unsigned long int forw_ip,
				const char* forw_name,
				unsigned int forw_port,
				const char* forw_user,

				time_t specific_time,

				unsigned long int proxy_ip,
				unsigned int proxy_port,

				int c_servertype,

				struct hostent_list** hostc_list,
				int config_state,
				int in_forwarded_tag,
				struct section_t* (*section_function_match)
							(struct section_t*),
				struct section_t* (*section_function_nomatch)
							(struct section_t*)
				) {

	if (in_forwarded_tag) {
		to_ip   = forw_ip;
		to_name = forw_name;
		to_port = forw_port;
		to_user = forw_user;
	}

	if (section->next) {
		section->next = config_match_section(section->next, from_ip,
				to_ip, to_name, to_port, to_user,
				forw_ip, forw_name, forw_port, forw_user,
				specific_time,
				proxy_ip, proxy_port,
				c_servertype,
				hostc_list,
				config_state,
				in_forwarded_tag,
				section_function_match,
				section_function_nomatch);
	}

	if (section->tag_name == TAG_FROM && (config_state & TAG_FROM)) {
		/* if NOT included OR excluded ... delete */
		if (
			! config_matches_hosts ( from_ip, (char*) 0,
				section->hosts, hostc_list)
		||
			config_matches_hosts ( from_ip, (char*) 0,
				section->hosts_exclude, hostc_list)) {

			return section_function_nomatch(section);
		}
	}

	if (section->tag_name == TAG_TO && (config_state & TAG_TO)) {
		/* if NOT included OR excluded ... delete */
		if (
			! config_matches_hosts ( to_ip, to_name,
				section->hosts, hostc_list)
		||
			config_matches_hosts ( to_ip, to_name,
				section->hosts_exclude, hostc_list)) {

			return section_function_nomatch(section);
		}
	}

	if (section->tag_name == TAG_PROXYIP && (config_state & TAG_PROXYIP)) {
		/* if NOT included OR excluded ... delete */
		if (
			! config_matches_hosts ( proxy_ip, (char*) 0,
				section->hosts, hostc_list)
		||
			config_matches_hosts ( proxy_ip, (char*) 0,
				section->hosts_exclude, hostc_list)) {

			return section_function_nomatch(section);
		}
	}

	if (section->tag_name == TAG_PROXYPORT
				&& (config_state & TAG_PROXYPORT)) {
		/* if NOT included OR excluded ... delete */
		if (
			! config_matches_port ( proxy_port,
					section->ports)
		||
			config_matches_port ( proxy_port,
					section->ports_exclude)) {

			return section_function_nomatch(section);
		}
	}

	if (to_user && section->tag_name == TAG_USER && (config_state & TAG_USER)) {
		/* if NOT included OR excluded ... delete */
		if (
			! config_matches_user ( to_user,
					section->users)
		||
			config_matches_user ( to_user,
					section->users_exclude)) {

			return section_function_nomatch(section);
		}
	}

	if (section->tag_name == TAG_TIME && (config_state & TAG_TIME)) {
		/* if NOT included OR excluded ... delete */
		if (
			! config_matches_time (section->time, specific_time)
		||
			config_matches_time (section->time_exclude,
							specific_time)) {

			return section_function_nomatch(section);
		}
	}

	if (section->tag_name == TAG_FORWARDED
			&& (config_state & TAG_FORWARDED)) {
		if ( ! forw_user ) {
			return section_function_nomatch(section);
		}
	}

	if (to_port != 0 && section->tag_name == TAG_PORT
			&& (config_state & TAG_PORT)) {
		/* if NOT included OR excluded ... delete */
		if (
			! config_matches_port ( to_port,
					section->ports)
		||
			config_matches_port ( to_port,
					section->ports_exclude)) {

			return section_function_nomatch(section);
		}
	}

	if (section->tag_name == TAG_SERVERTYPE
			&& (config_state & TAG_SERVERTYPE)) {
		if (c_servertype != section->servertype) {
			return section_function_nomatch(section);
		}
	}

	if (section->nested && (section->tag_name & config_state)) {
		if (section->tag_name == TAG_FORWARDED) {
			in_forwarded_tag = 1;
		}
		section->nested = config_match_section(section->nested,
				from_ip,
				to_ip, to_name, to_port, to_user,
				forw_ip, forw_name, forw_port, forw_user,
				specific_time,
				proxy_ip, proxy_port,
				c_servertype,
				hostc_list, config_state,
				in_forwarded_tag,
				section_function_match,
				section_function_nomatch);
	}

	return section_function_match(section);
}

/* ------------------ end keep matching functions ------------------- */


/* --------------- begin option list creating functions ---------------- */

/* recursively create an optionlist and append the nested and following
 * sections */
struct option_t* config_generate_option_list(struct section_t* section,
						int config_state) {
	struct option_t* opts;

	if (section->tag_name & config_state) {
		opts = optionlist_clone(section->options);
		if (section->nested) {
			opts = optionlist_append(opts,
				config_generate_option_list(section->nested,
					config_state));
		}
	} else {
		opts = (struct option_t*) 0;
	}

	if (section->next) {
		opts = optionlist_append(opts,
			config_generate_option_list(section->next,
				config_state));
	}

	return opts;
}

const struct option_t* config_get_option_list() {
	return option_list;
}

void config_option_list_delete(const char* key) {
	if ( option_list ) {
		optionlist_delete_key(option_list, key);
	}
}

void config_option_list_add(const char* key, const char* value) {
	if ( option_list ) {
		optionlist_push(option_list, key, value);
	}
}

/* --------------- end option list creating functions ---------------- */

/* ------------------- begin exported functions ------------------- */

int config_shrink_config(unsigned long int from_ip,

				unsigned long int to_ip,
				const char* to_name,
				unsigned int to_port,
				const char* to_user,

				unsigned long int forw_ip,
				const char* forw_name,
				unsigned int forw_port,
				const char* forw_user,

				time_t specific_time,

				unsigned long int proxy_ip,
				unsigned int proxy_port,

				int c_servertype,

				struct hostent_list** hostc_list,
				int config_state) {

		/* transparent proxy, TO and PORT have been read with
		 * getsockname */
		/* ???????
		 * config_state = TAG_GLOBAL | TAG_FROM | TAG_TO | TAG_PORT;
		 * log(9, "Checking TAG_GLOBAL | TAG_FROM | TAG_TO | TAG_PORT");		 */
	config_match_section(base_section, from_ip,
			to_ip, to_name, to_port, to_user,
			forw_ip, forw_name, forw_port, forw_user,
			specific_time,
			proxy_ip, proxy_port,
			c_servertype, hostc_list, config_state,
			0, /* not in a forwarded tag at first */
			config_process_section_nothing,   /* match */
			config_process_section_shrink);   /* no match */

	optionlist_destroy( option_list );
	option_list = (struct option_t*) 0;
	option_list = config_generate_option_list(base_section, config_state);

	/* dump configuration */
	if (debug) {
		printf("\n--------------------Shrinking------------------\n");
		config_debug_outputsections();

		printf("\n\nOptionlist:\n");
		config_debug_output_options( config_get_option_list(), "");
	}

	if (srvinfo.ready_to_serve >= SVR_LAUNCH_READY) {
		log_detect_log_change();
	}

	return 0;
}

int config_counter_increase(unsigned long int from_ip,
			    unsigned long int proxy_ip,
			    unsigned int proxy_port,
			    time_t specific_time) {

	config_match_section(base_section, from_ip,
			-1,                    /* to ip */
			(char*) 0,             /* to name */
			0,                     /* to port */
			(char*) 0,             /* to user */
			-1,                    /* forwarded ip   */
			(char*) 0,             /* forwarded name */
			0,                     /* forwarded port */
			(char*) 0,             /* forwarded user */
			specific_time,         /* specific_time */
			proxy_ip,              /* proxy ip */
			proxy_port,            /* proxy port */
			srvinfo.servertype,    /* global variable */
			&hostcache,
			TAG_CONNECTED,
			0,             /* not in a forwarded tag */
			config_process_section_increase_conn_counter, /*match*/
			config_process_section_nothing);         /* no match */
	return 0;
}


int config_counter_decrease(unsigned long int from_ip,
			    unsigned long int proxy_ip,
			    unsigned int proxy_port,
			    time_t specific_time) {

	config_match_section(base_section, from_ip,
			-1,                    /* to ip */
			(char*) 0,             /* to name */
			0,                     /* to port */
			(char*) 0,             /* to user */
			-1,                    /* forwarded ip   */
			(char*) 0,             /* forwarded name */
			0,                     /* forwarded port */
			(char*) 0,             /* forwarded user */
			specific_time,         /* specific_time */
			proxy_ip,              /* proxy ip */
			proxy_port,            /* proxy port */
			srvinfo.servertype,    /* global variable */
			&hostcache,
			TAG_CONNECTED,
			0,             /* not in a forwarded tag */
			config_process_section_decrease_conn_counter, /*match*/
			config_process_section_nothing);         /* no match */
	return 0;
}

int config_check_limit_violation() {
	return config_check_limits(base_section);
}

void config_counter_add_connected(struct connliststruct* conn_cli) {
	struct connliststruct* cls = conn_cli;
	while (cls) {
		config_counter_increase(cls->from_ip,
					cls->proxy_ip,
					cls->proxy_port,
					cls->start_time);
		cls = cls->next;
	}
}

const char* config_get_default_value(const char* key) {
	const char* val = (char*) 0;
	int i = 0;

	while (configuration_data[i].name) {
		if (strcasecmp(configuration_data[i].name, key) == 0) {
			val = configuration_data[i].defaultvalue;
			if (val) {
				log(8, "Did not find configuration "
					"entry for \"%s\", using "
					"\"%s\" as default", key, val);
				/* insert into option list */
				config_option_list_add(key, val);
			}
			break;
		}
		i++;
	}
	return val;
}

const char* config_get_option(const char* key) {
	const struct option_t* options = config_get_option_list();
	const char* val = (char*) 0;

	if ( ! key ) {
		return (char*) 0;
	}

	while (key && options) {
		if (options->key && strcasecmp(options->key, key) == 0) {
			val = options->value;
		}
		options = options->next;
	}
	if ( ! val ) {
		/* option not found, look for default value */
		val = config_get_default_value(key);
	}
	return val;
}


struct slist_t* config_get_option_array(const char* key) {
	const struct option_t* options = config_get_option_list();
	struct slist_t* slist = (struct slist_t*) 0;

	if ( ! key ) {
		return (struct slist_t*) 0;
	}

	while (key && options) {
		if (options->key && strcasecmp(options->key, key) == 0) {
			/* element matches */
			if (! slist) {
				slist = slist_cinit(options->value);
			} else {
				slist_cpush(slist, options->value);
			}
		}
		options = options->next;
	}

	return slist;
}


long conv_char2long(const char* s, long err_return) {
	long retval;

	if ( ! s ) {
		return err_return;
	}
	retval = strtol(s, NULL, 10);
	if (errno == ERANGE
			&& (retval == LONG_MIN || retval == LONG_MAX)) {
		return err_return;
	}
	return retval;
}

unsigned long conv_char2ulong(const char* s, unsigned long err_return) {
	unsigned long retval;

	if ( ! s ) {
		return err_return;
	}
	retval = strtoul(s, NULL, 10);
	if (errno == ERANGE
			&& (retval == LONG_MIN || retval == LONG_MAX)) {
		return err_return;
	}
	return retval;
}

long config_get_loption(const char* key, long err_return) {
	const char* optstr = config_get_option(key);
	return conv_char2long(optstr, err_return);
}

unsigned long config_get_uloption(const char* key, unsigned long err_return) {
	const char* optstr = config_get_option(key);
	return conv_char2ulong(optstr, err_return);
}

unsigned long config_get_addroption(const char* key, unsigned long err_return){
	const char* optstr = config_get_option(key);
	struct sockaddr_in sin;
	if (!optstr) {
		return err_return;
	}
	if (get_interface_ip(optstr, &sin) == 0) {
		/* there was an interface with this name */
		return sin.sin_addr.s_addr;
	}
	if (inet_aton(optstr, &sin.sin_addr)) {
		/* IP was recognized */
		return sin.sin_addr.s_addr;
	}
	log(6, "Invalid IP/interface: %s", optstr);
	return err_return;
}

int config_get_ioption(const char* key, int err_return) {
	return (int) config_get_loption(key, (long) err_return);
}

float config_get_foption(const char* key, float err_return) {
	const char* optstr = config_get_option(key);
	float retval;
	int success;

	if ( ! optstr ) {
		return err_return;
	}

	success = sscanf(optstr, "%f", &retval);
	if (success) {
		return retval;
	} else {
		return err_return;
	}
}

int config_get_bool(const char* key) {
	const char* optstr = config_get_option(key);

	if (strcasecmp(optstr, "on") == 0) {
		return 1;
	}
	if (strcasecmp(optstr, "off") == 0) {
		return 0;
	}
	if (strcasecmp(optstr, "yes") == 0) {
		return 1;
	}
	if (strcasecmp(optstr, "no") == 0) {
		return 0;
	}
	if (strcasecmp(optstr, "true") == 0) {
		return 1;
	}
	if (strcasecmp(optstr, "false") == 0) {
		return 0;
	}
	if (strcmp(optstr, "1") == 0) {
		return 1;
	}
	if (strcmp(optstr, "0") == 0) {
		return 0;
	}

	log(5, "There was no value for %s, using \"off\"", key);
	return 0;
}

unsigned long int config_get_size(const char* key,
					unsigned long int err_return) {
	const char* optstr = config_get_option(key);
	unsigned long int size;
	int i, nondigits;
	char multiplier;

	if (strcasecmp(optstr, "unlimited") == 0) {
		return ULONG_MAX;
	}

	for (i = 0, nondigits = 0; i < strlen(optstr); i ++) {
		if ( ! isdigit((int) optstr[i]) ) {
			nondigits = 1;
			break;
		}
	}

	if ( ! nondigits ) {
		/* only a number was given */
		return config_get_uloption(key, err_return);
	}

	i = sscanf(optstr, "%lu%c", &size, &multiplier);
	if (i != 2) {
		log(5, "%s not a valid size", optstr);
		return err_return;
	}
	multiplier = toupper((int) multiplier);
	switch (multiplier) {
		case 'B':
			return size;
		case 'K':
			return size * 1024;
		case 'M':
			return size * 1024 * 1024;
		case 'G':
			return size * 1024 * 1024 * 1024;
		default:
			log(5, "%s not a valid size", optstr);
	}
	return err_return;
}

int config_compare_option(const char* key, const char* compare) {
	const char* option = config_get_option(key);
	if (! option || ! compare) {
		return 0;
	}
	return !strcasecmp(option, compare);
}

void config_delete_config() {
	config_destroy_section(base_section);
	base_section = (struct section_t*) 0;
	optionlist_destroy( option_list );
	option_list = (struct option_t*) 0;
	hostent_destroy(hostcache);
	hostcache = (struct hostent_list*) 0;
}

void config_delete_master() {
	config_destroy_section(base_section);
	base_section = (struct section_t*) 0;
}

void config_create_backup() {
	if (backup_base_section) {
		config_destroy_section(backup_base_section);
	}
	backup_base_section = section_clone(base_section);
}

void config_delete_backup() {
	config_destroy_section(backup_base_section);
	backup_base_section = (struct section_t*) 0;
}




/* ------------------- end exported functions ------------------- */


/*****-------------------- begin DEBUG ---------------------******/

void config_debug_output_options ( const struct option_t* opt,
					char* prefix ) {

	while (opt) {
		printf("%sKey: %s --> %s\n",
				prefix, opt->key, opt->value);
		opt = opt->next;
	}

}

void config_debug_output_portrange ( struct portrangestruct* pr,
						char* prefix ) {
	char* toprint = "";
	if (pr) {
		toprint = "\n";
	}
	while (pr) {
		printf("%s%d:%d ", prefix, pr->startport, pr->endport);
		pr = pr->next;
	}
	printf("%s", toprint);
}

void config_debug_output_users ( struct slist_t* ul, char* prefix ) {
	while(ul) {
		printf("%s%s\n", prefix, ul->value);
		ul = ul->next;
	}
}

void config_debug_output_host ( struct host_t host, char* prefix ) {
	if (host.name) {
		printf("%sname: %s", prefix, host.name);
	} else {
		/* we have to save one, inet_ntoa uses an internal memory
		 * that is overwritten by successive calls */
		char* tmp = strdup(inet_ntoa(*((struct in_addr*)&host.ip.ip)));
		enough_mem(tmp);
		printf("%s%s/%s", prefix, tmp,
			inet_ntoa(*((struct in_addr*) &host.ip.netmask)));
		free(tmp);
	}
	printf("\n");
}

void config_debug_output_hostlist ( struct hostlist_t* hosts, char* prefix ) {
	while(hosts) {
		config_debug_output_host(hosts->host, prefix);
		hosts = hosts->next;
	}
}

char* day_name(int number) {
	switch(number) {
		case 0:
			return("Sun"); break;
		case 1:
			return("Mon"); break;
		case 2:
			return("Tue"); break;
		case 3:
			return("Wed"); break;
		case 4:
			return("Thu"); break;
		case 5:
			return("Fri"); break;
		case 6:
			return("Sat"); break;
		default:
			return("ERR"); break;
	}
}

void config_debug_output_time( struct timestruct* time, char* prefix) {
	while(time) {
		printf("%s", prefix);

		if (time->days) {
			struct ilist_t* il = time->days;
			printf("List for ");
			while(il) {
				if (il != time->days) {
					printf("/");
				}
				printf("%s", day_name(il->value));
				il = il->next;
			}
			printf(" from %02d.%02d to %02d.%02d\n",
				time->start_hour, time->start_minute,
				time->end_hour, time->end_minute);
		} else {
			printf("Range starting on %s at %02d.%02d, "
					"ending on %s at %02d.%02d\n",
					day_name(time->start_day),
					time->start_hour, time->start_minute,
					day_name(time->end_day),
					time->end_hour, time->end_minute);
		}

		time = time->next;
	}
}

void config_debug_output_section( struct section_t* section, char* prefix ) {
	if (!section) {
		printf("%s----empty section\n", prefix);
		return;
	}

	printf("%sid: %d\n", prefix, section->id);

	printf("%shosts:\n", prefix);
	config_debug_output_hostlist ( section->hosts, prefix );

	printf("%shosts exclude:\n", prefix);
	config_debug_output_hostlist ( section->hosts_exclude, prefix );

	printf("%susers:\n", prefix);
	config_debug_output_users ( section->users, prefix );

	printf("%susers exclude:\n", prefix);
	config_debug_output_users ( section->users_exclude, prefix );

	printf("%sports:\n", prefix);
	config_debug_output_portrange ( section->ports, prefix );

	printf("%sports exclude:\n", prefix);
	config_debug_output_portrange ( section->ports_exclude, prefix );

	printf("%stime:\n", prefix);
	config_debug_output_time ( section->time, prefix );

	printf("%stime exclude:\n", prefix);
	config_debug_output_time ( section->time_exclude, prefix );

	printf("%sservertype:\n", prefix);
	if (section->tag_name == TAG_SERVERTYPE) {
		if (section->servertype == SERVERTYPE_STANDALONE) {
			printf("%sStandalone\n", prefix);
		} else {
			printf("%sInetd\n", prefix);
		}
	}
	printf("%soptions:\n", prefix);
	config_debug_output_options ( section->options, prefix );

	if (section->nested) {
		char* prefix_new = malloc(strlen(prefix) + 4);

		printf("%s----- nested:\n", prefix);
		memset(prefix_new, (int) ' ', strlen(prefix) + 3);
		prefix_new[strlen(prefix) + 3] = '\0';
		config_debug_output_section( section->nested, prefix_new );
		free(prefix_new);
	}
	if (section->next) {
		printf("%s----- next:\n", prefix);
		config_debug_output_section( section->next, prefix);
	}
}

void config_debug_outputsections() {
	printf("\n\n");
	config_debug_output_section( base_section, "" );

/*	printf("\n\n-------Backup-------\n\n");
	config_debug_output_section( backup_base_section, "" );
*/
}

/*****---------------------- end DEBUG ---------------------******/
