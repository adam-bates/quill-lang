#ifndef quill_fs_h
#define quill_fs_h

#include "./arena.h"
#include "./base.h"
#include "./string.h"

String file_read(Arena* arena, String path_s);
void write_file(Arena* arena, String dir, String file, String content);

#endif
