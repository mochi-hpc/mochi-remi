#include <thallium.hpp>
#include "remi/remi-client.h"
#include "remi-fileset.hpp"

namespace tl = thallium;

struct remi_client {

    margo_instance_id    m_mid = MARGO_INSTANCE_NULL;
    tl::engine*          m_engine = nullptr;
    uint64_t             m_num_providers = 0;
    tl::remote_procedure m_migrate_rpc;

    remi_client(tl::engine* e)
    : m_engine(e)
    , m_migrate_rpc(m_engine->define("remi_migrate")) {}

};

struct remi_provider_handle : public tl::provider_handle {

    remi_client_t m_client    = nullptr;
    uint64_t      m_ref_count = 0;
    
    template<typename ... Args>
    remi_provider_handle(Args&&... args)
    : tl::provider_handle(std::forward<Args>(args)...) {}
};

extern "C" int remi_client_init(margo_instance_id mid, remi_client_t* client)
{
    auto theEngine           = new tl::engine(mid, THALLIUM_CLIENT_MODE);
    remi_client_t theClient  = new remi_client(theEngine);
    theClient->m_mid         = mid;
    *client = theClient;
    return REMI_SUCCESS;
}

extern "C" int remi_client_finalize(remi_client_t client)
{
    if(client == REMI_CLIENT_NULL)
        return REMI_SUCCESS;
    delete client->m_engine;
    delete client;
    return REMI_SUCCESS;
}

extern "C" int remi_provider_handle_create(
        remi_client_t client,
        hg_addr_t addr,
        uint16_t provider_id,
        remi_provider_handle_t* handle)
{
    if(client == REMI_CLIENT_NULL)
        return REMI_ERR_INVALID_ARG;
    auto theHandle = new remi_provider_handle(
            tl::endpoint(*(client->m_engine), addr, false), provider_id);
    theHandle->m_client = client;
    *handle = theHandle;
    client->m_num_providers += 1;
    return REMI_SUCCESS;
}

extern "C" int remi_provider_handle_ref_incr(remi_provider_handle_t handle)
{
    if(handle == REMI_PROVIDER_HANDLE_NULL)
        return REMI_ERR_INVALID_ARG;
    handle->m_ref_count += 1;
    return REMI_SUCCESS;
}

extern "C" int remi_provider_handle_release(remi_provider_handle_t handle)
{
    if(handle == REMI_PROVIDER_HANDLE_NULL)
        return REMI_SUCCESS;
    handle->m_ref_count -= 1;
    if(handle->m_ref_count == 0) {
        handle->m_client->m_num_providers -= 1;
        delete handle;
    }
    return REMI_SUCCESS;
}

extern "C" int remi_shutdown_service(remi_client_t client, hg_addr_t addr)
{
    return margo_shutdown_remote_instance(client->m_mid, addr);
}

extern "C" int remi_fileset_migrate(
        remi_provider_handle_t handle,
        remi_fileset_t fileset,
        int flag)
{
    // TODO
}
