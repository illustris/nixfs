#include <errno.h>
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base64.h"
#include "debug.h"
#include "nixfs.h"
#include "urldec.h"

// define the fixed parts of the tree
fs_node fs_nodes[] = {
	{ "/",             S_IFDIR | 0755, 0 },
	{ "/flake",        S_IFDIR | 0755, 0 },
	{ "/flake/b64",    S_IFDIR | 0755, 0 },
	{ "/flake/str",    S_IFDIR | 0755, 0 },
	{ "/flake/urlenc", S_IFDIR | 0755, 0 },
};



#define N_FS_NODES sizeof(fs_nodes) / sizeof(fs_nodes[0])
#define STR_PREFIX(path, match) (strncmp(path, match, strlen(match)) == 0)

int nixfs_getattr(const char *path, struct stat *stbuf) {
	log_debug("nixfs_getattr: path='%s'\n", path);

	memset(stbuf, 0, sizeof(struct stat));

	// iterate through fs_nodes till a matching path is found
	for (size_t i = 0; i < N_FS_NODES; i++) {
		if (strcmp(path, fs_nodes[i].path) == 0) {
			stbuf->st_mode = fs_nodes[i].mode;
			stbuf->st_nlink = (fs_nodes[i].mode & S_IFDIR) ? 2 : 1;
			stbuf->st_size = fs_nodes[i].size;
			return 0;
		}
	}

	// Handle dynamic paths under /flake/str/ and /flake/b64/
	// TODO: avoid hardcoding each valid subpath
	if (STR_PREFIX(path, "/flake/str/") || STR_PREFIX(path, "/flake/b64/") || STR_PREFIX(path, "/flake/urlenc/")) {
		stbuf->st_mode = S_IFLNK | 0777;
		stbuf->st_nlink = 1;
		stbuf->st_size = 0;
		return 0;
	}

	return -ENOENT;
}

int nixfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                  off_t offset, struct fuse_file_info *fi) {
	log_debug("nixfs_readdir: path='%s'\n", path);

	fs_node *parent = NULL;

	for (size_t i = 0; i < N_FS_NODES; i++) {
		if (strcmp(path, fs_nodes[i].path) == 0) {
			parent = &fs_nodes[i];
			break;
		}
	}

	if (!parent || !(parent->mode & S_IFDIR)) {
		log_debug("nixfs_readdir: parent not found\n");
		return -ENOENT;
	}

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	// find all child dirs of parent
	for (size_t i = 0; i < N_FS_NODES; i++) {
		// match the start of the path with parent
		if (fs_nodes[i].path != parent->path && STR_PREFIX(fs_nodes[i].path, parent->path)) {
			const char *child_name = fs_nodes[i].path + strlen(parent->path);
			// pass only up to the next / to filler()
			if (child_name[0] != '\0' && strchr(child_name + 1, '/') == NULL) {
				filler(buf, child_name + (child_name[0] == '/' ? 1 : 0), NULL, 0);
			}
		}
	}

	return 0;
}

int nixfs_open(const char *path, struct fuse_file_info *fi) {
	log_debug("nixfs_open: path='%s'\n", path);

	// only allow read-only open()
	if (strcmp(path, "/flake/b64") == 0 || strcmp(path, "/flake/str") == 0) {
		if ((fi->flags & O_ACCMODE) != O_RDONLY) {
			return -EACCES;
		}
	} else {
		return -ENOENT;
	}

	return 0;
}

// minimal implementation of read()
int nixfs_read(const char *path, char *buf, size_t size, off_t offset,
		       struct fuse_file_info *fi) {
	return -ENOENT;
}

int nixfs_readlink(const char *path, char *buf, size_t size) {
	log_debug("nixfs_readlink: path='%s'\n", path);

	if (!STR_PREFIX(path, "/flake/str/") && !STR_PREFIX(path, "/flake/b64/") && !STR_PREFIX(path, "/flake/urlenc/"))
		return -ENOENT;


	/* const char *flake_spec; */
	const char *encoded_spec;
	const char *flake_spec;
	size_t decoded_len;
	char *decoded_spec;

	if (STR_PREFIX(path, "/flake/str/")) {
		// extract flake spec from path
		flake_spec = path + strlen("/flake/str/");
	} else {
		// Determine decoding method and encoded spec based on prefix
		int (*decode_func)(const char*, size_t, char*, size_t*);
		if (STR_PREFIX(path, "/flake/b64/")) {
			decode_func = base64_decode;
			encoded_spec = path + strlen("/flake/b64/");
		} else if (STR_PREFIX(path, "/flake/urlenc/")) {
			decode_func = urldecode;
			encoded_spec = path + strlen("/flake/urlenc/");
		} else {
			// handle error - unknown prefix
			return -ENOENT;
		}
		char *decoded_spec = malloc(strlen(encoded_spec) + 1); // Allocate memory for the decoded spec
		if (decoded_spec == NULL) {
			return -ENOENT;
		};
		if (decode_func(encoded_spec, strlen(encoded_spec), decoded_spec, &decoded_len) == -1) {
			// handle decoding failure
			free(decoded_spec);
			return -ENOENT;
		}
		decoded_spec[decoded_len] = '\0';
		flake_spec = strdup(decoded_spec); // copy the string flake spec to a new string
		free(decoded_spec);
	}

	int pipe_fd[2];

	if (pipe(pipe_fd) == -1) {
		perror("pipe");
		return -EIO;
	}

	pid_t pid = fork();
	if (pid == -1) {
		perror("fork");
		return -EIO;
	}

	if (pid == 0) { // Child process
		dup2(pipe_fd[1], STDOUT_FILENO);
		close(pipe_fd[0]);
		close(pipe_fd[1]);

		execlp("nix", "nix",
		       "--extra-experimental-features", "nix-command",
		       "--extra-experimental-features", "flakes",
		       "build", "--no-link",
		       "--print-out-paths", flake_spec, NULL);
		perror("execlp");
		_exit(1); // If execlp fails, exit the child process
	} else { // Parent process
		close(pipe_fd[1]);

		ssize_t nread = read(pipe_fd[0], buf, size - 1);
		if (nread == -1) {
			perror("read");
			return -EIO;
		}
		buf[nread] = '\0';
		buf[strcspn(buf, "\n")] = '\0'; // Remove newline character

		close(pipe_fd[0]);

		int wstatus;
		waitpid(pid, &wstatus, 0);
		if (WEXITSTATUS(wstatus) != 0) {
			log_debug("nix command exited with status %d\n", WEXITSTATUS(wstatus));
			return -ENOENT;
		}

		if (STR_PREFIX(path, "/flake/b64/") || STR_PREFIX(path, "/flake/urlenc/")) {
			// only dynamically allocated for b64
			free((void *)flake_spec);
		}

		return 0;
	}

	return -ENOENT;
}
