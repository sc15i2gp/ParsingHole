#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef uint32_t u32;
typedef int32_t  s32;
typedef uint64_t u64;
typedef int64_t  s64;
typedef float    f32;
typedef double   f64;

const char test_json[] =
"{\n"
"\"Hello\":\"500\",\n"
"\"Goodbye\":100,\n"
"\"Weehee\":10.433,\n"
"\"Obj\":{\"new\":\"ci\", \"old\":{\"a\":1, \"b\":2}},\n"
"\"Arr\":[0, 1, \"2\", \"three\", 4.0]\n"
"\"Arr\":[0, 1, [2, 3, 4], \"2\", \"three\", 4.0, {\"Me\":5, \"You\": \"6.0\", \"Them\":[0, 1, \"2\"]}]\n"
"}";

//TODO:
//  - Validate
//      - No duplicate keys
//  - Access members
//  - Stress test
//  - Perf test

u32 is_letter(char c)
{
    c = c & (~0x20);
    return c >= 'A' && c <= 'Z';
}

u32 is_number(char c)
{
    return c >= '0' && c <= '9';
}

u32 is_number_char(char c)
{
    if(is_number(c)) return 1;
    switch(c)
    {
        case '+':
        case '-':
        case '.':
            return 1;
        default:
            return 0;
    }
}

u32 is_whitespace(char c)
{
    switch(c)
    {
        case ' ':
        case '\n':
        case '\t':
        case '\r':
            return 1;
        default:
            return 0;
    }
}

typedef struct
{
    char *cstr;
    u32  len;
} string;

void print_string(string s)
{
    printf("%.*s", s.len, s.cstr);
}

string init_cstring(const char *cstr)
{
    string s;
    s.len  = strlen(cstr);
    s.cstr = malloc(s.len);
    memcpy(s.cstr, cstr, s.len);

    return s;
}

string init_string(const char *cstr, u32 len)
{
    string s;
    s.len = len;
    s.cstr = malloc(s.len);
    memcpy(s.cstr, cstr, s.len);

    return s;
}

string init_static_cstring(const char *cstr)
{
    string s;
    s.len = strlen(cstr);
    s.cstr = cstr;

    return s;
}

string init_static_string(const char *cstr, u32 len)
{
    string s;
    s.len = len;
    s.cstr = cstr;

    return s;
}

typedef enum
{
    TOKEN_NONE,
    TOKEN_WORD,
    TOKEN_NUMBER,
    TOKEN_COMMA = ',',
    TOKEN_COLON = ':',
    TOKEN_OBRACK = '[',
    TOKEN_CBRACK = ']',
    TOKEN_OBRACE = '{',
    TOKEN_CBRACE = '}',
    TOKEN_END,
    TOKEN_COUNT
} json_token_type;

typedef struct
{
    json_token_type type;
    const char *loc;
    u32 len;
    f64 num_val;
} json_token;

void print_json_token_info(json_token *t)
{
    const char *type;
    switch(t->type)
    {
        case TOKEN_WORD:   type = "Word"; break;
        case TOKEN_NUMBER: type = "Number"; break;
        case TOKEN_END:    type = "End"; break;
        case TOKEN_COMMA:  type = "Comma"; break;
        case TOKEN_COLON:  type = "Colon"; break;
        case TOKEN_OBRACK: type = "Obrack"; break;
        case TOKEN_CBRACK: type = "Cbrack"; break;
        case TOKEN_OBRACE: type = "Obrace"; break;
        case TOKEN_CBRACE: type = "Cbrace"; break;
        default:           type = "None"; break;
    }
    printf("TOKEN (%.*s): Type(%s) Loc(%p) Len(%u) Val(%f) Next(%.10s)", t->len, t->loc, type, t->loc, t->len, t->num_val, t->loc);
}

