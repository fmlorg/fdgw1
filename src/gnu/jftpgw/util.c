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
#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <net/if.h>


#ifdef HAVE_CRYPT_H
#include <pwd.h>    /* getpass() */
#define _XOPEN_SOURCE
#include <crypt.h>  /* crypt() */
#endif

static int changecode(char *const, const char*);

extern int timeout;
extern int chlds_exited;
extern struct uidstruct runasuser;
extern struct serverinfo srvinfo;
sigset_t chldterm_sigset, chldterm_oldset;
char* errstr = 0;

void enough_mem(const void* ptr) {
	if (ptr == (void*) 0) {
		log(0, "Not enough memory for malloc. Exiting.");
		exit(1);
	}
}

/* concating snprintf
 *  *
 *  * determines the length of the string pointed to by `os', appending
 *  * formatted string to a maximium length of `len'.
 *  *
 *  */

void scnprintf (char *os, size_t len, const char *str, ...) {
	va_list vl;
	char *ostmp = os + strlen (os);

	va_start (vl, str);
	vsnprintf (ostmp, len - strlen (os) - 1, str, vl);
	va_end (vl);

	return;
}

void set_errstr(const char* s) {
	if (errstr) {
		free(errstr);
	}
	errstr = strdup(s);
}

const char* get_errstr(void) {
	if (errstr) {
		return errstr;
	} else {
		return "No detailed error information available... :-(";
	}
}

void free_errstr(void) {
	if (errstr) {
		free(errstr);
		errstr = (char*) 0;
	}
}


const char* strcasestr(const char* haystack, const char* needle) {
	char* nhay = strdup(haystack);
	char* nneed = strdup(needle);
	const char* match;
	char* p;

	enough_mem(nhay);
	enough_mem(nneed);

	p = nhay;
	while (*p) {
		*p = (char) toupper((int)*p);
		p++;
	}
	p = nneed;
	while (*p) {
		*p = (char) toupper((int)*p);
		p++;
	}
	match = strstr(nhay, nneed);

	if (match) {
		match = haystack + (match - nhay);
	}

	free(nhay);
	free(nneed);

	return match;
}


/* writes a char* to an fd and checks the return value */

int say(int fd, const char* phrase) {
	int i;
	struct timeval writetime = { 300, 0 };
	fd_set writeset;
	FD_ZERO(&writeset);
	FD_SET(fd, &writeset);

	i = select(fd + 1, NULL, &writeset, NULL, &writetime);
	if (i == 0) {
		log(2, "Timeout reached in say()");
		log(2, "Timeout in %s line %d\n", __FILE__ ,__LINE__);
		timeout = 1;
		return -1;
	}
	if (i == -1) {
		log(1, "Error in select() in say(): %s", strerror(errno));
		return -1;
	}

	if (strcasestr(phrase, "PASS ") == (char*) 0) {
		log(9, "Write(%d): %s", fd, phrase);
	} else {
		log(9, "Write(%d): ***hidden***", fd);
	}
	i = write(fd, phrase, strlen(phrase));
	if (i < 0) {
		log(3, "write (say) failed: %s", strerror(errno));
		if (strcasestr(phrase, "PASS ") == (char*) 0) {
			log(3, "should say %s to %d", phrase, fd);
		} else {
			log(3, "should say ***hidden*** to %d", fd);
		}
	}
	return i;
}

#define SAYBUFFERSIZE		200
int sayf(int fd, const char* fmt, ...) {
	va_list args;
	static char str[SAYBUFFERSIZE];

	va_start(args, fmt);
	vsnprintf(str, SAYBUFFERSIZE - 1, fmt, args);
	va_end(args);

	return say(fd, str);
}

