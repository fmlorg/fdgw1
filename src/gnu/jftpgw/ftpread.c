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
#include <signal.h>
#include <ctype.h>

/* reads a single line from fd and returns a pointer to a malloc()ed 
 * char array.
 *
 * Parameters: fd: The file descriptor to read from
 *
 * Return values: (char*) 0 on error, a pointer to a malloc()ed char array
 *                that contains the read data on success
 *
 * Called by: various functions
 */

extern int timeout;
static char *readline_check(int, int);

char* readline(int fd) {
	/* do not check for valid FTP responses */
	return readline_check(fd, 0);
}

char* ftp_readline(int fd) {
	/* check for valid FTP responses */
	return readline_check(fd, 1);
}

static char *readline_check(int fd, int check_ftp_format) {
	const int MAXSIZE = 3;
	int n, ret, length = 0;
	int linecnt;
	int data_read = 0;
	char *temp, *buffer;
	fd_set readset;
	struct timeval rtimeout;
	sigset_t sigset, oldset;
	rtimeout.tv_sec = config_get_ioption("commandtimeout", 300);
	rtimeout.tv_usec = 0;

	FD_ZERO(&readset);
	FD_SET(fd, &readset);

	timeout = 0;

	sigemptyset(&sigset);
	sigemptyset(&oldset);
	sigaddset(&sigset, SIGCHLD);
	ret = sigprocmask(SIG_BLOCK, &sigset, &oldset);
	if (ret < 0) {
		log(3, "sigprocmask() error: %s", strerror(errno));
	}
	if ((ret = select(fd + 1, &readset, NULL, NULL, &rtimeout)) <= 0) {
		if (ret == 0) {
			timeout = 1;
		}
		/* save the errno value from sigprocmask() */
		n = errno;
		ret = sigprocmask(SIG_UNBLOCK, &sigset, &oldset);
		if (ret < 0) {
			log(3, "sigprocmask() error releasing the blocked"
				" signals: %s", strerror(errno));
		}
		errno = n;
		return 0;
	}
	ret = sigprocmask(SIG_UNBLOCK, &sigset, &oldset);
	if (ret < 0) {
		log(3, "sigprocmask() error releasing the blocked"
			" signals: %s", strerror(errno));
	}
	
	if (fd < 0) {
		log(1, "readline_check: Not connected");
		return 0;
	}

	buffer = (char *)malloc(MAXSIZE);
	enough_mem(buffer);

	temp = buffer;

	linecnt = 0;
	while ((n = read(fd, temp, 1)) > 0) {
		data_read = 1;
		if (*temp == '\r') { linecnt++; continue; }
		if (*temp == '\n') { linecnt++; break; }
		if (*temp == '\0') break;
		/* check for a valid FTP response. It must start with either
		 * xxx <text> (mind the space)
		 * or
		 * xxx-<text> (with a dash)
		 */
		length++;
		if (check_ftp_format && linecnt == 0
			&& ((length <= 3 && !isdigit((int) *temp))
				|| (length == 4 && *temp != ' '
						&& *temp != '-'))) {

				buffer[length] = 0;
				log(4, "malformed FTP response: %s",
					buffer);
				log(9, "in line: %d", linecnt);
				set_errstr("malformed FTP response");
				return 0;
		}
		if ((length+1) % MAXSIZE == 0) {
			buffer = (char*) realloc(buffer, length + 1 + MAXSIZE);
			enough_mem(buffer);
			temp = buffer + length - 1;
		}
		temp++;
	}
	if (n < 0 || (n == 0 && data_read == 0)) {
		if (n < 0) {
			set_errstr(strerror(errno));
			log(2, "Error reading: %s", strerror(errno));
		}
		free(buffer);
		return 0;
	}
	buffer[length] = '\0';
	if (strcasestr(buffer, "PASS ") == (char*) 0) {
		log(9, "Read (%d): %s", fd, buffer);
	} else {
		log(9, "Read (%d): ***hidden***", fd);
	}
	return buffer;
}


/* reads from fd until it has a "xxx data" response and returns the
 * response code xxx.
 * If **data != 0, *data is set to the address of the malloc()ed array that
 * contains the read data.
 *
 * Parameters: fd: The file descriptor to read from
 *             data: A pointer to a pointer that should be set to the address
 *                   of the read data.
 *
 * Return value: 0 on error
 *               the response code on success
 * Called by: checkforabort()
 *
 */

