@c_header(<stdio.h>);

@c_restrict
@c_FILE
void mut* stdout;

uint fwrite(
	@c_restrict void* buffer,
	uint size,
	uint count,
	@c_restrict @c_FILE void mut* stream,
);
