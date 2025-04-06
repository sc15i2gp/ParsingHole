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

    // Use case - Testing and traversing for values
    // Use case - JSON structure known, no need for testing.

    u32 root = find_root_json_object(&parsed_json);  
    u32 num_vals = get_num_json_values(root, &parsed_json);

    printf("Checking root object pairs...\n");
    for(u32 i = 0; i < num_vals; i += 1)
    {
        u32 ith_key = get_json_object_key(root, i, &parsed_json);
        u32 ith_val = get_json_value(root, i, &parsed_json);
        
        printf("Root[%u]: Key = "); print_json_key(ith_key, &parsed_json);
        printf(", Value = ");       print_json_value(ith_val, &parsed_json);
        printf("\n");
    }

    printf("Checking value retrieval...\n");
    for(u32 i = 0; i < num_vals; i += 1)
    {
        u32 ith_key = get_json_object_key(root, i, &parsed_json);
        u32 ith_val = get_json_value(root, i, &parsed_json);

        printf("Root[%u]: Key = "); print_json_key(ith_key, &parsed_json);
        printf(", Value = ");
        if(is_json_value_number(ith_val, &parsed_json))
        {
            f64 f = get_json_value_number(ith_val, &parsed_json);
            printf("(NUMBER)%f", f);
        }
        if(is_json_value_bool(ith_val, &parsed_json))
        {
            u8 u = get_json_value_bool(ith_val, &parsed_json);
            printf("(BOOL)%u", u);
        }
        if(is_json_value_null(ith_val, &parsed_json))
        {
            printf("(NULL)null");
        }
        if(is_json_value_string(ith_val, &parsed_json))
        {
            json_string str = get_json_value_string(ith_val, &parsed_json);
            printf("(STRING)\"%.*s\"", str.size, str.chars);
        }
        if(is_json_value_object(ith_val, &parsed_json))
        {
            u32 obj = get_json_value_object(ith_val, &parsed_json);
            printf("(OBJECT)%u", obj);
        }
        if(is_json_value_array(ith_val, &parsed_json))
        {
            u32 arr = get_json_value_array(ith_val, &parsed_json);
            printf("(ARRAY)%u", arr);
        }

        printf("\n");
    }

    // Get root's "Obj" object value and print its values
    json_string key = to_json_string("Obj");
    u32 obj = find_json_value(root, key, &parsed_json);
    if(json_value_exists(obj))
    {
        obj = get_json_value_object(obj, &parsed_json);
        u32 obj_num_vals = get_num_json_values(obj, &parsed_json);
        printf("OBJ %u: Num vals = %u\n", obj, obj_num_vals);
        print_json_object_formatted(obj, &parsed_json, 0, 2);
        printf("\n");
    }
    else
    {
        printf("Value "); print_json_string(key);
        printf(" in object root doesn't exist!\n");
    }

    key = to_json_string("Arr");
    u32 arr = find_json_value(root, key, &parsed_json);
    if(json_value_exists(arr))
    {
        arr = get_json_value_array(arr, &parsed_json);
        u32 arr_num_vals = get_num_json_values(arr, &parsed_json);
        printf("ARR %u: Num vals = %u\n", arr, arr_num_vals);
        print_json_array_formatted(arr, &parsed_json, 0, 2);
        printf("\n");
        u32 arr_val_1 = get_json_value(arr, 1, &parsed_json);
        printf("Arr[1] = "); print_json_value(arr_val_1, &parsed_json);
        printf("\n");
    }
    else
    {
        printf("Value "); print_json_string(key);
        printf(" in object root doesn't exist!\n");
    }

    key = to_json_string("Arr[7].Them[2]");
    u32 find = find_json_value(root, key, &parsed_json);
    if(json_value_exists(find))
    {
        printf("FOUND!\n");
    }
    else
    {        
        printf("Value "); print_json_string(key);
        printf(" in object root doesn't exist!\n");
    }
    return 0;
}