int changeid(int who_id, int what, const char* reason) {
	int i = 0;
	char* wstr;
	const char* who = (char*) 0;
	uid_t uid  = getuid();
	uid_t euid = geteuid();
	gid_t gid  = getgid();

	if (who_id == PRIV) {
		who = "root";
	} else {
		if (what == EUID || what == UID) {
			who = config_get_option("runasuser");
			if (!who) {
				if (what == EUID) {
					log(4, "No runasuser option specified (but only euid)!");
					return 0;
				}
				if (what == UID) {
					log(4, "No runasuser option specified!");
					return -1;
				}
			}
		}
		if (what == EGID || what == GID) {
			who = config_get_option("runasgroup");
			if (!who) {
				if (what == EGID) {
					log(4, "No runasgroup option specified (but only egid)!");
					return 0;
				}
				if (what == GID) {
					log(4, "No runasgroup option specified!");
					return -1;
				}
			}
		}
	}

	/* we can't change the UID/GID if we are not root and if we don't
	 * want to become root */
	if (strcmp(who, "root") != 0  &&  uid != 0) {
		return 0;
	}
	if (strcmp(who, "root") == 0  &&  euid == 0 
			&& (what == EUID || what == UID)) {
		/* we are done - root already */
		return 0;
	}
	if (uid != 0) {
		/* can't change the (E)(U/G)ID if we are not root */
		return 0;
	}
	if (uid == 0 && strcmp(who, "root") != 0 && euid != 0
			&& (what == UID || what == GID || what == EGID)) {
		/* UID is root, but effective UID is not. The UID/GID should
		 * be changed. Change to root first */
		if (changeid(PRIV, UID, "Want to change UID/GID. "
					"Change back to root first") < 0) {
			return -1;
		}
	}

	log(7, reason);

	if (strcmp(who, "root") == 0) {
		uid = 0;
	} else if (strcmp(who, runasuser.username) == 0) {
		uid = runasuser.uid;
	} else if (strcmp(who, runasuser.groupname) == 0) {
		gid = runasuser.gid;
	} else {
		log(2, "Invalid user/group specified: %s", who);
		return -1;
	}
	if (what == EUID) {
#ifdef HAVE_SETEUID
		i = seteuid(uid);
#else
		/* HP-UX does not know seteuid() */
		i = setreuid(-1, uid);
#endif
	} else if (what == UID) {
		i = setuid(uid);
	} else if (what == EGID) {
#ifdef HAVE_SETEGID
		i = setegid(gid);
#else
		/* HP-UX does not know setegid() */
		i = setregid(-1, gid);
#endif
	} else  if (what == GID) {
		i = setgid(gid);
	}
	switch (what) {
		case UID: wstr = "UID"; break;
		case EUID: wstr = "EUID"; break;
		/* map gid on uid */
		case GID: wstr = "GID"; uid = gid; break;
		/* map gid on uid */
		case EGID: wstr = "EGID"; uid = gid; break;
		default: wstr = "ERROR";
	}
	if (i) {
		log(3, "Could not change the %s to %d: %s",
				wstr, uid, strerror(errno));
	}
	else {
		log(8, "Changed %s to %d (%s)", wstr, uid, who);
	}
	return i;
}


/* just get the status of the child so that it can end */
void reap_chld_info (int signo) {
	int err = errno;
	int status;
	/* signal handler but waitpid() is reentrant */
	while (waitpid (-1, &status, WNOHANG) > 0) {};
	errno = err;
}

/* get the status of the child and unregister it */
/* this function is the signal handler */
void childterm (int signo) {

	chlds_exited++;

}

int get_chld_pid() {
	int ret;
	int status;
	pid_t pid;

	while ((pid = waitpid (-1, &status, WNOHANG)) > 0) {
		log(9, "A child exited. Pid: %d", pid);
		log(9, "unregistering pid ...");
		ret = unregister_pid(pid);
		if (ret) {
			log(3, "Error unregistering pid");
		} else {
			log(9, "unregistered");
		}
	}
	chlds_exited = 0;
	return 0;
}


/* returns the first characters up to a whitespace character */

