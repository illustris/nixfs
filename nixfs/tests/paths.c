#define FUSE_USE_VERSION 35
#include <errno.h>
#include <fuse.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "nixfs.h"

int debug_enabled = 0;
uid_t eval_uid = (uid_t)-1;
gid_t eval_gid = (gid_t)-1;
char eval_cache_dir[64] = "";

static const struct {
	const char *path;
	mode_t type;
} cases[] = {
	{ "/flake/str/nixpkgs#hello", S_IFLNK },
	{ "/expr/str/--option/#one/#two/--option/#three/#four/value", S_IFLNK },
	{ "/flake/urlenc/--refresh", S_IFDIR },
	{ "/expr/b64/--option/#one/#two/--option/#three/#four/--offline", S_IFDIR },
	{ "/unknown", 0 },
	{ "/unknown/str/value", 0 },
	{ "/flake/unknown/value", 0 },
	{ "/expr/unknown/value", 0 },
};

static pthread_barrier_t barrier;

static void *check_attributes(void *arg) {
	size_t index = *(size_t *)arg;
	pthread_barrier_wait(&barrier);
	for (int i = 0; i < 10000; i++) {
		struct stat st;
		int result = nixfs_getattr(cases[index].path, &st, NULL);
		if (cases[index].type == 0 ? result != -ENOENT :
		    result != 0 || (st.st_mode & S_IFMT) != cases[index].type) {
			fprintf(stderr, "%s: result=%d mode=%o\n", cases[index].path,
				result, (unsigned int)st.st_mode);
			abort();
		}
	}
	return NULL;
}

int main(void) {
	const size_t count = sizeof(cases) / sizeof(cases[0]);
	pthread_t threads[count];
	size_t indexes[count];
	if (pthread_barrier_init(&barrier, NULL, count) != 0) {
		return 1;
	}
	for (size_t i = 0; i < count; i++) {
		indexes[i] = i;
		if (pthread_create(&threads[i], NULL, check_attributes, &indexes[i]) != 0) {
			return 1;
		}
	}
	for (size_t i = 0; i < count; i++) {
		if (pthread_join(threads[i], NULL) != 0) {
			return 1;
		}
	}
	pthread_barrier_destroy(&barrier);

	const char *invalid_links[] = { "/", "/flake", "/expr", "/flake/str", "/expr/b64", "/unknown/str/value" };
	for (size_t i = 0; i < sizeof(invalid_links) / sizeof(invalid_links[0]); i++) {
		char buf[64];
		int result = nixfs_readlink(invalid_links[i], buf, sizeof(buf));
		if (result != -ENOENT) {
			fprintf(stderr, "%s: readlink result=%d\n", invalid_links[i], result);
			return 1;
		}
	}
	char buf = 'x';
	if (nixfs_readlink("/flake/str/nixpkgs#hello", &buf, 0) != -EINVAL || buf != 'x') {
		return 1;
	}
	puts("Concurrent path attributes and invalid readlinks passed");
	return 0;
}
