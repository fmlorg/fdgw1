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

/*  ----- from tinyproxy for jftpgw
 *
 * Logs the various messages which tinyproxy produces to either a log file or
 * the syslog daemon. Not much to it...
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
 *
 * log.c - For the manipulation of log files.
 */

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>

#include "jftpgw.h"
#include "log.h"

#define LENGTH 64
#define LOGSIZE 800

#ifndef HAVE_SNPRINTF
#   include "snprintf.c"
#else
#   ifndef HAVE_VSNPRINTF
#     include "snprintf.c"
#   endif
#endif

struct loginfo_st loginfo;
struct serverinfo srvinfo;
static struct loginfo_st* loginfo_bk;

/*
 * This routine logs messages to either the log file or the syslog function.
 */
void log(int level, const char *fmt, ...)
{
	va_list args;
	time_t nowtime;
	FILE *cf;

	static char time_string[LENGTH];
	static char str[LOGSIZE];

	if (level > loginfo.debuglevel) {
		return;
	}

	/* we may dump the log to stderr if we have not yet
	 * opened a log file or the syslog */
	/* but don't log if we are run by inetd */
	if (!loginfo.logf
		&& !loginfo.syslog
		&& srvinfo.servertype == SERVERTYPE_INETD
		&& level > 5) {

		return;
	}

	va_start(args, fmt);
	if (!loginfo.syslog) {
		/* log via files */
		nowtime = time(NULL);
		/* Format is month day hour:minute:second (24 time) */
		strftime(time_string, LENGTH, "%b %d %H:%M:%S", localtime(&nowtime));
		if (!(cf = loginfo.logf)) {
			cf = stderr;
		}

		fprintf(cf, "%s [%ld]: ", time_string, (long int) getpid());
		vfprintf(cf, fmt, args);
		fprintf(cf, "\n");
		fflush(cf);
	} else {
		int logtype = LOG_DEBUG;
		if (level < 8) {
			logtype = LOG_INFO;
		}
		if (level < 6) {
			logtype = LOG_WARNING;
		}
		if (level < 4) {
			logtype = LOG_ERR;
		}
		vsnprintf(str, LOGSIZE - 1, fmt, args);
		syslog(logtype, str);
	}

	va_end(args);
}




#define NUMBER_BUFFER   30
char* conv_int2char(signed int i) {
	char* s = (char*) malloc( NUMBER_BUFFER );
	enough_mem(s);
	snprintf(s, NUMBER_BUFFER, "%d", i);
	return s;
}

char* conv_uint2char(unsigned int i) {
	return conv_int2char(i);
}

char* conv_lint2char(signed long int i) {
	char* s = (char*) malloc( NUMBER_BUFFER );
	enough_mem(s);
	snprintf(s, NUMBER_BUFFER, "%ld", i);
	return s;
}

char* conv_luint2char(unsigned long int i) {
	return conv_lint2char(i);
}

char* conv_float2char(float f) {
	char* s = (char*) malloc( NUMBER_BUFFER );
	enough_mem(s);
	snprintf(s, NUMBER_BUFFER, "%.2f", f);
	return s;
}

const char* base_name(const char* s) {
	const char* r, *t;

	if (!s) {
		return "(null)";
	}
	t = r = s;
	while ((t = strchr(t, '/'))) {
		t++;
		r = t;
	}
	return r;
}


#define LOG_REPLACE_STRING       1
#define LOG_REPLACE_CHAR         2
#define LOG_REPLACE_UINT         3
#define LOG_REPLACE_LUINT        4
#define LOG_REPLACE_INT          5
#define LOG_REPLACE_LINT         6
#define LOG_REPLACE_FLOAT        7

