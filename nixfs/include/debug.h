#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

extern int debug_enabled;

#define log_debug(...) do { if (debug_enabled) fprintf(stderr, __VA_ARGS__); } while(0)

#endif // DEBUG_H
