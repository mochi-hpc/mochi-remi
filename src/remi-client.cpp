#include "remi/remi-client.h"

#if 0
typedef struct remi_client* remi_client_t;
#define REMI_CLIENT_NULL ((remi_client_t)0)

typedef struct remi_provider_handle* remi_provider_handle_t;
#define REMI_PROVIDER_HANDLE_NULL ((remi_provider_handle_t)0)
#endif

extern "C" int remi_client_init(margo_instance_id mid, remi_client_t* client)
{

}

extern "C" int remi_client_finalize(remi_client_t client)
{

}

extern "C" int remi_provider_handle_create(
        remi_client_t client,
        hg_addr_t addr,
        uint16_t provider_id,
        remi_provider_handle_t* handle)
{

}

extern "C" int remi_provider_handle_ref_incr(remi_provider_handle_t handle)
{

}

extern "C" int remi_provider_handle_release(remi_provider_handle_t handle)
{

}

extern "C" int remi_shutdown_service(remi_client_t client, hg_addr_t addr)
{

}