/* Checks if PATTERN is the beginning of RESPONSE */

int checkbegin(const char* response, const char* pattern) {
	if (strlen(response) < strlen(pattern)) {
		return 0;
	}
	return !strncasecmp(response, pattern, strlen(pattern));
}


/* returns the code in a response */

int getcode(const char* response) {
	char buffer[4];
	strncpy(buffer, response, sizeof(buffer) - 1);
	buffer[3] = '\0';
	return atoi(buffer);
}


/* Checks if the string RESONSE starts with SHOULDBE */

int checkdigits(const char* response, const int shouldbe) {
	return (getcode(response) == shouldbe);
}




/* extracts the numerical code out of an FTP server response. */

int respcode(const char* response) {
	int resp = 0;
	int i;
	const char* respoff = response;

	if (!response) {
		return 0;
	}

	while (respoff[3] != ' ') {
		respoff = strchr(respoff, '\n');
		if (!respoff) {
			return -1;
		} else {
			respoff++;   /* skip over '\n' */
		}
	}

	i = sscanf(respoff, "%d ", &resp);
	if (i != 1 && resp < 100) {
		return -1;
	}
	return resp;
}

const char* gethostentip(const char* iplist) {
	static char ipbuf[16];
	snprintf(ipbuf, 16, "%d.%d.%d.%d",
		(unsigned char) iplist[0],
		(unsigned char) iplist[1],
		(unsigned char) iplist[2],
		(unsigned char) iplist[3]
	);
	return ipbuf;
}


/* parsesock parses a comma separated list of IP and Port like in
 *  PORT 127,0,0,1,15,216
 * or the PASV answer.
 *
 */

int parsesock(char* buffer, struct sockaddr_in *sin, int mode) {
	int i1, i2, i3, i4, lo, hi, count;
	unsigned long int iaddr;
	unsigned int port;
	char ipbuf[16];

	memset((void*)sin, 0, sizeof(*sin));
	count = sscanf(buffer, "%d,%d,%d,%d,%d,%d",
			&i1, &i2, &i3, &i4, &hi, &lo);
	/* sscanf must have read 6 arguments and all the parameters must be
	 * less than 255 ( 0xff )
	 */
	if (!(count != 6 || i1 > 0xff || i2 > 0xff || i3 > 0xff || i4 > 0xff
			|| hi > 0xff || lo > 0xff)) {
		snprintf(ipbuf, 16, "%d.%d.%d.%d",
				i1, i2, i3, i4);
		iaddr = inet_addr(ipbuf);
		if (iaddr != -1 || !strcmp(ipbuf, BROADCAST)) {
			sin->sin_family = AF_INET;
			sin->sin_addr.s_addr = iaddr;
			port = hi * 256 + lo;
			sin->sin_port = htons(port);
			return 0;
		}
		else {
			log(3, "Invalid address in the PASV or PORT command: %s",
					buffer);
		}
	}
	log(3, "Error parsing IP and port from %s", buffer);
	return -1;
}

/* sets HOWMANY bits on. */

unsigned long int setlastbits(int howmany) {
	unsigned long e = 0;
	int i;

	for (i=0; i < 32; i++) {
		if (i <= howmany) {
			e |= 1;
		}
		e = e << 1;
	}

	return ntohl(e);
}

void toupstr(char* s) {
	while (*s) {
		*s = toupper((int)*s);
		s++;
	}
}

void char_squeeze(char *const s, int c) {
	char last = 0;
	int i = 0, j = 0;
	char* tmp = malloc(strlen(s) + 1);
	enough_mem(tmp);

	do {
		if (s[i] == last && s[i] == c) { /* do nothing */ }
		else { tmp[j++] = s[i]; }

		last = s[i];

		i++;
	} while (s[i]);
	tmp[j] = '\0';
	strcpy(s, tmp);
	free(tmp);
}


