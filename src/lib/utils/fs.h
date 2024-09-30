#ifndef quill_fs_h
#define quill_fs_h

#include "./arena.h"
#include "./base.h"
#include "./string.h"

String file_read(Arena* const arena, String path_s);

#endif
