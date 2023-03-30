#!/bin/bash

MOUNT_POINT="./mnt/"
FUSE_COMMAND="nix run -L .# -- ${MOUNT_POINT} --debug -f -s"
SRC_DIR="src/"

function cleanup {
    fusermount -u ${MOUNT_POINT}
    echo "Unmounted ${MOUNT_POINT}"
}

trap cleanup EXIT

while true; do
    # Run the FUSE command in the background
    ${FUSE_COMMAND} &
    FUSE_PID=$!

    # Wait for any change in the src directory, excluding filenames ending with ~
    inotifywait -r -e modify,move,create,delete --exclude '~$' --exclude '#' "${SRC_DIR}"

    # Kill the FUSE process
    kill ${FUSE_PID}
    wait ${FUSE_PID} 2>/dev/null

    # Unmount the FUSE filesystem
    cleanup
done
