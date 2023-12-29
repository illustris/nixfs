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

char** tokenize_path(const char *path) {
	// Copy the input path since strtok modifies the string
	char *path_copy = strdup(path);
	if (path_copy == NULL) {
		return NULL; // Memory allocation failed
	}

	// Count the number of tokens
	int token_count = 0;
	for (int i = 0; path_copy[i] != '\0'; i++) {
		if (path_copy[i] == '/') {
			token_count++;
		}
	}

	// Allocate memory for the token pointers
	char **tokens = malloc(sizeof(char*) * (token_count + 2)); // +1 for the last token and +1 for NULL
	if (tokens == NULL) {
		free(path_copy);
		return NULL; // Memory allocation failed
	}

	// Tokenize the path
	int index = 0;
	char *token = strtok(path_copy, "/");
	while (token != NULL) {
		tokens[index++] = strdup(token); // Duplicate token
		token = strtok(NULL, "/");
	}
	tokens[index] = NULL; // Null-terminate the array

	free(path_copy); // Free the copy as it's no longer needed
	return tokens;
}

void free_tokens(char **tokens) {
	if (tokens != NULL) {
		for (int i = 0; tokens[i] != NULL; i++) {
			free(tokens[i]);
		}
		free(tokens);
	}
}

int validate_path(const char *valid_paths[], int num_valid_paths, const char *token) {
	int is_valid_path = 0;
	for (int i = 0; i < num_valid_paths; i++) {
		if (strcmp(token, valid_paths[i]) == 0) {
			is_valid_path = 1;
			break;
		}
	}
	return is_valid_path;
}

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

	char **tokens = tokenize_path(path);
	int token_count = 0;
	if (tokens != NULL) {
		for (int i = 0; tokens[i] != NULL; i++) {
			log_debug("nixfs_getattr: tokens[%d] = %s\n", i, tokens[i]);
			token_count++;
		}
	}

	// Handle dynamic paths under /flake/str/ and /flake/b64/
	// TODO: avoid hardcoding each valid subpath
	if (strcmp(tokens[0], "flake")) {
		free_tokens(tokens);
		return -ENOENT;
	}

	if(!validate_path((const char*[]){ "str", "b64", "urlenc" }, 3, tokens[1])) {
		log_debug("nixfs_getattr: invalid path\n");
		free_tokens(tokens);
		return -ENOENT;
	}
	log_debug("nixfs_getattr: path passed basic validation\n");

	// if the final token starts with a hash or dash,
	// assume it is a flag and say it is a dir
	if (tokens[token_count-1][0] == '#' || tokens[token_count-1][0] == '-') {
		stbuf->st_mode = S_IFDIR;
		stbuf->st_nlink = 2;
		stbuf->st_size = 0;
		free_tokens(tokens);
		return 0;
	}

	// if it matches none of the above, assume it is a flake path
	stbuf->st_mode = S_IFLNK | 0777;
	stbuf->st_nlink = 1;
	stbuf->st_size = 0;
	free_tokens(tokens);
	return 0;
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

void exec_with_tokens(char **tokens, const char *flake_spec) {
	// Count the number of tokens
	int token_count;
	for (token_count = 0; tokens[token_count] != NULL; token_count++);

	// Prepare the arguments array
	const int FIXED_ARGS = 8;  // Number of fixed arguments before tokens
	char **args = malloc((FIXED_ARGS + token_count + 2) * sizeof(char*));

	if (args == NULL) {
			perror("malloc");
			_exit(1); // If execlp fails, exit the child process
	}

	// Add fixed arguments
	args[0] = "nix";
	args[1] = "--extra-experimental-features";
	args[2] = "nix-command";
	args[3] = "--extra-experimental-features";
	args[4] = "flakes";
	args[5] = "build";
	args[6] = "--no-link";
	args[7] = "--print-out-paths";

	// Add tokens
	for (int i = 0; i < token_count; i++) {
		if (tokens[i][0] == '#') tokens[i]++;
		log_debug("exec_with_tokens: tokens[%d] = %s\n", i, tokens[i]);
		args[FIXED_ARGS + i] = tokens[i];
	}

	// Add flake_spec and NULL
	args[FIXED_ARGS + token_count] = (char*)flake_spec;
	args[FIXED_ARGS + token_count + 1] = NULL;

	for(int i=0; i<(FIXED_ARGS + token_count + 2); i++) {
		log_debug("exec_with_tokens: args[%d] = %s\n", i, args[i]);
	}

	// Execute the command
	execvp("nix", args);
	perror("exec");
	_exit(1); // If execlp fails, exit the child process
}