char* log_replace_char(const char pattern, struct log_cmd_st* lcs) {
	union {
		const char* replace_str;
		char replace_char;
		unsigned int replace_uint;
		unsigned long int replace_luint;
		signed int replace_int;
		signed long int replace_lint;
		float replace_float;
	} replace_val;
	int replace_type;

	switch (pattern) {
		case 'c': /* complete */
			replace_val.replace_char = lcs->complete ? 'c' : 'i';
			replace_type = LOG_REPLACE_CHAR;
			break;
		case 'T': /* Time taken to transmit/receive file, in seconds */
			replace_val.replace_uint = lcs->transfer_duration;
			replace_type = LOG_REPLACE_UINT;
			break;
		case 'b': /* Bytes sent for request */
			replace_val.replace_luint = lcs->transferred;
			replace_type = LOG_REPLACE_LUINT;
			break;
		case 'R': /* throughput rate in kbyte/s */
			if (lcs->transfer_duration) {
				replace_val.replace_float =
				      (lcs->transferred / 1024) /
						lcs->transfer_duration;
				replace_type = LOG_REPLACE_FLOAT;
			} else {
				replace_val.replace_char = '-';
				replace_type = LOG_REPLACE_CHAR;
			}
			break;
		case 'f': /* Filename stored or retrieved, absolute path */
			replace_val.replace_str = lcs->filename;
			replace_type = LOG_REPLACE_STRING;
			break;
		case 'F':
			/* Filename stored or retrieved, as the client sees
			 * it base_name is just a pointer within lcs->filename
			 * */
			replace_val.replace_str = base_name(lcs->filename);
			replace_type = LOG_REPLACE_STRING;
			break;
		case 'm': /* Command (method) name received from client,
			     e.g., RETR */
			replace_val.replace_str = lcs->method;
			replace_type = LOG_REPLACE_STRING;
			break;
		case 'r': /* full commandline */
			replace_val.replace_str = lcs->cmd;
			replace_type = LOG_REPLACE_STRING;
			break;
		case 'P': /* pid */
			replace_val.replace_uint = getpid();
			replace_type = LOG_REPLACE_UINT;
			break;
		case 's': /*  Numeric FTP response code (status) */
			replace_val.replace_int = lcs->respcode;
			replace_type = LOG_REPLACE_INT;
			break;
		case 'y': /* tYpe */
			replace_val.replace_char = lcs->type;
			replace_type = LOG_REPLACE_CHAR;
			break;
		case 'w':  /* direction */
			replace_val.replace_char = lcs->direction;
			replace_type = LOG_REPLACE_CHAR;
			break;
		case 'o':  /* anonymous? */
			/* logged in ? */
			if (!lcs || !lcs->userlogin) {
				replace_val.replace_char = '-';
				replace_type = LOG_REPLACE_CHAR;
				break;
			}
			replace_val.replace_char =
				   strcmp(lcs->userlogin, "anonymous") == 0
				|| strcmp(lcs->userlogin, "ftp") == 0 ? 'a':'r';
			replace_type = LOG_REPLACE_CHAR;
			break;
		case 'e': /* sErvice */
			replace_val.replace_str = lcs->service;
			replace_type = LOG_REPLACE_STRING;
			break;
		case 'n': /* aNon-user */
			replace_val.replace_str = lcs->anon_user;
			replace_type = LOG_REPLACE_STRING;
			break;
		case 'H': /* Server host name */
			replace_val.replace_str = lcs->svrname;
			replace_type = LOG_REPLACE_STRING;
			break;
		case 'A': /* Server host IP */
			replace_val.replace_str = lcs->svrip;
			replace_type = LOG_REPLACE_STRING;
			break;
		case 'd': /*  Server host name as specified in the login  */
			replace_val.replace_str = lcs->svrlogin;
			replace_type = LOG_REPLACE_STRING;
			break;
		case 'h': /* Client host name */
			replace_val.replace_str = lcs->clntname;
			replace_type = LOG_REPLACE_STRING;
			break;
		case 'a': /* Client host IP */
			replace_val.replace_str = lcs->clntip;
			replace_type = LOG_REPLACE_STRING;
			break;
		case 'I': /* Server host name */
			replace_val.replace_str = lcs->ifipsvr;
			replace_type = LOG_REPLACE_STRING;
			break;
		case 'i': /* Client host IP */
			replace_val.replace_str = lcs->ifipclnt;
			replace_type = LOG_REPLACE_STRING;
			break;
		case 'l': /* Server user name (login) */
			replace_val.replace_str = lcs->userlogin;
			replace_type = LOG_REPLACE_STRING;
			break;
		case 'L': /* Effective server user name */
			replace_val.replace_str = lcs->usereffective;
			replace_type = LOG_REPLACE_STRING;
			break;
		case 'C': /* Forwarded server user name */
			replace_val.replace_str = lcs->userforwarded;
			replace_type = LOG_REPLACE_STRING;
			break;
		case '%': /* percent sign */
			replace_val.replace_char = '%';
			replace_type = LOG_REPLACE_CHAR;
			break;
		default:
			return (char*) 0;
	}

	switch(replace_type) {
		char* s;

		case LOG_REPLACE_STRING:
			if (replace_val.replace_str) {
				s = strdup(replace_val.replace_str);
				enough_mem(s);
			} else {
				s = strdup("");
				enough_mem(s);
			}
			return s;
		case LOG_REPLACE_CHAR:
			s = (char*) malloc(2);
			enough_mem(s);
			s[0] = replace_val.replace_char;
			s[1] = '\0';
			return s;
		case LOG_REPLACE_INT:
			return conv_int2char(replace_val.replace_int);
		case LOG_REPLACE_UINT:
			return conv_uint2char(replace_val.replace_uint);
		case LOG_REPLACE_LINT:
			return conv_lint2char(replace_val.replace_lint);
		case LOG_REPLACE_LUINT:
			return conv_luint2char(replace_val.replace_luint);
		case LOG_REPLACE_FLOAT:
			return conv_float2char(replace_val.replace_float);
	}
	return (char*) 0;
}

