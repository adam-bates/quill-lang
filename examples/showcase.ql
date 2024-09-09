// single-line comment

/*
    multi-line comment
*/

// import statements
import std;

/*
    entry-point to the program

    There's a couple options for what main can look like.

    return type can be one of:
    - void
    - int
    - std::ExitCode

    params can be either:
    - ()
    - (String[] args)
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
    int[] v = []{ 1, 2, 3 }; // <-- prefixed by the length
    int*  v =   { 1, 2, 3 }; // <-- no length, just the raw data

    // strings
    std::String v = "hello";

    char[] v = []{ 'hello' };
    char[] v = []{ 'h', 'e', 'l', 'l', 'o' };

    /*
    note:
    std::String looks like:
    `struct String { uint length; char* chars; }`

    char[] has a .length of type uint, followed by characters in memory

    Thus they are interchangable for free
    */
    char[] v = "hello"
    std::String v = []{ 'hello' };
    std::String v = []{ 'h', 'e', 'l', 'l', 'o' };

    // template strings
    std::String v = `{v}, world`;

    // c strings
    char*  v = { 'hello' };
    char*  v = { 'h', 'e', 'l', 'l', 'o' };

    // optionals (ie. nullables)
    int? v = 0;
    int? v = null;

    int v2 = v else 42;
    int v2 = if v { v } else 42;

    bool v2 = v; <-- (int?)0 infers to true, as it is not null
    bool v2 = 0; <-- 0 infers as false, as is usually expected

    // you might see something weird here...
    // but the compiler tries to flag when you're doing things that may be unexpected
    int? v = 0;
    bool b1 = (bool)v;          // true
    bool b2 = (bool)(v else 0); // false

    // errors
    std::Error err = std::err_create(std::ErrorType::UNSPECIFIED, "example error");
    std::Error err = {
        .type = std::ErrorType::UNSPECIFIED,
        .msg  = "example error",
    };

    // results (union T or Error)
    std::Result<int> res = std::res_ok(0);
    std::Result<int> res = { .val = 0 };

    std::Result<int> res = std::res_err_create(std::ErrorType::UNSPECIFIED, "example error");
    std::Result<int> res = std::res_err(err);
    std::Result<int> res = { .err = err };

    int v = 3;
    switch v {
        case 1, 2, 3 {
            //
        }
        else {
            //
        }
    }

    int v = res else 0;

    if res {
        int v = res.val;
    } else {
        int err = res.err;
    }

    // for-loops
    for i in 0..5 {
        // i = 0, 1, 2, 3, 4
    }

    int[] fibs = []{ 1, 1, 3, 5, 8 };
    for i, n in fibs {
        // i = 0, 1, 2, 3, 4
        // n = 1, 1, 3, 5, 8
    }

    // while-loops
    while false { }

    int? x = 3;
    while x { // <-- (int?)0 is true, as the optional type only returns false for null
        x -= 1;
        if x < 0 { x = null; }
    }

    int x = 3;
    while (bool)x { // <-- 0 is casted as false, all others are true
        x -= 1;
    }

    // IO
    std::io::println("Hello, world!");

    // panics
    CRASH "example panic";

    // TODO: figure out if anything is missing, add here
}

/*
    std library is optional
    language features will also work with your own custom duck-types
    the stipulation is they have to look the same as the std library type
*/
void no_std();

struct CustomString {
    uint  length;
    char* chars;
}

globaltag CustomErrorType {
    MY_CUSTOM_ERR,
}

struct CustomError {
    CustomErrorType type;
    CustomString    msg;
}

union CustomResult<T> {
    T val;
    CustomError err;
}

void no_std() {
    CustomString v = "hi, mom";

    CustomError err = {
        .type = CustomErrorType::MY_CUSTOM_ERR,
        .msg  = "custom error",
    };

    CustomResult<CustomString> res = { .val = "hello, again" };

    CustomString v = res else "";

    if res {
        CustomString v = res.val;
    } else {
        CustomError v = res.err;
    }
}
