// single-line comment

/*
    multi-line comment

    /* nested multi-line comments! */

    note: the end of the above "inner" comment didn't close the "outer" one.
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
    int[] v = []{ 1, 2, 3 };
    int*  v = []{ 1, 2, 3 };

    int? v0 = v[0];
    int? v0 = *v;

    //

    int[3] v = []{1, 2, 3};

    int v0 = v[0];

    //

    std::Array<int> v = std::array_create([]{ 1, 2, 3 });
    uint len = v.length;
    int[] data = v.data;

    int? v0 = v.get(0);

    // strings
    std::String v = "hello";

    // c strings
    char*  v = { 'hello' };
    char*  v = { 'h', 'e', 'l', 'l', 'o' };

    char[] v = []{ 'hello' };
    char[] v = []{ 'h', 'e', 'l', 'l', 'o' };

    // template strings
    std::String v = `{v}, world`;

    // optionals (ie. nullables)
    int? v = 0;
    int? v = null;

    if v {
        int _ = v;
    }

    int v2 = v else 42;
    int v2 = v else { CRASH "uh oh!" };

    int v = 1;
    int* p = &v;

    int*? p2 = null;
    p2 = &v;

    int* v = std::assert_some(p2);
    std::assert_none(p2);

    // errors
    std::Error err = std::err_create(std::ErrorType::UNSPECIFIED, "example error");
    std::Error err = {
        .type = std::ErrorType::UNSPECIFIED,
        .msg  = "example error",
    };

    // results
    int! res = 0;
    int! res = err;

    if (let val = res) {
        int v = val;
    } catch err {
        std::Error e = err
    }

    int v = res else 42;
    int v = res catch e { CRASH `Error: {e}`; };

    int v = std::assert_ok(res);
    std::Error err = std::assert_err(res);

    //

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
struct CustomString {
    uint  length;
    char* chars;
}

globaltag CustomErrorType {
    MY_CUSTOM_ERR,
}

struct CustomError {
    CustomErrorType  type;
    CustomString?    msg;
}

void no_std() {
    CustomString v = "hi, mom";

    CustomError err = {
        .type = CustomErrorType::MY_CUSTOM_ERR,
        .msg  = "custom error",
    };

    CustomString!CustomError res = v;
    CustomString!CustomError res = err;

    CustomString v = res else "";

    if (let res) {
        CustomString v = res;
    } catch err {
        CustomError e = err;
    }
}
