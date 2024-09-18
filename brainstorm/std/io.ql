import libc::stdio;

import ./std;

uint println(String str) {
	return stdio::fwrite(str.bytes, sizeof(char), str.length, stdio::stdout);
}