void print_json_token(json_token *t)
{
    switch(t->type)
    {
        case TOKEN_WORD:
        {
            printf("%.*s", t->len, t->loc);
            break;
        }
        case TOKEN_NUMBER:
        {
            printf("%f", t->num_val);
            break;
        }
        case TOKEN_END:
        {
            printf("<end>");
            break;
        }
        case TOKEN_COMMA:
        {
            printf(",");
            break;
        }
        case TOKEN_COLON:
        {
            printf(":");
            break;
        }
        case TOKEN_OBRACK:
        {
            printf("[");
            break;
        }
        case TOKEN_CBRACK:
        {
            printf("]");
            break;
        }
        case TOKEN_OBRACE:
        {
            printf("{");
            break;
        }
        case TOKEN_CBRACE:
        {
            printf("}");
            break;
        }
        default:
        {
            printf("<n/a>");
            break;
        }
    }
}

void read_json_token(json_token *dst, const char *src, const char *end)
{
    //Ignore whitespace
    for(; is_whitespace(*src); src += 1);

    if(src >= end)
    {
        dst->type = TOKEN_END;
        dst->loc  = end;
        dst->len  = 0;
    }
    else
    switch(*src)
    {
        case ',':
        case ':':
        case '[':
        case ']':
        case '{':
        case '}':
        {
            //Punctuation
            dst->type = *src;
            dst->loc  = src;
            dst->len  = 1;
            break;
        }
        case '"':
        {
            //Word
            dst->type = TOKEN_WORD;
            dst->loc  = src;
            
            const char *c = src + 1;
            for(; *c != '"'; c += 1);
            dst->len = (c - src) + 1;
            break;
        }
        default:
        {
            //Number
            if(is_number_char(*src))
            {
                dst->type = TOKEN_NUMBER;
                dst->loc = src;

                const char *c = src + 1;
                for(; is_number_char(*c); c += 1);

                dst->len     = c - src;
                dst->num_val = atof(dst->loc);
            }
            //Bogus
            else
            {
                dst->type = TOKEN_NONE;
                dst->loc  = src;
                dst->len  = 0;
            }
        }
    }
}

typedef struct
{
    const char *src_begin;
    const char *src_end;

    const char *src_loc;
} json_tokeniser;

void init_json_tokeniser(json_tokeniser *tokeniser, const char *src, u32 src_len)
{
    tokeniser->src_begin = src;
    tokeniser->src_end   = src + src_len;
    tokeniser->src_loc   = src;
}

json_token next_json_token(json_tokeniser *tokeniser)
{
    json_token t;

    const char *loc = tokeniser->src_loc;
    const char *end = tokeniser->src_end;

    read_json_token(&t, loc, end);
    tokeniser->src_loc = t.loc + t.len;

    return t;
}

json_token lookahead_json_token(json_tokeniser *tokeniser)
{
    json_token t;

    const char *loc = tokeniser->src_loc;
    const char *end = tokeniser->src_end;

    read_json_token(&t, loc, end);

    return t;
}

typedef struct json_obj json_obj;

typedef enum
{
    JSON_NONE,
    JSON_NUM,
    JSON_STR,
    JSON_ARR,
    JSON_OBJ,
    JSON_TYPE_COUNT
} json_val_type;

typedef struct
{
    json_val_type type;
    union
    {
        f64      num;
        string   str;
        json_obj *obj;
    };
} json_val;

void print_json_val(json_val *val)
{
    switch(val->type)
    {
        case JSON_STR:
        {
            print_string(val->str);
            break;
        }
        case JSON_NUM:
        {
            printf("%f", val->num);
            break;
        }
        case JSON_OBJ:
        {
            printf("<obj %p>", val->obj);
            break;
        }
        case JSON_ARR:
        {
            printf("<array %p>", val->obj);
            break;
        }
    }
}

typedef struct
{
    string   name;
    json_val value;
} json_pair;

void print_json_pair(json_pair *p)
{
    print_string(p->name);
    printf(":");
    print_json_val(&p->value);
}

//Obj with pairs with no names is an array
struct json_obj
{
    u32 num_pairs;
    json_pair *pairs;
};

typedef struct
{
    u32 capacity;
    u32 num_objs;
    json_obj *objs;
} json_obj_list;

