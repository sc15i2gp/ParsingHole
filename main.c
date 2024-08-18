#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef uint8_t  byte;
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
"\"Arr\":[0, 1, [2, 3, 4], {\"Hi\": \"Bye\"}, \"2\", \"three\", 4.0, {\"Me\":5, \"You\": \"6.0\", \"Them\":[0, 1, \"2\"]}]\n"
"}";

//TODO:
//  - Test json 165 bytes of data (string, numerical, obj pointers), with 1 token and 8 objs allocated 1499 bytes used
//      - With mem arena, don't need to have an obj list capacity, unreserve mem once parsed
//  - Don't store array contents as pairs
//  - Obj rep
//      - Hash map w/ pair list for ordering
//      - Hash map entries contain space for key-value pairs
//  - Repetition testing
//  - Quality pass on parse code
//      - More parse error info
//      - Better perf?
//  - Security and performance
//  - Use cases
//      - CI error log retrieval
//      - JSON manipulation (insertion, deletion etc.)
//      - Pretty printing

// =========================== Arena ==========================//

//Reserve some number of pages, commit some number of pages and extend when out of capacity
typedef struct
{
    u32 page_size;
    u32 reserved;
    u32 committed;
    u32 allocated;
    byte *buffer;
} mem_arena;

void print_arena_info(mem_arena *arena)
{
    printf("Base = %p, Reserved = %u, Committed = %u, Allocated = %u\n",
            arena->buffer, arena->reserved, arena->committed, arena->allocated);
}

void commit_mem(mem_arena *arena, u32 commit_size)
{
    byte *base = arena->buffer + arena->committed;
    commit_size = (commit_size + (arena->page_size - 1)) & ~(arena->page_size - 1);
    VirtualAlloc(base, commit_size, MEM_COMMIT, PAGE_READWRITE);
    arena->committed += commit_size;
}

void init_mem_arena(mem_arena *arena, u32 reserve_size, u32 page_size)
{
    //Init mem_arena struct and reserve reserve_size bytes
    arena->buffer = (byte*)VirtualAlloc(NULL, reserve_size, MEM_RESERVE, PAGE_READWRITE);
    arena->page_size = page_size;
    arena->reserved = reserve_size;
    arena->committed = 0;
    arena->allocated = 0;
}

byte *alloc(mem_arena *arena, u32 alloc_size)
{
    u32 new_allocated = arena->allocated + alloc_size;
    //NOTE: rn this works on page size
    if(new_allocated > arena->committed)
    {
        u32 to_commit = new_allocated - arena->committed;
        commit_mem(arena, to_commit);
    }
    byte *ptr = arena->buffer + arena->allocated;
    arena->allocated += alloc_size;
    return ptr;
}

void unalloc(mem_arena *arena, u32 unalloc_size)
{
    arena->allocated -= unalloc_size;
}

// =========================== Tokens ======================= //

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

string null_str = {NULL, 0};

void print_string(string s)
{
    printf("%.*s", s.len, s.cstr);
}

string init_cstring(const char *cstr, mem_arena *arena)
{
    string s;
    s.len  = strlen(cstr);
    s.cstr = alloc(arena, s.len);
    memcpy(s.cstr, cstr, s.len);

    return s;
}

