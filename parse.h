#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// JSON PARSING:
//  - Library structure/API
//      - Reorder structs + funcs
//      - Make good looking public-facing API at top of header
//  - Print parsed json
//      - Align value positions in objects? (arrays already aligned)
//      - May need additional info for neatly aligning values
//          - Namely what the largest key in an object is
//  - Validation
//      - Escape characters, backslashes, no control chars in strings
//      - Numbers in valid formats
//      - Have tildes in error arrow cover the offending token
//      - No duplicate keys
//  - Tidy
//      - Reorder structs like node_list, tokenised_json etc. to be tidier
//      - Get rid of quote marks weirdness with "-2" to string lengths when copying
//  - Retrieve values
//  - Editing
//  - Control how much mem used for token array
//  - Testing
//  - Remove recursion in favour of linear functions with stack for control flow?
//  - Asserts?
//  - Strings are assumed UTF-8 - Anyone sending emoji over json is insane

typedef uint8_t  u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int32_t  s32;
typedef int64_t  s64;
typedef float    f32;
typedef double   f64;

typedef void* (*alloc_func)(u64);
typedef void* (*realloc_func)(void*,u64);
typedef void  (*dealloc_func)(void*);

alloc_func   alloc;
realloc_func resize_alloc;
dealloc_func dealloc;

u8 is_letter(unsigned char c)
{
    c &= 0b11011111;
    c -= 'A';
    return c < 26;
}

u8 is_digit(unsigned char c)
{
    c -= '0';
    return c < 10;
}

