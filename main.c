#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#ifndef _WIN32
#define _WIN32
#endif
#include "json_parse.h"

const char test_json[] =
"{\n"
"\"Hello\":\"500\",\n"
"\"Goodbye\":100,\n"
"\"Weehee\":10.433,\n"
"\"Obj\":{\"new\":\"ci\", \"old\":{\"a\":1, \"b\":2}},\n"
"\"Arr\":[0, 1, [2, 3, 4], {\"Hi\": \"Bye\"}, \"2\", \"three\", 4.0, {\"Me\":5, \"You\": \"6.0\", \"Them\":[0, 1, \"2\"]}]\n"
"}";

//TODO:
// - JSON is dogshit, just get this ready for use
//   - Handle multiple json parses
//   - Maybe tidy up idk

// =============================================================== //

int main()
{
    const char *cstr = "\"Hello\": 10.0\n";
    string hw = init_static_cstring(cstr);
    print_string(hw);
    const char *json = test_json;
    u32 json_len = strlen(json);

    parsed_json p_json = parse_json(json, json_len);
    for(u32 o = 0; o < p_json.objs.num_objs; o += 1)
    {
        json_obj *obj = &p_json.objs.objs[o];
        printf("Obj[%u] %p Num pairs %u:\n", o, obj, obj->num_pairs);
        for(u32 p = 0; p < obj->num_pairs; p += 1)
        {
            print_json_pair(&obj->pairs[p]);
            printf("\n");
        }
        printf("\n");
    }
    printf("Parsed JSON:\n");
    print_parsed_json(&p_json);
    printf("\nLooking for Hello\n");
    json_val val = get_json_val(&p_json.objs.objs[0], init_static_cstring("Hello"));
    print_json_val(&val);
    printf("\n");
    print_arena_info(&p_json.mem);
    dealloc_parsed_json(&p_json);
    print_arena_info(&p_json.mem);
    return 0;
}
