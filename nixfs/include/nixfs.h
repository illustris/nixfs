#ifndef NIXFS_H
#define NIXFS_H

#include <sys/types.h>

int nixfs_getattr(const char *path, struct stat *stbuf);
int nixfs_readlink(const char *path, char *buf, size_t size);
int nixfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		  off_t offset, struct fuse_file_info *fi);
int nixfs_open(const char *path, struct fuse_file_info *fi);
int nixfs_read(const char *path, char *buf, size_t size, off_t offset,
	       struct fuse_file_info *fi);

#define MAX_PATH_LENGTH 256

typedef struct {
	char *path;
	mode_t mode;
	off_t size;
} fs_node;

extern uid_t eval_uid;
extern gid_t eval_gid;
extern char eval_cache_dir[64];

#endif // NIXFS_H
