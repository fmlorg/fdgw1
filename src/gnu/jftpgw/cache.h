#include <time.h>

#define CACHE_AVAILABLE			0
#define CACHE_NOTAVL_EXIST		1
#define CACHE_NOTAVL_SIZE		2
#define CACHE_NOTAVL_DATE		3
#define CACHE_NOTAVL_CHECKSUM		4
#define CACHE_NOTAVL_DEACTIVATED	5

struct cache_filestruct {
	char* host;
	int port;
	char* user;
	char* filepath;
	char* filename;
	unsigned long size;
	char* checksum;
	time_t date;
};


int cache_add(struct cache_filestruct);
int cache_delete(struct cache_filestruct, int warn);
int cache_readfd(struct cache_filestruct);
int cache_writefd(struct cache_filestruct);
int cache_want(struct cache_filestruct);

struct clientinfo;
int cache_init(struct clientinfo*);
int cache_shutdown(struct clientinfo*);
struct cache_filestruct cache_gather_info(const char* filename,
		struct clientinfo*);