string init_string(const char *cstr, u32 len, mem_arena *arena)
{
    string s;
    s.len = len;
    s.cstr = alloc(arena, s.len);
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

u32 string_eq(string s0, string s1)
{
    if(s0.len != s1.len) return 0;

    for(u32 i = 0; i < s0.len; i += 1)
    {
        if(s0.cstr[i] != s1.cstr[i]) return 0;
    }

    return 1;
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
    u32 len;
    const char *loc;
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
    u32 token_array_capacity;
    u32 token_array_length;
    u32 tokens_read;
    json_token *token_array;
} json_tokeniser;

void init_json_tokeniser(json_tokeniser *tokeniser, const char *src, u32 src_len, u32 capacity, mem_arena *arena)
{
    tokeniser->src_begin = src;
    tokeniser->src_end   = src + src_len;
    tokeniser->src_loc   = src;

    tokeniser->token_array_capacity = capacity;
    tokeniser->token_array_length = 0;
    tokeniser->tokens_read = 0;
    tokeniser->token_array = (json_token*)alloc(arena, capacity * sizeof(json_token));
}

void reset_json_tokeniser(json_tokeniser *tokeniser)
{
    tokeniser->src_loc = tokeniser->src_begin;
    tokeniser->token_array_length = 0;
    tokeniser->tokens_read = 0;
}

json_token next_json_token(json_tokeniser *tokeniser)
{
    if(tokeniser->tokens_read == tokeniser->token_array_length)
    {
        u32 t_len = 0;
        json_token *t;
        do
        {
            const char *loc = tokeniser->src_loc;
            const char *end = tokeniser->src_end;
            t = &tokeniser->token_array[t_len];
            read_json_token(t, loc, end);
            t_len += 1;
            tokeniser->src_loc = t->loc + t->len;
        }
        while((t_len < tokeniser->token_array_capacity) && (t->type != TOKEN_END));
        tokeniser->tokens_read = 0;
        tokeniser->token_array_length = t_len;
    }

    json_token *t = &tokeniser->token_array[tokeniser->tokens_read];
    tokeniser->tokens_read += 1;

    return *t;
}

json_token lookahead_json_token(json_tokeniser *tokeniser)
{
    json_token t;

    const char *loc = tokeniser->src_loc;
    const char *end = tokeniser->src_end;

    if(tokeniser->tokens_read == tokeniser->token_array_length)
    {
        read_json_token(&t, loc, end);
    }
    else
    {
        t = tokeniser->token_array[tokeniser->tokens_read];
    }

    return t;
}

// =================== Types ================== //

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

typedef struct
{
    string   name;
    json_val value;
} json_pair;

//Obj with pairs with no names is an array
struct json_obj
{
    u32 num_pairs;
    json_pair *pairs;
};
typedef json_obj json_arr;

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

void print_json_pair(json_pair *p)
{
    print_string(p->name);
    printf(":");
    print_json_val(&p->value);
}

u32 json_obj_has(json_obj *obj, string key)
{
    for(u32 i = 0; i < obj->num_pairs; i += 1)
    {
        if(string_eq(obj->pairs[i].name, key)) return 1;
    }
    return 0;
}

json_val get_json_val(json_obj *obj, string key)
{
    json_val val = {};
    for(u32 i = 0; i < obj->num_pairs; i += 1)
    {
        if(string_eq(obj->pairs[i].name, key))
        {
            val = obj->pairs[i].value;
        }
    }
    return val;
}

json_val get_arr_element(json_arr *arr, u32 index)
{
    return arr->pairs[index].value;
}

f64 get_num_val(json_obj *obj, string key)
{
    json_val v = get_json_val(obj, key);
    return v.num;
}

string get_str_val(json_obj *obj, string key)
{
    json_val v = get_json_val(obj, key);
    return v.str;
}

json_obj *get_json_obj(json_obj *obj, string key)
{
    json_val v = get_json_val(obj, key);
    return v.obj;
}

json_arr *get_json_arr(json_obj *obj, string key)
{
    return get_json_obj(obj, key);
}

void insert_num_val(json_obj *obj, string key, f64 num)
{
    json_pair *dst_pair = &obj->pairs[obj->num_pairs];
    dst_pair->name = key;
    dst_pair->value.type = JSON_NUM;
    dst_pair->value.num = num;
    obj->num_pairs += 1;
}

void insert_str_val(json_obj *obj, string key, string str)
{
    json_pair *dst_pair = &obj->pairs[obj->num_pairs];
    dst_pair->name = key;
    dst_pair->value.type = JSON_STR;
    dst_pair->value.str = str;
    obj->num_pairs += 1;
}

void insert_json_obj(json_obj *obj, string key, json_obj *val)
{
    json_pair *dst_pair = &obj->pairs[obj->num_pairs];
    dst_pair->name = key;
    dst_pair->value.type = JSON_OBJ;
    dst_pair->value.obj = val;
    obj->num_pairs += 1;
}

void insert_json_arr(json_obj *obj, string key, json_arr *arr)
{
    json_pair *dst_pair = &obj->pairs[obj->num_pairs];
    dst_pair->name = key;
    dst_pair->value.type = JSON_ARR;
    dst_pair->value.obj = arr;
    obj->num_pairs += 1;
}

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

// ========================== Parsing ============================== //

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
    for(; t.type != TOKEN_CBRACK;)
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

        t = next_json_token(jt);
        if(t.type == TOKEN_COMMA)
        {
            lh = lookahead_json_token(jt);
            if(lh.type == TOKEN_CBRACK) json_parse_error(&t);
        }
        else if(t.type != TOKEN_CBRACK) json_parse_error(&t);

        parent->num_pairs += 1;
    }
}