char* log_replace_line(const char* line, struct log_cmd_st* lcs) {

	char* replaced_line = (char*) 0;
	char* fragment;
	char* insert;
	int offset = 0;

	/* split up line */

	/* line:  blabla %u blubb %h bla */

	if (line[0] == '%') {
		replaced_line = log_replace_char(line[1], lcs);
		offset = 2;
	}
	while ((fragment = quotstrtok(line, "%", &offset))) {
		if ( ! replaced_line ) {
			replaced_line = fragment;
		} else {
			replaced_line = realloc(replaced_line,
				strlen(replaced_line) + strlen(fragment) + 1);
			enough_mem(replaced_line);
			strcat(replaced_line, fragment);
		}
		/* the key is at line[offset+1]; */
		offset++;
		insert = log_replace_char(line[offset], lcs);
		if ( ! insert ) {
			insert = strdup("<%x not found>");
			enough_mem(insert);
			/* replace `x' above */
			insert[2] = line[offset];
		}
		offset++;
		replaced_line = realloc(replaced_line,
				strlen(replaced_line) + strlen(insert) + 1);
		enough_mem(replaced_line);
		strcat(replaced_line, insert);
		free(insert);
	}
	return replaced_line;
}



/* Idea by Bernd Eckenfels <ecki@lina.inka.de>
 * check of the command 'needle' is in the list of specified 
 * commands 'haystack'. Haystack may contain '*' which is a match-all
 * criteria */
int incommandpattern(const char *haystack, const char *needle) {
	char* negation = char_prepend(" ", needle);
	/* we now have "  NEEDLE " */
	negation[1] = '-';
	/* we now have " -NEEDLE " */

	if (strstr(haystack, negation)) {
		free(negation);
		return 0;
	}
	free(negation);

	if (strstr(haystack, " * ")) {
		return 1;
	}
	if (strstr(haystack, needle)) {
		return 1;
	}
	return 0;
}


void log_cmd_ent(struct cmdlogent_t* lent, struct log_cmd_st* lcs) {

	char* commandpattern;
	size_t commandpatternsize;
	static char time_string[LENGTH];
	time_t nowtime;
	char* ws;

	commandpatternsize = strlen(lcs->cmd) + 3;
	commandpattern = (char*) malloc(commandpatternsize);
	enough_mem(commandpattern);

	if ((ws = strpbrk(lcs->cmd, " \t")) == NULL) {
		/* Command consisting of a single word */
		snprintf(commandpattern, commandpatternsize, " %s ", lcs->cmd);
	} else {
		commandpattern[0] = ' ';
		strncpy(&commandpattern[1], lcs->cmd, ws - lcs->cmd);
		commandpattern[ws-lcs->cmd+1] = ' ';
		commandpattern[ws-lcs->cmd+2] = '\0';
	}
	toupstr(commandpattern);

	if (incommandpattern(lent->specs, commandpattern)) {
		/* found, log it */
		nowtime = time(NULL);

		if (strcmp(lent->style, "commonlog") == 0) {
			/* [18/Feb/2001:11:42:46 CET] */
			strftime(time_string, LENGTH,
				"[%d/%b/%Y:%H:%M:%S %Z]",
				localtime(&nowtime));
			if (checkbegin(lcs->cmd, "PASS")) {
				fprintf(lent->logf, "\"PASS *\"");
			} else {
				fprintf(lent->logf, "\"%s\"", lcs->cmd);
			}
			fprintf(lent->logf, " %d %ld\n",
					lcs->respcode, lcs->transferred);
		} else if (strcmp(lent->style, "xferlog") == 0) {
			char* replaced;
			const char* line_pattern =
				"%d %h %t %f %y %w _ %n %l %s 0 * %c";

			/* Wed Feb 14 01:41:28 2001 */
			strftime(time_string, LENGTH,
				"%a %b %d %H:%M:%S %Y",
				localtime(&nowtime));
			replaced = log_replace_line(line_pattern, lcs);
			fprintf(lent->logf, "%s %s\n", time_string, replaced);
			lcs->filename = (char*) 0;
			free(replaced);
		} else {
			char* line_pattern = lent->style;
			char* replaced;

			/* Wed Feb 14 01:41:28 2001 */
			strftime(time_string, LENGTH,
				"%a %b %d %H:%M:%S %Y",
				localtime(&nowtime));
			replaced = log_replace_line(line_pattern, lcs);
			fprintf(lent->logf, "%s %s\n", time_string, replaced);
			lcs->filename = (char*) 0;
			free(replaced);
		}
		fflush(lent->logf);
	}
	free(commandpattern);
}



