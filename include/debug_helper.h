#include <stdio.h>

#ifdef DEBUG
    #define DEBUG_PRINT(fmt, ...) \
        do { fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__); } while (0)
#else
    #define DEBUG_PRINT(fmt, ...) do {} while (0)
#endif