json_obj *add_json_obj(json_obj_list *obj_list)
{
    u32 num_objs = obj_list->num_objs;
    u32 capacity = obj_list->capacity;
    json_obj *objs = obj_list->objs;
    if(num_objs == capacity)
    {
        //TODO
    }
    json_obj *obj = &objs[num_objs];
    obj->num_pairs = 0;
    obj->pairs = NULL;

    obj_list->capacity = capacity;
    obj_list->num_objs = num_objs + 1;
    obj_list->objs = objs;

    return obj;
}

void json_parse_error(json_token *t)
{
    printf("PARSE ERROR AT ");
    print_json_token_info(t);
    printf("\n");
    exit(-1);
}

void count_obj_pairs(json_obj_list *obj_list, json_tokeniser *jt);

void count_arr_vals(json_obj_list *obj_list, json_tokeniser *jt)
{
    json_token t = next_json_token(jt);
    if(t.type != TOKEN_OBRACK) json_parse_error(&t);

    json_obj *parent = add_json_obj(obj_list);
    for(; t.type != TOKEN_CBRACK && t.type != TOKEN_END; t = next_json_token(jt))
    {
        json_token lh = lookahead_json_token(jt);
        if(lh.type == TOKEN_OBRACE)
        {
            count_obj_pairs(obj_list, jt);
        }
        else if(lh.type == TOKEN_OBRACK)
        {
            count_arr_vals(obj_list, jt);
        }
        else
        {
            t = next_json_token(jt);
            switch(t.type)
            {
                case TOKEN_WORD:
                case TOKEN_NUMBER:
                break;
                default: json_parse_error(&t);
            }
        }

        parent->num_pairs += 1;
    }
}

void count_obj_pairs(json_obj_list *obj_list, json_tokeniser *jt)
{
    json_token t = next_json_token(jt);
    if(t.type != TOKEN_OBRACE) json_parse_error(&t);

    json_obj *parent = add_json_obj(obj_list);
    //For each pair in obj
    for(t = next_json_token(jt); t.type != TOKEN_CBRACE && t.type != TOKEN_END; t = next_json_token(jt))
    {
        if(t.type != TOKEN_WORD) json_parse_error(&t);
        t = next_json_token(jt);
        if(t.type != TOKEN_COLON) json_parse_error(&t);
        json_token lh = lookahead_json_token(jt);
        if(lh.type == TOKEN_OBRACE)
        {
            count_obj_pairs(obj_list, jt);
        }
        else if(lh.type == TOKEN_OBRACK)
        {
            count_arr_vals(obj_list, jt);
        }
        else
        {
            t = next_json_token(jt);
            switch(t.type)
            {
                case TOKEN_WORD:
                case TOKEN_NUMBER:
                break;
                default: json_parse_error(&t);
            }
        }

        lh = lookahead_json_token(jt);
        if(lh.type == TOKEN_COMMA) t = next_json_token(jt);

        parent->num_pairs += 1;
    }
}

json_obj_list count_json_objs(const char *json, u32 json_len)
{
    json_tokeniser jt;
    init_json_tokeniser(&jt, json, json_len);

    json_obj_list obj_list = {};
    obj_list.capacity = 1024;
    obj_list.objs = malloc(obj_list.capacity * sizeof(json_obj));
    count_obj_pairs(&obj_list, &jt);

    return obj_list;
}

u32 parse_json_obj(json_tokeniser *jt, json_obj_list *obj_list, u32 parsed_objs);

u32 parse_json_arr(json_tokeniser *jt, json_obj_list *obj_list, u32 parsed_objs)
{
    json_token t = next_json_token(jt); //OBRACK

    json_obj *dst = &obj_list->objs[parsed_objs];
    json_pair *dst_pair = &dst->pairs[0];
    parsed_objs += 1;

    for(; t.type != TOKEN_CBRACK; t = next_json_token(jt))
    {
        json_token lh = lookahead_json_token(jt);
        if(lh.type == TOKEN_OBRACE)
        {
            dst_pair->value.type = JSON_OBJ;
            dst_pair->value.obj = &obj_list->objs[parsed_objs];
            parsed_objs = parse_json_obj(jt, obj_list, parsed_objs);
        }
        else if(lh.type == TOKEN_OBRACK)
        {
            dst_pair->value.type = JSON_ARR;
            dst_pair->value.obj = &obj_list->objs[parsed_objs];
            parsed_objs = parse_json_arr(jt, obj_list, parsed_objs);
        }
        else
        {
            t = next_json_token(jt);
            switch(t.type)
            {
                case TOKEN_WORD:
                {
                    dst_pair->value.type = JSON_STR;
                    dst_pair->value.str = init_string(t.loc, t.len);
                    break;
                }
                case TOKEN_NUMBER:
                {
                    dst_pair->value.type = JSON_NUM;
                    dst_pair->value.num = t.num_val;
                    break;
                }
            }
        }

        dst->num_pairs += 1;
        dst_pair += 1;
    }

    return parsed_objs;
}

