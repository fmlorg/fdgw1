#include "jftpgw.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>

#define INFO_SUFFIX ".info"

extern struct hostent_list* hostcache;

/* only the user should be able to read/write the cache */
int cache_perms = S_IRWXU;

int cache_available(struct cache_filestruct);
int cache_readinfo(struct cache_filestruct*);
int cache_writeinfo(struct cache_filestruct);
int cache_want(struct cache_filestruct);
char* cache_qualifypath(const struct cache_filestruct);
char* cache_qualifyfile(const struct cache_filestruct);
char* cache_qualifyinfo(const struct cache_filestruct);
int recursive_mkdir(const char* pathname, int perms);


struct cache_filestruct cache_gather_info(const char* filename,
						struct clientinfo* clntinfo) {
	char* pwd;
	char* complete_fname;
	size_t size;
	struct cache_filestruct cfs;

	if (filename[0] != '/') {
		/* get the directory */
		pwd = getftpwd(clntinfo);
	} else {
		pwd = strdup("");
		enough_mem(pwd);
	}
	size = strlen(filename) + 1 + strlen(pwd) + 1;
	complete_fname = (char*) malloc(size);
	enough_mem(complete_fname);
	complete_fname = rel2abs(filename, pwd, complete_fname, size);
	if ((char*) 0 == complete_fname) {
		log(4, "Error in rel2abs: filename: %s, path: %s",
				filename, pwd);
	}
	free(pwd);
	cfs.filepath = extract_path(complete_fname);
	cfs.filename = extract_file(complete_fname);
	cfs.size = getftpsize(complete_fname, clntinfo);
	cfs.date = getftpmdtm(complete_fname, clntinfo);
	free(complete_fname);
	/* filename is not free()ed, it points inside args and thus inside
	 * buffer in cmds.c */

	cfs.user = clntinfo->user;
	cfs.host = clntinfo->destination;
	cfs.port = clntinfo->destinationport;

	return cfs;
}

int cache_available(struct cache_filestruct cfs) {
	char* fname = cache_qualifyfile(cfs);
	struct cache_filestruct cfs_info;
	struct stat st;

	if (stat(fname, &st) < 0) {
		if (errno == ENOENT) {
			return CACHE_NOTAVL_EXIST;
		}
		/* other error */
		log(2, "Could not stat %s: %s",
				fname, strerror(errno));
		return -1;
	}

	if (cfs.size != st.st_size) {
		/* the new file seems to differ in size -> delete our copy
		 * */
		log(8, "cache copy differs in size");
		cache_delete(cfs, 1);
		return CACHE_NOTAVL_SIZE;
	}

	if (cfs.date != st.st_mtime) {
		/* the new file seems to differ in the date -> delete our
		 * copy */
		log(8, "cache copy differs in the date");
		cache_delete(cfs, 1);
		return CACHE_NOTAVL_DATE;
	}

	cfs_info = cfs;
	cache_readinfo(&cfs_info);

	if (cfs.checksum != cfs_info.checksum) {
		/* the new file seems to differ in the checksum -> delete
		 * our copy */
		cache_delete(cfs, 1);
		return CACHE_NOTAVL_CHECKSUM;
	}

	return CACHE_AVAILABLE;
}


int cache_add(struct cache_filestruct cfs) {

	struct stat st;
	char* fname = cache_qualifyfile(cfs);
	struct utimbuf ut;

	/* okay, we're sure the path exists, let's try to create a file */

	/* compare size, set date... */

	if (stat(fname, &st) < 0) {
		log(2, "Could not stat %s: %s",
				fname, strerror(errno));
		return -1;
	}
	if (st.st_size != cfs.size) {
		log(6, "Had to delete %s again. Size did not match", fname);
		cache_delete(cfs, 1);
	}

	/* set the date */
	ut.actime = ut.modtime = cfs.date;
	if (utime(fname, &ut) < 0) {
		log(6, "Could net set date/time information to %s: %s",
				fname, strerror(errno));
	}

	return cache_writeinfo(cfs);

}

int cache_writefd(struct cache_filestruct cfs) {
/*
	Add a file to the cache.

	The directory structure is like

	<cache_prefix> / <user>@<host>:<port> / <filepath> / <filename>
*/
	char* path;
	char* filefn;
	int fd;

	if (!cache_want(cfs)) {
		return -1;
	}

	/* just delete for sure, don't care about the return value */
	cache_delete(cfs, 0);

	path = cache_qualifypath(cfs);
	if (recursive_mkdir(path, cache_perms) < 0 && errno != EEXIST) {
		log(2, "Could not create directory %s: %s",
			path, strerror(errno));
		return -1;
	}

	filefn = cache_qualifyfile(cfs);
	fd = creat(filefn, cache_perms);
	if (fd < 0) {
		log(2, "Could not create data file %s in cache: %s",
				filefn, strerror(errno));
	}
	return fd;
}

int cache_delete(struct cache_filestruct cfs, int warn) {
	char* infofile, *datafile;
	int err = 0;

	infofile = cache_qualifyinfo(cfs);
	datafile = cache_qualifyfile(cfs);

	/*if (unlink(infofile) < 0) {
		log(2, "Could not unlink file %s: %s",
				infofile, strerror(errno));
		* do not return immediately, try to delete the other entry,
		 * too *
		err = -1;
	}*/
	if (unlink(datafile) < 0 && warn) {
		log(2, "Could not unlink file %s: %s",
				datafile, strerror(errno));
		err = -1;
	}
	return err;
}