struct ip_t parse_ip(const char* s) {
	struct ip_t s_ip;
	struct in_addr iaddr;
	int ret;
	const char* slash = strchr(s, '/');
	const char* dot;
	char* ip;
	if (!slash) {
		/* no netmask specified */
		slash = s + strlen(s);
	}
	ip = malloc(slash - s + 1);
	enough_mem(ip);
	strncpy(ip, s, slash - s);
	ip[slash - s] = '\0';

	ret = inet_aton(ip, &iaddr);
	free(ip); ip =0;
	if (ret == 0) {
		/* inet_aton error */
		s_ip.ip = -1;
		s_ip.netmask = -1;
		return s_ip;
	}
	s_ip.ip = iaddr.s_addr;

	if (*slash) {
		slash++;
	}
	/* slash points to the netmask now or is 0 if none has been specified */
	if (!*slash) {
		s_ip.netmask = -1;   /* 255.255.255.255 */
		return s_ip;
	}
	dot = strchr(slash, '.');
	if (!dot) {
		/* a decimal number netmask has been specified */
		s_ip.netmask = setlastbits(atoi(slash));
		return s_ip;
	}
	ret = inet_aton(slash, &iaddr);
	if (ret == 0) {
		/* error */
		s_ip.netmask = inet_addr("255.255.255.255");
	} else {
		s_ip.netmask = iaddr.s_addr;
	}

	return s_ip;
}

int cmp_domains(const char* name, const char* pattern) {
	const char* start;
	/* the hostname may not be shorter than the pattern
	 *
	 * pattern: .foobar.mit.edu
	 * name:        bla.mit.edu
	 *
	 * => won't match
	 */

	if (strlen(name) < strlen(pattern)) {
		return 0;
	}
	start = name + strlen(name) - strlen(pattern);
	return !strcasecmp(start, pattern);
}

void err_time_readline(int fd) {
	log(2, "Timeout reached in readline()");
	say(fd, "500 Connection timed out\r\n");
}

void err_readline(int fd) {
	char* m;
	int e = errno;
	size_t msize;
	const char* err;
	char* s = "An error occurred in ftp_readline: %s";

	if (!(err = get_errstr())) {
		err = strerror(e);
	}
	log(2, s, err);
	msize = strlen(s) + strlen(err) + 7;
	m = (char*) malloc(msize);
	enough_mem(m);
	strcpy(m, "500 ");
	snprintf(m + strlen(m),
			msize         /* initial size */
			- strlen(m)   /* "500 " */
			- 3,          /* \r\n\0 */
			s, err);
	strcat(m, "\r\n");
	say(fd, m);
	free(m);
}

/* extracts joe from joe@foo,21 or   foo bar from "foo bar"@bla,21 */

char* extract_username(const char* s) {
	const char* p;
	char* r;
	if (*s == '"') {
		p = strchr(s+1, '"');
		if (!p) {
			return 0;
		}
	} else {
		p = strchr(s, '@');
		if (!p) {
			p = strchr(s, ',');
		}
		if (!p) {
			p = s + strlen(s);
		}
	}
	r = malloc(p - s + 1);
	enough_mem(r);
	strncpy(r, s, p - s);
	r[p - s] = '\0';
	return r;
}

/* extracts joe@host from joe@host,21 */

char* extract_userhost(const char* s) {
	const char *p;
	char *r;
	if (*s == '"') {
		p = strchr(s+1, '"');
		if (!p) {
			return 0;
		}
		p = strchr(p+1, ',');
		if (!p) {
			p = s + strlen(s);
		}
	} else {
		p = strchr(s, ',');
		if (!p) {
			p = s + strlen(s);
		}
	}
	r = (char*)malloc(p - s + 1);
	enough_mem(r);
	strncpy(r, s, p - s);
	r[p - s] = '\0';
	return r;
}

