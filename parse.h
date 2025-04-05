#ifndef _JSON_PARSE_H_
#define _JSON_PARSE_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// JSON PARSING:
//  - Add get_ functions for json_parsed e.g. get_key_ptr(parsed_json, index)
//  - ooas can be differentiated by whether they have keys - they don't need types
//  - Retrieve values
//  - Editing
//  - Validation
//      - Escape characters, backslashes, no control chars in strings
//      - Numbers in valid formats
//      - Have tildes in error arrow cover the offending token
//      - No duplicate keys
//  - Tidy
//      - Reorder stuff
//          - Bare API at the top, otherwise structs near code that uses them.
//      - Make sure code looks good
//  - Control how much mem used for token array
//  - Testing
//  - Performance
//  - Remove recursion in favour of linear functions with stack for control flow?
//  - Asserts?
//  - Strings are assumed UTF-8 - Anyone sending emoji over json is insane

// Some util stuff - Typedefs and easy functions

typedef uint8_t  u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int32_t  s32;
typedef int64_t  s64;
typedef float    f32;
typedef double   f64;

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

typedef void* (*alloc_func)(u64);
typedef void* (*realloc_func)(void*,u64);
typedef void  (*dealloc_func)(void*);

alloc_func   alloc;
realloc_func resize_alloc;
dealloc_func dealloc;

void set_allocation_functions(alloc_func given_alloc, realloc_func given_realloc, dealloc_func given_dealloc)
{
    alloc        = given_alloc;
    resize_alloc = given_realloc;
    dealloc      = given_dealloc;
}

typedef enum
{
    JSON_NONE,
    JSON_DOESNT_EXIST,
    JSON_NUMBER,
    JSON_BOOL,
    JSON_NULL,
    JSON_STRING,
    JSON_OBJECT,
    JSON_ARRAY,
} json_type;

typedef struct
{
    u32   hash;
    u32   size;
    char *chars;
} json_string;

void compute_json_string_hash(json_string *str)
{
    u32 hash = 5381;
    for(u32 i = 0; i < str->size; i += 1)
    {
        s32 c = (s32)str->chars[i];
        hash = ((hash << 5) + hash) + c;
    }
    str->hash = hash;
}

void print_json_string(json_string s)
{
    printf("\"%.*s\"", s.size, s.chars);
}

u8 json_string_eq(json_string s0, json_string s1)
{
    if(s0.size != s1.size || s0.hash != s1.hash) return 0;

    for(u32 i = 0; i < s0.size; i += 1)
    {
        if(s0.chars[i] != s1.chars[i]) return 0;
    }
    return 1;
}

#define to_json_string(s) (json_string){strlen(s), (char*)s}

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

typedef u32 json_val_ptr;
typedef u32 json_str_ptr;
typedef u32 json_ooa_ptr;

typedef struct
{
    // References and inferences for ooa
    json_type    type;
    u32          size;
    json_val_ptr vals_index;
    json_str_ptr keys_index;
} json_ooa;

typedef struct
{
    u32 size;
    u32 cap;
    json_ooa *ooas;
} json_ooa_list;

typedef struct
{
    u32   cap;
    u32   allocd;
    u32   allocs;
    void *buffer;
} json_mem_arena;

u32 alloc_arena_mem(json_mem_arena *arena, u32 alloc_size, u32 num_allocs)
{
    u32 total_alloc_size = alloc_size * num_allocs;
    u32 alloc_loc = arena->allocs;
    arena->allocd += total_alloc_size;
    arena->allocs += num_allocs;
    return alloc_loc;
}

#define get_arena_nth_alloc(arena, n, type) &((type*)arena->buffer)[n]

#define alloc_json_values(arena, num_values)   (json_val_ptr)alloc_arena_mem(arena, sizeof(json_value), num_values)
#define alloc_json_strings(arena, num_strings) (json_str_ptr)alloc_arena_mem(arena, sizeof(json_string), num_strings)
#define alloc_json_chars(arena, num_chars)     (char*)(arena->buffer + alloc_arena_mem(arena, 1, num_chars))

typedef struct
{
    json_parse_status status;
    u32               num_tokens_parsed;
    json_tokenised    token_src;
    u32               num_chars_counted;
    u32               num_ooas_parsed;
    json_ooa_list     ooa_list;
    json_mem_arena    keys_arena;
    json_mem_arena    values_arena;
    json_mem_arena    chars_arena;
} json_parse_state;

typedef struct
{
    json_type type;
    union
    {
        f64          number;
        u8           boolean;
        json_string  string;
        json_ooa_ptr ooa;
    };
} json_value;

