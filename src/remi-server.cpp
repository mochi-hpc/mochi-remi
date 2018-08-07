#include <remi/remi-server.h>

#if 0
typedef struct remi_provider* remi_provider_t;
#define REMI_PROVIDER_NULL ((remi_provider_t)0)

typedef void (*remi_migration_callback_t)(remi_provider_t, remi_fileset_t, void*);
#define REMI_MIGRATION_CALLBACK_NULL ((remi_migration_callback_t)0)

#endif

extern "C" int remi_provider_register(
        margo_instance_id mid,
        uint16_t provider_id,
        ABT_pool pool,
        remi_provider_t* provider)
{

}

extern "C" int remi_provider_add_migration_class(
        remi_provider_t provider,
        const char* class_name,
        remi_migration_callback_t callback,
        void* uargs)
{

}
