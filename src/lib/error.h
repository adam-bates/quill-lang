#ifndef snowy_error_h
#define snowy_error_h

typedef enum {
    ET_UNSPECIFIED,
    ET_OUT_OF_MEMORY,

    ET_COUNT,
} ErrorType;

typedef struct {
    ErrorType   type;
    char const* msg;
} Error;

Error err_create(ErrorType type, char const* msg);
Error err_create_nomsg(ErrorType type);
Error err_create_unspecified(char const* msg);
Error err_create_empty(void);

#endif
