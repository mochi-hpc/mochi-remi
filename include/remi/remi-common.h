#ifndef __REMI_COMMON_H
#define __REMI_COMMON_H

#include <margo.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define REMI_KEEP_SOURCE 0
#define REMI_REMOVE_SOURCE 1

#define REMI_SUCCESS             0 /* Success */
#define REMI_ERR_ALLOCATION     -1 /* Error allocating something */
#define REMI_ERR_INVALID_ARG    -2 /* An argument is invalid */
#define REMI_ERR_MERCURY        -3 /* An error happened calling a Mercury function */
#define REMI_ERR_UNKNOWN_CLASS  -4 /* Database refered to by id is not known to provider */
#define REMI_ERR_UNKNOWN_FILE   -5 /* File not found in fileset or in file system */
#define REMI_ERR_UNKNOWN_PR     -6 /* Provider id could not be matched with a provider */
#define REMI_ERR_UNKNOWN_META   -7 /* Unknown metadata entry */
#define REMI_ERR_SIZE           -8 /* Client did not allocate enough for the requested data */
#define REMI_ERR_MIGRATION      -9 /* Error during data migration */
#define REMI_ERR_CLASS_EXISTS  -10 /* Migration class already registered */
#define REMI_ERR_STAT          -11 /* Error in stat call */

typedef struct remi_fileset* remi_fileset_t;
#define REMI_FILESET_NULL ((remi_fileset_t)0)

typedef void (*remi_fileset_callback_t)(const char*, void*);
#define REMI_FILESET_CALLBACK_NULL ((remi_fileset_callback_t)0)

typedef void (*remi_metadata_callback_t)(const char*, const char*, void*);
#define REMI_METADATA_CALLBACK_NULL ((remi_metadata_callback_t)0)

int remi_fileset_create(
        const char* fileset_class,
        const char* fileset_root,
        remi_fileset_t* fileset);

int remi_fileset_free(remi_fileset_t fileset);

int remi_fileset_get_class(
        remi_fileset_t fileset,
        char* buf,
        size_t* size);

int remi_fileset_get_root(
        remi_fileset_t fileset,
        char* buf,
        size_t* size);

int remi_fileset_register_file(
        remi_fileset_t fileset,
        const char* filename);

int remi_fileset_deregister_file(
        remi_fileset_t fileset,
        const char* filename);

int remi_fileset_foreach_file(
        remi_fileset_t fileset,
        remi_fileset_callback_t callback,
        void* uargs);

int remi_fileset_register_metadata(
        remi_fileset_t fileset,
        const char* key,
        const char* value);

int remi_fileset_deregister_metadata(
        remi_fileset_t fileset,
        const char* key);

int remi_fileset_foreach_metadata(
        remi_fileset_t fileset,
        remi_metadata_callback_t callback,
        void* uargs);

#if defined(__cplusplus)
}
#endif

#endif
