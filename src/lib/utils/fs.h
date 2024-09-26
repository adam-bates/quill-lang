#ifndef quill_fs_h
#define quill_fs_h

#include "./allocator.h"
#include "./base.h"
#include "./string.h"

String file_read(Allocator const allocator, char const* path);

#endif