/* extracts foo from joe@foo,21 or bla from "foo bar"@bla,21 */
char* extract_hostname(const char* s) {
	const char* p;
	char* t = extract_userhost(s);
	char* r;

	if (!t) {
		return (char*) 0;
	}
	p = strrchr(t, '@');
	if (!p) {
		return (char*) 0;
	}

	p++;

	r = strdup(p);
	enough_mem(r);
	free(t);

	return r;
}

unsigned int extract_port(const char* s) {
	const char* p, *t;
	unsigned int pno;
	int i;


	if (!s || !*s) {
		return 0;
	}
	if ( ! (p = strrchr(s, '@')) ) {
		return 0;
	}

	if ( ! (t = strchr(p, ':')) ) {
		t = strchr(p, ',');
	}
	if (!t) {
		return 0;
	}
	i = sscanf(t, "%d", &pno);
	if ( i != 1 ) {
		return 0;
	}

	return pno;

}

char* extract_path(const char* pathfile) {
	const char* last_slash = strrchr(pathfile, '/');
	char* path;
	size_t size;

	if ( ! last_slash) {
		/* no path, it's just a filename */
		return strdup(".");
	}

	size = last_slash - pathfile + 1 + 1;

	path = (char*) malloc(size);
	enough_mem(path);

	snprintf(path, size, "%s", pathfile);

	return path;
}

char* extract_file(const char* pathfile) {
	const char* last_slash = strrchr(pathfile, '/');
	char* file;
	size_t size;

	if ( ! last_slash) {
		/* no path, it's just a filename */
		return strdup(pathfile);
	}

	size = strlen(pathfile) - ( last_slash - pathfile + 1 ) + 1;

	file = (char*) malloc(size);
	enough_mem(file);

	snprintf(file, size, "%s", last_slash + 1);

	return file;
}

int cryptcmp(const char* encrypted, const char* clear) {
	const char* cmp;
#ifdef HAVE_CRYPT
	char salt[3];
	salt[0] = encrypted[0];
	salt[1] = encrypted[1];
	salt[2] = '\0';
	cmp = crypt(clear, salt);
#else
	cmp = clear;
#endif
	if ( ! clear || ! encrypted ) {
		return 1;
	}
	return strcmp(cmp, encrypted);
}

char* cryptpw(const char* clear) {
#ifdef HAVE_CRYPT
	char* crypted = 0;
	char* ret = 0;
	char salt[3];
	char saltposs[] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
		'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
		'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D', 'E',
		'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
		'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '0',
		'1', '2', '3', '4', '5', '6', '7', '8', '9', '.', '/' };
	int idx1, idx2;

	time_t tm;
	tm = time(NULL) * getpid();
	srand(tm);

	idx1 = (int)((double)rand() / ((double)RAND_MAX + 1) * sizeof saltposs);
	idx2 = (int)((double)rand() / ((double)RAND_MAX + 1) * sizeof saltposs);
	salt[0] = saltposs[idx1];
	salt[1] = saltposs[idx2];
	salt[2] = '\0';

	crypted = crypt(clear, salt);
	ret = (char*) malloc(strlen(crypted) + 1);
	enough_mem(ret);
	strcpy(ret, crypted);
	printf("%s\n", crypted);
	return ret;
#else
	return "foo";
#endif
}


/* encrypt_password() just reads a password from stdin and outputs the
 * encrypted version */

void encrypt_password() {
#ifdef HAVE_CRYPT
	char* pw = getpass("Password: ");
	char* crypted = cryptpw(pw);
	memset(crypted, 0, strlen(pw));
#else
	char* crypted = "No crypt support compiled in.";
#endif
	printf("%s\n", crypted);
}


char* to_ascii(char *data, int *len) {
	int count, len2 = *len;
	char* buffer2 = (char*) malloc(*len * 2);
	int last = 0;
	char *b2ptr = buffer2;

	for (count = 0; count < len2; count++) {
		if ((data[count] == 10) && (last != 13)) {
			*b2ptr = 13;
			b2ptr++;
			(*len)++;
		}
		*b2ptr = data[count];
		last = *b2ptr;
		b2ptr++;
	}
	return buffer2;
}


