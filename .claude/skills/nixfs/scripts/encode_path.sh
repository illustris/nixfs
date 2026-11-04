#!/usr/bin/env bash
# Encode flake references for use with nixfs
# Usage: encode_path.sh <method> <path>
#   method: urlenc, b64, or str
#   path: the flake reference or expression to encode

set -euo pipefail

usage() {
	cat <<EOF
Usage: $(basename "$0") <method> <path>

Encode a flake reference or Nix expression for use with nixfs.

Methods:
  str      - No encoding (use for simple paths without special chars)
  urlenc   - URL encoding (recommended for paths with slashes, #, etc.)
  b64      - Base64url encoding (for very complex paths)

Examples:
  $(basename "$0") str 'nixpkgs#hello'
  $(basename "$0") urlenc 'github:owner/repo#package'
  $(basename "$0") b64 'complex-expression-here'

Output:
  Prints the encoded path to stdout, suitable for use in nixfs paths like:
  /nixfs/flake/<method>/<encoded-path>/bin/binary
EOF
	exit 1
}

# Check arguments
if [[ $# -ne 2 ]]; then
	echo "Error: Expected 2 arguments, got $#" >&2
	usage
fi

METHOD="$1"
PATH_TO_ENCODE="$2"

case "$METHOD" in
	str)
		# No encoding needed
		echo "$PATH_TO_ENCODE"
		;;

	urlenc)
		# URL encoding using jq if available, otherwise python3
		if command -v jq >/dev/null 2>&1; then
			echo -n "$PATH_TO_ENCODE" | jq -sRr @uri
		elif command -v python3 >/dev/null 2>&1; then
			python3 -c 'import sys, urllib.parse; print(urllib.parse.quote(sys.argv[1], safe=""))' "$PATH_TO_ENCODE"
		else
			echo "Error: Neither jq nor python3 found. Please install one of them." >&2
			exit 1
		fi
		;;

	b64)
		# Base64url encoding (RFC 4648 §5)
		# Standard base64 but replace + with -, / with _, and remove padding =
		if command -v base64 >/dev/null 2>&1; then
			echo -n "$PATH_TO_ENCODE" | base64 -w0 | tr '+/' '-_' | tr -d '='
		else
			echo "Error: base64 command not found" >&2
			exit 1
		fi
		;;

	*)
		echo "Error: Unknown encoding method '$METHOD'" >&2
		echo "Valid methods: str, urlenc, b64" >&2
		usage
		;;
esac
