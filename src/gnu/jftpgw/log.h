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

/*
 * See 'log.c' for a detailed description.
 *
 * Copyright (C) 1998  Steven Young
 * Copyright (C) 1999  Robert James Kaes (rjkaes@flarenet.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef _LOG_H
#define _LOG_H

extern void log(int, const char *fmt, ...);

/* used:
 *          a A b C d (e) f F h H i I l L m n p P r R s t T u v        */

struct log_cmd_st {

	const char* svrname;
	char* svrip;
	const char* svrlogin;

	const char* clntname;
	char* clntip;

	char* ifipclnt;
	char* ifipsvr;

	char* userlogin;
	char* usereffective;
	char* userforwarded;
	const char* anon_user;

	const char* cmd;
	char* method;
	const char* filename;
	const char* service;   /* "ftp" */
	char direction;  /* either 'o'utgoing or 'i'ncoming */
	char type;	 /* either 'i'mage or 'a'scii       */
	int respcode;
	int complete;
	unsigned int transfer_duration;
	unsigned long int transferred;
};


struct loginfo_st {
	int syslog;
	char* syslog_facility;
	int debuglevel;
	char* logf_name;
	FILE* logf;
	struct cmdlogent_t {
		/* list of opened logfiles */
		char* logf_name;
		int logf_size;  /* not used for files but for dirs */
		char* specs;
		FILE* logf;
		char* style;
		struct cmdlogent_t* next;
	} *cmdlogfiles, *cmdlogdirs;
};


void log_cmd(struct log_cmd_st*);
int log_init(void);
int log_detect_log_change(void);


#endif