json_ooa *push_ooa_to_list(json_ooa_list *ooa_list, json_type type)
{
    u32 cap  = ooa_list->cap;
    u32 size = ooa_list->size;
    if(size == cap)
    {
        cap = 2 * cap;
        ooa_list->ooas = (json_ooa*)realloc(ooa_list->ooas, cap * sizeof(json_ooa));
        ooa_list->cap  = cap;
    }
    json_ooa *ooa    = &ooa_list->ooas[size];
    ooa->type        = type;
    ooa->size        = 0;
    ooa->vals_index  = 0;
    ooa->keys_index  = 0;
    ooa_list->size  += 1;
    return ooa;
}

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
    compute_json_string_hash(&string);
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

void count_json_object(json_parse_state*);
void count_json_array(json_parse_state*);

void count_json_array(json_parse_state *parse_state)
{
    u32 num_values  = 0;
    u32 num_chars   = 0;

    json_ooa   *dst   = push_ooa_to_list(&parse_state->ooa_list, JSON_ARRAY);
    json_token *token = next_token(parse_state);
    json_token *lh    = lookahead_token(parse_state);
    while(lh->type != TOKEN_CBRACK)
    {
        num_values += 1;
        lh = lookahead_token(parse_state);
        if(lh->type == TOKEN_OBRACE) count_json_object(parse_state);
        else
        if(lh->type == TOKEN_OBRACK) count_json_array(parse_state);
        else
        {
            token = next_token(parse_state);
            if(token->type == TOKEN_STRING)
            {
                num_chars   += token->length - 2; // Exclude quote marks
            }
        }
        lh = lookahead_token(parse_state);
        if(lh->type == TOKEN_COMMA)
        {
            token = next_token(parse_state);
            lh    = lookahead_token(parse_state);
        }
    }
    token = next_token(parse_state); // Consume cbrack

    dst->size = num_values;
    parse_state->num_chars_counted += num_chars;
}

