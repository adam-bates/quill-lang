import libc::stdio;

import ./std;

uint println(String str) {
	return stdio::fwrite(str.bytes, size_of<char>(), str.length, stdio::stdout);
}