int nixfs_readlink(const char *path, char *buf, size_t size) {
	log_debug("nixfs_readlink: path='%s'\n", path);

	// this malloc needs to be freed before every possible return
	// maybe a full zig rewrite is worth doing
	char **tokens = tokenize_path(path);
	int token_count = 0;
	if (tokens != NULL) {
		for (int i = 0; tokens[i] != NULL; i++) {
			log_debug("nixfs_readlink: tokens[%d] = %s\n", i, tokens[i]);
			token_count++;
		}
	}

	if(strcmp(tokens[0], "flake")) {
		log_debug("nixfs_readlink: invalid path\n");
		free_tokens(tokens);
		return -ENOENT;
	}
	if(!validate_path((const char*[]){ "str", "b64", "urlenc" }, 3, tokens[1])) {
		log_debug("nixfs_readlink: invalid path\n");
		free_tokens(tokens);
		return -ENOENT;
	}
	log_debug("nixfs_readlink: path passed basic validation\n");

	/* const char *flake_spec; */
	const char *encoded_spec;
	const char *flake_spec;
	size_t decoded_len;
	char *decoded_spec;

	if (strcmp(tokens[1], "str") == 0) {
		flake_spec = strdup(tokens[token_count-1]); // strdup just to make the free later unconditional
		log_debug("nixfs_readlink: flake type str\n");
	} else {
		// Determine decoding method and encoded spec based on prefix
		int (*decode_func)(const char*, size_t, char*, size_t*);
		if (strcmp(tokens[1], "b64") == 0) {
			log_debug("nixfs_readlink: flake type b64\n");
			decode_func = base64_decode;
		} else if (strcmp(tokens[1], "urlenc") == 0) {
			log_debug("nixfs_readlink: flake type urlenc\n");
			decode_func = urldecode;
		} else {
			// handle error - unknown prefix
			free_tokens(tokens);
			return -ENOENT;
		}
		encoded_spec = tokens[token_count-1];
		log_debug("nixfs_readlink: encoded flake spec = %s\n", encoded_spec);
		char *decoded_spec = malloc(strlen(encoded_spec) + 1); // Allocate memory for the decoded spec
		if (decoded_spec == NULL) {
			free_tokens(tokens);
			return -ENOENT;
		};
		if (decode_func(encoded_spec, strlen(encoded_spec), decoded_spec, &decoded_len) == -1) {
			// handle decoding failure
			free(decoded_spec);
			free_tokens(tokens);
			return -ENOENT;
		}
		decoded_spec[decoded_len] = '\0';
		flake_spec = strdup(decoded_spec); // copy the string flake spec to a new string
		free(decoded_spec);
	}
	log_debug("nixfs_readlink: flake spec = %s\n",flake_spec);

	int pipe_fd[2];

	if (pipe(pipe_fd) == -1) {
		perror("pipe");
		free_tokens(tokens);
		free((void *)flake_spec);
		return -EIO;
	}

	pid_t pid = fork();
	if (pid == -1) {
		perror("fork");
		free_tokens(tokens);
		free((void *)flake_spec);
		return -EIO;
	}

	if (pid == 0) { // Child process
		dup2(pipe_fd[1], STDOUT_FILENO);
		close(pipe_fd[0]);
		close(pipe_fd[1]);

		// The last token's pointer is overwritten without freeing,
		// but shouldn't matter at this point right before an exec
		tokens[token_count-1] = NULL;
		exec_with_tokens(tokens+2, flake_spec);
	} else { // Parent process
		close(pipe_fd[1]);

		ssize_t nread = read(pipe_fd[0], buf, size - 1);
		if (nread == -1) {
			perror("read");
			free_tokens(tokens);
			free((void *)flake_spec);
			return -EIO;
		}
		buf[nread] = '\0';
		buf[strcspn(buf, "\n")] = '\0'; // Remove newline character

		close(pipe_fd[0]);

		int wstatus;
		waitpid(pid, &wstatus, 0);
		if (WEXITSTATUS(wstatus) != 0) {
			log_debug("nix command exited with status %d\n", WEXITSTATUS(wstatus));
			free_tokens(tokens);
			free((void *)flake_spec);
			return -ENOENT;
		}

		free_tokens(tokens);
		free((void *)flake_spec);
		return 0;
	}

 cleanup: // TODO: move all the freeing logic here
	free_tokens(tokens);
	free((void *)flake_spec);
	return -ENOENT;
}
