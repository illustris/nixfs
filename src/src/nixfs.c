#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <sys/wait.h>
#include <unistd.h>

#include "debug.h"
#include "nixfs.h"

fs_node fs_nodes[] = {
	{ "/",          S_IFDIR | 0755, 0 },
	{ "/flake",     S_IFDIR | 0755, 0 },
	{ "/flake/b64", S_IFDIR | 0755, 0 },
	{ "/flake/str", S_IFDIR | 0755, 0 },
};



#define N_FS_NODES sizeof(fs_nodes) / sizeof(fs_nodes[0])
#define STR_PREFIX(path, match) (strncmp(path, match, strlen(match)) == 0)

int nixfs_getattr(const char *path, struct stat *stbuf) {
	memset(stbuf, 0, sizeof(struct stat));

	for (size_t i = 0; i < N_FS_NODES; i++) {
		if (strcmp(path, fs_nodes[i].path) == 0) {
			stbuf->st_mode = fs_nodes[i].mode;
			stbuf->st_nlink = (fs_nodes[i].mode & S_IFDIR) ? 2 : 1;
			stbuf->st_size = fs_nodes[i].size;
			return 0;
		}
	}

	// Handle dynamic paths under /flake/str/
	if STR_PREFIX(path, "/flake/str/") {
		stbuf->st_mode = S_IFLNK | 0777;
		stbuf->st_nlink = 1;
		stbuf->st_size = 0;
		return 0;
	}

	return -ENOENT;
}

int nixfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                  off_t offset, struct fuse_file_info *fi) {
	fs_node *parent = NULL;

	log_debug("Here\n");

	for (size_t i = 0; i < N_FS_NODES; i++) {
		if (strcmp(path, fs_nodes[i].path) == 0) {
			log_debug("matched %s\n",path);
			parent = &fs_nodes[i];
			break;
		}
	}

	if (!parent || !(parent->mode & S_IFDIR)) {
		log_debug("No parent\n");
		return -ENOENT;
	}

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	for (size_t i = 0; i < N_FS_NODES; i++) {
		if (fs_nodes[i].path != parent->path && STR_PREFIX(fs_nodes[i].path, parent->path)) {
			const char *child_name = fs_nodes[i].path + strlen(parent->path);
			if (child_name[0] != '\0' && strchr(child_name + 1, '/') == NULL) {
				filler(buf, child_name + (child_name[0] == '/' ? 1 : 0), NULL, 0);
			}
		}
	}



	return 0;
}

int nixfs_open(const char *path, struct fuse_file_info *fi) {
	if (strcmp(path, "/flake/b64") == 0 || strcmp(path, "/flake/str") == 0) {
		if ((fi->flags & O_ACCMODE) != O_RDONLY) {
			return -EACCES;
		}
	} else {
		return -ENOENT;
	}

	return 0;
}

int nixfs_readlink(const char *path, char *buf, size_t size) {
	log_debug("nixfs_readlink: size=%lu\n", size);

	if (STR_PREFIX(path, "/flake/str/")) {
		const char *flake_spec = path + strlen("/flake/str/");
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

			execlp("nix", "nix", "build", "--no-link", "--print-out-paths", flake_spec, NULL);
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

			return 0;
		}
	}

	return -ENOENT;
}
