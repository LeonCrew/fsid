/*
 * Fast string identifier
 * -----------------------------------------------------------------------------
 *
 * Copyright(c) 2021 Alexandr Murashko.
 * Licensed under the MIT license.
 */

#include "fsid.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define FSID_EMPTY_STRING_VALUE (0)

#define FSID_NODE_MAX_LEVEL (sizeof(int) * 8 * 145 / 100 + 1)
#define FSID_NODE_POOL_CAPACITY (16)
#define FSID_NODE_HASH_MASK (0xffffffc0)
#define FSID_NODE_HEIGHT_MASK (0x3f)

/* Empty string with compiler-time check */
static const char emptyString[FSID_NODE_MAX_LEVEL > FSID_NODE_HEIGHT_MASK ? 0 : 1] = { 0 };

/*
 * Internal struct
 */

/* String record */
typedef struct fsid_record_struct
{
    struct fsid_record_struct* next;
    size_t length;
    int value;
    char data[1];
} *fsid_record_t;

/* Binary tree node */
typedef struct fsid_node_struct
{
    struct fsid_node_struct* left;
    struct fsid_node_struct* right;
    struct fsid_record_struct* record;
    uint32_t flags;
} *fsid_node_t;

/* Binary tree node pool */
typedef struct fsid_node_pool_struct
{
    struct fsid_node_pool_struct* next;
    struct fsid_node_struct nodes[FSID_NODE_POOL_CAPACITY];
    size_t count;
} *fsid_node_pool_t;

/* FSID */
struct fsid_struct
{
    void*           userData;
    int             nextValue;

    fsid_alloc      allocFunc;
    fsid_free       freeFunc;
    fsid_hash       hashFunc;
    fsid_rolock     rolockFunc;
    fsid_rounlock   rounlockFunc;
    fsid_rwlock     rwlockFunc;
    fsid_rwunlock   rwunlockFunc;

    fsid_node_t      root;
    fsid_node_pool_t pool;

#ifdef FSID_STATISTICS
    size_t          nodesCount;
    size_t          recordsCount;
    size_t          memorySize;
#endif
};

/*
 * Default callbacks
 */

/* Default alloc */
static void* FSID_CALLBACK fsid_alloc_default(void* _userData, size_t _size)
{
    return malloc(_size);
}

/* Default free */
static void FSID_CALLBACK fsid_free_default(void* _userData, void* _pointer)
{
    free(_pointer);
}

static void FSID_CALLBACK fsid_lock_default(void* _userData)
{
}

static void FSID_CALLBACK fsid_unlock_default(void* _userData)
{
}

