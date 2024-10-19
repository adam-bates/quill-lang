@c_header("<stdlib.h>")
package libc/stdlib;

void* malloc(uint size);
void* calloc(uint count, uint size);
void* realloc(void* ptr, uint size);
void free(void* ptr);

uint llabs(int n);
