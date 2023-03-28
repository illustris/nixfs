#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <fcntl.h>

#include "debug.h"

static const char *hello_str = "Hello, World!\n";
static const char *hello_path = "/hello.txt";
int debug_enabled = 0;

typedef struct {
	int debug_enabled;
	// Add other custom flags as needed
} simple_options;

simple_options options;

#define SIMPLE_OPT(t, p) { t, offsetof(simple_options, p), 1 }
static struct fuse_opt simple_opts[] = {
	SIMPLE_OPT("--debug", debug_enabled),
	// Add more custom flags here
	FUSE_OPT_END
};

static int simple_opt_proc(void *data, const char *arg, int key,
			   struct fuse_args *outargs) {
	// supress warnings
	(void)data;
	(void)arg;

	switch (key) {
	case FUSE_OPT_KEY_NONOPT:
		// Pass non-option arguments to fuse
		return 1;
	default:
		// Pass options not defined in simple_opts to fuse
		return 1;
	}
}

static int simple_getattr(const char *path, struct stat *stbuf) {
	memset(stbuf, 0, sizeof(struct stat));

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (strcmp(path, hello_path) == 0) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(hello_str);
	} else {
		return -ENOENT;
	}

	return 0;
}

static int simple_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			  off_t offset, struct fuse_file_info *fi) {
	if (strcmp(path, "/") != 0) {
		return -ENOENT;
	}

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	filler(buf, hello_path + 1, NULL, 0);

	log_debug("Here\n");

	return 0;
}

static int simple_open(const char *path, struct fuse_file_info *fi) {
	if (strcmp(path, hello_path) != 0) {
		return -ENOENT;
	}

	if ((fi->flags & O_ACCMODE) != O_RDONLY) {
		return -EACCES;
	}

	return 0;
}

static int simple_read(const char *path, char *buf, size_t size, off_t offset,
		       struct fuse_file_info *fi) {
	size_t len;
	if (strcmp(path, hello_path) != 0) {
		return -ENOENT;
	}

	len = strlen(hello_str);
	if (offset >= len) {
		return 0;
	}

	if (offset + size > len) {
		size = len - offset;
	}

	memcpy(buf, hello_str + offset, size);

	return size;
}

static struct fuse_operations simple_oper = {
	.getattr = simple_getattr,
	.readdir = simple_readdir,
	.open = simple_open,
	.read = simple_read,
};

int main(int argc, char *argv[]) {
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	memset(&options, 0, sizeof(options));
	fuse_opt_parse(&args, &options, simple_opts, simple_opt_proc);

	if (options.debug_enabled) {
		// Debug is enabled, add your debug related code here
		debug_enabled = 1;
		log_debug("Debug logging enabled\n");
	}

	return fuse_main(args.argc, args.argv, &simple_oper, NULL);
}