u32 parse_json_obj(json_tokeniser *jt, json_obj_list *obj_list, u32 parsed_objs)
{
    json_token t = next_json_token(jt); //OBRACE

    json_obj *dst = &obj_list->objs[parsed_objs];
    json_pair *dst_pair = &dst->pairs[0];
    parsed_objs += 1;
    for(t = next_json_token(jt); t.type != TOKEN_CBRACE; t = next_json_token(jt))
    {
        //t = WORD
        dst_pair->name = init_string(t.loc, t.len);

        t = next_json_token(jt); //COLON

        json_token lh = lookahead_json_token(jt); //lh value
        if(lh.type == TOKEN_OBRACE)
        {
            dst_pair->value.type = JSON_OBJ;
            dst_pair->value.obj = &obj_list->objs[parsed_objs];
            parsed_objs = parse_json_obj(jt, obj_list, parsed_objs);
        }
        else if(lh.type == TOKEN_OBRACK)
        {
            dst_pair->value.type = JSON_ARR;
            dst_pair->value.obj = &obj_list->objs[parsed_objs];
            parsed_objs = parse_json_arr(jt, obj_list, parsed_objs);
        }
        else
        {
            t = next_json_token(jt); //value
            switch(t.type)
            {
                case TOKEN_WORD:
                {
                    dst_pair->value.type = JSON_STR;
                    dst_pair->value.str = init_string(t.loc, t.len);
                    break;
                }
                case TOKEN_NUMBER:
                {
                    dst_pair->value.type = JSON_NUM;
                    dst_pair->value.num = t.num_val;
                    break;
                }
            }
        }

        lh = lookahead_json_token(jt);
        if(lh.type == TOKEN_COMMA) t = next_json_token(jt);

        dst->num_pairs += 1;
        dst_pair += 1;
    }

    return parsed_objs;
}

void parse_json(const char *json, u32 json_len, json_obj_list *obj_list)
{
    u32 total_pairs = 0;
    for(u32 o = 0; o < obj_list->num_objs; o += 1)
    {
        total_pairs += obj_list->objs[o].num_pairs;
    }
    json_pair *pair_buffer = malloc(total_pairs * sizeof(json_pair));

    u32 p = 0;
    for(u32 o = 0; o < obj_list->num_objs; o += 1)
    {
        json_obj *obj = &obj_list->objs[o];
        obj->pairs = &pair_buffer[p];
        p += obj->num_pairs;
        obj->num_pairs = 0;
    }

    json_tokeniser jt;
    init_json_tokeniser(&jt, json, json_len);

    parse_json_obj(&jt, obj_list, 0);
}

typedef struct
{
    u32 pairs_left;
    json_val_type type;
    json_obj *obj;
} json_stack_entry;

typedef struct
{
    u32 size;
    json_stack_entry *stack;
} json_print_stack;

void push_jps(json_print_stack *stack, json_obj *obj, json_val_type type)
{
    json_stack_entry *dst = &stack->stack[stack->size];
    dst->pairs_left = obj->num_pairs;
    dst->obj = obj;
    dst->type = type;
    stack->size += 1;
}

void pop_jps(json_print_stack *stack)
{
    stack->size -= 1;
}

json_stack_entry *top_jps(json_print_stack *stack)
{
    json_stack_entry *ret = &stack->stack[stack->size - 1];
    return ret;
}

void print_indent(u32 indent)
{
    for(u32 i = 0; i < indent; i += 1) printf("  ");
}

