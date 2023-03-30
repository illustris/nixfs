#ifndef NIXFS_H
#define NIXFS_H

int nixfs_getattr(const char *path, struct stat *stbuf);
int nixfs_readlink(const char *path, char *buf, size_t size);
int nixfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		  off_t offset, struct fuse_file_info *fi);

#define MAX_PATH_LENGTH 256

typedef struct {
	char path[MAX_PATH_LENGTH];
	mode_t mode;
	off_t size;
} fs_node;


#endif // NIXFS_H
