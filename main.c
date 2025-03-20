#include "parse.h"

const char test_json_0[] =
"{\n"
"\"Hello\":\"500\",\n"
"\"Goodbye\":null,\n"
"\"Weehee\":10.433,\n"
"\"Obj\":{\"new\":\"ci\", \"old\":{\"a\":1, \"b\":2}},\n"
"\"Arr\":[0, 1, [2, 3, 4], {\"Hi\": \"Bye\"}, \"2\", \"three\", 4.0, {\"Me\":5, \"You\": \"6.0\", \"Them\":[0, 1, \"2\"]}]\n"
"}";

const char test_json_1[] =
"{\n"
"\"Hello\":20,\n"
"\"Goodbye\":10,\n"
"\"Hooray\":\"\",\n"
"\"Arr\": []\n"
"}";

int main(int argc, char **argv)
{
    const char *test_json = test_json_0;
    u32 test_json_size = strlen(test_json);

        set_allocation_functions(&malloc, &realloc, &free);
        json_parsed parsed_json = parse_json(test_json, test_json_size);
        print_json_parsed(&parsed_json);
    return 0;
}