u8 is_whitespace(unsigned char c)
{
    return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

u8 is_number_char(unsigned char c)
{
    return is_digit(c) || c == '-' || c == '+' || c == '.' || c == 'E' || c == 'e';
}

// ============================== Parsing ===================================

typedef enum
{
    JSON_NONE,
    JSON_NUMBER,
    JSON_BOOL,
    JSON_NULL,
    JSON_STRING,
    JSON_OBJECT,
    JSON_ARRAY,
} json_type;

typedef struct
{
    u32   size;
    char *chars;
} json_string;

typedef struct json_object json_object;
typedef struct json_array  json_array;
typedef struct json_value  json_value;

struct json_array
{
    u32         num_values;
    json_value *values;
};

struct json_object
{
    u32          num_pairs;
    json_string *keys;
    json_value  *values;
};

struct json_value
{
    json_type type;
    union
    {
        f64         number;
        u8          boolean;
        json_string string;
        json_object object;
        json_array  array;
    };
};

typedef struct
{
    u32 num_allocd_values;
    u32 total_num_values;
    json_value *values;
} json_values_arena;

json_value *alloc_json_values(json_values_arena *arena, u32 num_to_alloc)
{
    json_value *allocation = arena->values + arena->num_allocd_values;
    arena->num_allocd_values += num_to_alloc;
    return allocation;
}

typedef struct
{
    u32 num_allocd_chars;
    u32 total_num_chars;
    char *chars;
} string_chars_arena;

char *alloc_string_chars(string_chars_arena *arena, u32 num_to_alloc)
{
    char *allocation = arena->chars + arena->num_allocd_chars;
    arena->num_allocd_chars += num_to_alloc;
    return allocation;
}

typedef struct
{
    u32 num_allocd_keys;
    u32 total_num_keys;
    json_string *keys;
} key_strings_arena;

json_string *alloc_key_strings(key_strings_arena *arena, u32 num_to_alloc)
{
    json_string *allocation = arena->keys + arena->num_allocd_keys;
    arena->num_allocd_keys += num_to_alloc;
    return allocation;
}

typedef struct
{
    json_type type;
    u32       num_values;
    u32       num_string_chars;
    u32       num_key_strings;
} json_node;

typedef struct
{
    u32        num_nodes;
    u32        cap_nodes;
    json_node *nodes;
} json_node_list;

typedef enum
{
    TOKEN_NONE,
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_BOOL,
    TOKEN_NULL,
    TOKEN_COMMA  = ',',
    TOKEN_COLON  = ':',
    TOKEN_OBRACK = '[',
    TOKEN_CBRACK = ']',
    TOKEN_OBRACE = '{',
    TOKEN_CBRACE = '}',
    TOKEN_END,
} json_token_type;

typedef struct
{
    json_token_type type;
    const char     *loc;
    u32             length;
    u32             loc_by_chars;
    u32             loc_from_end_by_chars;
    union
    {
        f64 numeric_value;
        u8  boolean_value;
    };
} json_token;

typedef struct
{
    u32         num_tokens;
    json_token *tokens;
    u32         src_size;
    const char *src;
} json_tokenised;

typedef enum
{
    JSON_STATUS_NONE,
    JSON_STATUS_TOKENISED,
    JSON_STATUS_VALID,
    JSON_STATUS_INVALID,
    JSON_STATUS_COUNTED,
    JSON_STATUS_PARSED,
} json_parse_status;

typedef struct
{
    json_parse_status status;
    u32 num_tokens_parsed;
    json_tokenised token_src;
    u32 num_nodes_consumed;
    json_node_list node_list;
    json_values_arena values_arena;
    string_chars_arena chars_arena;
    key_strings_arena keys_arena;
} json_parse_state;

// ============================== Tokenising ===================================

void print_token_type(json_token_type t)
{
    switch(t)
    {
        case TOKEN_STRING: printf("TOKEN_STRING"); break;
        case TOKEN_NUMBER: printf("TOKEN_NUMBER"); break;
        case TOKEN_BOOL:   printf("TOKEN_BOOL");   break;
        case TOKEN_NULL:   printf("TOKEN_NULL");   break;
        case TOKEN_COMMA:  printf("TOKEN_COMMA");  break;
        case TOKEN_COLON:  printf("TOKEN_COLON");  break;
        case TOKEN_OBRACK: printf("TOKEN_OBRACK"); break;
        case TOKEN_CBRACK: printf("TOKEN_CBRACK"); break;
        case TOKEN_OBRACE: printf("TOKEN_OBRACE"); break;
        case TOKEN_CBRACE: printf("TOKEN_CBRACE"); break;
        default:           printf("TOKEN_UNKNOWN");break;
    }
}

json_string copy_to_json_string_no_quotes(json_token *token, char *string_buffer)
{
    json_string string = {.size = token->length-2, .chars = string_buffer};
    for(u32 i = 1; i < token->length-1; i += 1)
    {
        string.chars[i-1] = token->loc[i];
    }
    return string;
}

void print_json_token_info(json_token *t)
{
    printf("Token: Type("); print_token_type(t->type); printf(") ");
    printf("Loc(%p), Len(%u)\n", t->loc, t->length);
}

void print_json_token(json_token *t)
{
    switch(t->type)
    {
        case TOKEN_STRING:
        {
            printf("%.*s", t->length, t->loc);
            break;
        }
        case TOKEN_NUMBER:
        {
            printf("%f", t->numeric_value);
            break;
        }
        case TOKEN_BOOL:
        {
            if(t->boolean_value) printf("true");
            else                 printf("false");
            break;
        }
        case TOKEN_NULL:
        {
            printf("null");
            break;
        }
        case TOKEN_END:
        {
            printf("<END>");
            break;
        }
        case TOKEN_COMMA:
        case TOKEN_COLON:
        case TOKEN_OBRACK:
        case TOKEN_CBRACK:
        case TOKEN_OBRACE:
        case TOKEN_CBRACE:
        {
            printf("%c", t->type);
            break;
        }
        default:
        {
            printf("<???>");
            break;
        }
    }
}

json_token read_json_token(const char *src, const char *src_start, const char *src_end)
{
    for(; is_whitespace(*src); src += 1);
    json_token token = {.loc = src, .loc_by_chars = src - src_start, .loc_from_end_by_chars = src_end - src};

    if(src >= src_end)
    {
        token.type   = TOKEN_END;
        token.loc    = src_end;
        token.length = 0;
    }
    else switch(*src)
    {
        case ',':
        case ':':
        case '[':
        case ']':
        case '{':
        case '}':
        {
            token.type   = *src;
            token.length = 1;
            break;
        }
        case '"':
        {
            // String token includes the surrounding quote marks
            token.type = TOKEN_STRING;
            const char *c = src + 1;
            for(; *c != '"'; c += 1);
            token.length = (c - src) + 1;
            break;
        }
        case 'n':
        {
            if(src[1] == 'u' && src[2] == 'l' && src[3] == 'l')
            {
                token.type = TOKEN_NULL;
                token.length = 4;
            }
            break;
        }
        case 't':
        {
            if(src[1] == 'r' && src[2] == 'u' && src[3] == 'e')
            {
                token.type          = TOKEN_BOOL;
                token.length        = 4;
                token.boolean_value = 1;
            }
            break;
        }
        case 'f':
        {
            if(src[1] == 'a' && src[2] == 'l' && src[3] == 's' && src[4] == 'e')
            {
                token.type          = TOKEN_BOOL;
                token.length        = 5;
                token.boolean_value = 0;
            }
            break;
        }
        case '-':
        default:
        {
            // Numbers always start with minus or digit
            if(is_digit(*src) || *src == '-')
            {
                token.type = TOKEN_NUMBER;
                const char *c = src + 1;
                for(; is_number_char(*c); c += 1);
                token.length = c - src;
                token.numeric_value = atof(src);
            }
        }
    }

    return token;
}

void tokenise_json(json_parse_state *parse_state, const char *src, u32 src_size)
{
    //Initially the returned token array is alloc'd at 128 tokens
    u32 token_cap      = 128;
    u32 num_tokens     = 0;
    json_token *tokens = (json_token*)alloc(token_cap * sizeof(json_token));
    
    json_token *last_read = NULL;
    const char *src_current = src;
    const char *src_end     = src + src_size;
    do
    {
        if(num_tokens >= token_cap)
        {
            token_cap *= 2;
            tokens = (json_token*)resize_alloc(tokens, token_cap * sizeof(json_token));
        }
        last_read    = &tokens[num_tokens];
        *last_read   = read_json_token(src_current, src, src_end);
        src_current  = last_read->loc + last_read->length;
        num_tokens  += 1;
    }
    while(last_read->type != TOKEN_END && last_read->type != TOKEN_NONE);

    json_tokenised tokenised_json =
    {
        .num_tokens = num_tokens,
        .tokens     = tokens,
        .src        = src,
        .src_size   = src_size
    };
    parse_state->token_src = tokenised_json;
    parse_state->status    = JSON_STATUS_TOKENISED;
}

// Gets last consumed token
json_token *current_token(json_parse_state *parse_state)
{
    // Assume no one is calling at the start of a token array
    u32 token_index = parse_state->num_tokens_parsed - 1;
    json_token *token = &parse_state->token_src.tokens[token_index];
    return token;
}

json_token *next_token(json_parse_state *parse_state)
{
    u32 token_index = parse_state->num_tokens_parsed;
    json_token *token = &parse_state->token_src.tokens[token_index];
    token_index += 1;
    parse_state->num_tokens_parsed = token_index;
    return token;
}

// Same as next_token but without considering next token parsed
json_token *lookahead_token(json_parse_state *parse_state)
{
    u32 token_index = parse_state->num_tokens_parsed;
    json_token *token = &parse_state->token_src.tokens[token_index];
    return token;
}

// ============================== Validation ===================================

void print_offending_token(json_parse_state *parse_state, json_token *offending_token)
{
    // These figures don't include trailing ellipses
    u32 max_chars_before_offending = 20;
    u32 max_chars_after_offending  = 10;

    const char *src_end = parse_state->token_src.src + parse_state->token_src.src_size;

    const char *src_info_start_loc;
    const char *src_info_end_loc;

    if(max_chars_before_offending > offending_token->loc_by_chars)
    {
        src_info_start_loc = parse_state->token_src.src;
    }
    else
    {
        src_info_start_loc = offending_token->loc - max_chars_before_offending;
    }
    if(max_chars_after_offending > (offending_token->loc_from_end_by_chars - offending_token->length))
    {
        src_info_end_loc = src_end;
    }
    else
    {
        src_info_end_loc = offending_token->loc + offending_token->length + max_chars_after_offending;
    }

    u32 src_info_str_token_loc = offending_token->loc - src_info_start_loc;

    char src_info_str[64] = {0};
    u32  src_info_str_len = 0;
    
    if(src_info_start_loc != parse_state->token_src.src)
    {
        for(u32 i = 0; i < 3; i += 1)
        {
            src_info_str[i]   = '.';
            src_info_str_len += 1;
            src_info_str_token_loc += 1;
        }
    }

    for(const char *c = src_info_start_loc; c != src_info_end_loc; c += 1)
    {
        if(*c == '\n') src_info_str[src_info_str_len] = ' ';
        else           src_info_str[src_info_str_len] = *c;

        src_info_str_len += 1;
    }

    if(src_info_end_loc != src_end)
    {
        for(u32 i = 0; i < 3; i += 1)
        {
            src_info_str[src_info_str_len] = '.';
            src_info_str_len += 1;
        }
    }

    printf("Parse error at:\n");

    // Print offending token and surrounding characters, including ellipses where appropriate
    printf("%.*s\n", src_info_str_len, src_info_str);

    // Print arrow underneath, pointing to offending token
    for(u32 i = 0; i < src_info_str_token_loc; i += 1) printf(" ");
    printf("^");
    for(u32 i = 0; i < offending_token->length; i += 1) printf("~");
    printf("\n");
}

void _json_validation_error(json_parse_state *parse_state, json_token *offending_token, json_token_type *expected_types, u32 num_expected_types)
{
    print_offending_token(parse_state, offending_token);

    // Print cause of error
    json_token_type got_type = offending_token->type;
    printf("Got: "); print_token_type(got_type);
    printf(". Expected: [");
    for(u32 i = 0; i < num_expected_types; i += 1)
    {
        print_token_type(expected_types[i]); printf(" ");
    }
    printf("]\n");
}

void json_empty_key_error(json_parse_state *parse_state, json_token *offending_token)
{
    print_offending_token(parse_state, offending_token);
    printf("You cannot have empty key strings in JSON object fields!\n");
}

#define json_validation_error(parse_state, got_token, ...) \
    json_token_type expected[] = {__VA_ARGS__}; \
    u32 num_expected = sizeof(expected)/sizeof(json_token_type); \
    _json_validation_error(parse_state, got_token, expected, num_expected)

u8 validate_json_object(json_parse_state*);
u8 validate_json_array(json_parse_state*);

u8 validate_json_value(json_parse_state *parse_state)
{
    json_token *lh = lookahead_token(parse_state);
    if(lh->type == TOKEN_OBRACE)
    {
        u8 is_value_valid = validate_json_object(parse_state);
        if(!is_value_valid) return 0;
    }
    else if(lh->type == TOKEN_OBRACK)
    {
        u8 is_value_valid = validate_json_array(parse_state);
        if(!is_value_valid) return 0;
    }
    else
    {
        json_token *token = next_token(parse_state);
        switch(token->type)
        {
            case TOKEN_STRING:
            case TOKEN_NUMBER:
            case TOKEN_BOOL:
            case TOKEN_NULL:
            break;
            default:
            {
                json_validation_error(parse_state, token, TOKEN_STRING, TOKEN_NUMBER, TOKEN_BOOL, TOKEN_NULL, TOKEN_OBRACE, TOKEN_OBRACK);
                return 0;
            }
        }
    }

    return 1;
}

u8 validate_json_pair(json_parse_state *parse_state)
{
    // Key string
    json_token *token = next_token(parse_state);
    if(token->type != TOKEN_STRING)
    {
        json_validation_error(parse_state, token, TOKEN_STRING);
        return 0;
    }
    if(token->length == 2)
    {
        json_empty_key_error(parse_state, token);
        return 0;
    }

    // Colon
    token = next_token(parse_state);
    if(token->type != TOKEN_COLON)
    {
        json_validation_error(parse_state, token, TOKEN_COLON);
        return 0;
    }

    u8 is_value_valid = validate_json_value(parse_state);
    return is_value_valid;

    return 1;
}

u8 validate_json_array(json_parse_state *parse_state)
{
    json_token *token = next_token(parse_state);
    if(token->type != TOKEN_OBRACK)
    {
        json_validation_error(parse_state, token, TOKEN_OBRACK);
        return 0;
    }

    json_token *lh = lookahead_token(parse_state);
    while(lh->type != TOKEN_CBRACK)
    {
        u8 value_valid = validate_json_value(parse_state);
        if(!value_valid) return 0;

        token = current_token(parse_state);
        lh    = lookahead_token(parse_state);
        if(lh->type == TOKEN_COMMA)
        {
            token = next_token(parse_state);
            lh    = lookahead_token(parse_state);
        }
        else if(lh->type != TOKEN_CBRACK)
        {
            json_validation_error(parse_state, lh, TOKEN_COMMA, TOKEN_CBRACK);
            return 0;
        }
    }
    if(token->type == TOKEN_COMMA)
    {
        // Array ends with comma followed by cbrack
        json_validation_error(parse_state, lh, TOKEN_NUMBER, TOKEN_STRING, TOKEN_BOOL, TOKEN_NULL);
        return 0;
    }
    token = next_token(parse_state); // Consume CBRACK token
    return 1;
}

// Object is obrace, 0 or more key-value pairs followed by commas then cbrace
u8 validate_json_object(json_parse_state *parse_state)
{
    json_token *token = next_token(parse_state);
    if(token->type != TOKEN_OBRACE)
    {
        json_validation_error(parse_state, token, TOKEN_OBRACE);
        return 0;
    }

    json_token *lh = lookahead_token(parse_state);
    while(lh->type != TOKEN_CBRACE)
    {
        u8 pair_valid = validate_json_pair(parse_state);
        if(!pair_valid) return 0;

        token = current_token(parse_state);
        lh    = lookahead_token(parse_state);
        if(lh->type == TOKEN_COMMA)
        {
            token = next_token(parse_state);
            lh    = lookahead_token(parse_state);
        }
        else if(lh->type != TOKEN_CBRACE)
        {
            json_validation_error(parse_state, lh, TOKEN_COMMA, TOKEN_CBRACE);
            return 0;
        }
    }
    if(token->type == TOKEN_COMMA)
    {
        // Object ends with comma followed by cbrace
        json_validation_error(parse_state, lh, TOKEN_NUMBER, TOKEN_STRING, TOKEN_BOOL, TOKEN_NULL);
        return 0;
    }
    token = next_token(parse_state); // Consume CBRACE token
    return 1;
}

void validate_json(json_parse_state *parse_state)
{
    if(parse_state->status != JSON_STATUS_TOKENISED)
    {
        printf("Error: Cannot validate JSON. It hasn't been tokenised yet!\n");
        return;
    }

    parse_state->num_tokens_parsed = 0;
    u8 json_validated = validate_json_object(parse_state);
    if(json_validated) parse_state->status = JSON_STATUS_VALID;
    else               parse_state->status = JSON_STATUS_INVALID;
}

// ============================== Count JSON ===================================

// Returns index of the new node
u32 push_node_to_list(json_node_list *node_list, json_type node_type)
{
    u32 num_nodes = node_list->num_nodes;
    u32 cap_nodes = node_list->cap_nodes;
    if(num_nodes >= cap_nodes)
    {
        cap_nodes *= 2;
        node_list->nodes = (json_node*)resize_alloc(node_list->nodes, cap_nodes * sizeof(json_node));
        node_list->cap_nodes = cap_nodes;
    }
    u32 index = num_nodes;
    node_list->nodes[index].type = node_type;
    node_list->num_nodes += 1;

    return index;
}

void inc_node_num_values(json_node_list *node_list, u32 node_index)
{
    node_list->nodes[node_index].num_values += 1;
}

void inc_node_num_string_chars(json_node_list *node_list, u32 node_index, u32 num_chars)
{
    node_list->nodes[node_index].num_string_chars += num_chars;
}

void inc_node_num_key_strings(json_node_list *node_list, u32 node_index)
{
    node_list->nodes[node_index].num_key_strings += 1;
}

void count_json_object_node(json_parse_state*);
void count_json_array_node(json_parse_state*);

void count_json_array_node(json_parse_state *parse_state)
{
    u32 node_index = push_node_to_list(&parse_state->node_list, JSON_ARRAY);

    json_token *token = next_token(parse_state); // Obrack
    json_token *lh    = lookahead_token(parse_state);
    while(lh->type != TOKEN_CBRACK)
    {
        inc_node_num_values(&parse_state->node_list, node_index);
        lh = lookahead_token(parse_state);
        if(lh->type == TOKEN_OBRACE)
        {
            count_json_object_node(parse_state);
        }
        else if(lh->type == TOKEN_OBRACK)
        {
            count_json_array_node(parse_state);
        }
        else
        {
            token = next_token(parse_state);
            if(token->type == TOKEN_STRING)
            {
                inc_node_num_string_chars(&parse_state->node_list, node_index, token->length-2);
            }
        }
        lh = lookahead_token(parse_state);
        if(lh->type == TOKEN_COMMA)
        {
            token = next_token(parse_state);
            lh    = lookahead_token(parse_state);
        }
    }
    token = next_token(parse_state);
}

void count_json_object_node(json_parse_state *parse_state)
{
    u32 node_index = push_node_to_list(&parse_state->node_list, JSON_OBJECT);

    json_token *token = next_token(parse_state); // Obrace
    json_token *lh    = lookahead_token(parse_state);
    while(lh->type != TOKEN_CBRACE)
    {
        inc_node_num_values(&parse_state->node_list, node_index);
        token = next_token(parse_state); // Key string
        inc_node_num_key_strings(&parse_state->node_list, node_index);
        inc_node_num_string_chars(&parse_state->node_list, node_index, token->length-2);
        token = next_token(parse_state); // Colon
        lh    = lookahead_token(parse_state);
        if(lh->type == TOKEN_OBRACE)
        {
            count_json_object_node(parse_state);
        }
        else if(lh->type == TOKEN_OBRACK)
        {
            count_json_array_node(parse_state);
        }
        else
        {
            token = next_token(parse_state);
            if(token->type == TOKEN_STRING)
            {
                inc_node_num_string_chars(&parse_state->node_list, node_index, token->length-2);
            }
        }
        lh = lookahead_token(parse_state);
        if(lh->type == TOKEN_COMMA)
        {
            token = next_token(parse_state);
            lh    = lookahead_token(parse_state);
        }
    }
    token = next_token(parse_state); // Consume cbrace
}

void count_json_nodes(json_parse_state *parse_state)
{
    if(parse_state->status != JSON_STATUS_VALID)
    {
        printf("Error: Cannot count JSON nodes. It hasn't been validated yet!\n");
        return;
    }

    u32 cap_nodes = 128;
    json_node *nodes = (json_node*)alloc(cap_nodes * sizeof(json_node));
    json_node_list node_list = {.num_nodes = 0, .cap_nodes = cap_nodes, .nodes = nodes};

    parse_state->node_list = node_list;
    parse_state->num_tokens_parsed = 0;

    count_json_object_node(parse_state);

    parse_state->status = JSON_STATUS_COUNTED;
}

// Mem needed:
//  - Buffer for json_values
//  - Buffer for chars contained in json_strings
//  - Key json_strings
void count_json_mem_requirements(json_node_list *node_list, u32 *dst_num_values, u32 *dst_num_chars, u32 *dst_num_keys)
{
    u32 num_values = 0;
    u32 num_chars  = 0;
    u32 num_keys   = 0;
    for(u32 i = 0; i < node_list->num_nodes; i += 1)
    {
        json_node *node = &node_list->nodes[i];
        num_values += node->num_values;
        num_chars  += node->num_string_chars;
        num_keys   += node->num_key_strings;
    }
    *dst_num_values = num_values;
    *dst_num_chars  = num_chars;
    *dst_num_keys   = num_keys;
}

typedef struct
{
    void *free_mem_base;
    json_object root;
} json_parsed;

json_node *get_next_node(json_parse_state *parse_state)
{
    json_node *next = &parse_state->node_list.nodes[parse_state->num_nodes_consumed];
    parse_state->num_nodes_consumed += 1;
    return next;
}

void populate_json_object(json_object*, json_parse_state*);
void populate_json_array(json_array*, json_parse_state*);

void populate_json_value(json_value *dst, json_parse_state *parse_state)
{
    json_token *token = lookahead_token(parse_state); // Value
    switch(token->type)
    {
        case TOKEN_NUMBER:
        {
            dst->type   = JSON_NUMBER;
            dst->number = token->numeric_value;
            token = next_token(parse_state);
            break;
        }
        case TOKEN_BOOL:
        {
            dst->type    = JSON_BOOL;
            dst->boolean = token->boolean_value;
            token = next_token(parse_state);
            break;
        }
        case TOKEN_STRING:
        {
            char *string_buffer = alloc_string_chars(&parse_state->chars_arena, token->length-2);
            dst->type = JSON_STRING;
            dst->string = copy_to_json_string_no_quotes(token, string_buffer);
            token = next_token(parse_state);
            break;
        }
        case TOKEN_NULL:
        {
            dst->type = JSON_NULL;
            token = next_token(parse_state);
            break;
        }
        case TOKEN_OBRACE:
        {
            dst->type = JSON_OBJECT;
            populate_json_object(&dst->object, parse_state);
            break;
        }
        case TOKEN_OBRACK:
        {
            dst->type = JSON_ARRAY;
            populate_json_array(&dst->array, parse_state);
            break;
        }
    }
}

void populate_json_array(json_array *dst, json_parse_state *parse_state)
{
    json_node *node = get_next_node(parse_state);
    u32 num_node_vals = node->num_values;
    u32 num_node_keys = node->num_key_strings;

    json_value *node_vals = alloc_json_values(&parse_state->values_arena, num_node_vals);
    dst->values = node_vals;

    json_token *token = next_token(parse_state); // Obrack 
    if(num_node_vals == 0) token = next_token(parse_state); // Skip array if empty
    else for(u32 i = 0; i < num_node_vals; i += 1)
    {
        populate_json_value(&dst->values[i], parse_state);
        token = next_token(parse_state); // Comma or Cbrack
    }
    dst->num_values = num_node_vals;
}

void populate_json_object(json_object *dst, json_parse_state *parse_state)
{
    json_node *node    = get_next_node(parse_state);
    u32 num_node_vals  = node->num_values;
    u32 num_node_keys  = node->num_key_strings;

    json_value *node_vals  = alloc_json_values(&parse_state->values_arena, num_node_vals);
    json_string *node_keys  = alloc_key_strings(&parse_state->keys_arena, num_node_keys);

    dst->keys   = node_keys;
    dst->values = node_vals;

    json_token *token = next_token(parse_state); // Obrace
    if(num_node_vals == 0) token = next_token(parse_state); // Skip object if no values
    else for(u32 i = 0; i < num_node_vals; i += 1)
    {
        token = next_token(parse_state); // Key string
        char *key_string_chars = alloc_string_chars(&parse_state->chars_arena, token->length-2);
        dst->keys[i] = copy_to_json_string_no_quotes(token, key_string_chars);

        token = next_token(parse_state); // Colon

        populate_json_value(&dst->values[i], parse_state);

        token = next_token(parse_state); // Comma or cbrace
    }
    dst->num_pairs = num_node_vals;
}

json_parsed populate_parsed_json(json_parse_state *parse_state)
{
    json_parsed parsed_json = {0};
    if(parse_state->status != JSON_STATUS_COUNTED)
    {
        printf("Error: Cannot do final parse of JSON. It's nodes haven't been counted yet!\n");
        return parsed_json;
    }

    u32 total_num_values;
    u32 total_string_chars;
    u32 total_key_strings;
    count_json_mem_requirements(&parse_state->node_list, &total_num_values, &total_string_chars, &total_key_strings);

    // Allocate memory for json_values, key strings and value strings
    u32 json_values_size  = total_num_values   * sizeof(json_value);
    u32 string_chars_size = total_string_chars * sizeof(char);
    u32 key_strings_size  = total_key_strings  * sizeof(json_string);
    u32 free_mem_size     = json_values_size + string_chars_size + key_strings_size;

    void *free_mem_base = alloc(free_mem_size);
    memset(free_mem_base, 0, free_mem_size);
    json_value *json_values_buffer  = free_mem_base;
    char       *string_char_buffer  = free_mem_base + json_values_size;
    json_string *key_strings_buffer = free_mem_base + json_values_size + string_chars_size;

    json_values_arena values_arena = {.num_allocd_values = 0, .total_num_values = total_num_values, .values = json_values_buffer};
    string_chars_arena chars_arena = {.num_allocd_chars = 0, .total_num_chars = total_string_chars, .chars = string_char_buffer};
    key_strings_arena keys_arena = {.num_allocd_keys = 0, .total_num_keys = total_key_strings, .keys = key_strings_buffer};
    parse_state->values_arena = values_arena;
    parse_state->chars_arena  = chars_arena;
    parse_state->keys_arena   = keys_arena;
    parse_state->num_tokens_parsed = 0;
    parse_state->num_nodes_consumed = 0;

    populate_json_object(&parsed_json.root, parse_state);

    if(parse_state->status == JSON_STATUS_PARSED) parsed_json.free_mem_base = free_mem_base;
    else                                          dealloc(free_mem_base);

    return parsed_json;
}

json_parsed parse_json(const char *src, u32 src_size)
{
    json_parse_state parse_state = {0};
    tokenise_json(&parse_state, src, src_size);

    // Validate json to make populating object values easier
    validate_json(&parse_state);

    // Get json structure
    //  Parse objects and arrays in order
    count_json_nodes(&parse_state);

    // Parse and divvy json_values memory
    json_parsed parsed_json = populate_parsed_json(&parse_state);

    if(parse_state.node_list.nodes)  dealloc(parse_state.node_list.nodes);
    if(parse_state.token_src.tokens) dealloc(parse_state.token_src.tokens);

    return parsed_json;
}

void print_json_object(json_object*,u32,u32);
void print_json_array(json_array*,u32,u32);

void print_json_string(json_string *string)
{
    printf("\"%.*s\"", string->size, string->chars);
}

void print_json_value(json_value *value, u32 indent)
{
    switch(value->type)
    {
        case JSON_NUMBER:
        {
            printf("%f", value->number);
            break;
        }
        case JSON_STRING:
        {
            print_json_string(&value->string);
            break;
        }
        case JSON_BOOL:
        {
            if(value->boolean == 1) printf("true");
            else                    printf("false");
            break;
        }
        case JSON_NULL:
        {
            printf("null");
            break;
        }
        case JSON_OBJECT:
        {
            print_json_object(&value->object, indent, indent+2);
            break;
        }
        case JSON_ARRAY:
        {
            print_json_array(&value->array, indent, indent+2);
            break;
        }
    }
}

void print_json_array(json_array *array, u32 start_column, u32 indent)
{
    printf("[");
    if(array->num_values > 0)
    {
        printf("\n");
        for(u32 i = 0; i < array->num_values; i += 1)
        {
            for(u32 j = 0; j < indent; j += 1) printf("  ");
            print_json_value(&array->values[i], indent);
            if(i < array->num_values - 1) printf(",");
            printf("\n");
        }
        for(u32 i = 0; i < start_column; i += 1) printf("  ");
    }
    printf("]");
}

void print_json_object(json_object *object, u32 start_column, u32 indent)
{
    printf("{");
    if(object->num_pairs > 0)
    {
        printf("\n");
        for(u32 i = 0; i < object->num_pairs; i += 1)
        {
            for(u32 j = 0; j < indent; j += 1) printf("  ");
            print_json_string(&object->keys[i]);
            printf(":");
            print_json_value(&object->values[i], indent);
            if(i < object->num_pairs - 1) printf(",");
            printf("\n");
        }
        for(u32 i = 0; i < start_column; i += 1) printf("  ");
    }
    printf("}");
}

void print_json_parsed(json_parsed *parsed_json)
{    
    print_json_object(&parsed_json->root, 0, 2);
    printf("\n");
}

void set_allocation_functions(alloc_func given_alloc, realloc_func given_realloc, dealloc_func given_dealloc)
{
    alloc        = given_alloc;
    resize_alloc = given_realloc;
    dealloc      = given_dealloc;
}

void dealloc_parsed_json(json_parsed parsed_json)
{
    dealloc(parsed_json.free_mem_base);
}
