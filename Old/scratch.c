

typedef struct _h_entry _h_entry;
struct _h_entry
{
    u32 key_hash;
    json_pair key_val;
    _h_entry *next;
};

typedef struct
{
    u32 capacity;
    u32 allocated;
    _h_entry *entries;
} _h_entry_pool;

void _init_h_pool(_h_entry_pool *pool, u32 capacity)
{
    pool->entries;
}

_h_entry *_h_entry_alloc(_h_entry_pool *pool)
{
    _h_entry *e = &pool->entries[pool->allocated];
    pool->allocated += 1;
    return e;
}

typedef struct
{
    u32 num_pairs;
    u32 num_buckets;
    _h_entry **buckets;
    _h_entry **pair_list;
} _json_obj;

u64 _hash(string s)
{
    u64 h = 5381;
    s32 c;
    unsigned char *str = s.cstr;
    while(c = *str++)
    {
        h = ((h << 5) + h) + c;
    }
    return h;
}

u32 _json_obj_has(_json_obj *obj, string key)
{
    u32 key_hash = _hash(key);

    _h_entry *entry = obj->buckets[key_hash % obj->num_buckets];

    for(; entry != NULL; entry = entry->next)
    {
        if(entry->key_hash == key_hash && string_eq(entry->key_val.name, key))
        {
            return 1;
        }
    }

    return 0;
}

//Don't check for duplicate keys
void _insert_json_num(_json_obj *dst, string key, f64 num, _h_entry_pool *pool)
{
    u32 key_hash = _hash(key);

    _h_entry *dst_entry = _h_entry_alloc(pool);
    dst_entry->key_hash = key_hash;
    dst_entry->key_val.name = key;
    dst_entry->key_val.value.type = JSON_NUM;
    dst_entry->key_val.value.num = num;
    dst_entry->next = NULL;

    _h_entry **bucket = &dst->buckets[key_hash % dst->num_buckets];

    //Find end of bucket list
    if(*bucket != NULL)
    {
        for(bucket = &((*bucket)->next); *bucket != NULL; bucket = &((*bucket)->next));
    }

    //Append to bucket list
    *bucket = dst_entry;

    //Append to pair list
    dst->pair_list[dst->num_pairs] = dst_entry;
    dst->num_pairs += 1;
}
