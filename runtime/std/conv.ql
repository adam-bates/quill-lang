package std/conv;

import std::String;
import std::Result;

Result<uint> parse_uint(String str) {
	if str.length == 0 {
		return .{
			.is_ok = false,
			.err = "Empty string",
		};
	}

	uint n = 0;
	foreach i in 0..str.length {
		char c = str.bytes[i];

		if c <= '0' || '9' <= c {
			return .{
				.is_ok = false,
				.err = "Non-digit found",
			};
		}

		n *= 10;
		n += c - '0';
	}

	return .{
		.is_ok = true,
		.val = n,
	};
}
