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

#ifndef __CONFIG_HEADER_H__
#define __CONFIG_HEADER_H__

#define TAG_NUMBER         9
#define TAG_UNKNOWN        (-1)

#define TAG_GLOBAL         1
#define TAG_GLOBAL_STR     "global"
#define TAG_FROM           2
#define TAG_FROM_STR       "from"
#define TAG_TO             4
#define TAG_TO_STR         "to"
#define TAG_USER           8
#define TAG_USER_STR       "user"
#define TAG_PORT           16
#define TAG_PORT_STR       "port"
#define TAG_FORWARDED      32
#define TAG_FORWARDED_STR  "forwarded"
#define TAG_TIME           64
#define TAG_TIME_STR       "time"
#define TAG_SERVERTYPE     128
#define TAG_SERVERTYPE_STR "servertype"
#define TAG_PROXYIP        256
#define TAG_PROXYIP_STR    "proxyip"
#define TAG_PROXYPORT      512
#define TAG_PROXYPORT_STR  "proxyport"

#define TAG_OPENING        0
#define TAG_CLOSING        1

#define TAG_ALL (TAG_FROM | TAG_TO | TAG_GLOBAL | TAG_USER | TAG_PORT | TAG_FORWARDED | TAG_TIME | TAG_SERVERTYPE | TAG_PROXYIP | TAG_PROXYPORT)
#define TAG_ALL_NOT_FORWARDED  (TAG_ALL & (~TAG_FORWARDED))
#define TAG_STARTUP            (TAG_GLOBAL | TAG_SERVERTYPE)
#define TAG_CONNECTED          (TAG_STARTUP | TAG_FROM | TAG_PROXYIP | TAG_PROXYPORT | TAG_TIME)

#define SERVERTYPE_STANDALONE		0
#define SERVERTYPE_INETD		1

struct ilist_t {
	int value;
	struct ilist_t *next;
};

struct ullist_t {
	unsigned long int value;
	struct ullist_t *next;
};

struct slist_t {
	char* value;
	struct slist_t *next;
};

struct option_t {
	char* key;
	char* value;
	struct option_t* next;
};

struct tag_options_t {
	char* list_str;
	char* list_exclude_str;
};

struct host_t {
	struct ip_t ip;
	char* name;
};

struct hostlist_t {
	struct host_t host;
	struct hostlist_t* next;
};

struct timestruct {
	struct ilist_t* days;
	int start_day;
	int start_hour;
	int start_minute;
	int end_day;
	int end_hour;
	int end_minute;
	struct timestruct* next;
};

struct section_t {
	int                                tag_name;
	unsigned int                       id;
	struct hostlist_t*                 hosts;
	struct hostlist_t*                 hosts_exclude;
	struct slist_t*                    users;
	struct slist_t*                    users_exclude;
	struct slist_t*                    forwarded;
	struct slist_t*                    forwarded_exclude;
	struct portrangestruct*            ports;
	struct portrangestruct*            ports_exclude;
	struct timestruct*                 time;
	struct timestruct*                 time_exclude;
	int                                servertype;
	struct option_t*                   options;
	struct section_t*                  nested;
	struct section_t*                  next;
	long int connection_counter;
	long int limit;
};

struct hostent_list {
	char* name;
	unsigned long int ip;
	struct ullist_t* addr_list;
	struct slist_t* aliases_list;
	struct hostent_list* next;
	time_t lookup_time;
};


/* exported functions from config.c */

int config_shrink_config(unsigned long int, /* from ip */
			unsigned long int,  /* dest ip */
			const char*,        /* dest hostname */
			unsigned int,       /* dest port */
			const char*,        /* dest user */
			unsigned long int,  /* forwarded ip */
			const char*,        /* forwarded hostname */
			unsigned int,       /* forwarded destinationport */
			const char*,        /* forwarded user */
			time_t,             /* specific time */
			unsigned long int,  /* proxy ip */
			unsigned int,       /* proxy port */
			int,                /* servertype */
			struct hostent_list**, int);
int config_counter_decrease(unsigned long int from_ip,
			    unsigned long int proxy_ip,
			    unsigned int proxy_port,
			    time_t specific_time);
int config_counter_increase(unsigned long int from_ip,
			    unsigned long int proxy_ip,
			    unsigned int proxy_port,
			    time_t specific_time);
int config_check_limit_violation(void);
void config_counter_add_connected(struct connliststruct*);
const char* config_get_option(const char* key);
struct slist_t* config_get_option_array(const char* key);
struct slist_t* config_split_line(const char* line, const char* pattern);
struct slist_t* slist_reverse(struct slist_t* sl);
int slist_case_contains(const struct slist_t*, const char*);
void slist_destroy(struct slist_t* sl);
char* slist_pop(struct slist_t* sl);
int slist_count(const struct slist_t* haystack);
void config_destroy_portrange(struct portrangestruct* plist);
int config_get_ioption(const char* key, int err_return);
long config_get_loption(const char* key, long err_return);
unsigned long int config_get_addroption(const char* key, unsigned long int);
int config_get_bool(const char* key);
float config_get_foption(const char* key, float err_return);
unsigned long int config_get_size(const char* key, unsigned long int err);
void config_option_list_delete(const char* key);
void config_option_list_add(const char* key, const char* value);
int config_compare_option(const char* key, const char* compare);
void config_delete_config();
void config_delete_backup();
void config_delete_master();
void config_create_backup();
int config_activate_backup();
void config_destroy_sectionconfig();

const char* hostent_get_name(struct hostent_list** h, unsigned long int ip);
unsigned long int hostent_get_ip(struct hostent_list** h, const char* name);

long conv_char2long(const char*, long);

struct portrangestruct* config_parse_portranges(const char* line);
unsigned int config_count_portrange(const struct portrangestruct* prs);
struct portrangestruct* config_port2portrange(unsigned int port);

#endif