char* from_ascii(char *data, int *len) {
	char *dptr = data;
	int count, len2 = *len;

	for (count = 0; count < len2; count++) {
		if (data[count] != 13) {
			*dptr = data[count];
			dptr++;
		} else {
			(*len)--;
		}
	}
	return data;
}


FILE* open_logfile(const char* fname) {
	FILE* logf;
	int err;

	umask(0x077);
	logf = fopen(fname, "a");
	umask(0x000);
	err = errno;
	if (!logf) {
		log(1, "Couldn't open the log file %s: %s", fname,
				strerror(errno));
	}
	errno = err;
	return logf;
}


/* changecode() changes the response code at the beginning of each line to a
 * new value. In fact, NEW is a _char*_, so you can put any data at the
 * beginning of each line, not just a number
 * 
 * Parameters: msg: the origianl message
 *             new: the portion that should be prepended.
 *
 * Return values: 0 on success
 *
 * Called by login() in order to display the original 220 welcome message
 *           after the login with a changed code.
 * 
 * */

static int changecode(char *const msg, const char* new) {
	char *nextline = msg;
	while (nextline) {
		if (nextline[0] >= '0' && nextline[0] <= '9') {
			/* we subtract 2 because of "\r\n". If nextline is
			 * "foo\r\n" we may at most substitute "bar" or
			 * another 3 character long string, since 
			 *      3       ==        5         - 2  */
			if (strlen(new) <= strlen(nextline) - 2) {
				strncpy(nextline, new, strlen(new));
			}
		}
		nextline = strstr(nextline, "\r\n");
		nextline += 2;
		if (!*nextline) { /* end */
			nextline = 0;
		}
	}
	return 0;
}

/* merges two FTP resopnses.
 *
 * Let resp1 be "220 Bla" and resp2 "331 Foo", the function will return a
 * malloc()ed string 
 * "331-Bla
 *  331 Foo"
 *
 * The code will be read from the char* pointing to resp2
 */

char* merge_responses(const char* resp1, const char* resp2) {
	char codebuf[5];
	char *ret_str;
	size_t ret_size;

	codebuf[0] = codebuf[1] = codebuf[2] = ' ';

	strncpy(codebuf, resp2, 3);

	codebuf[3] = '-';
	codebuf[4] = '\0';

	ret_size = strlen(resp1) + strlen(resp2) + 1 + 1;
	ret_str = (char*) malloc(ret_size);
	enough_mem(ret_str);

	snprintf(ret_str, ret_size, "%s", resp1);
	changecode(ret_str, codebuf);

	scnprintf(ret_str, ret_size, "%s", resp2);
	return ret_str;
}


int change_root(const char* stage) {
	int i;
	int change_id_back = 0;
	const char* directory = config_get_option("changerootdir");
	const char* req_stage = config_get_option("changeroot");

	if (srvinfo.chrooted) {
		return 0;
	}
	if (strcasecmp(req_stage, stage) != 0) {
		return 0;
	}
	if (!directory) {
		return 0;
	}
	if (geteuid() != 0) {
		if (getuid() != 0) {
			log(4, "Not root - no attempt to chroot()");
			return 0;
		}
		/* getuid == 0 but geteuid != 0 */
		if (changeid(PRIV, UID,
				"Changing UID to root for chroot") < 0) {
			return -1;
		}
		change_id_back = 1;
	}
	/* getuid == 0, we have a directory and the state matches */

	i = chdir(directory);
	if (i) {
		log(4, "Could not chdir to %s", directory);
		changeid(UNPRIV, EUID, "After chroot (failed)");
		return -1;
	}
	i = chroot(directory);
	if (!i) {
		log(7, "Changed root directory to %s", directory);
		srvinfo.chrooted = 1;
	} else {
		log(4, "Error changeing root directory to %s: %s",
						directory, strerror(errno));
		changeid(UNPRIV, EUID, "After chroot (failed)");
		return -1;
	}
	if (changeid(UNPRIV, EUID, "After chroot (succeeded)") < 0) {
		return -1;
	}
	i = chdir("/");
	if (i) {
		log(4, "Could not chdir to / of the chrooted environment");
		return -1;
	}
	return 0;
}

