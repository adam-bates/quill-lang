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
	foreach ix in 0..str.length {
		char c = str.bytes[ix];

		if c < '0' || '9' < c {
			return .{
				.is_ok = false,
				.err = `Non-digit character: '{c}'`,
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

Result<uint8> parse_uint8(String str) {
	if str.length == 0 {
		return .{
			.is_ok = false,
			.err = "Empty string",
		};
	}

	uint8 MAX = 255;

	uint8 n = 0;
	foreach ix in 0..str.length {
		char c = str.bytes[ix];

		if c < '0' || '9' < c {
			return .{
				.is_ok = false,
				.err = `Non-digit character: '{c}'`,
			};
		}

		if (MAX - (c - '0')) / 10 < n {
			return .{
				.is_ok = false,
				.err = `Overflow!`
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
