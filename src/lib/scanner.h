#ifndef snowy_scanner_h
#define snowy_scanner_h

#include "common.h"
#include "token.h"

typedef struct {
    char const* source;

    char const* current;
    int line;
} Scanner;

typedef struct {
    bool ok;
    union {
        Tokens tokens;
        Error err;
    } res;
} ScanResult;

Scanner scanner_create(char const* source);
ScanResult scanner_scan(Scanner scanner);

#endif