int dropprivileges(const char* stage) {
	if (config_compare_option("dropprivileges", stage)) {
		if (config_get_option("runasgroup")) {
			if (changeid(UNPRIV, GID,
					"Dropping group privileges") < 0) {
				return -1;
			}
		}
		if (config_get_option("runasuser")) {
			if (changeid(UNPRIV, UID,
					"Dropping user privileges") < 0) {
				return -1;
			}
		} else {
			log(2, "Option runasuser not set but dropping "
					"privileges requested - terminating");
			return -1;
		}
	}
	return 0;
}

unsigned long int get_uint_ip(int type, struct clientinfo* clntinfo) {
	unsigned long int *ip_ptr;
	int *fd_ptr;

	if (type == GET_IP_SERVER) {
		ip_ptr = &clntinfo->server_ip;
		fd_ptr = &clntinfo->serversocket;
	} else if (type == GET_IP_CLIENT) {
		ip_ptr = &clntinfo->client_ip;
		fd_ptr = &clntinfo->clientsocket;
	} else {
		return UINT_MAX;
	}

	/* The IP is already known, return it */
	if (*ip_ptr != UINT_MAX) {
		return *ip_ptr;
	}

	/* The IP is not known but if we are connected we look at the socket */
	if (*fd_ptr != -1) {
		*ip_ptr = get_uint_peer_ip(*fd_ptr);
		return *ip_ptr;
	}

	/* if we are testing for the destination, i.e. SERVER and
	 * clntinfo->destination happens to be an IP, use this one */

	if (type == GET_IP_SERVER && clntinfo->destination) {
		*ip_ptr = inet_addr(clntinfo->destination);
	}

	return *ip_ptr;
}


const char* get_char_ip(int type, struct clientinfo *clntinfo) {
	unsigned long int ip = get_uint_ip(type, clntinfo);

	return inet_ntoa(*((struct in_addr*) &ip));
}

/*
 * Takes a socket descriptor and returns the string contain the peer's
 * IP address.
 */
const char *get_char_peer_ip(int fd) {
	unsigned long int ip;

	ip = get_uint_peer_ip(fd);

	return inet_ntoa(*((struct in_addr*) &ip));
}


/*
 * Takes a socket descriptor and returns the peer's IP address as a unsigned
 * int
 */
unsigned long int get_uint_peer_ip(int fd) {
	struct sockaddr_in name;
#ifdef HAVE_SOCKLEN_T
	socklen_t namelen;
#else
	int namelen;
#endif
	namelen = sizeof(name);

	if (getpeername(fd, (struct sockaddr *) &name, &namelen) != 0) {
		log(2, "Could not get peername: %s", strerror(errno));
		return UINT_MAX;
	}

	return name.sin_addr.s_addr;
}

int get_interface_ip(const char* iface, struct sockaddr_in *sin) {
#ifdef HAVE_SIOCGIFADDR
	struct ifreq req;
	int fd = socket(PF_INET, SOCK_DGRAM, 0); 
	int ret;

#ifndef IF_NAMESIZE
#  define IF_NAMESIZE IFNAMSIZ
#endif

	memset(req.ifr_name, 0, IF_NAMESIZE);
	strncpy(req.ifr_name, iface, IF_NAMESIZE - 1);
	req.ifr_addr.sa_family = AF_INET; 

	ret = ioctl(fd, SIOCGIFADDR, &req);
	close(fd);

	if (ret == 0) {
		memcpy(sin, &req.ifr_addr, sizeof(struct sockaddr_in));
		return 0;
	}
#endif
	return -1;
}

