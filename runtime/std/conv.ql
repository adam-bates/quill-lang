package std/conv;

import std;
import std::String;
import std::Result;

import std/ds;

Result<uint> parse_uint(String str) {
	if str.length == 0 {
		return std::res_err<uint>("Empty string");
	}

	uint MAX = 18446744073709551615;

	uint n = 0;
	foreach ix in 0..str.length {
		char c = str.bytes[ix];

		if c < '0' || '9' < c {
			return std::res_err<uint>(`Non-digit character: '{c}'`);
		}

		if (MAX - (c - '0')) / 10 < n {
			return std::res_err<uint>("Overflow");
		}

		n *= 10;
		n += c - '0';
	}

	return std::res_ok<uint>(n);
}

Result<uint8> parse_uint8(String str) {
	if str.length == 0 {
		return std::res_err<uint8>("Empty string");
	}

	uint8 MAX = 255;

	uint8 n = 0;
	foreach ix in 0..str.length {
		char c = str.bytes[ix];

		if c < '0' || '9' < c {
			return std::res_err<uint8>(`Non-digit character: '{c}'`);
		}

		if (MAX - (c - '0')) / 10 < n {
			return std::res_err<uint8>("Overflow");
		}

		n *= 10;
		n += c - '0';
	}

	return std::res_ok<uint8>(n);
}

String int_to_str(int val) {
	let mut sb = ds::strbuf_create(1);
	ds::strbuf_append_int(&sb, val);

	return ds::strbuf_as_str(sb);
}

String uint_to_str(uint val) {
	let mut sb = ds::strbuf_create(1);
	ds::strbuf_append_uint(&sb, val);

	return ds::strbuf_as_str(sb);
}
