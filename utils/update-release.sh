#!/usr/bin/env bash

execute() {
	if [ "$dry_run" == "1" ]; then
		echo "Dry run: $*"
	else
		"$@"
	fi
}

dry_run=0
[ "$1" == "--dry-run" ] && dry_run=1

read -p "Enter new version number: " new_version

execute sed -i "s/project(nixfs VERSION [0-9.]* LANGUAGES C)/project(nixfs VERSION $new_version LANGUAGES C)/" nixfs/CMakeLists.txt
execute sed -i "s/version = \"[0-9.]*\";/version = \"$new_version\";/" flake.nix

git diff

read -p "Confirm changes and continue? (y/n): " confirm
[ "$confirm" != "y" ] && echo "Aborting." && exit 1

execute git add nixfs/CMakeLists.txt flake.nix

git diff --cached

read -p "Confirm commit and continue? (y/n): " confirm_commit
[ "$confirm_commit" != "y" ] && echo "Aborting." && git reset && exit 1

execute git commit -m "Bump version to $new_version"
execute git tag "v$new_version"

read -p "Push changes and tag to remote? (y/n): " confirm_push
[ "$confirm_push" != "y" ] && echo "Aborting." && exit 1

execute git push origin master
execute git push origin "v$new_version"

echo "Done!"