void log_cmd(struct log_cmd_st* lcs) {

	/* go through the specifications of the logfiles and write
	 * them if they match */

	struct cmdlogent_t* files = loginfo.cmdlogfiles;
	struct cmdlogent_t* dirs  = loginfo.cmdlogdirs;

	while (files && files->logf_name) {
		log_cmd_ent(files, lcs);
		files = files->next;
	}

	while (dirs && dirs->logf_name) {
		log_cmd_ent(dirs, lcs);
		dirs = dirs->next;
	}
}

static
int log_name_exists(const struct cmdlogent_t* cl, const char* needle) {
	while (cl) {
		if (strcmp(cl->logf_name, needle) == 0) {
			return 1;
		}
		cl = cl->next;
	}
	return 0;
}

static
int log_init_debuglevel() {
	int debuglevel;
	const char* dbl;

	if (!(dbl = config_get_option("debuglevel"))) {
		log(4, "No debug level specified. Using level 7");
		debuglevel = 7;
	} else {
		errno = 0;
		/* with conv_char2* we wouldn't be able to log a warning */
		debuglevel = strtol(dbl, (char**) 0, 10);
		if (errno || debuglevel <= 0 || debuglevel > 9) {
			log(4, "Invalid debuglevel specified: \"%s\". Using "
				"level 7", dbl);
			debuglevel = 7;
		}
	}
	return debuglevel;
}

