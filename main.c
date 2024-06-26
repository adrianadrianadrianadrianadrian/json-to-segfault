#include "list.h"
#include "utils.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Stream
typedef struct {
    char *data;
    size_t current_position;
    size_t size;
    FILE *source;
} Stream;

Stream create_static_stream(char *input) {
    size_t input_len = strlen(input);
    List_char list = create_list_char(input_len);
    memcpy(list.data, input, input_len);
    list.size = input_len;

    return (Stream){.data = list.data,
                    .size = list.size,
                    .current_position = 0,
                    .source = NULL};
}

List_char read_file_chunk(FILE *fstream) {
    char buffer[1024];
    size_t read_amount = fread(&buffer, sizeof(char), 1024, fstream);

    if (read_amount == 0) {
        List_char chunk;
        chunk.data = NULL;
        chunk.size = 0;
        chunk.capacity = 0;
        return chunk;
    } else {
        List_char chunk = create_list_char(read_amount);
        memcpy(chunk.data, &buffer, read_amount);
        chunk.size = read_amount;
        return chunk;
    }
}

Stream create_file_stream(FILE *fstream) {
    return (Stream){
        .data = NULL, .size = 0, .current_position = 0, .source = fstream};
}

int consume_stream(Stream *s, size_t amount, char *out) {
    if (s->current_position + amount > s->size) {
        if (s->source != NULL) {
            List_char next_chunk = read_file_chunk(s->source);
            if (next_chunk.size >= amount) {
                char *data =
                    malloc(sizeof(*data) * (s->size + next_chunk.size));
                memcpy(data, s->data, s->size);
                memcpy(data + s->size, next_chunk.data, next_chunk.size);
                s->data = data;
                s->size += next_chunk.size;
                free_list_char(&next_chunk);
            } else {
                free_list_char(&next_chunk);
                return 0;
            }
        } else {
            return 0;
        }
    }

    assert(s->current_position + amount <= s->size);

    for (size_t i = 0; i < amount; ++i) {
        out[i] = s->data[s->current_position + i];
    }

    s->current_position += amount;
    return 1;
}

void stream_back(Stream *s, size_t amount) {
    assert(s->current_position >= amount);
    s->current_position -= amount;
}

int conditional_eat(Stream *stream, int (*p)(char *)) {
    char test;

    if (consume_stream(stream, 1, (char *)&test)) {
        if (p(&test))
            return 1;

        stream_back(stream, 1);
    }

    return 0;
}

int eat_char(Stream *stream, char c) {
    char test;

    if (consume_stream(stream, 1, (char *)&test)) {
        if (c == test)
            return 1;

        stream_back(stream, 1);
    }

    return 0;
}

void eat_whitespace(Stream *s) {
    while (conditional_eat(s, whitespace))
        ;
}

int eat_char_between_whitespace(Stream *stream, char c) {
    int result = 0;

    eat_whitespace(stream);
    result = eat_char(stream, c);

    if (result)
        eat_whitespace(stream);

    return result;
}

void display_error(Stream *s) {
    printf("%s error: %s unexpected value '%c'\n", RED, NO_COLOUR,
           s->data[s->current_position]);

    long delta = 20;
    long from_position = (long)s->current_position - delta < 0
                             ? 0
                             : (long)s->current_position - delta;
    long to_position = (long)s->current_position + delta > (long)s->size
                           ? s->size
                           : s->current_position + delta;

    printf("  |  ");
    for (long i = from_position; i < to_position; i++) {
        char c = s->data[i];
        if (i == (long)s->current_position) {
            printf("%s", BLUE);

            if (c != '\n')
                printf("%c", c);

            printf("%s", NO_COLOUR);
        } else {
            if (c != '\n')
                printf("%c", c);
        }
    }

    printf("\n  |  %s", BLUE);
    for (long i = from_position; i < (long)s->current_position - 1; i++)
        printf("~");

    printf("^\n%s", NO_COLOUR);
}

// Json
typedef enum { PARSED, NOT_PARSED, ERROR } ParseResult;

struct Json;

typedef struct KeyValuePair {
    char *key;
    struct Json *value;
} KeyValuePair;

LIST(KeyValuePair);
CREATE_LIST(KeyValuePair);
APPEND_LIST(KeyValuePair);
FREE_LIST(KeyValuePair);

