// single-line comment

/*
    multi-line comment

    /* nested multi-line comments! */

    note: the end of the above "inner" comment didn't close the "outer" one.
*/

// import statements
import std;
import std/ds;
import std/io;

/*
    entry-point to the program

    There's a couple options for what main can look like.

    return type can be one of:
    - None:        void
    - c-style:     int
    - Quill-style: std::ExitCode

    params can be either:
    - None:        ()
    - c-style:     (int argc, char** argv)
    - Quill-style: (std::Array<std::String> args)
*/
void main() {

    // type inference
    let v = 1;

    // mutation
    let mut v = 1; // <-- data is constant by default; `mut` opts-in to mutation
    v = 2;

    // booleans
    bool v = true || false;

    // signed integers
    int    v = 0; // <-- no size == architecture size (ie. c's size_t)
    int8   v = 0;
    int16  v = 0;
    int32  v = 0;
    int64  v = 0;
    int128 v = 0;

    // unsigned integers
    uint    v = 0; // <-- used to represent pointers; no size == architecture size
    uint8   v = 0; // <-- used to represent 1 byte
    uint16  v = 0;
    uint32  v = 0;
    uint64  v = 0;
    uint128 v = 0;

    // floating-point numbers
    float    v = 0.0; // <-- no size == architecture size
    float16  v = 0.0;
    float32  v = 0.0;
    float64  v = 0.0;
    float128 v = 0.0;

    // characters
    char  v = '\0'; // <-- default behaviour
    uint8 v = '\0';
    char  v = 0;

    // pointers
    int128* ptr = &0;    // <-- ptr to in-place data is ok, lifetime is current scope
    void*   ptr = ptr;
    uint    ptr = (uint)ptr;

    // arrays
    int[]  v = []{ 1, 2, 3 };
    int[]* v = &[]{ 1, 2, 3 };
    int*   v = (int*)v;

    int v0 = v[0];
    int v0 = *v;

    //

    int[3] v = []{ 1, 2, 3 };

    int v0 = v[0];

    //

    std::Array<int> v = std::arr_create([]{ 1, 2, 3 });
    uint len = v.length;
    int[] data = v.data;

    std::Maybe<int> v0 = std::arr_get(&v, 0);

    // list
    ds::ArrayBuffer<int> mut v = ds::arrbuf_create();
    ds::arrbuf_push(&v, 42);
    std::Array<int> vv = ds::arrbuf_to_arr(v);

    // strings
    std::String v = "hello";

    // character arrays & c strings
    char[] v = []{ 'hello\0' };
    char[] v = []{ 'h', 'e', 'l', 'l', 'o', 0 };

    char[]* v = &[]{ 'hello\0' };
    char[]* v = &[]{ 'h', 'e', 'l', 'l', 'o', 0 };

    char* v = (char*)&[]{ 'hello\0' };
    char* v = (char*)&[]{ 'h', 'e', 'l', 'l', 'o', 0 };

    char* v = @c_str "hello";

    // template strings
    ds::StringBuffer v = `{v}, world`;

    ds::StringBuffer mut v = ds::strbuf_create();
    ds::strbuf_append_char(&v, '!');
    std::String vv = ds::strbuf_to_str(v);

    // optionals (ie. "nullables")
    std::Maybe<int> m = std::some(0);
    std::Maybe<int> m = std::none();

    if m.is_some {
        int _ = m.val;
    }

    int v2 = m else 42;
    int v2 = m else do { CRASH "uh oh!"; };

    int v = 1;
    int* p = &v;

    std::Maybe<int*> mut p2 = std::none();
    std::assert_none(p2);

    p2 = std::some(&v);
    int* v = std::assert_some(p2);

    p2 = std::some((int*)0); // this is still valid, but compiler warning

    std::Maybe<std::NonullPtr<int>> mut p3 = std::none();
    p3 = std::some(std::assert_nonull(&v));

    std::NullablePtr<int> mut p4 = std::null();
    p3 = std::nullable(&v);

    // errors
    std::Error err = std::err_create(std::ErrorType::UNSPECIFIED, "example error");
    std::Error err = {
        .type = std::ErrorType::UNSPECIFIED,
        .msg  = "example error",
    };

    // results
    std::Result<int> res = std::res_ok(0);
    std::Result<int> res = std::res_err(err);

    if res.is_ok {
        int _ = res.val;
    } else {
        std::Error _ = res.err;
    }

    int v = res else 42;
    int v = res catch e sizeof(e);
    int v = res catch e do { CRASH `Error: {e}`; };

    int v = std::assert_ok(res);
    std::Error err = std::assert_err(res);

    // switch
    int v = 3;
    switch v {
        case 1, 2, 3 {
            //
        }
        else {
            //
        }
    }

    // for-loops
    for (int i = 0; i < 5; ++i) {
        // i = 0, 1, 2, 3, 4
    }
    
    // foreach-loops
    foreach i in 0..5 {
        // i = 0, 1, 2, 3, 4
    }

    int[_] fibs = []{ 1, 1, 3, 5, 8 };
    for i, n in fibs {
        // i = 0, 1, 2, 3, 4
        // n = 1, 1, 3, 5, 8
    }

    // while-loops
    while false { }

    std::Maybe<int> x = std::some(3);
    /*
        note for when x == std::some(0)
        `while x` resolves x to true, as Maybe doesn't look at the underlying data. It is only false when is_some = false.
    */
    while x { 
        x -= 1;

        if x < 0 { x = null; }
    }

    int x = 3;
    while (bool)x { // <-- int x = 0; (bool)x is false
        x -= 1;
    }

    // do-expressions
    int x = do {
        break 1;
    };

    int x = do if true {
        break 1;
    } else {
        break 2;
    };

    int x = do for i in 0..10 {
        if i > 0 && ((i - 1) % 4 == 0) {
            break i;
        }
    } else {
        break 5;
    };

    // IO
    io::println("Hello, world!");

    // defer
    // the following codeblock prints: "1,2,3"
    {
        defer io::print("2");

        defer {
            io::print("1,");
        }

        io::print(",3");
    }


    // panics
    CRASH "example panic";

    // TODO: figure out if anything is missing, add here
}

// file separator
// Everything under this line is considered "private"
---

void try_example() {
    int double = fetch_double(true) else return;    
    int double = fetch_double(false) else return;
}

std::Result<int> fetch_double(bool should_succeed) {
    // Here's the first time I'm showcasing `try`
    // This will bubble up an error, or resolve to the ok value.

    int n = try fetch_int(should_succeed);
    return n * 2;
}

std::Result<int> fetch_int(bool should_succeed) {
    if !should_succeed {
        return std::res_err_create(std::ErrorType::UNSPECIFIED, "failed!");
    }

    return std::res_ok(42);
}
