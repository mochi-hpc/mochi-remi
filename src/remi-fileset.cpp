#include "remi/remi-common.h"

#if 0
typedef struct remi_fileset* remi_fileset_t;
#define REMI_FILESET_NULL ((remi_fileset_t)0)

typedef void (*remi_fileset_callback_t)(remi_fileset_t, const char*, void*);
#define REMI_FILESET_CALLBACK_NULL ((remi_fileset_callback_t)0)

typedef void (*remi_metadata_callback_t)(remi_fileset_t, const char*, const char*, void*);
#define REMI_METADATA_CALLBACK_NULL ((remi_metadata_callback_t)0)

#endif

extern "C" int remi_fileset_create(
        const char* fileset_class,
        const char* fileset_root,
        remi_fileset_t* fileset) 
{

}

extern "C" int remi_fileset_free(remi_fileset_t fileset)
{

}

extern "C" int remi_fileset_get_class(
        remi_fileset_t fileset,
        char* buf,
        size_t* size)
{

}

extern "C" int remi_fileset_get_root(
        remi_fileset_t fileset,
        char* buf,
        size_t* size)
{

}

extern "C" int remi_fileset_register_file(
        remi_fileset_t fileset,
        const char* filename)
{

}

extern "C" int remi_fileset_deregister_file(
        remi_fileset_t fileset,
        const char* filename)
{

}

extern "C" int remi_fileset_foreach_file(
        remi_fileset_t fileset,
        remi_fileset_callback_t callback,
        void* uargs)
{

}

extern "C" int remi_fileset_register_metadata(
        remi_fileset_t fileset,
        const char* key,
        const char* value)
{

}

extern "C" int remi_fileset_deregister_metadata(
        remi_fileset_t fileset,
        const char* key,
        const char* value)
{

}

extern "C" int remi_fileset_foreach_metadata(
        remi_fileset_t fileset,
        remi_metadata_callback_t callback,
        void* uargs)
{

}
