#include <limits.h>

#include "./numbers.h"

int count_digits(int const num) {
    int n = num;
  
    if (n < 0) {
        if (n == INT_MIN) {
            n = INT_MAX;
        } else {
            n = -n;
        }
    }

    if (n < 10) { return 1; }
    if (n < 100) { return 2; }
    if (n < 1000) { return 3; }
    if (n < 10000) { return 4; }
    if (n < 100000) { return 5; }
    if (n < 1000000) { return 6; }
    if (n < 10000000) { return 7; }
    if (n < 100000000) { return 8; }
    if (n < 1000000000) { return 9; }

    /*      2147483647 is 2^31-1 - add more ifs as needed
       and adjust this final return as well. */
    return 10;
}