static
int log_init_syslog(struct loginfo_st* cfg) {
	const char* log_facility_opt = config_get_option("syslogfacility");
	int log_facility;

	cfg->syslog_facility = strdup(log_facility_opt);
	enough_mem(cfg->syslog_facility);

	if (0) {
#ifdef HAVE_LOG_FACILITY_LOG_AUTH
	} else if (strcasecmp(log_facility_opt, "auth") == 0) {
		log_facility = LOG_AUTH;
#endif
#ifdef HAVE_LOG_FACILITY_LOG_AUTHPRIV
	} else if (strcasecmp(log_facility_opt, "authpriv") == 0) {
		log_facility = LOG_AUTHPRIV;
#endif
#ifdef HAVE_LOG_FACILITY_LOG_CRON
	} else if (strcasecmp(log_facility_opt, "cron") == 0) {
		log_facility = LOG_CRON;
#endif
#ifdef HAVE_LOG_FACILITY_LOG_DAEMON
	} else if (strcasecmp(log_facility_opt, "daemon") == 0) {
		log_facility = LOG_DAEMON;
#endif
#ifdef HAVE_LOG_FACILITY_LOG_FTP
	} else if (strcasecmp(log_facility_opt, "ftp") == 0) {
		log_facility = LOG_FTP;
#endif
#ifdef HAVE_LOG_FACILITY_LOG_KERN
	} else if (strcasecmp(log_facility_opt, "kern") == 0) {
		log_facility = LOG_KERN;
#endif
#ifdef HAVE_LOG_FACILITY_LOG_LOCAL0
	} else if (strcasecmp(log_facility_opt, "local0") == 0) {
		log_facility = LOG_LOCAL0;
#endif
#ifdef HAVE_LOG_FACILITY_LOG_LOCAL1
	} else if (strcasecmp(log_facility_opt, "local1") == 0) {
		log_facility = LOG_LOCAL1;
#endif
#ifdef HAVE_LOG_FACILITY_LOG_LOCAL2
	} else if (strcasecmp(log_facility_opt, "local2") == 0) {
		log_facility = LOG_LOCAL2;
#endif
#ifdef HAVE_LOG_FACILITY_LOG_LOCAL3
	} else if (strcasecmp(log_facility_opt, "local3") == 0) {
		log_facility = LOG_LOCAL3;
#endif
#ifdef HAVE_LOG_FACILITY_LOG_LOCAL4
	} else if (strcasecmp(log_facility_opt, "local4") == 0) {
		log_facility = LOG_LOCAL4;
#endif
#ifdef HAVE_LOG_FACILITY_LOG_LOCAL5
	} else if (strcasecmp(log_facility_opt, "local5") == 0) {
		log_facility = LOG_LOCAL5;
#endif
#ifdef HAVE_LOG_FACILITY_LOG_LOCAL6
	} else if (strcasecmp(log_facility_opt, "local6") == 0) {
		log_facility = LOG_LOCAL6;
#endif
#ifdef HAVE_LOG_FACILITY_LOG_LOCAL7
	} else if (strcasecmp(log_facility_opt, "local7") == 0) {
		log_facility = LOG_LOCAL7;
#endif
#ifdef HAVE_LOG_FACILITY_LOG_LPR
	} else if (strcasecmp(log_facility_opt, "lpr") == 0) {
		log_facility = LOG_LPR;
#endif
#ifdef HAVE_LOG_FACILITY_LOG_MAIL
	} else if (strcasecmp(log_facility_opt, "mail") == 0) {
		log_facility = LOG_MAIL;
#endif
#ifdef HAVE_LOG_FACILITY_LOG_NEWS
	} else if (strcasecmp(log_facility_opt, "news") == 0) {
		log_facility = LOG_NEWS;
#endif
#ifdef HAVE_LOG_FACILITY_LOG_SYSLOG
	} else if (strcasecmp(log_facility_opt, "syslog") == 0) {
		log_facility = LOG_SYSLOG;
#endif
#ifdef HAVE_LOG_FACILITY_LOG_USER
	} else if (strcasecmp(log_facility_opt, "user") == 0) {
		log_facility = LOG_USER;
#endif
#ifdef HAVE_LOG_FACILITY_LOG_UUCP
	} else if (strcasecmp(log_facility_opt, "uucp") == 0) {
		log_facility = LOG_UUCP;
#endif
#ifdef HAVE_LOG_FACILITY_LOG_CONSOLE
	} else if (strcasecmp(log_facility_opt, "console") == 0) {
		log_facility = LOG_CONSOLE;
#endif
#ifdef HAVE_LOG_FACILITY_LOG_SECURITY
	} else if (strcasecmp(log_facility_opt, "security") == 0) {
		log_facility = LOG_SECURITY;
#endif
	} else {
		/* facility not found */
		fprintf(stderr,
			"Facility %s not recognized, using default facility ",
			log_facility_opt);

		/* get default facility */
		if (srvinfo.multithread) {
			log_facility = LOG_DAEMON;
			fprintf(stderr, "LOG_DAEMON\n");
		} else {
			log_facility = LOG_USER;
			fprintf(stderr, "LOG_USER\n");
		}
	}
	openlog(srvinfo.binaryname, LOG_PID, log_facility);
	syslog(LOG_INFO, "jftpgw v"JFTPGW_VERSION" opened syslog");
	cfg->syslog = 1;
	return 0;
}

static
int log_init_logfile(struct loginfo_st* cfg) {
	const char* option;

	if (cfg->logf) {
		/* already logging */
		return 0;
	}
	cfg->syslog = 0;
	/* read the logfile name from the configuration file */
	option = config_get_option("logfile");
	if (!option) {
		/* no logfile set, use the default */
		option = DEFAULTLOGFILE;
	}

	/* now option is set in every case */
	cfg->logf_name = chrooted_path(option);

	if (!(cfg->logf = open_logfile(cfg->logf_name))) {
		return -1;
	}
	log(7, "jftpgw v"JFTPGW_VERSION" opened the logfile");
	return 0;
}


