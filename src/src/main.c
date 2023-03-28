#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <fcntl.h>

static const char *hello_str = "Hello, World!\n";
static const char *hello_path = "/hello.txt";

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
	return fuse_main(argc, argv, &simple_oper, NULL);
}