typedef enum {
    OBJECT,
    STRING,
    NUMBER,
    ARRAY,
    TRUE,
    FALSE,
    J_NULL
} JsonVariantType;

typedef struct Json {
    JsonVariantType variant;
    union {
        struct {
            long value;
        } j_number;
        List_char j_string;
        struct {
            struct Json *data;
            size_t size;
            size_t capacity;
        } j_array;
        List_KeyValuePair j_object;
    } value;
} Json;

LIST(Json);
CREATE_LIST(Json);
APPEND_LIST(Json);
FREE_LIST(Json);

ParseResult parse_json(Stream *stream, Json *out);

ParseResult parse_null(Stream *stream, Json *out) {
    size_t null_size = 4;
    char test[null_size];

    if (consume_stream(stream, null_size, (char *)&test)) {
        if (!strncmp("null", (char *)&test, null_size)) {
            out->variant = J_NULL;
            return PARSED;
        } else {
            stream_back(stream, null_size);
        }
    }

    return NOT_PARSED;
}

ParseResult parse_true(Stream *stream, Json *out) {
    size_t true_size = 4;
    char test[true_size];

    if (consume_stream(stream, true_size, (char *)&test)) {
        if (!strncmp("true", (char *)&test, true_size)) {
            out->variant = TRUE;
            return PARSED;
        } else {
            stream_back(stream, true_size);
        }
    }

    return NOT_PARSED;
}

ParseResult parse_false(Stream *stream, Json *out) {
    size_t false_size = 5;
    char test[false_size];

    if (consume_stream(stream, false_size, (char *)&test)) {
        if (!strncmp("false", (char *)&test, false_size)) {
            out->variant = FALSE;
            return PARSED;
        } else {
            stream_back(stream, false_size);
        }
    }

    return NOT_PARSED;
}

ParseResult parse_number(Stream *stream, Json *out) {
    size_t original_position = stream->current_position;
    List_char s = create_list_char(100);
    ParseResult result = PARSED;
    char test;
    long sign = 1;

    if (consume_stream(stream, 1, &test)) {
        test == '-' ? sign = -1 : stream_back(stream, 1);

        while (consume_stream(stream, 1, &test)) {
            if (char_is_digit(test)) {
                append_list_char(&s, test);
            } else {
                stream_back(stream, 1);
                break;
            }
        }
    } else {
        result = NOT_PARSED;
    }

    if (result == PARSED && s.size > 0) {
        out->variant = NUMBER;
        out->value.j_number.value = sign * atol(s.data);
    } else {
        result =
            original_position != stream->current_position ? ERROR : NOT_PARSED;
    }

    free_list_char(&s);
    return result;
}

ParseResult parse_string(Stream *stream, Json *out) {
    ParseResult result = PARSED;
    char test;

    if (eat_char(stream, '"')) {
        List_char s = create_list_char(100);

        while (!eat_char(stream, '"')) {
            if (consume_stream(stream, 1, &test)) {
                append_list_char(&s, test);
            } else {
                result = ERROR;
                break;
            }
        }

        if (result == PARSED) {
            out->variant = STRING;
            out->value.j_string = s;
        } else {
            free_list_char(&s);
        }
    } else {
        result = NOT_PARSED;
    }

    return result;
}

ParseResult parse_array(Stream *stream, Json *out) {
    ParseResult result = PARSED;
    Json test;

    if (eat_char_between_whitespace(stream, '[')) {
        List_Json j = create_list_Json(100);

        while (!eat_char_between_whitespace(stream, ']')) {
            ParseResult inner_result = parse_json(stream, &test);

            if (inner_result == PARSED) {
                append_list_Json(&j, test);

                if (!eat_char_between_whitespace(stream, ',')) {
                    result = eat_char_between_whitespace(stream, ']') ? PARSED
                                                                      : ERROR;
                    break;
                }
            } else {
                result = ERROR;
                break;
            }
        }

        if (result == PARSED) {
            out->variant = ARRAY;
            out->value.j_array.size = j.size;
            out->value.j_array.capacity = j.size;
            out->value.j_array.data = j.data;
        } else {
            free_list_Json(&j);
        }
    } else {
        result = NOT_PARSED;
    }

    return result;
}