static
int log_init_cmdlog(struct loginfo_st* cfg, int open) {
	char* option, *specs, *style;
	struct cmdlogent_t* files = cfg->cmdlogfiles;
	struct cmdlogent_t* new;
	char* logfile;
	int i;

	struct slist_t* opt_names =config_get_option_array("cmdlogfile");
	struct slist_t* opt_styles=config_get_option_array("cmdlogfile-style");
	struct slist_t* opt_specs =config_get_option_array("cmdlogfile-specs");

	int count_names = slist_count(opt_names);
	int count_styles = slist_count(opt_styles);
	int count_specs = slist_count(opt_specs);

	if (!(count_names == count_styles && count_styles == count_specs)) {
		log(4, "Counted %d times \"cmdlogfile\" as option",
				count_names);
		log(4, "Counted %d times \"cmdlogfile-style\" as option",
				count_styles);
		log(4, "Counted %d times \"cmdlogfile-specs\" as option",
				count_specs);
		log(4, "This is not balanced, please fix your configuration");
		return -1;
	}

	/* reverse them */
	opt_names  = slist_reverse(opt_names);
	opt_styles = slist_reverse(opt_styles);
	opt_specs  = slist_reverse(opt_specs);

	for (i = 0; i < count_names; i++) {
		option = slist_pop(opt_names);
		specs  = slist_pop(opt_specs);
		style  = slist_pop(opt_styles);
		logfile = chrooted_path(option);
		if (log_name_exists(cfg->cmdlogfiles, logfile)
		    || log_name_exists(cfg->cmdlogfiles, option)) {

			free(logfile);
			free(option);
			free(specs);
			free(style);
			continue;
		}
		free(option);

		new = malloc(sizeof (struct cmdlogent_t));
		enough_mem(new);
		if (files) {
			/* there are existing entries */
			files->next = new;
		} else {
			/* this is the first entry */
			cfg->cmdlogfiles = new;
		}
		/* files always keeps the current entry and iterates on */
		files = new;
		files->logf_name = logfile;
		files->specs = char_enclose(" ", specs, " "); free(specs);
		files->style = style;
		files->next = (struct cmdlogent_t*) 0;
		if (open && (files->logf = open_logfile(files->logf_name))
								== NULL) {
			/* the malloc()ed memory will be freed by
			 * reset_loginfo() */
			if (i < count_names - 1) {
				/* don't free if we're already at the end */
				slist_destroy(opt_names);
				slist_destroy(opt_styles);
				slist_destroy(opt_specs);
			}
			return -1;
		}
	}
	/* The values are already free()ed, just the structures are still
	 * malloc()ed */
	free(opt_names);
	free(opt_styles);
	free(opt_specs);
	return 0;
}


#define FILENAME_EXTEND		64
static
FILE* log_init_dirlog_open(const char* fname_pattern) {
	time_t nowtime;
	char* filename, *fname;
	size_t size_max;
	FILE* file;

	size_max = strlen(fname_pattern) + FILENAME_EXTEND;
	fname = (char*) malloc(size_max);
	filename = (char*) malloc(size_max);
	enough_mem(fname);
	enough_mem(filename);

	nowtime = time(NULL);
	strftime(fname, size_max, fname_pattern, localtime(&nowtime));
	snprintf(filename, size_max, fname, getpid());
	free(fname); fname = (char*) 0;

	file = open_logfile(filename);
	free(filename);
	return file;
}