int cache_readfd(struct cache_filestruct cfs) {
	char* fname = cache_qualifyfile(cfs);
	int fd;

	if (cache_available(cfs) != CACHE_AVAILABLE) {
		return -1;
	}

	fd = open(fname, O_RDONLY);
	return fd;
}

int cache_want(struct cache_filestruct cfs) {
/*
	See if we want a file to be added to the cache.
*/
	unsigned long int minsize = config_get_size("cacheminsize", 0);
	unsigned long int maxsize = config_get_size("cachemaxsize", ULONG_MAX);

	return (cfs.size <= maxsize && cfs.size >= minsize);
}

int cache_readinfo(struct cache_filestruct *cfs) {
	char* fname = cache_qualifyinfo(*cfs);
	char checksumbuf[128];
	int fdinfo;
	int i;

	return 0; /* not yet implemented */

	if (cfs->checksum) {
		free(cfs->checksum);
		cfs->checksum = (char*) 0;
	}

	fdinfo = open(fname, O_RDONLY);

	if (fdinfo < 0) {
		log(2, "Could not open file %s in cache: %s",
				fname, strerror(errno));
		return -1;
	}

	/* file is opened */
	i = read(fdinfo, checksumbuf, sizeof(checksumbuf) - 1);
	if (i < 0) {
		log(3, "Could not read info from file %s: %s",
				fname, strerror(errno));
		close(fdinfo);
		return -1;
	}

	if (i > 0 && i < sizeof(checksumbuf)) {
		checksumbuf[i] = '\0';
	}
	cfs->checksum = strdup(checksumbuf);
	close(fdinfo);
	return 0;
}


int cache_writeinfo(struct cache_filestruct cfs) {
	char* fname = cache_qualifyinfo(cfs);
	int fdinfo;
	int i;

	return 0; /* not yet implemented */

	fdinfo = creat(fname, cache_perms);
	if (fdinfo < 0) {
		log(2, "Could not create info file %s in cache: %s",
				fname, strerror(errno));
		return -1;
	}

	/* file is created and opened */
	/* write the info */
	i = write(fdinfo, cfs.checksum, strlen(cfs.checksum));
	i += write(fdinfo, "\n", 1);

	close(fdinfo);

	if (i == strlen(cfs.checksum) + 1) {
		return 0;
	} else {
		return -1;
	}
}


char* cache_qualifypath(const struct cache_filestruct cfs) {
	size_t size;
	static char* path;
	const char* hostname;
	unsigned long iaddr;
	const char* cache_prefix = config_get_option("cacheprefix");

	if (!cache_prefix || config_get_bool("cache") == 0) {
		return (char*) 0;
	}
	iaddr = inet_addr(cfs.host);
	if (iaddr == (unsigned long int) UINT_MAX) {
		/* cfs.host was not a valid IP */
		hostname = cfs.host;
	} else {
		/* try to look it up */
		if ( !(hostname = hostent_get_name(&hostcache, iaddr)) ) {
			hostname = cfs.host;
		}
	}

	size =  	  strlen(cache_prefix) + 1
			+ strlen(cfs.user)     + 1
			+ strlen(hostname)     + 1
			+ 20
			+ strlen(cfs.filepath) + 1
			+ 1;

	if (path) {
		path = (char*) realloc(path, size);
	} else {
		path = (char*) malloc(size);
	}
	enough_mem(path);

	snprintf(path, size, "%s/%s@%s:%d/%s",
			cache_prefix,
			cfs.user,
			hostname,
			cfs.port,
			cfs.filepath);

	return path;
}


char* cache_qualifyfile(const struct cache_filestruct cfs) {
	char* path = cache_qualifypath(cfs);
	size_t size = strlen(path) + 1 + strlen(cfs.filename) + 1;
	static char* filename;

	if (filename) {
		filename = (char*) realloc(filename, size);
	} else {
		filename = (char*) malloc(size);
	}
	enough_mem(filename);

	snprintf(filename, size, "%s/%s", path, cfs.filename);

	return filename;
}


char* cache_qualifyinfo(const struct cache_filestruct cfs) {
	char* filename = cache_qualifyfile(cfs);
	size_t size = strlen(filename) + strlen(INFO_SUFFIX) + 1;
	static char* infoname;

	if (infoname) {
		infoname = (char*) realloc(infoname, size);
	} else {
		infoname = (char*) malloc(size);
	}
	enough_mem(infoname);

	snprintf(infoname, size, "%s/%s", filename, INFO_SUFFIX);

	return infoname;
}


int recursive_mkdir(const char* pathname, int perms) {
	char* tocreate;
	int sidx = 0;
	int failed = 0;

	tocreate = (char*) malloc(strlen(pathname) + 1);
	enough_mem(tocreate);

	do {
		while(pathname[sidx] && pathname[sidx] != '/') {
			tocreate[sidx] = pathname[sidx];
			sidx++;
		}
		tocreate[sidx] = pathname[sidx];
		sidx++;
		tocreate[sidx] = '\0';

		log(9, "Creating %s\n", tocreate);
		if (mkdir(tocreate, perms) < 0 && errno != EEXIST) {
			failed = -1;
		}
	} while(pathname[sidx] && failed == 0);

	free(tocreate);
	return -failed;
}

