package std/conv;

import std::String;
import std::Result;

Result<uint> parse_uint(String str) {
	uint n = 0;
	foreach i in 0..str.length {
		n *= 10;
		n += str.bytes[i] - '0';
	}
	return .{
		.is_ok = true,
		.val = n,
	};
}