ParseResult parse_key_value_pair(Stream *stream, KeyValuePair *kvp) {
    ParseResult result = PARSED;
    Json key;

    if (parse_string(stream, &key) == PARSED) {
        assert(key.variant == STRING);
        Json *value = malloc(sizeof(*value));

        if (eat_char_between_whitespace(stream, ':')) {
            result = parse_json(stream, value);
        } else {
            result = ERROR;
        }

        if (result == PARSED) {
            size_t key_size = key.value.j_string.size;
            char *k = malloc(sizeof(*k) * (key_size + 1));
            memcpy(k, key.value.j_string.data, key_size);
            k[key_size] = '\0';
            kvp->key = k;
            kvp->value = value;
        } else {
            free(value);
        }
    } else {
        result = NOT_PARSED;
    };

    return result;
}

ParseResult parse_object(Stream *stream, Json *out) {
    ParseResult result = PARSED;
    KeyValuePair test;

    if (eat_char_between_whitespace(stream, '{')) {
        List_KeyValuePair l = create_list_KeyValuePair(100);

        while (!eat_char_between_whitespace(stream, '}')) {
            ParseResult inner_result = parse_key_value_pair(stream, &test);

            if (inner_result == PARSED) {
                append_list_KeyValuePair(&l, test);

                if (!eat_char_between_whitespace(stream, ',')) {
                    result = eat_char_between_whitespace(stream, '}') ? PARSED
                                                                      : ERROR;
                    break;
                }
            } else {
                result = ERROR;
                break;
            }
        }

        if (result == PARSED) {
            out->variant = OBJECT;
            out->value.j_object = l;
        } else {
            free_list_KeyValuePair(&l);
        }
    } else {
        result = NOT_PARSED;
    }

    return result;
}

ParseResult parse_json(Stream *s, Json *out) {
    if (s->data != NULL && s->current_position == s->size)
        return ERROR;

    ParseResult result = PARSED;

    eat_whitespace(s);

    result = parse_null(s, out);

    if (result != NOT_PARSED)
        return result;

    result = parse_true(s, out);

    if (result != NOT_PARSED)
        return result;

    result = parse_false(s, out);

    if (result != NOT_PARSED)
        return result;

    result = parse_number(s, out);

    if (result != NOT_PARSED)
        return result;

    result = parse_string(s, out);

    if (result != NOT_PARSED)
        return result;

    result = parse_array(s, out);

    if (result != NOT_PARSED)
        return result;

    result = parse_object(s, out);

    if (s->current_position != s->size && result != PARSED)
        result = ERROR;

    return result;
}

void not_pretty_print(Json *json, int depth) {
    if (depth > 0)
        depth += 2;

    int inner_depth = depth + 2;
    int previous_depth = depth - 2 >= 0 ? depth - 2 : 0;

    switch (json->variant) {
    case OBJECT:
        printf("{\n");
        for (size_t i = 0; i < json->value.j_object.size; i++) {
            printf("%*c\"%s\": ", inner_depth, ' ',
                   json->value.j_object.data[i].key);
            not_pretty_print(json->value.j_object.data[i].value, inner_depth);

            if (i != json->value.j_object.size - 1)
                printf(",\n");
        }
        printf("\n%*c}", previous_depth, ' ');
        break;
    case STRING:
        printf("\"%s\"", json->value.j_string.data);
        break;
    case NUMBER:
        printf("%ld", json->value.j_number.value);
        break;
    case ARRAY:
        printf("[\n");
        for (size_t i = 0; i < json->value.j_array.size; i++) {
            printf("%*c", inner_depth, ' ');
            not_pretty_print(&json->value.j_array.data[i], inner_depth);

            if (i != json->value.j_array.size - 1)
                printf(",\n");
        }
        printf("\n%*c]", previous_depth, ' ');
        break;
    case TRUE:
        printf("true");
        break;
    case FALSE:
        printf("false");
        break;
    case J_NULL:
        printf("null");
        break;
    }
}

// Tests
void test_null(char *input, ParseResult result) {
    Stream s = create_static_stream(input);
    Json j;

    ParseResult test_result = parse_null(&s, &j);
    assert(test_result == result);

    if (test_result == PARSED)
        assert(j.variant == J_NULL);
}

void test_true(char *input, ParseResult result) {
    Stream s = create_static_stream(input);
    Json j;

    ParseResult test_result = parse_true(&s, &j);
    assert(test_result == result);

    if (test_result == PARSED)
        assert(j.variant == TRUE);
}

