#include <stdio.h>
#include <ctype.h>

#include "urldec.h"

int hex_to_decimal(int c) {
	if ('0' <= c && c <= '9') {
		return c - '0';
	} else if ('a' <= c && c <= 'f') {
		return c - 'a' + 10;
	} else if ('A' <= c && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

int urldecode(const char* in, size_t inlen, char* out, size_t* outlen) {
	size_t i = 0;
	size_t j = 0;

	while (i < inlen) {
		if (in[i] == '%') {
			if (i + 2 < inlen) {
				int high = hex_to_decimal(in[i+1]);
				int low = hex_to_decimal(in[i+2]);
				if (high == -1 || low == -1) {
					return -1;  // invalid URL-encoded string
				}
				out[j++] = (char)((high << 4) | low);
				i += 3;
			} else {
				return -1;  // invalid URL-encoded string
			}
		} else if (in[i] == '+') {
			out[j++] = ' ';
			i++;
		} else {
			out[j++] = in[i++];
		}
	}

	out[j] = '\0';  // null-terminate the output string
	*outlen = j;    // set output length

	return 0;
}