static
int log_init_dirlog(struct loginfo_st* cfg, int open) {
	char* option, *specs, *style;
	char* prefix, *suffix;
	char* logdir, *logfile, *logf_name, *logf_name_chroot;
	struct cmdlogent_t* dirs = cfg->cmdlogdirs;
	struct cmdlogent_t* new;
	int i;

	struct slist_t* opt_names
		= config_get_option_array("connectionlogdir");
	struct slist_t* opt_styles
		= config_get_option_array("connectionlogdir-style");
	struct slist_t* opt_specs
		= config_get_option_array("connectionlogdir-specs");
	struct slist_t* opt_prefix
		= config_get_option_array("connectionlogdir-fileprefix");
	struct slist_t* opt_suffix
		= config_get_option_array("connectionlogdir-filesuffix");

	int count_names = slist_count(opt_names);
	int count_styles = slist_count(opt_styles);
	int count_specs = slist_count(opt_specs);
	int count_prefix = slist_count(opt_prefix);
	int count_suffix = slist_count(opt_suffix);

	if (! (count_names == count_styles
			&& count_styles == count_specs
			&& count_specs == count_prefix
			&& count_prefix == count_suffix)) {

		log(4, "Counted %d times \"connectionlogdir\" as option",
				count_names);
		log(4, "Counted %d times \"connectionlogdir-style\" as option",
				count_styles);
		log(4, "Counted %d times \"connectionlogdir-specs\" as option",
				count_specs);
		log(4, "Counted %d times \"connectionlogdir-fileprefix\""
				" as option", count_prefix);
		log(4, "Counted %d times \"connectionlogdir-filesuffix\""
				" as option", count_suffix);
		log(4, "This is not balanced, please fix your configuration");
		return -1;
	}

	/* reverse them */
	opt_names  = slist_reverse(opt_names);
	opt_styles = slist_reverse(opt_styles);
	opt_specs  = slist_reverse(opt_specs);
	opt_prefix = slist_reverse(opt_prefix);
	opt_suffix = slist_reverse(opt_suffix);

	for (i = 0; i < count_names; i++) {
		option = slist_pop(opt_names);
		specs  = slist_pop(opt_specs);
		style  = slist_pop(opt_styles);
		prefix = slist_pop(opt_prefix);
		suffix = slist_pop(opt_suffix);

		logdir = chrooted_path(option);
		logfile = char_enclose(
				prefix, "%Y-%m-%d--%H:%M:%S-%%d", suffix);
		free(prefix); free(suffix); free(option);

		logf_name = char_enclose(logdir, "/", logfile);
		logf_name_chroot = chrooted_path(logf_name);
		free(logdir);    logdir    = (char*) 0;
		free(logfile);   logfile   = (char*) 0;

		if (log_name_exists(cfg->cmdlogdirs, logf_name)
		      || log_name_exists(cfg->cmdlogdirs, logf_name_chroot)) {

			free(logf_name);        logf_name        = (char*) 0;
			free(logf_name_chroot); logf_name_chroot = (char*) 0;
			free(style);
			free(specs);
			continue;
		}
		free(logf_name); logf_name = (char*) 0;

		new = malloc(sizeof (struct cmdlogent_t));
		enough_mem(new);
		if (dirs) {
			/* there are existing entries */
			dirs->next = new;
		} else {
			/* this is the first entry */
			cfg->cmdlogdirs = new;
		}
		/* dirs always keeps the current entry and iterates on */
		dirs = new;
		dirs->next = (struct cmdlogent_t*) 0;
		dirs->specs = char_enclose(" ", specs, " "); free(specs);
		dirs->logf_name = logf_name_chroot;
		dirs->style = style;

		if (open && (dirs->logf =
			log_init_dirlog_open(dirs->logf_name)) == NULL) {
			if (i < count_names - 1) {
				/* don't free if we're already at the end */
				slist_destroy(opt_names);
				slist_destroy(opt_styles);
				slist_destroy(opt_specs);
				slist_destroy(opt_prefix);
				slist_destroy(opt_suffix);
			}
			return -1;
		}
	}
	return 0;
}

int log_init() {
	loginfo.debuglevel = log_init_debuglevel();

	/* open the general logfile or syslog */
	if (config_compare_option("logstyle", "syslog")) {
		if (log_init_syslog(&loginfo) < 0) {
			return -1;
		}
	} else {
		if (log_init_logfile(&loginfo) < 0) {
			return -1;
		}
	}

	/* Open the command log logfile(s) */
	if (log_init_cmdlog(&loginfo, 1) < 0) {
		return -1;
	}

	/* Do the same with directories */
	if (log_init_dirlog(&loginfo, 1) < 0) {
		return -1;
	}

	return 0;
}


int log_detect_logfile_change() {
	/* syslog is now disabled, it was enabled previously, logging to
	 * files is now enabled */
	if (!config_compare_option("logstyle", "syslog")
						&& loginfo.syslog == 1) {
		return 1;
	}
	/* the name of the logfile has changed */
	if (!config_compare_option("logfile", loginfo.logf_name)) {
		return 1;
	}
	return 0;
}

int log_detect_syslog_change() {
	/* syslog is now enabled, it was not enabled previously */
	if (config_compare_option("logstyle", "syslog")
						&& loginfo.syslog == 0) {
		return 1;
	}
	/* syslog is enabled, but the facility has changed */
	if (!config_compare_option("syslogfacility", loginfo.syslog_facility)
						&& loginfo.syslog == 1){
		return 1;
	}
	return 0;
}

