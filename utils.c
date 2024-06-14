#include "list.h"
#include <stdlib.h>
#include <string.h>

#define BLUE "\x1B[34m"
#define NO_COLOUR "\x1B[0m"
#define RED "\x1B[31m"

LIST(char);
CREATE_LIST(char);
APPEND_LIST(char);
FREE_LIST(char);
FILTER_LIST(char);

int char_is_digit(char c) {
    switch (c) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return 1;
    default:
        return 0;
    }
}

int whitespace(char *c) {
    switch (*c) {
    case ' ':
    case '\n':
    case '\t':
    case '\r':
        return 1;
    default:
        return 0;
    }
}