void test_false(char *input, ParseResult result) {
    Stream s = create_static_stream(input);
    Json j;

    ParseResult test_result = parse_false(&s, &j);
    assert(test_result == result);

    if (test_result == PARSED)
        assert(j.variant == FALSE);
}

void test_number(char *input, long value, ParseResult result) {
    Stream s = create_static_stream(input);
    Json j;

    ParseResult test_result = parse_number(&s, &j);
    assert(test_result == result);

    if (test_result == PARSED) {
        assert(j.variant == NUMBER);
        assert(j.value.j_number.value == value);
    }
}

void test_array(char *input, ParseResult result) {
    Stream s = create_static_stream(input);
    Json j;

    ParseResult test_result = parse_array(&s, &j);
    assert(test_result == result);

    if (test_result == PARSED)
        assert(j.variant == ARRAY);
}

void test_object(char *input, ParseResult result) {
    Stream s = create_static_stream(input);
    Json j;

    ParseResult test_result = parse_object(&s, &j);
    assert(test_result == result);

    if (test_result == PARSED)
        assert(j.variant == OBJECT);
}

void test_json(char *input, ParseResult result) {
    Stream s = create_static_stream(input);
    Json j;

    assert(parse_json(&s, &j) == result);
}

void test_string(char *input, ParseResult result) {
    Stream s = create_static_stream(input);
    Json j;

    ParseResult test_result = parse_string(&s, &j);
    assert(test_result == result);

    if (test_result == PARSED)
        assert(j.variant == STRING);
}

int run_tests() {
    test_null("null", PARSED);
    test_null("nul", NOT_PARSED);
    test_null("", NOT_PARSED);

    test_true("true", PARSED);
    test_true("truue", NOT_PARSED);
    test_true("", NOT_PARSED);

    test_false("false", PARSED);
    test_false("falze", NOT_PARSED);
    test_false("", NOT_PARSED);

    test_number("1", 1, PARSED);
    test_number("2", 2, PARSED);
    test_number("0", 0, PARSED);
    test_number("-", 0, ERROR);
    test_number("", -1, NOT_PARSED);
    test_number("-1", -1, PARSED);
    test_number("-1235235", -1235235, PARSED);

    test_string("\"h ahahaah\"", PARSED);
    test_string("", NOT_PARSED);
    test_string("\"ahhahaha", ERROR);

    test_array("[],", PARSED);
    test_array("[", ERROR);
    test_array("]", NOT_PARSED);
    test_array("[1,2]", PARSED);
    test_array("[1,true,2]", PARSED);
    test_array("[1,true,2   ]", PARSED);
    test_array("[\t\n\t\n\n   ]", PARSED);
    test_array("[\t\n\t\n\ntrue,\n\"ahahha\",false,   ]", PARSED);
    test_array("[1,true,2, null, true, [], [[], [[]]]]", PARSED);
    test_array("[1,true,2, null, true, [, [[], [[]]]]", ERROR);
    test_array("", NOT_PARSED);

    test_object("{}", PARSED);
    test_object("{", ERROR);
    test_object("}", NOT_PARSED);
    test_object("{\"key\":1}", PARSED);
    test_object("{\"key\":1, \"name\":   \"adrian\", \"arr\": [1,2]}", PARSED);
    test_object("{\"key\":1, \"name:   \"adrian\", \"arr\": [1,2]}", ERROR);

    test_json("", ERROR);
    test_json("1", PARSED);
    test_json("true", PARSED);
    test_json("false", PARSED);
    test_json("null", PARSED);
    test_json("[]", PARSED);
    test_json("{}", PARSED);
    test_json("\"asfsadf\"", PARSED);
    test_json("{\"test\":   [2s]}", ERROR);
    test_json("{\"test\":   [2]}", PARSED);
    test_json("{\"test\": {\n\t}}", PARSED);
    test_json("[{\"test\":[\n\n\t\"ahah\",\n\t\r\"test\",2]}]", PARSED);

    return 1;
}

int main(int argc, char **argv) {
    assert(run_tests());

    if (argc < 2) {
        printf("Gimmi some json");
        return -1;
    }

    FILE *f = fopen(argv[1], "r");
    if (f == NULL) {
        printf("File '%s' not found\n", argv[1]);
        return -1;
    }

    Stream s = create_file_stream(f);
    Json j;
    ParseResult result = parse_json(&s, &j);

    if (result == PARSED) {
        not_pretty_print(&j, 0);
    } else {
        display_error(&s);
    }

    return 0;
}