void log_cmdlogent_just_free(struct cmdlogent_t* cmd) {
	if (!cmd) {
		return;
	}
	log_cmdlogent_just_free(cmd->next);
	free(cmd->logf_name);
	free(cmd->style);
	free(cmd->specs);
	free(cmd);
}

struct cmdlogent_t* log_entry_in_cmdlogent(struct cmdlogent_t* cmd,
					   const char* logf_name,
					   const char* specs,
					   const char* style) {

	while(cmd) {
		if (strcmp(logf_name, cmd->logf_name) == 0
		 && strcmp(specs, cmd->specs) == 0
		 && strcmp(style, cmd->style) == 0) {

			return cmd;
		}
		cmd = cmd->next;
	}
	return 0;
}

int log_cmd_compare(struct cmdlogent_t** cmdn,
		    struct cmdlogent_t** cmdo,
		    FILE*(*f)(const char* s)) {

	struct cmdlogent_t* cmditer = *cmdn;
	struct cmdlogent_t* cmdpos;
	/* see, what is in cmdn, but not in cmdo. They are new, create them. */

	while (cmditer) {
		if (!(cmdpos = log_entry_in_cmdlogent(*cmdo,
			cmditer->logf_name, cmditer->specs, cmditer->style))) {
			/* in cmdn, but not in cmdo */
			cmditer->logf = (*f)(cmditer->logf_name);
		} else {
			/* since we have to replace cmdo by cmdn, we also
			 * have to copy the file handle even if the entry
			 * exists in the same way */
			cmditer->logf = cmdpos->logf;
			cmditer->logf_size = cmdpos->logf_size;
		}
		cmditer = cmditer->next;
	}

	/* see what is in cmdo, but not in cmdn. They are old, close them */
	cmditer = *cmdo;

	while(cmditer) {
		if (!(cmdpos = log_entry_in_cmdlogent(*cmdn,
			cmditer->logf_name, cmditer->specs, cmditer->style))) {
			fclose(cmditer->logf);
		} else {
			cmdpos->logf = cmditer->logf;
			cmdpos->logf_size = cmditer->logf_size;
		}
		cmditer = cmditer->next;
	}

	/* delete the old cmdlogent structure */
	log_cmdlogent_just_free(*cmdo);
	/* copy the new over the old */
	*cmdo = *cmdn;

	return 0;
}


int log_detect_cmdlog_change() {
	/* logf_name, specs and style may change */
	loginfo_bk = (struct loginfo_st*) malloc(sizeof(struct loginfo_st));
	enough_mem(loginfo_bk);

	memset(loginfo_bk, (int) 0, sizeof(struct loginfo_st));

	if (log_init_cmdlog(loginfo_bk, 0) < 0) {
		return -1;
	}
	if (log_init_dirlog(loginfo_bk, 0) < 0) {
		return -1;
	}

	/* check for new ones - first step
	 * a         a - found, ok
	 * b         c - found, ok
	 * c         d - found, ok
	 * d         e - not found, new
	 *           f - not found, new
	 *
	 *  check for old ones - second step
	 * a         a - found, ok
	 * c         b - not found, delete
	 * d         c - found, ok
	 * e         d - found, ok
	 * f
	 */

	log_cmd_compare(&loginfo_bk->cmdlogfiles, &loginfo.cmdlogfiles,
								open_logfile);
	log_cmd_compare(&loginfo_bk->cmdlogdirs, &loginfo.cmdlogdirs,
							log_init_dirlog_open);
	free(loginfo_bk);

	return 0;
}

int log_detect_log_change() {
	int newlevel = config_get_ioption("debuglevel", loginfo.debuglevel);
	if (newlevel != loginfo.debuglevel) {
		/* the debug level has changed, very simple to adjust  :-) */
		loginfo.debuglevel = log_init_debuglevel();
	}
	if (log_detect_logfile_change()) {
		/* close the old logfile */
		if (loginfo.logf) {
			fclose(loginfo.logf);
			loginfo.logf = (FILE*) 0;
		}
		if (loginfo.logf_name) {
			free(loginfo.logf_name);
			loginfo.logf_name = (char*) 0;
		}
		/* open a new logfile */
		log_init_logfile(&loginfo);
	}
	if (log_detect_syslog_change()) {
		/* close the syslog */
		closelog();
		/* and re-open it */
		log_init_syslog(&loginfo);
	}
	log_detect_cmdlog_change();
	return 0;
}