int get_interface_name(const struct sockaddr_in sin_req, char* iface) {
#ifdef HAVE_SIOCGIFCONF
	struct ifconf ifc;
	struct sockaddr_in sin;
	int ret, fd = -1, nr = 30, n, found;
	struct ifreq *ifr;

	fd = socket (PF_INET, SOCK_DGRAM, 0);

	if (fd < 0) {
		log(7, "Failed to create socket in get_interface_name");
		return -1;
	}

	memset (&ifc, 0, sizeof(ifc));
	ifc.ifc_buf = (void*) 0;
	ifc.ifc_len = sizeof(struct ifreq) * nr;
	ifc.ifc_buf = malloc(ifc.ifc_len);
	enough_mem(ifc.ifc_buf);

	for (;;) {
		ifc.ifc_len = sizeof(struct ifreq) * nr;
		ifc.ifc_buf = realloc(ifc.ifc_buf, ifc.ifc_len);
		enough_mem(ifc.ifc_buf);

		if ((ret = ioctl(fd, SIOCGIFCONF, &ifc)) < 0) {
			log(5, "ioctl error: %s", strerror(errno));
			break;
		}
		if (ifc.ifc_len == sizeof(struct ifreq) * nr) {
			/* assume it overflowed and try again */
			nr += 10;
			continue;
		}
		break;
	}

	if (ret < 0) {
		free(ifc.ifc_buf);
		return -1;
	}

	/* loop through interfaces returned from SIOCGIFCONF */
	found = 0;
	ifr = ifc.ifc_req;
	for (n = 0; n < ifc.ifc_len; n += sizeof(struct ifreq)) {
		if (get_interface_ip(ifr->ifr_name, &sin) == 0) {
			if (sin.sin_addr.s_addr == sin_req.sin_addr.s_addr) {
				snprintf(iface, IF_NAMESIZE, 
							"%s", ifr->ifr_name);
				log(8, "Interface %s was IP %s", ifr->ifr_name,
					inet_ntoa(sin.sin_addr));
				found = 1;
				break;
			}
		}
		ifr++;
	}

	/* we don't need this memory any more */
	free (ifc.ifc_buf);
	close (fd);
	if (found == 1) {
		return 0;
	}
#endif
	return -1;
}


const char* conv_ip_to_char(unsigned long int ip) {
	return inet_ntoa(*((struct in_addr*) &ip));
}


void replace_not_larger(char* s, char* replace_what, char* replace_with) {
	char* p, *pend;
	p = s;

	while ((p = strstr(p, replace_what))) {
		pend = p + strlen(replace_what);
		snprintf(p, strlen(p), "%s%s", replace_with, pend);
	}
}

char* char_prepend(const char* prefix, const char* ostr) {
	char* nstr;
	int newsize;

	if ( ! ostr && ! prefix ) {
		return (char*) 0;
	}
	if ( ! ostr ) {
		return strdup(prefix);
	}
	if ( ! prefix ) {
		return strdup(ostr);
	}
	newsize = strlen(ostr) + strlen(prefix) + 1;
	nstr = (char*) malloc( newsize );
	enough_mem(nstr);
	snprintf(nstr, newsize, "%s%s", prefix, ostr);

	return nstr;
}

char* char_append(const char* ostr, const char* suffix) {
	return char_prepend(ostr, suffix);
}

char* char_enclose(const char* prefix, const char* ostr, const char* suffix) {
	char* nstr1, *nstr2;

	nstr1 = char_prepend(prefix, ostr);
	nstr2 = char_prepend(nstr1, suffix);

	free(nstr1);

	return nstr2;
}

char* strnulldup(const char* str) {
	char* s;
	if ( ! str ) {
		return (char*) 0;
	}

	s = strdup(str);
	enough_mem(s);

	return s;
}

