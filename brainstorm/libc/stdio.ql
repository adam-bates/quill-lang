@c_header("stdio.h")
package libc::stdio;

typedef FILE = @c_restrict @c_FILE void;

FILE mut* stdout;
FILE mut* stderr;

uint fwrite(
	@c_restrict void* buffer,
	uint size,
	uint count,
	FILE mut* stream,
);
