#include "remi/remi-client.h"
#include "remi-fileset.hpp"

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

extern "C" int remi_fileset_migrate(
        remi_provider_handle_t handle,
        remi_fileset_t fileset,
        int flag)
{

}
