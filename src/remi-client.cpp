#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
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
        remi_provider_handle_t ph,
        remi_fileset_t fileset,
        const char* remote_root,
        int flag)
{

    if(ph == REMI_PROVIDER_HANDLE_NULL
    || fileset == REMI_FILESET_NULL
    || remote_root == NULL)
        return REMI_ERR_INVALID_ARG;
    if(remote_root[0] != '/')
        return REMI_ERR_INVALID_ARG;

    std::string theRemoteRoot(remote_root);
    if(theRemoteRoot[theRemoteRoot.size()-1] != '/')
        theRemoteRoot += "/";

    // expose the data
    std::vector<std::pair<void*,std::size_t>> theData;
    std::vector<std::size_t> theSizes;

    // prepare lambda for cleanin up mapped files
    auto cleanup = [&theData]() {
        for(auto& seg : theData) {
            munmap(seg.first, seg.second);
        }
    };

    for(auto& filename : fileset->m_files) {
        // compose full name
        auto theFilename = fileset->m_root + filename;
        // open file
        int fd = open(theFilename.c_str(), O_RDONLY, 0);
        if(fd == -1) {
            cleanup();
            return REMI_ERR_UNKNOWN_FILE;
        }
        // get file size
        struct stat st;
        if(0 != fstat(fd, &st)) {
            close(fd);
            cleanup();
            return REMI_ERR_STAT;
        }
        auto size = st.st_size;
        theSizes.push_back(size);
        if(size == 0) {
            close(fd);
            continue;
        }
        // map the file
        void* segment = mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if(segment == NULL) {
            cleanup();
            return REMI_ERR_ALLOCATION;
        }
        // close file descriptor
        close(fd);
        // insert the segment
        theData.emplace_back(segment, size);
    }

    // expose the segments for bulk operations
    auto localBulk = ph->m_client->m_engine->expose(theData, tl::bulk_mode::read_only);

    // send the RPC
    auto localRoot = fileset->m_root;
    fileset->m_root = theRemoteRoot;
    int32_t result = ph->m_client->m_migrate_rpc.on(*ph)(*fileset, theSizes, localBulk);
    fileset->m_root = localRoot;

    cleanup();

    if(result != REMI_SUCCESS) {
        return result;
    }

    if(flag == REMI_REMOVE_SOURCE) {
        for(auto& filename : fileset->m_files) {
            auto theFilename = fileset->m_root + filename;
            remove(theFilename.c_str());
        }
    }

    return REMI_SUCCESS;
}
