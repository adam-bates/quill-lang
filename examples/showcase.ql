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
import std/mem;

/*
    entry-point to the program

    main always takes 0 params and returns void type.
    - std::args provides an Array<String> for program args
    - std::exit(1) exits the program with the error code 1
*/
void main() {
    // Different allocators available
    // We'll just use the default (arena over libc's malloc)
    // note: this can be changed using mem::set_default_alloc(...)
    mem::Alloc* alloc = mem::default_alloc();

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

    void* nptr = null;

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

    std::Array<int> v = std::arr([]{ 1, 2, 3 });
    uint len = v.length;
    int[] data = v.data;

    std::Maybe<int> v0 = std::arr_get(&v, 0);

    // list
    ds::ArrayBuffer<int> mut v = ds::arrbuf_create(alloc, 8); // or just call ds::arrbuf_default()
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
    ds::StringBuffer v = `{v}, world`; // uses mem::default_alloc() (which can be rewritten)

    ds::StringBuffer mut v = ds::strbuf_default();
    ds::strbuf_append_char(&v, '!');
    std::String vv = ds::strbuf_to_str(v);

    // optionals
    std::Maybe<int> m = std::some(0);
    std::Maybe<int> m = std::none();

    if m.is_some {
        int _ = m.val;
    }

    int v2;
    if m.is_some {
        v2 = m.val;
    } else {
        v2 = 42;
    }

    int v2;
    if m.is_some {
        v2 = m.val;
    } else {
        CRASH "uh oh!";
    }

    int v = 1;
    int* p = &v;

    std::Maybe<int*> mut p2 = std::none();
    std::assert_none(p2);

    p2 = std::some(&v);
    int* v = std::assert_some(p2);

    p2 = std::some(null);

    std::Maybe<std::Nonull<int>> mut p3 = std::none();
    p3 = std::some(std::assert_nonull(&v));

    std::Nullable<int> mut p4 = std::nullable(null);
    p3 = std::nullable(&v);

    // errors
    std::Error err = std::err_create(std::ErrorType::UNSPECIFIED, "example error");
    std::Error err = .{
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

    int v;
    if res.is_ok {
        v = res.val;
    } else {
        v = 42;
    }

    int v;
    if res.is_ok {
        v = res.val;
    } else {
        v = sizeof(res.err);
    }

    int v;
    if res.is_ok {
        v = res.val;
    } else {
        CRASH `Error: {e}`;
    }

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