void count_obj_pairs(json_obj_list *obj_list, json_tokeniser *jt)
{
    json_token t = next_json_token(jt);
    if(t.type != TOKEN_OBRACE) json_parse_error(&t);

    json_obj *parent = add_json_obj(obj_list);
    //For each pair in obj
    for(t = next_json_token(jt); t.type != TOKEN_CBRACE; t = next_json_token(jt))
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
        if(lh.type == TOKEN_COMMA)
        {
            t = next_json_token(jt);
            lh = lookahead_json_token(jt);
            
            //t must be TOKEN_COMMA here
            if(lh.type == TOKEN_CBRACE) json_parse_error(&t);
        }

        parent->num_pairs += 1;
    }
}

json_obj_list count_json_objs(json_tokeniser *jt, mem_arena *arena)
{
    json_obj_list obj_list = {};
    obj_list.capacity = 8;
    obj_list.objs = (json_obj*)alloc(arena, obj_list.capacity * sizeof(json_obj));
    count_obj_pairs(&obj_list, jt);

    return obj_list;
}

u32 parse_json_obj(json_tokeniser *jt, json_obj_list *obj_list, u32 parsed_objs, mem_arena *arena);

u32 parse_json_arr(json_tokeniser *jt, json_obj_list *obj_list, u32 parsed_objs, mem_arena *arena)
{
    json_token t = next_json_token(jt); //OBRACK

    json_obj *dst = &obj_list->objs[parsed_objs];
    parsed_objs += 1;

    for(; t.type != TOKEN_CBRACK; t = next_json_token(jt))
    {
        json_token lh = lookahead_json_token(jt);
        if(lh.type == TOKEN_OBRACE)
        {
            insert_json_obj(dst, null_str, &obj_list->objs[parsed_objs]);
            parsed_objs = parse_json_obj(jt, obj_list, parsed_objs, arena);
        }
        else if(lh.type == TOKEN_OBRACK)
        {
            insert_json_arr(dst, null_str, &obj_list->objs[parsed_objs]);
            parsed_objs = parse_json_arr(jt, obj_list, parsed_objs, arena);
        }
        else
        {
            t = next_json_token(jt);
            switch(t.type)
            {
                case TOKEN_WORD:
                {
                    insert_str_val(dst, null_str, init_string(t.loc, t.len, arena));
                    break;
                }
                case TOKEN_NUMBER:
                {
                    insert_num_val(dst, null_str, t.num_val);
                    break;
                }
            }
        }
    }

    return parsed_objs;
}

u32 parse_json_obj(json_tokeniser *jt, json_obj_list *obj_list, u32 parsed_objs, mem_arena *arena)
{
    json_token t = next_json_token(jt); //OBRACE

    json_obj *dst = &obj_list->objs[parsed_objs];
    parsed_objs += 1;
    for(t = next_json_token(jt); t.type != TOKEN_CBRACE; t = next_json_token(jt))
    {
        //t = WORD
        string pair_name = init_string(t.loc+1, t.len-2, arena);
        if(json_obj_has(dst, pair_name)) json_parse_error(&t);

        t = next_json_token(jt); //COLON

        json_token lh = lookahead_json_token(jt); //lh value
        if(lh.type == TOKEN_OBRACE)
        {
            insert_json_obj(dst, pair_name, &obj_list->objs[parsed_objs]);
            parsed_objs = parse_json_obj(jt, obj_list, parsed_objs, arena);
        }
        else if(lh.type == TOKEN_OBRACK)
        {
            insert_json_arr(dst, pair_name, &obj_list->objs[parsed_objs]);
            parsed_objs = parse_json_arr(jt, obj_list, parsed_objs, arena);
        }
        else
        {
            t = next_json_token(jt); //value
            switch(t.type)
            {
                case TOKEN_WORD:
                {
                    insert_str_val(dst, pair_name, init_string(t.loc, t.len, arena));
                    break;
                }
                case TOKEN_NUMBER:
                {
                    insert_num_val(dst, pair_name, t.num_val);
                    break;
                }
            }
        }

        lh = lookahead_json_token(jt);
        if(lh.type == TOKEN_COMMA) t = next_json_token(jt);
    }

    return parsed_objs;
}