void count_json_object(json_parse_state *parse_state)
{
    u32 num_values  = 0;
    u32 num_chars   = 0;

    json_ooa   *dst   = push_ooa_to_list(&parse_state->ooa_list, JSON_OBJECT);
    json_token *token = next_token(parse_state); // Obrace
    json_token *lh    = lookahead_token(parse_state);
    while(lh->type != TOKEN_CBRACE)
    {
        token        = next_token(parse_state); // Key string
        num_values  += 1;
        num_chars   += token->length - 2; // Exclude quote marks around strings
        
        token = next_token(parse_state); // Colon
        lh    = lookahead_token(parse_state);
        if(lh->type == TOKEN_OBRACE) count_json_object(parse_state);
        else
        if(lh->type == TOKEN_OBRACK) count_json_array(parse_state);
        else
        {
            token = next_token(parse_state);
            if(token->type == TOKEN_STRING)
            {
                num_chars   += token->length - 2; // Exclude quote marks
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

    dst->size = num_values;
    parse_state->num_chars_counted += num_chars;
}

void count_json_ooas_values_and_strings(json_parse_state *parse_state)
{   
    if(parse_state->status != JSON_STATUS_VALID)
    {
        printf("Error: Cannot count JSON. It hasn't been validated yet!\n");
        return;
    }

    u32 cap                        = 128;
    parse_state->num_tokens_parsed = 0;
    parse_state->ooa_list.cap      = cap;
    parse_state->ooa_list.size     = 0;
    parse_state->ooa_list.ooas     = (json_ooa*)alloc(cap * sizeof(json_ooa));
    count_json_object(parse_state);

    parse_state->status = JSON_STATUS_COUNTED;
}

// ============================== Populate parsed JSON ===================================

u32 get_next_ooa(json_parse_state *parse_state)
{
    u32 ooa = parse_state->num_ooas_parsed;
    parse_state->num_ooas_parsed += 1;
    return ooa;
}

json_ooa_ptr populate_json_object(json_parse_state*);
json_ooa_ptr populate_json_array(json_parse_state*);

void populate_json_value(json_value *dst, json_parse_state *parse_state)
{
    json_token *token = lookahead_token(parse_state);
    switch(token->type)
    {
        case TOKEN_NUMBER:
        {
            dst->type   = JSON_NUMBER;
            dst->number = token->numeric_value;
            token       = next_token(parse_state);
            break;
        }
        case TOKEN_BOOL:
        {
            dst->type    = JSON_BOOL;
            dst->boolean = token->boolean_value;
            token        = next_token(parse_state);
            break;
        }
        case TOKEN_STRING:
        {
            dst->type   = JSON_STRING;
            char *cstr  = alloc_json_chars((&parse_state->chars_arena), token->length-2);
            dst->string = copy_to_json_string_no_quotes(token, cstr);
            token       = next_token(parse_state);
            break;
        }
        case TOKEN_NULL:
        {
            dst->type = JSON_NULL;
            token     = next_token(parse_state);
            break;
        }
        case TOKEN_OBRACE:
        {
            dst->type = JSON_OBJECT;
            dst->ooa  = populate_json_object(parse_state);
            break;
        }
        case TOKEN_OBRACK:
        {
            dst->type = JSON_ARRAY;
            dst->ooa  = populate_json_array(parse_state);
            break;
        }
    }
}

json_ooa_ptr populate_json_array(json_parse_state *parse_state)
{
    u32       array_ooa_index = get_next_ooa(parse_state);
    json_ooa *array_ooa       = &parse_state->ooa_list.ooas[array_ooa_index];

    json_val_ptr start_value_index = alloc_json_values(&parse_state->values_arena, array_ooa->size);
    json_value  *value_ptr         = get_arena_nth_alloc((&parse_state->values_arena), start_value_index, json_value);

    json_token *token = next_token(parse_state);
    if(array_ooa->size == 0) token = next_token(parse_state); // Consume empty array cbrack
    for(u32 i = 0; i < array_ooa->size; i += 1)
    {
        populate_json_value(value_ptr, parse_state);
        value_ptr += 1;
        token = next_token(parse_state); // Comma or cbrack
    }

    array_ooa->vals_index = start_value_index;
    return array_ooa_index;
}

json_ooa_ptr populate_json_object(json_parse_state *parse_state)
{
    u32       object_ooa_index = get_next_ooa(parse_state);
    json_ooa *object_ooa       = &parse_state->ooa_list.ooas[object_ooa_index];

    json_val_ptr start_value_index  = alloc_json_values(&parse_state->values_arena, object_ooa->size);
    json_str_ptr start_string_index = alloc_json_strings(&parse_state->keys_arena, object_ooa->size);
    json_string *string_ptr         = get_arena_nth_alloc((&parse_state->keys_arena), start_string_index, json_string);
    json_value  *value_ptr          = get_arena_nth_alloc((&parse_state->values_arena), start_value_index, json_value);

    json_token *token = next_token(parse_state);
    if(object_ooa->size == 0) token = next_token(parse_state); // Consume empty object cbrace
    for(u32 i = 0; i < object_ooa->size; i += 1)
    {
        token           = next_token(parse_state); // Key string
        char *key_chars = alloc_json_chars((&parse_state->chars_arena), token->length-2);
        *string_ptr     = copy_to_json_string_no_quotes(token, key_chars);
        string_ptr     += 1;
        token           = next_token(parse_state); // Colon

        populate_json_value(value_ptr, parse_state);
        value_ptr += 1;
        token = next_token(parse_state); // Comma or cbrace
    }

    object_ooa->keys_index = start_string_index;
    object_ooa->vals_index = start_value_index;
    return object_ooa_index;
}

typedef struct
{
    void *free_mem_base;
    json_ooa_list  ooa_list;
    json_mem_arena keys_arena;
    json_mem_arena values_arena;
    json_mem_arena chars_arena;
} json_parsed;

json_ooa *get_json_ooa(json_parsed *json, u32 index)
{
    json_ooa *ooas = json->ooa_list.ooas;
    return &ooas[index];
}

json_string *get_json_key(json_parsed *json, u32 index)
{
    json_string *keys = (json_string*)json->keys_arena.buffer;
    return &keys[index];
}

json_value *get_json_value(json_parsed *json, u32 index)
{
    json_value *values = (json_value*)json->values_arena.buffer;
    return &values[index];
}

json_parsed populate_parsed_json(json_parse_state *parse_state)
{
    json_parsed parsed_json = {0};
    if(parse_state->status != JSON_STATUS_COUNTED)
    {
        printf("Error: Cannot fill parsed json. It hasn't been counted yet!\n");
    }
    else
    {
        u32 num_keys   = 0; 
        u32 num_values = 1;
        for(u32 i = 0; i < parse_state->ooa_list.size; i += 1)
        {
            json_ooa *ooa = &parse_state->ooa_list.ooas[i];
            num_values += ooa->size;
            if(ooa->type == JSON_OBJECT) num_keys += ooa->size;
        }
        u32 num_chars = parse_state->num_chars_counted;

        u32 keys_buffer_size   = num_keys   * sizeof(json_string);
        u32 values_buffer_size = num_values * sizeof(json_value);
        u32 chars_buffer_size  = num_chars  * sizeof(char);
        u32 total_buffer_size = keys_buffer_size + values_buffer_size + chars_buffer_size;

        void *parsed_buffer = alloc(total_buffer_size);
        void *keys_buffer   = parsed_buffer;
        void *values_buffer = parsed_buffer + keys_buffer_size;
        void *chars_buffer  = values_buffer + values_buffer_size;

        json_mem_arena keys_arena   = {.cap = keys_buffer_size,   .allocd = 0, .allocs = 0, .buffer = keys_buffer};
        json_mem_arena values_arena = {.cap = values_buffer_size, .allocd = 0, .allocs = 0, .buffer = values_buffer};
        json_mem_arena chars_arena  = {.cap = chars_buffer_size,  .allocd = 0, .allocs = 0, .buffer = chars_buffer};

        json_val_ptr none_value_index = alloc_json_values(&values_arena, 1);
        json_value *val = get_arena_nth_alloc((&values_arena), none_value_index, json_value);
        val->type = JSON_DOESNT_EXIST;

        parse_state->num_tokens_parsed = 0;
        parse_state->num_ooas_parsed   = 0;
        parse_state->keys_arena        = keys_arena;
        parse_state->values_arena      = values_arena;
        parse_state->chars_arena       = chars_arena;
        // Needs to fill values, strings and chars memory
        populate_json_object(parse_state);

        parsed_json.free_mem_base = parsed_buffer;
        parsed_json.ooa_list      = parse_state->ooa_list;
        parsed_json.keys_arena    = parse_state->keys_arena;
        parsed_json.values_arena  = parse_state->values_arena;
        parsed_json.chars_arena   = parse_state->chars_arena;
    }
    return parsed_json;
}

json_parsed parse_json(const char *src, u32 src_size)
{
    json_parse_state parse_state = {0};
    tokenise_json(&parse_state, src, src_size);

    // Validate json to make populating object values easier
    validate_json(&parse_state);

    // Get json structure
    // Parse objects and arrays in order
    count_json_ooas_values_and_strings(&parse_state);

    // Parse and divvy json_values memory
    json_parsed parsed_json = populate_parsed_json(&parse_state);
    return parsed_json;
}

// ============================== Print parsed JSON ===================================

void print_indent(u32 indent)
{
    for(u32 i = 0; i < indent; i += 1) printf("  ");
}

void print_json_array(u32,json_parsed*,u32,u32);
void print_json_object(u32,json_parsed*,u32,u32);

void print_json_value(u32 value_index, json_parsed *parsed_json, u32 indent)
{
    json_value *value = get_json_value(parsed_json, value_index);
    switch(value->type)
    {
        case JSON_NUMBER: printf("%f", value->number);       break;
        case JSON_STRING: print_json_string(value->string);  break;
        case JSON_NULL:   printf("null");                    break;
        case JSON_BOOL:
        {
            if(value->boolean) printf("true");
            else               printf("false");
            break;
        }
        case JSON_OBJECT: print_json_object(value->ooa, parsed_json, indent, indent+2); break;
        case JSON_ARRAY:  print_json_array(value->ooa, parsed_json, indent, indent+2);  break;
    }
}

void print_json_array(u32 array_index, json_parsed *parsed_json, u32 start_column, u32 indent)
{
    json_ooa *array = get_json_ooa(parsed_json, array_index);
    
    printf("[");
    if(array->size > 0)
    {
        printf("\n");
        for(u32 i = 0; i < array->size; i += 1)
        {
            print_indent(indent);
            print_json_value(array->vals_index+i, parsed_json, indent);
            if(i < array->size - 1) printf(",");
            printf("\n");
        }
        print_indent(start_column);
    }
    printf("]");
}

void print_json_object(u32 object_index, json_parsed *parsed_json, u32 start_column, u32 indent)
{
    json_ooa *object  = get_json_ooa(parsed_json, object_index);
    json_string *keys = get_json_key(parsed_json, object->keys_index);

    printf("{");
    if(object->size > 0)
    {
        printf("\n");
        for(u32 i = 0; i < object->size; i += 1)
        {
            print_indent(indent);
            print_json_string(keys[i]);
            printf(":");
            print_json_value(object->vals_index+i, parsed_json, indent);
            if(i < object->size - 1) printf(",");
            printf("\n");
        }
        print_indent(start_column);
    }
    printf("}");
}

void print_json_parsed(json_parsed *parsed_json)
{
    printf("PARSED\n");
    print_json_object(0, parsed_json, 0, 2);
    printf("\n");
}

void dealloc_parsed_json(json_parsed parsed_json)
{
    dealloc(parsed_json.free_mem_base);
}

// ============================== Retrieval ===================================

// TODO

#endif
