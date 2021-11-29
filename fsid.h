/*
 * Fast string identifier
 * version 0.1.0t - Nov 26th, 2021
 * -----------------------------------------------------------------------------
 * 
 * Copyright(c) 2021 Alexandr Murashko.
 * Licensed under the MIT license.
 */ 

#ifndef FSID_INCLUDE
#define FSID_INCLUDE

#include <stddef.h>
#include <stdint.h>

/**
* Version
*/
#define FSID_VERSION 0x00000100

/**
* Compiler specific settings
*/
#ifndef FSID_EXTERN
#define FSID_EXTERN
#endif

#ifndef FSID_API
#define FSID_API
#endif

#ifndef FSID_CALLBACK
#define FSID_CALLBACK
#endif

/**
* Use statistics
*/
#define FSID_STATISTICS 1

/**
* Result codes
*/
#define FSID_SUCCESSFUL         (0)
#define FSID_ERR_INVALID_PARAM  (-1)
#define FSID_ERR_OUT_OF_MEMORY  (-2)
#define FSID_ERR_INVALID_VALUE  (-3)

#ifdef __cplusplus
extern "C" {
#endif

    /**
    * FSID broker type
    */
    typedef struct fsid_struct* fsid_t;

    /**
    * User callback to allocates _size bytes and returns a pointer to the allocated memory.
    * @param _userData Private data passed from fsid_init_t.
    * @param _size Bytes to allocate.
    * @return Pointer to the allocated memory if the function fails, the return value is NULL.
    */
    typedef void* (FSID_CALLBACK *fsid_alloc)(void* _userData, size_t _size);

    /**
    * User callback to deallocate by _pointer memory block.
    * @param _userData Private data passed from fsid_init_t.
    * @param _pointer Previously allocated memory block to be freed.
    */
    typedef void (FSID_CALLBACK *fsid_free)(void* _userData, void* _pointer);

    /**
    * User callback to hash computation of _string data with _length bytes.
    * @param _userData Private data passed from fsid_init_t.
    * @param _string Pointer to byte string.
    * @param _length Length the string in bytes.
    * @return Hash value.
    */
    typedef uint32_t (FSID_CALLBACK *fsid_hash)(void* _userData, const char* _string, size_t _length);

    /**
    * User callback to read-only lock the broker.
    * @param _userData Private data passed from fsid_init_t.
    */
    typedef void (FSID_CALLBACK *fsid_rolock)(void* _userData);

    /**
    * User callback to read-only unlock the broker.
    * @param _userData Private data passed from fsid_init_t.
    */
    typedef void (FSID_CALLBACK *fsid_rounlock)(void* _userData);

    /**
    * User callback to read-write lock the broker.
    * @param _userData Private data passed from fsid_init_t.
    */
    typedef void (FSID_CALLBACK *fsid_rwlock)(void* _userData);

    /**
    * User callback to read-write unlock the broker.
    * @param _userData Private data passed from fsid_init_t.
    */
    typedef void (FSID_CALLBACK *fsid_rwunlock)(void* _userData);

    /**
    * Structure contains the parameters to initialize broker.
    */
    typedef struct fsid_init_struct
    {
        void*           userData;       /*< Pointer to private data passed to callbacks */
        fsid_alloc      allocFunc;      /*< Used to allocate the internal state */
        fsid_free       freeFunc;       /*< Used to free the internal state */
        fsid_hash       hashFunc;       /*< Used to specific hash function */
        fsid_rolock     rolockFunc;     /*< Used to read-only lock internal state */
        fsid_rounlock   rounlockFunc;   /*< Used to read-only unlock internal state */
        fsid_rwlock     rwlockFunc;     /*< Used to read-write lock internal state */
        fsid_rwunlock   rwunlockFunc;   /*< Used to read-write unlock internal state */
    } fsid_init_t;

    /**
    * Initialize broker with specific parameters.
    * @param _fsid Pointer to broker.
    * @param _params Pointer to fsid_init_t struct, can be NULL.
    * @return FSID_SUCCESSFUL if successful.
    *         FSID_ERR_INVALID_PARAM if _fsid is NULL or invalid parameters in _params.
    *         FSID_ERR_OUT_OF_MEMORY if not enough of free memory.
    */
    FSID_EXTERN int FSID_API fsid_initialize(fsid_t* _fsid, const fsid_init_t* _params);

    /**
    * Release broker.
    * @param _fsid Broker to be released.
    * @return FSID_SUCCESSFUL if successful.
    *         FSID_ERR_INVALID_PARAM if _fsid is NULL.
    */
    FSID_EXTERN int FSID_API fsid_release(fsid_t _fsid);

    /**
    * Checks if there is a null-terminated byte string contained in the broker.
    * @param _fsid Broker.
    * @param _string Pointer to the null-terminated byte string.
    * @return Non-negative value associated with this string, otherwise negative value.
    *         FSID_ERR_INVALID_PARAM if _fsid is NULL or _string is NULL.
    *         FSID_ERR_INVALID_VALUE if this string is not contained in the broker.
    */
    FSID_EXTERN int FSID_API fsid_check_string(fsid_t _fsid, const char* _string);

    /**
    * Checks if there is a byte string with specific length contained in the broker.
    * @param _fsid Broker.
    * @param _string Pointer to byte string.
    * @param _length Length the string in bytes.
    * @return Non-negative value associated with this string, otherwise negative value.
    *         FSID_ERR_INVALID_PARAM if _fsid is NULL or _string is NULL.
    *         FSID_ERR_INVALID_VALUE if this string is not contained in the broker.
    */
    FSID_EXTERN int FSID_API fsid_check_stringlen(fsid_t _fsid, const char* _string, size_t _length);

    /**
    * Inserts a null-terminated byte string into the broker, if the broker doesn't already contain an equivalent string.
    * @param _fsid Broker.
    * @param _string Pointer to the null-terminated byte string.
    * @return Non-negative value associated with this string, otherwise negative value.
    *         FSID_ERR_INVALID_PARAM if _fsid is NULL or _string is NULL.
    *         FSID_ERR_OUT_OF_MEMORY if not enough of free memory.
    */
    FSID_EXTERN int FSID_API fsid_insert_string(fsid_t _fsid, const char* _string);

    /**
    * Inserts a null-terminated byte string with specific length into the broker, if the broker doesn't already contain an equivalent string.
    * @param _fsid Broker.
    * @param _string Pointer to byte string.
    * @param _length Length the string in bytes.
    * @return Non-negative value associated with this string, otherwise negative value.
    *         FSID_ERR_INVALID_PARAM if _fsid is NULL or _string is NULL.
    *         FSID_ERR_OUT_OF_MEMORY if not enough of free memory.
    */
    FSID_EXTERN int FSID_API fsid_insert_stringlen(fsid_t _fsid, const char* _string, size_t _length);

    /**
    * Checks if there is a value associated with the string in the broker.
    * @param _fsid Broker.
    * @param _value Value.
    * @param _pointer Pointer to byte string pointer to receive null-terminated string that associated with the value, can be NULL.
    * @param _length Pointer to size_t to receive string length that associated with the value, can be NULL.
    * @return FSID_SUCCESSFUL if successful.
    *         FSID_ERR_INVALID_PARAM if _fsid is NULL.
    *         FSID_ERR_INVALID_VALUE if _value is not associated with the string in the broker.
    */
    FSID_EXTERN int FSID_API fsid_check_value(fsid_t _fsid, int _value, const char** _pointer, size_t* _length);

#ifdef FSID_STATISTICS
    /**
    * Contains the broker statistics.
    */
    typedef struct fsid_statistics_struct
    {
        size_t memorySize;  /*< Memory used */
        size_t hashesCount; /*< Number of hashes in broker */
        size_t valuesCount; /*< Number of associated strings in broker */
    } fsid_statistics_t;

    /**
    * Get the broker statistics.
    * @param _stat Pointer to the statistic structure.
    * @param _fsid Broker.
    * @return FSID_SUCCESSFUL if successful.
    *         FSID_ERR_INVALID_PARAM if _fsid is NULL or _stat is NULL.
    */
    FSID_EXTERN int FSID_API fsid_get_statistics(fsid_statistics_t* _stat, const fsid_t _fsid);
#endif /* FSID_STATISTICS */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FSID_INCLUDE */
