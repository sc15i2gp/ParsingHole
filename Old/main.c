#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#ifndef _WIN32
#define _WIN32
#endif
#include "json_parse.h"

const char test_json_0[] =
"{\n"
"\"Hello\":\"500\",\n"
"\"Goodbye\":100,\n"
"\"Weehee\":10.433,\n"
"\"Obj\":{\"new\":\"ci\", \"old\":{\"a\":1, \"b\":2}},\n"
"\"Arr\":[0, 1, [2, 3, 4], {\"Hi\": \"Bye\"}, \"2\", \"three\", 4.0, {\"Me\":5, \"You\": \"6.0\", \"Them\":[0, 1, \"2\"]}]\n"
"}";

const char test_json_1[] =
"{\"Hello\":20,\"Goodbye\":10,\"Gay\":\"Me\"}";

//TODO:
// - JSON is dogshit, just get this ready for use
//   - Better value retrieval
//     - Returned type from "get_json_val"
//     - Don't have to call it multiple times for traversing nested objects/arrays
//     - "Compound" keys (e.g. obj.field.thing[0].hello)
//   - Handle dealloc properly
//   - Maybe tidy up idk
// - Stretch:
//   - Hash table objects
//   - Cache friendly code

// =============================================================== //

// =========================== Arena ==========================//

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

#ifdef _WIN32
void commit_mem(mem_arena *arena, u32 commit_size)
{
    byte *base = arena->buffer + arena->committed;
    commit_size = (commit_size + (arena->page_size - 1)) & ~(arena->page_size - 1);
    VirtualAlloc(base, commit_size, MEM_COMMIT, PAGE_READWRITE);
    arena->committed += commit_size;
}
#endif

//Reserve some number of pages, commit some number of pages and extend when out of capacity
void init_mem_arena(mem_arena *arena, u32 reserve_size, u32 page_size)
{
    //Init mem_arena struct and reserve reserve_size bytes
    #ifdef _WIN32
    arena->buffer = (byte*)VirtualAlloc(NULL, reserve_size, MEM_RESERVE, PAGE_READWRITE);
    #else
    arena->buffer = (byte*)malloc(reserve_size);
    #endif
    arena->page_size = page_size;
    arena->reserved = reserve_size;
    arena->committed = 0;
    arena->allocated = 0;
}

void delete_mem_arena(mem_arena *arena)
{
    #ifdef _WIN32
    VirtualFree(arena->buffer, 0, MEM_RELEASE);
    #else
    free(arena->buffer);
    #endif
    arena->reserved = 0;
    arena->committed = 0;
    arena->allocated = 0;
}

byte *alloc(mem_arena *arena, u32 alloc_size)
{
    u32 new_allocated = arena->allocated + alloc_size;
    //NOTE: rn this works on page size
    #ifdef _WIN32
    if(new_allocated > arena->committed)
    {
        u32 to_commit = new_allocated - arena->committed;
        commit_mem(arena, to_commit);
    }
    #else
    if(new_allocated > arena->reserved)
    {
        u32 new_reserved = 2 * arena->reserved;
        u32 to_reserve = (new_allocated > new_reserved) ? new_allocated : new_reserved;
        arena->buffer = realloc(arena->buffer, to_reserve);
        arena->reserved = to_reserve;
    }
    #endif
    byte *ptr = arena->buffer + arena->allocated;
    arena->allocated += alloc_size;
    return ptr;
}

void unalloc(mem_arena *arena, u32 unalloc_size)
{
    arena->allocated -= unalloc_size;
}

mem_arena lib_arena;
byte *lib_alloc(u32 alloc_size)
{
    return alloc(&lib_arena, alloc_size);
}

void lib_dealloc(void *ptr, u32 alloc_size)
{
    unalloc(&lib_arena, alloc_size);
}

int main()
{
    init_mem_arena(&lib_arena, 2*1024*1024, 4096);

    const char *json_0 = test_json_0;
    const char *json_1 = test_json_1;

    parsed_json p_json_0 = parse_json(json_0, strlen(json_0), lib_alloc);
    parsed_json p_json_1 = parse_json(json_1, strlen(json_1), lib_alloc);

    printf("JSON 0:\n");
    print_parsed_json(&p_json_0, lib_alloc, lib_dealloc);
    printf("\n");

    printf("JSON 1:\n");
    print_parsed_json(&p_json_1, lib_alloc, lib_dealloc);
    printf("\n");

    printf("\nLooking for Hello...\n");
    const char *key = "Hello";
    json_val val_0 = get_json_value(&p_json_0, key);
    json_val val_1 = get_json_value(&p_json_1, key);
    printf("Val 0:");
    print_json_val(&val_0);
    printf("\n");
    printf("Val 1:");
    print_json_val(&val_1);
    printf("\n");
    dealloc_parsed_json(&p_json_0, lib_dealloc);
    dealloc_parsed_json(&p_json_1, lib_dealloc);
    return 0;
}
