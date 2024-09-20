package std;

struct Array<T> {
	uint length,
	T*   bytes,
}

typedef String = Array<char>;