void print_parsed_json(json_obj_list *json)
{
    json_print_stack stack;
    stack.size = 0;
    stack.stack = malloc(json->num_objs * sizeof(json_stack_entry));
    push_jps(&stack, &json->objs[0], JSON_OBJ);
    
    json_val_type prev_type = JSON_OBJ;
    u32 indent = 0;
    printf("{\n");
    while(stack.size > 0)
    {
        json_stack_entry *t = top_jps(&stack);
        json_pair *pair = &(t->obj->pairs[t->obj->num_pairs - t->pairs_left]);
        json_val_type val_type = pair->value.type;
        json_val_type current_type = t->type;
        
        if(t->pairs_left == 0)
        {
            pop_jps(&stack);
            t = top_jps(&stack);
            if(current_type == JSON_OBJ)
            {
                print_indent(indent);
                printf("}");
                if(stack.size > 0 && t->pairs_left > 1) printf(",");
                printf("\n");
                prev_type = JSON_OBJ;
            }
            else if(current_type == JSON_ARR)
            {
                if(prev_type == JSON_OBJ)
                print_indent(indent);

                printf("]");
                if(stack.size > 0 && t->pairs_left > 1) printf(",");
                printf("\n");
                prev_type = JSON_ARR;
            }
            if(stack.size > 0)
            {
                t->pairs_left -= 1;
            }
            indent -= 1;
        }
        else
        {
            if(val_type == JSON_OBJ)
            {
                push_jps(&stack, pair->value.obj, JSON_OBJ);

                if(current_type == JSON_OBJ)
                {
                    print_indent(indent);
                    print_string(pair->name);
                    printf(":");
                }
                printf("{\n");

                current_type = JSON_OBJ;
                indent += 1;
            }
            else if(val_type == JSON_ARR)
            {
                push_jps(&stack, pair->value.obj, JSON_ARR);

                if(current_type == JSON_OBJ)
                {
                    print_indent(indent);
                    print_string(pair->name);
                    printf(":");
                }
                printf("[");
                current_type = JSON_ARR;
                indent += 1;
            }
            else
            {
                if(current_type == JSON_OBJ) 
                {
                    print_indent(indent);
                    print_json_pair(pair);
                    if(t->pairs_left > 1) printf(",");
                    printf("\n");
                }
                else if(current_type == JSON_ARR)
                {
                    print_json_val(&pair->value);
                    if(t->pairs_left > 1) printf(",");
                    prev_type = val_type;
                }
                t->pairs_left -= 1;
            }
        }
    }
}

int main()
{
    const char *cstr = "\"Hello\": 10.0\n";
    string hw = init_static_cstring(cstr);
    print_string(hw);

    const char *json = test_json;
    u32 json_len = strlen(json);

    json_tokeniser jt;
    init_json_tokeniser(&jt, json, json_len);

    json_token t;
    for(t = next_json_token(&jt); t.type != TOKEN_END; t = next_json_token(&jt))
    {
        print_json_token(&t);
    }
    printf("\n");
    print_json_token(&t);
    printf("\n");

    printf("Counting...\n");
    json_obj_list obj_list = count_json_objs(json, json_len);
    printf("Counted.\n");
    printf("Num objs: %u\n", obj_list.num_objs);
    printf("Pairs[0]: %u\n", obj_list.objs[0].num_pairs);
    printf("Pairs[1]: %u\n", obj_list.objs[1].num_pairs);
    printf("Pairs[2]: %u\n", obj_list.objs[2].num_pairs);
    printf("Pairs[3]: %u\n", obj_list.objs[3].num_pairs);

    printf("Parsing...\n");
    parse_json(json, json_len, &obj_list);
    printf("Parsed.\n");

    printf("\n");
    for(u32 o = 0; o < obj_list.num_objs; o += 1)
    {
        json_obj *obj = &obj_list.objs[o];
        printf("Obj[%u] %p Num pairs %u:\n", o, obj, obj->num_pairs);
        for(u32 p = 0; p < obj->num_pairs; p += 1)
        {
            print_json_pair(&obj->pairs[p]);
            printf("\n");
        }
        printf("\n");
    }
    printf("Parsed JSON:\n");
    print_parsed_json(&obj_list);

    return 0;
}
