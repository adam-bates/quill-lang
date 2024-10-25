import std/io;

void main() {
    let mut sum = 0;

    foreach i in 0..5 {
        if i > 0 {
            io::print("+ ");
        } else {
            io::print("  ");
        }
        io::println(`{i}`);

        sum += i;
    }

    io::println(`= {sum}`);
}