/* Default hash */
static uint32_t FSID_CALLBACK fsid_hash_default(void* _userData, const char* _string, size_t _length)
{
    const uint32_t m = 0x5bd1e995;
    const int r = 24;
    uint32_t h = 0xcc9e2d51 ^ (_length & 0xffffffff);

    while (_length >= 4)
    {
        uint32_t k = *(uint32_t*)_string;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        _string += 4;
        _length -= 4;
    }

    switch (_length)
    {
    case 3: h ^= _string[2] << 16;
    case 2: h ^= _string[1] << 8;
    case 1: h ^= _string[0];
        h *= m;
    };

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

/*
 * Internal functions
 */

/* Allocate memory block */
static inline void* fsid_alloc_func(fsid_t _fsid, size_t _size)
{
    void* pointer = _fsid->allocFunc(_fsid->userData, _size);

    if (!pointer)
        return NULL;

#ifdef FSID_STATISTICS
    _fsid->memorySize += _size;
#endif /* FSID_STATISTICS */
    return pointer;
}

/* Free memory block */
static inline void fsid_free_func(fsid_t _fsid, void* _pointer, size_t _size)
{
#ifdef FSID_STATISTICS
    _fsid->memorySize -= _size;
#endif /* FSID_STATISTICS */

    _fsid->freeFunc(_fsid->userData, _pointer);
}

/* Calculate string hash */
static inline uint32_t fsid_hash_func(fsid_t _fsid, const char* _string, size_t _length)
{
    return _fsid->hashFunc(_fsid->userData, _string, _length) & FSID_NODE_HASH_MASK;
}

/* Read-only lock */
static inline void fsid_rolock_func(fsid_t _fsid)
{
    _fsid->rolockFunc(_fsid->userData);
}

/* Read-only unlock */
static inline void fsid_rounlock_func(fsid_t _fsid)
{
    _fsid->rounlockFunc(_fsid->userData);
}

/* Read-write lock */
static inline void fsid_rwlock_func(fsid_t _fsid)
{
    _fsid->rwlockFunc(_fsid->userData);
}

/* Read-write unlock */
static inline void fsid_rwunlock_func(fsid_t _fsid)
{
    _fsid->rwunlockFunc(_fsid->userData);
}

/* Create record */
static fsid_record_t fsid_record_create(fsid_t _fsid, int _value, const char* _string, size_t _length)
{
    const size_t size = sizeof(struct fsid_record_struct) + _length;
    fsid_record_t record = (fsid_record_t)fsid_alloc_func(_fsid, size);

    if (!record)
        return NULL;

    memcpy(record->data, _string, _length);
    record->data[_length] = 0;
    record->length = _length;
    record->value = _value;
    record->next = NULL;

#ifdef FSID_STATISTICS
    _fsid->recordsCount++;
#endif /* FSID_STATISTICS */

    return record;
}

/* Destroy record */
static void fsid_record_destroy(fsid_t _fsid, fsid_record_t _record)
{
    fsid_free_func(_fsid, _record, sizeof(struct fsid_record_struct) + _record->length);

#ifdef FSID_STATISTICS
    _fsid->recordsCount--;
#endif /* FSID_STATISTICS */
}

/* Create node of tree */
static fsid_node_t fsid_node_create(fsid_t _fsid, uint32_t _hash)
{
    fsid_node_pool_t pool = _fsid->pool;

    if (!pool || pool->count == FSID_NODE_POOL_CAPACITY)
    {
        pool = (fsid_node_pool_t)fsid_alloc_func(_fsid, sizeof(struct fsid_node_pool_struct));

        if (!pool)
            return NULL;

        pool->count = 0;
        pool->next = _fsid->pool;
        _fsid->pool = pool;
    }

#ifdef FSID_STATISTICS
    _fsid->nodesCount++;
#endif /* FSID_STATISTICS */

    fsid_node_t node = &pool->nodes[pool->count++];

    node->left = NULL;
    node->right = NULL;
    node->record = NULL;
    node->flags = _hash;
    return node;
}

static int fsid_node_height(const fsid_node_t _node)
{
    if (!_node)
        return -1;

    return _node->flags & FSID_NODE_HEIGHT_MASK;
}

static uint32_t fsid_node_hash(const fsid_node_t _node)
{
    if (!_node)
        return 0;

    return _node->flags & FSID_NODE_HASH_MASK;
}

static int fsid_node_balance_factor(const fsid_node_t _node)
{
    return fsid_node_height(_node->right) - fsid_node_height(_node->left);
}

static void fsid_node_fix_height(fsid_node_t _node)
{
    const int heightLeft = fsid_node_height(_node->left);
    const int heightRight = fsid_node_height(_node->right);

    _node->flags = fsid_node_hash(_node) | ((heightLeft > heightRight ? heightLeft : heightRight) + 1);
}

static fsid_node_t fsid_node_rotate_right(fsid_node_t _node)
{
    fsid_node_t other = _node->left;
    _node->left = other->right;
    other->right = _node;

    fsid_node_fix_height(_node);
    fsid_node_fix_height(other);

    return other;
}

static fsid_node_t fsid_node_rotate_left(fsid_node_t _node)
{
    fsid_node_t other = _node->right;
    _node->right = other->left;
    other->left = _node;
    
    fsid_node_fix_height(_node);
    fsid_node_fix_height(other);

    return other;
}

static fsid_node_t fsid_node_balance(fsid_node_t _node)
{
    fsid_node_fix_height(_node);

    const int balance = fsid_node_balance_factor(_node);

    if (balance == 2)
    {
        if (fsid_node_balance_factor(_node->right) < 0)
            _node->right = fsid_node_rotate_right(_node->right);
        return fsid_node_rotate_left(_node);
    }
    else if (balance == -2)
    {
        if (fsid_node_balance_factor(_node->left) > 0)
            _node->left = fsid_node_rotate_left(_node->left);
        return fsid_node_rotate_right(_node);
    }

    return _node;
}

/* Thread-safe methods */

int fsid_check_stringlen_safe(const fsid_t _fsid, const char* _string, size_t _length, const uint32_t _hash)
{
    fsid_node_t node = _fsid->root;

    while (node)
    {
        if (_hash == fsid_node_hash(node))
            break;
        else if (_hash < fsid_node_hash(node))
            node = node->left;
        else
            node = node->right;
    }

    if (node)
    {
        fsid_record_t record = node->record;

        while (record)
        {
            if (record->length == _length && memcmp(record->data, _string, _length) == 0)
                return record->value;

            record = record->next;
        }
    }
    return FSID_ERR_INVALID_VALUE;
}

int fsid_insert_stringlen_safe(fsid_t _fsid, const char* _string, size_t _length, const uint32_t _hash)
{
    fsid_node_t stack[FSID_NODE_MAX_LEVEL];
    fsid_node_t node = _fsid->root;
    int stackCount = 0;
    bool newNode = false;

    for (;;)
    {
        if (!node)
        {
            node = fsid_node_create(_fsid, _hash);

            if (!node)
                return FSID_ERR_OUT_OF_MEMORY;

            stack[stackCount++] = node;
            newNode = true;
            break;
        }
        else if (_hash == fsid_node_hash(node))
        {
            break;
        }
        else if (_hash < fsid_node_hash(node))
        {
            stack[stackCount++] = node;
            node = node->left;
        }
        else
        {
            stack[stackCount++] = node;
            node = node->right;
        }
    }

    if (newNode)
    {
        while (stackCount > 0)
        {
            fsid_node_t node = stack[--stackCount];
            node = fsid_node_balance(node);

            if (stackCount > 0)
            {
                fsid_node_t parent = stack[stackCount - 1];

                if (fsid_node_hash(node) < fsid_node_hash(parent))
                    parent->left = node;
                else
                    parent->right = node;
            }
            else
            {
                _fsid->root = node;
            }
        }
    }

    fsid_record_t record = node->record;

    while (record)
    {
        if (record->length == _length && memcmp(record->data, _string, _length) == 0)
            return record->value;

        record = record->next;
    }

    record = fsid_record_create(_fsid, _fsid->nextValue, _string, _length);

    if (!record)
        return FSID_ERR_OUT_OF_MEMORY;

    _fsid->nextValue++;
    record->next = node->record;
    node->record = record;
    return record->value;
}

int fsid_check_value_safe(fsid_t _fsid, int _value, const char** _pointer, size_t* _length)
{
    fsid_node_pool_t pool = _fsid->pool;

    while (pool)
    {
        for (size_t index = 0; index < pool->count; ++index)
        {
            fsid_node_t node = &pool->nodes[index];
            fsid_record_t record = node->record;

            while (record)
            {
                if (record->value == _value)
                {
                    if (_pointer)
                        *_pointer = record->data;

                    if (_length)
                        *_length = record->length;

                    return FSID_SUCCESSFUL;
                }

                record = record->next;
            }
        }
        pool = pool->next;
    }

    if (_pointer)
        *_pointer = NULL;

    if (_length)
        *_length = 0;

    return FSID_ERR_INVALID_VALUE;
}


/* Public methods */

#ifdef FSID_STATISTICS
int fsid_get_statistics(fsid_statistics_t* _stat, const fsid_t _fsid)
{
    if (!_stat)
        return FSID_ERR_INVALID_PARAM;

    if (!_fsid)
        return FSID_ERR_INVALID_PARAM;

    _stat->memorySize = _fsid->memorySize;
    _stat->hashesCount = _fsid->nodesCount;
    _stat->valuesCount = _fsid->recordsCount;
    return FSID_SUCCESSFUL;
}
#endif /* FSID_STATISTICS */

/**
*/
int fsid_initialize(fsid_t* _fsid, const fsid_init_t* _params)
{
    void* userData = NULL;
    fsid_alloc fAlloc = &fsid_alloc_default;
    fsid_free fFree = &fsid_free_default;
    fsid_hash fHash = &fsid_hash_default;
    fsid_rolock fROLock = &fsid_lock_default;
    fsid_rounlock fROUnlock = &fsid_unlock_default;
    fsid_rwlock fRWLock = &fsid_lock_default;
    fsid_rwunlock fRWUnlock = &fsid_unlock_default;
    fsid_t fsid_internal = NULL;

    if (!_fsid)
        return FSID_ERR_INVALID_PARAM;

    if (_params)
    {
        userData = _params->userData;

        if (_params->allocFunc && _params->freeFunc)
        {
            fAlloc = _params->allocFunc;
            fFree = _params->freeFunc;
        }
        else if (_params->allocFunc || _params->freeFunc)
        {
            return FSID_ERR_INVALID_PARAM;
        }

        if (_params->rwlockFunc && _params->rwunlockFunc)
        {
            fRWLock = _params->rwlockFunc;
            fRWUnlock = _params->rwunlockFunc;
        }
        else if (_params->rwlockFunc || _params->rwunlockFunc)
        {
            return FSID_ERR_INVALID_PARAM;
        }

        if (_params->rolockFunc && _params->rounlockFunc)
        {
            fROLock = _params->rolockFunc;
            fROUnlock = _params->rounlockFunc;
        }
        else if (!_params->rolockFunc && !_params->rounlockFunc)
        {
            fROLock = fRWLock;
            fROUnlock = fRWUnlock;
        }
        else
        {
            return FSID_ERR_INVALID_PARAM;
        }

        if (_params->hashFunc)
        {
            fHash = _params->hashFunc;
        }
    }

    fsid_internal = (fsid_t)fAlloc(userData, sizeof(struct fsid_struct));
    
    if (!fsid_internal)
        return FSID_ERR_OUT_OF_MEMORY;

    memset(fsid_internal, 0, sizeof(struct fsid_struct));

    fsid_internal->userData = userData;
    fsid_internal->allocFunc = fAlloc;
    fsid_internal->freeFunc = fFree;
    fsid_internal->hashFunc = fHash;
    fsid_internal->rolockFunc = fROLock;
    fsid_internal->rounlockFunc = fROUnlock;
    fsid_internal->rwlockFunc = fRWLock;
    fsid_internal->rwunlockFunc = fRWUnlock;

    fsid_internal->nextValue = FSID_EMPTY_STRING_VALUE + 1;
    fsid_internal->memorySize = sizeof(struct fsid_struct);

    *_fsid = fsid_internal;
    return FSID_SUCCESSFUL;
}

/**
*/
int fsid_release(fsid_t _fsid)
{
    if (!_fsid)
        return FSID_ERR_INVALID_PARAM;

    fsid_node_pool_t pool = _fsid->pool;

    while (pool)
    {
        fsid_node_pool_t next = pool->next;

        for (size_t i = 0; i < pool->count; ++i)
        {
            fsid_node_t node = &pool->nodes[i];
            fsid_record_t record = node->record;

            while (record)
            {
                fsid_record_t rnext = record->next;
                fsid_record_destroy(_fsid, record);
                record = rnext;
            }
#ifdef FSID_STATISTICS
            _fsid->nodesCount--;
#endif /* FSID_STATISTICS */
        }

        fsid_free_func(_fsid, pool, sizeof(struct fsid_node_pool_struct));
        pool = next;
    }

    void* userData = _fsid->userData;
    fsid_free fFree = _fsid->freeFunc;

    fFree(userData, _fsid);

    return FSID_SUCCESSFUL;
}

int fsid_check_string(const fsid_t _fsid, const char* _string)
{
    if (_string)
        return fsid_check_stringlen(_fsid, _string, strlen(_string));
    return FSID_ERR_INVALID_PARAM;
}

int fsid_check_stringlen(const fsid_t _fsid, const char* _string, size_t _length)
{
    if (!_fsid)
        return FSID_ERR_INVALID_PARAM;

    if (!_string)
        return FSID_ERR_INVALID_PARAM;

    if (_length == 0)
        return FSID_EMPTY_STRING_VALUE;

    const uint32_t hash = fsid_hash_func(_fsid, _string, _length);

    fsid_rolock_func(_fsid);
    int result = fsid_check_stringlen_safe(_fsid, _string, _length, hash);
    fsid_rounlock_func(_fsid);

    return result;
}

int fsid_insert_string(fsid_t _fsid, const char* _string)
{
    if (_string)
        return fsid_insert_stringlen(_fsid, _string, strlen(_string));
    return FSID_ERR_INVALID_PARAM;
}

int fsid_insert_stringlen(fsid_t _fsid, const char* _string, size_t _length)
{
    if (!_fsid)
        return FSID_ERR_INVALID_PARAM;

    if (!_string)
        return FSID_ERR_INVALID_PARAM;

    if (_length == 0)
        return FSID_EMPTY_STRING_VALUE;

    const uint32_t hash = fsid_hash_func(_fsid, _string, _length);

    fsid_rwlock_func(_fsid);
    int result = fsid_insert_stringlen_safe(_fsid, _string, _length, hash);
    fsid_rwunlock_func(_fsid);

    return result;
}

int fsid_check_value(fsid_t _fsid, int _value, const char** _pointer, size_t* _length)
{
    if (!_fsid)
        return FSID_ERR_INVALID_PARAM;

    if (_value == FSID_EMPTY_STRING_VALUE)
    {
        if (_pointer)
            *_pointer = emptyString;

        if (_length)
            *_length = 0;

        return FSID_SUCCESSFUL;
    }

    fsid_rolock_func(_fsid);
    int result = fsid_check_value_safe(_fsid, _value, _pointer, _length);
    fsid_rounlock_func(_fsid);

    return result;
}