void parse_json_objs(json_tokeniser *jt, json_obj_list *obj_list, mem_arena *arena)
{
    u32 total_pairs = 0;
    for(u32 o = 0; o < obj_list->num_objs; o += 1)
    {
        total_pairs += obj_list->objs[o].num_pairs;
    }
    printf("Size: %u %u\n", total_pairs, sizeof(json_pair));
    json_pair *pair_buffer = (json_pair*)alloc(arena, total_pairs * sizeof(json_pair));

    u32 p = 0;
    for(u32 o = 0; o < obj_list->num_objs; o += 1)
    {
        json_obj *obj = &obj_list->objs[o];
        obj->pairs = &pair_buffer[p];
        p += obj->num_pairs;
        obj->num_pairs = 0;
    }

    parse_json_obj(jt, obj_list, 0, arena);
}

typedef struct
{
    mem_arena mem;
    json_obj_list objs;
} parsed_json;

parsed_json parse_json(const char *json, u32 json_len)
{
    parsed_json ret_json;
    init_mem_arena(&ret_json.mem, 2*1024*1024, 4096);

    json_tokeniser jt;
    init_json_tokeniser(&jt, json, json_len, 1, &ret_json.mem);

    ret_json.objs = count_json_objs(&jt, &ret_json.mem);
    reset_json_tokeniser(&jt);
    parse_json_objs(&jt, &ret_json.objs, &ret_json.mem);

    return ret_json;
}

// ============================== Printing =========================== //

typedef struct
{
    u32 pairs_left;
    u32 indent;
    json_val_type scope_type;
    json_obj *obj;
} json_stack_entry;

typedef struct
{
    u32 size;
    json_stack_entry *stack;
} json_print_stack;

void push_jps(json_print_stack *stack, json_obj *obj, json_val_type type, u32 indent)
{
    json_stack_entry *dst = &stack->stack[stack->size];
    dst->pairs_left = obj->num_pairs;
    dst->obj = obj;
    dst->scope_type = type;
    dst->indent = indent;
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
    for(u32 i = 0; i < indent; i += 1) printf(" ");
}

void print_parsed_json(parsed_json *p_json)
{
    json_obj_list *json = &p_json->objs;
    mem_arena *arena = &p_json->mem;
    u32 stack_capacity = json->num_objs * sizeof(json_stack_entry);
    json_print_stack stack;
    stack.size = 0;
    stack.stack = (json_stack_entry*)alloc(arena, stack_capacity);
    push_jps(&stack, &json->objs[0], JSON_OBJ, 0);

    printf("{\n");
    u32 obj_ended = 0;
    u32 indent_len = 2;
    while(stack.size > 0)
    {
        json_stack_entry *t = top_jps(&stack);
        json_val_type scope = t->scope_type;
        json_pair *pair = &(t->obj->pairs[t->obj->num_pairs - t->pairs_left]);
        json_val_type val_type = pair->value.type;
        
        if(scope == JSON_OBJ || (obj_ended == 1 && scope == JSON_ARR)) print_indent(t->indent);
        if(t->pairs_left > 0)
        {
            if(scope == JSON_OBJ)
            {
                printf("\"");
                print_string(pair->name);
                printf("\":");
            }
            if(val_type == JSON_OBJ)
            {
                printf("{\n");
                push_jps(&stack, pair->value.obj, JSON_OBJ, t->indent + indent_len);
            }
            else if(val_type == JSON_ARR)
            {
                printf("[");
                push_jps(&stack, pair->value.obj, JSON_ARR, t->indent + pair->name.len + 1);
            }
            else
            {
                print_json_val(&pair->value);
                t->pairs_left -= 1;
                if(t->pairs_left > 0) printf(",");
                if(scope == JSON_OBJ) printf("\n");
            }
            obj_ended = 0;
        }
        else
        {
            if(scope == JSON_OBJ) 
            {
                printf("}");
                obj_ended = 1;
            }
            else if(scope == JSON_ARR) 
            {
                printf("]");
                obj_ended = 0;
            }

            pop_jps(&stack);
            if(stack.size > 0)
            {
                t = top_jps(&stack);
                t->pairs_left -= 1;
                if(t->pairs_left > 0) printf(",");
                if(scope == JSON_OBJ || t->scope_type == JSON_OBJ) printf("\n");
            }
        }
    }
}

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
    return 0;
}
