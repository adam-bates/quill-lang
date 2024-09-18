@c_header(<stdio.h>);

typedef 
    @c_restrict
    @c_FILE
    void
as FILE;

FILE mut* stdout;

uint fwrite(
	@c_restrict void* buffer,
	uint size,
	uint count,
	FILE mut* stream,
);
