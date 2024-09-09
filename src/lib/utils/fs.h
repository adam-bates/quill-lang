#ifndef quillc_fs_h
#define quillc_fs_h

#include "./allocator.h"
#include "./base.h"
#include "./strings.h"

String file_read(Allocator const allocator, char const* path);

#endif
