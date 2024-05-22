#include "list.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLUE "\x1B[34m"
#define NO_COLOUR "\x1B[0m"
#define RED "\x1B[31m"
#define LIST_NAME(ty) List_##ty

LIST(char);
CREATE_LIST(char);
APPEND_LIST(char);
FREE_LIST(char);

typedef struct {
  char *input;
  size_t current_position;
  size_t size;
  List_char (*read_chunk)();
} Stream;

Stream create_stream(char *input) {
  return (Stream){.input = input, .size = strlen(input), .current_position = 0};
}

int consume_stream(Stream *s, size_t amount, char *out) {
  if (s->current_position + amount > s->size) {
    // todo: read the stream in chunks via read_chuck
    return 0;
  }

  for (size_t i = 0; i < amount; ++i) {
    out[i] = s->input[s->current_position + i];
  }

  s->current_position += amount;
  return 1;
}

void stream_back(Stream *s, size_t amount) { s->current_position -= amount; }

int strcmp_s(const char *s1, const char *s2, size_t size) {
  for (size_t i = 0; i < size; i++) {
    if (s1[i] != s2[i])
      return 0;
  }

  return 1;
}

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
    if (strcmp_s("null", (char *)&test, null_size)) {
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
    if (strcmp_s("true", (char *)&test, true_size)) {
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
    if (strcmp_s("false", (char *)&test, false_size)) {
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
    if (test == '-') {
      sign = -1;
    } else {
      stream_back(stream, 1);
    }

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

  if (s.size > 0) {
    out->variant = NUMBER;
    out->value.j_number.value = sign * atol(s.data);
  } else {
    result = original_position != stream->current_position ? ERROR : NOT_PARSED;
  }

  free_list_char(&s);
  return result;
}

int eat_char(Stream *stream, char c) {
  char test;

  if (consume_stream(stream, 1, (char *)&test)) {
    if (test == c) {
      return 1;
    } else {
      stream_back(stream, 1);
    }
  }

  return 0;
}

int eat_sequential_char(Stream *stream, char c) {
  while (eat_char(stream, c))
    ;

  return 1;
}

ParseResult parse_string(Stream *stream, Json *out) {
  List_char s = create_list_char(100);
  ParseResult result = PARSED;
  char test;

  if (eat_char(stream, '"')) {
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
  List_Json j = create_list_Json(100);
  ParseResult result = PARSED;
  Json test;

  if (eat_char(stream, '[')) {
    int more_values = 0;

    while (!(eat_char(stream, ']') && !more_values)) {
      eat_sequential_char(stream, ' ');
      ParseResult inner_result = parse_json(stream, &test);

      if (inner_result == PARSED) {
        append_list_Json(&j, test);
        eat_sequential_char(stream, ' ');
        more_values = eat_char(stream, ',');
      } else {
        result = more_values || inner_result == ERROR ? ERROR : NOT_PARSED;
        break;
      }
    }
  } else {
    return NOT_PARSED;
  }

  if (result == PARSED) {
    out->variant = ARRAY;
    out->value.j_array.size = j.size;
    out->value.j_array.capacity = j.size;
    out->value.j_array.data = j.data;
  } else {
    free_list_Json(&j);
  }

  return result;
}

ParseResult parse_key_value_pair(Stream *stream, KeyValuePair *kvp) {
  ParseResult result = PARSED;
  Json key;
  Json *value = malloc(sizeof(*value));

  if (parse_string(stream, &key) == PARSED) {
    assert(key.variant == STRING);
    eat_sequential_char(stream, ' ');

    if (eat_char(stream, ':')) {
      eat_sequential_char(stream, ' ');
      result = parse_json(stream, value);
    } else {
      result = ERROR;
    }
  } else {
    result = NOT_PARSED;
  };

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

  return result;
}

ParseResult parse_object(Stream *stream, Json *out) {
  List_KeyValuePair l = create_list_KeyValuePair(100);
  ParseResult result = PARSED;
  KeyValuePair test;

  if (eat_char(stream, '{')) {
    int more_values = 0;

    while (!(eat_char(stream, '}') && !more_values)) {
      eat_sequential_char(stream, ' ');
      ParseResult inner_result = parse_key_value_pair(stream, &test);

      if (inner_result == PARSED) {
        append_list_KeyValuePair(&l, test);
        eat_sequential_char(stream, ' ');
        more_values = eat_char(stream, ',');
      } else {
        result = more_values || inner_result == ERROR || l.size == 0
                     ? ERROR
                     : NOT_PARSED;
        break;
      }
    }
  } else {
    result = NOT_PARSED;
  }

  if (result == PARSED) {
    out->variant = OBJECT;
    out->value.j_object = l;
  } else {
    free_list_KeyValuePair(&l);
  }

  return result;
}

ParseResult parse_json(Stream *s, Json *out) {
  if (s->current_position == s->size)
    return ERROR;

  ParseResult result = PARSED;

  result = parse_null(s, out);

  if (result == PARSED)
    return result;

  if (result != ERROR)
    result = parse_true(s, out);

  if (result == PARSED)
    return result;

  if (result != ERROR)
    result = parse_false(s, out);

  if (result == PARSED)
    return result;

  if (result != ERROR)
    result = parse_number(s, out);

  if (result == PARSED)
    return result;

  if (result != ERROR)
    result = parse_string(s, out);

  if (result == PARSED)
    return result;

  if (result != ERROR)
    result = parse_array(s, out);

  if (result == PARSED)
    return result;

  if (result != ERROR)
    result = parse_object(s, out);

  if (s->current_position != s->size && result != PARSED)
    result = ERROR;

  return result;
}

void not_pretty_print(Json *json) {
  switch (json->variant) {
  case OBJECT:
    printf("{");
    for (size_t i = 0; i < json->value.j_object.size; i++) {
      printf("\"%s\"", json->value.j_object.data[i].key);
      printf(":");
      not_pretty_print(json->value.j_object.data[i].value);

      if (i != json->value.j_object.size - 1)
        printf(", ");
    }
    printf("}");
    break;
  case STRING:
    printf("\"%s\"", json->value.j_string.data);
    break;
  case NUMBER:
    printf("%ld", json->value.j_number.value);
    break;
  case ARRAY:
    printf("[");
    for (size_t i = 0; i < json->value.j_array.size; i++) {
      not_pretty_print(&json->value.j_array.data[i]);

      if (i != json->value.j_array.size - 1)
        printf(", ");
    }
    printf("]");
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

void display_error(Stream *s) {
  if (s->size != s->current_position) {
    printf("%s", RED);
    printf("error: ");
    printf("%s", NO_COLOUR);
    printf("unexpected value '%c'\n", s->input[s->current_position]);

    long delta = 15;
    long from_position = (long)s->current_position - delta < 0
                             ? 0
                             : (long)s->current_position - delta;
    long to_position = (long)s->current_position + delta > (long)s->size
                           ? s->size
                           : s->current_position + delta;

    printf("  |  ");
    for (long i = from_position; i < to_position; i++) {
      if (i == (long)s->current_position) {
        printf("%s", BLUE);
        printf("%c", s->input[i]);
        printf("%s", NO_COLOUR);
      } else {
        printf("%c", s->input[i]);
      }
    }

    printf("\n");
    printf("  |  ");
    printf("%s", BLUE);
    for (long i = from_position; i < (long)s->current_position; i++)
      printf("~");

    printf("^\n");
    printf("%s", NO_COLOUR);
  }
}

void test_null(char *input, ParseResult result) {
  Stream s = create_stream(input);
  Json j;

  assert(parse_null(&s, &j) == result);
  assert(j.variant == J_NULL);
}

void test_true(char *input, ParseResult result) {
  Stream s = create_stream(input);
  Json j;

  assert(parse_true(&s, &j) == result);
  assert(j.variant == TRUE);
}

void test_false(char *input, ParseResult result) {
  Stream s = create_stream(input);
  Json j;

  assert(parse_false(&s, &j) == result);
  assert(j.variant == FALSE);
}

void test_number(char *input, long value, ParseResult result) {
  Stream s = create_stream(input);
  Json j;

  ParseResult test_result = parse_number(&s, &j);
  assert(test_result == result);
  assert(j.variant == NUMBER);

  if (test_result == PARSED)
    assert(j.value.j_number.value == value);
}

void test_array(char *input, ParseResult result) {
  Stream s = create_stream(input);
  Json j;

  assert(parse_array(&s, &j) == result);
  assert(j.variant == ARRAY);
}

void test_object(char *input, ParseResult result) {
  Stream s = create_stream(input);
  Json j;

  assert(parse_object(&s, &j) == result);
  assert(j.variant == OBJECT);
}

void test_json(char *input, ParseResult result) {
  Stream s = create_stream(input);
  Json j;

  assert(parse_json(&s, &j) == result);
}

void test_string(char *input, ParseResult result) {
  Stream s = create_stream(input);
  Json j;

  ParseResult test_result = parse_string(&s, &j);
  assert(test_result == result);
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

  test_string("\"hahahaah\"", PARSED);
  test_string("", NOT_PARSED);
  test_string("\"hahahaah", ERROR);

  test_array("[]", PARSED);
  test_array("[", ERROR);
  test_array("]", NOT_PARSED);
  test_array("[1,2]", PARSED);
  test_array("[1,true,2]", PARSED);
  test_array("[1,true,2   ]", PARSED);
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

  return 1;
}

int main(int argc, char **argv) {
  assert(run_tests());

  if (argc < 2) {
    printf("Gimmi some json plz\n");
    return -1;
  }

  Stream s = create_stream(argv[1]);
  Json j;

  ParseResult result = parse_json(&s, &j);

  if (result == PARSED) {
    not_pretty_print(&j);
    printf("\n");
  } else {
    display_error(&s);
  }

  return 0;
}
