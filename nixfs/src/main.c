#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <fcntl.h>

#include "debug.h"
#include "nixfs.h"
#include "version.h"

int debug_enabled = 0;

typedef struct {
	int debug_enabled;
	int print_version;
	const char *mount_point;
	// Add other custom flags as needed
} nixfs_options;

nixfs_options options;

#define NIXFS_OPT(t, p) { t, offsetof(nixfs_options, p), 1 }
static struct fuse_opt nixfs_opts[] = {
	NIXFS_OPT("--debug", debug_enabled),
	NIXFS_OPT("--version", print_version),
	// Add more custom flags here
	FUSE_OPT_END
};

static int nixfs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs) {
	nixfs_options *opts = (nixfs_options *)data;

	switch (key) {
	case FUSE_OPT_KEY_NONOPT:
		if (opts->mount_point == NULL) {
			if (strcmp(arg, "none") == 0) {
				// Ignore "none" for compatibility with fstab/mount syntax
				return 0; // Do not add this to outargs
			} else {
				opts->mount_point = arg;
				return 0; // Do not add this to outargs
			}
		}
		// Pass non-option arguments to fuse
		return 1;
	default:
		// Pass options not defined in nixfs_opts to fuse
		return 1;
	}
}

static struct fuse_operations nixfs_oper = {
	.getattr = nixfs_getattr,
	.readdir = nixfs_readdir,
	.open = nixfs_open,
	.read = nixfs_read,
	.readlink = nixfs_readlink,
};

int main(int argc, char *argv[]) {
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	memset(&options, 0, sizeof(options));

	// Parse options and set mount point if available
	fuse_opt_parse(&args, &options, nixfs_opts, nixfs_opt_proc);

	if (options.print_version) {
		printf("nixfs version %s\n", NIXFS_VERSION);
		return 0;
	}

	if (options.debug_enabled) {
		debug_enabled = 1;
		log_debug("Debug logging enabled\n");
	}

	// If no mount point was specified, show usage and exit
	if (options.mount_point == NULL) {
		fprintf(stderr, "Usage: %s [none] /path/to/mount/point [--options]\n", argv[0]);
		return 1;
	}

	// Add the mount point back to args if it was filtered out
	if (strcmp(options.mount_point, "none") != 0) {
		// Re-add the mount point to args for FUSE
		fuse_opt_add_arg(&args, options.mount_point);
	}

	return fuse_main(args.argc, args.argv, &nixfs_oper, NULL);
}
