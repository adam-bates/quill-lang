#include "common.h"
#include "error.h"

Error err_create(ErrorType type, char const* msg) {
    return (Error){ .type = type, .msg = msg };
}

Error err_create_nomsg(ErrorType type) {
    return err_create(type, 0);
}

Error err_create_unspecified(char const* msg) {
    return err_create(ET_UNSPECIFIED, msg);
}

Error err_create_empty(void) {
    return err_create_unspecified(0);
}