int ftp_getrc(int fd, char **data) {
	char *line;
	char *tmp =0;
	int response;

	while ((line = ftp_readline(fd))) {
		if (!*line) {
			log(1, "Big bad error.. empty string returned by ftp_readline");
			break;
		}
		if (line[0] < '0' || line[0] > '9') {
			free(line);
			continue;
		}
		if (line[3] == ' ') {
			tmp = strdup(line);
			enough_mem(tmp);
			break;
		}
		/* free the line if we continue in the loop */
		free(line);
	}
	if (!line || !*line) return 0;
	sscanf(line, "%d ", &response);
	if (data) {
		*data = tmp;
	} else {
		free(tmp);
	}
	/* free the line that was not freed within the whlie loop because of
	 * a break statement */
	free(line);
	tmp =0;
	if (response >= 100)
		return response;
	return 0;
}

/* readall() reads from an fd and returnes the whole data in a structure
 * message.
 * 
 * Parameters: sourcefd: The file descriptor to read from
 * 
 * Return values: A struct message with
 *                  - both char* arrays in it set to NULL on error
 *                  - one char* array pointing to the whole message and the
 *                    other one pointing to the last line of it, starting with
 *                    the response code.
 * 
 * Called by: login() to get the whole welcome message and to analyze
 *            the authentication message
 *
 */

struct message readall(int sourcefd) {
	char* line =0, *buf =0, *tmp =0, *last =0, *linestart =0;
	struct message ret;
	size_t tmpsize;

	while ((line = readline(sourcefd))) {
		if (!*line) {
			log(1, "Big bad error.. empty string returned by ftp_readline");
			free(line);
			break;
		}
		if (buf) {
			tmpsize = strlen(buf) + strlen(line) + 4;
			tmp = (char*) malloc(tmpsize);
			enough_mem(tmp);
			strncpy(tmp, buf, tmpsize);
			last = tmp + strlen(buf);
			free(buf);
			buf = 0;
		} else {
			tmpsize = strlen(line) + 4;
			tmp = (char*) malloc(tmpsize);
			enough_mem(tmp);
			tmp[0] = '\0';
			last = tmp;
		}
		linestart = tmp + strlen(tmp);
		scnprintf(tmp, tmpsize, "%s\r\n", line);
		buf = tmp;
		tmp =0;
		if (line[0] < '0' || line[0] > '9') {
			free(line);
			continue;
		}
		if (line[3] == ' ') {
			break;
		}
		free(line);
		line = (char*) 0;
	}
	if (!line) {
		free(buf);
		ret.fullmsg = (char*) 0;
		ret.lastmsg = (char*) 0;
		return ret;
	} else {
		free(line);
	}
	ret.fullmsg = buf;
	log(9, "Readall (%d): %s", sourcefd, buf);
	ret.lastmsg = last;
	return ret;
}

/* passall() reads from sourcefd until there is no data left to read and writes
 * everything to targetfd
 *
 * Parameters: sourcefd: The file descriptor to read from
 *             targetfd: The file descriptor to write to
 *
 * Return value: 0 on error
 *               a pointer that contains the malloc()ed char array with the
 *               passed data on success
 *
 * Called by: handlecmds() to pass the message sent after a QUIT
 *            passcmd() to pass the control connection when passing a command
 *            passcmd() to pass the control connection after having transmit
 *                      the file 
 */

char* passall(int sourcefd, int targetfd) {
	char* line =0, *sendbuf =0;
	size_t sendbufsize;

	while ((line = readline(sourcefd))) {
		if (!*line) {
			log(1, "Big bad error.. empty string returned by ftp_readline");
			break;
		}
		sendbufsize = strlen(line) + 3;
		sendbuf = (char*) malloc(sendbufsize);
		enough_mem(sendbuf);
		snprintf(sendbuf, sendbufsize, "%s\r\n", line);
		free(line);
		say(targetfd, sendbuf);

		if (strlen(sendbuf) > 4
			&& isdigit((int)sendbuf[0])
			&& isdigit((int)sendbuf[1])
			&& isdigit((int)sendbuf[2])
			&& sendbuf[3] == ' ') {

			/* the loop is left here */
			break;
		} else {
			free(sendbuf);
			sendbuf = 0;
		}
	}
	return sendbuf;
}

