/*
 * (C) 2018 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <abt-io.h>
#include <uuid/uuid.h>
#include <thallium.hpp>
#include <thallium/serialization/stl/pair.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>
#include "uuid-util.hpp"
#include "fs-util.hpp"
#include "remi/remi-client.h"
#include "remi-fileset.hpp"

namespace tl = thallium;

struct remi_client {

    margo_instance_id    m_mid = MARGO_INSTANCE_NULL;
    tl::engine*          m_engine = nullptr;
    uint64_t             m_num_providers = 0;
    tl::remote_procedure m_migrate_start_rpc;
    tl::remote_procedure m_migrate_mmap_rpc;
    tl::remote_procedure m_migrate_write_rpc;
    tl::remote_procedure m_migrate_end_rpc;
    abt_io_instance_id   m_abtio = ABT_IO_INSTANCE_NULL;

    remi_client(tl::engine* e, abt_io_instance_id abtio)
    : m_engine(e)
    , m_migrate_start_rpc(m_engine->define("remi_migrate_start"))
    , m_migrate_mmap_rpc(m_engine->define("remi_migrate_mmap"))
    , m_migrate_write_rpc(m_engine->define("remi_migrate_write"))
    , m_migrate_end_rpc(m_engine->define("remi_migrate_end"))
    , m_abtio(abtio) {}

};

struct remi_provider_handle : public tl::provider_handle {

    remi_client_t m_client    = nullptr;
    uint64_t      m_ref_count = 0;

    template<typename ... Args>
    remi_provider_handle(Args&&... args)
    : tl::provider_handle(std::forward<Args>(args)...) {}
};

extern "C" int remi_client_init(
        margo_instance_id mid,
        abt_io_instance_id abtio,
        remi_client_t* client)
{
    auto theEngine           = new tl::engine(mid);
    remi_client_t theClient  = new remi_client(theEngine, abtio);
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

extern "C" int remi_client_set_abt_io_instance(
        remi_client_t client,
        abt_io_instance_id abtio)
{
    if(client) {
        client->m_abtio = abtio;
        return REMI_SUCCESS;
    } else {
        return REMI_ERR_INVALID_ARG;
    }
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
    try {
        auto identity = theHandle->get_identity();
        if(identity != "remi") {
            delete theHandle;
            return REMI_ERR_UNKNOWN_PR;
        }
    } catch(...) {
        delete theHandle;
        return REMI_ERR_UNKNOWN_PR;
    }
    theHandle->m_client = client;
    theHandle->m_ref_count = 1;
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

static void list_existing_files(const char* filename, void* uargs) {
    auto files = static_cast<std::set<std::string>*>(uargs);
    files->emplace(filename);
}

static int migrate_using_mmap(
        remi_provider_handle_t ph,
        remi_fileset_t fileset,
        const std::set<std::string>& files,
        const std::string& remote_root,
        int* status);

static int migrate_using_abtio(
        remi_provider_handle_t ph,
        remi_fileset_t fileset,
        const std::set<std::string>& files,
        const std::string& remote_root,
        int* status);

extern "C" int remi_fileset_migrate(
        remi_provider_handle_t ph,
        remi_fileset_t fileset,
        const char* remote_root,
        int remove_source,
        int mode,
        int* status)
{
    int ret;

    if(ph == REMI_PROVIDER_HANDLE_NULL
    || fileset == REMI_FILESET_NULL
    || remote_root == NULL)
        return REMI_ERR_INVALID_ARG;
    if(remote_root[0] != '/')
        return REMI_ERR_INVALID_ARG;

    std::string theRemoteRoot(remote_root);
    if(theRemoteRoot[theRemoteRoot.size()-1] != '/')
        theRemoteRoot += "/";

    // find the set of files to migrate from the fileset
    std::set<std::string> files;
    remi_fileset_walkthrough(fileset, list_existing_files,
            static_cast<void*>(&files));

    if(mode == REMI_USE_MMAP) {
        ret = migrate_using_mmap(ph, fileset, files, theRemoteRoot.c_str(), status);
    } else {
        ret = migrate_using_abtio(ph, fileset, files, theRemoteRoot.c_str(), status);
    }

    if(ret != REMI_SUCCESS) {
        return ret;
    }

    if(*status != 0) {
        return REMI_ERR_USER;
    }

    if(remove_source == REMI_REMOVE_SOURCE) {
        for(auto& filename : files) {
            auto theFilename = fileset->m_root + filename;
            remove(theFilename.c_str());
        }
        for(auto& dirname : fileset->m_directories) {
            auto theDirname = fileset->m_root + dirname;
            removeRec(theDirname);
        }
    }

    return REMI_SUCCESS;
}

int migrate_using_mmap(
        remi_provider_handle_t ph,
        remi_fileset_t fileset,
        const std::set<std::string>& files,
        const std::string& remote_root,
        int* status)
{
    // expose the data
    std::vector<std::pair<void*,std::size_t>> theData;
    std::vector<std::size_t> theSizes;
    std::vector<mode_t> theModes;


    // prepare lambda for cleaning up mapped files
    auto cleanup = [&theData]() {
        for(auto& seg : theData) {
            munmap(seg.first, seg.second);
        }
    };

    for(auto& filename : files) {
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
            return REMI_ERR_IO;
        }
        auto size = st.st_size;
        theSizes.push_back(size);
        auto mode = st.st_mode;
        theModes.push_back(mode);
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
        // indicate sequential access
        madvise(segment, size, MADV_SEQUENTIAL);
        // close file descriptor
        close(fd);
        // insert the segment
        theData.emplace_back(segment, size);
    }

    // expose the segments for bulk operations
    tl::bulk localBulk;
    if(theData.size() != 0) 
        localBulk = ph->m_client->m_engine->expose(theData, tl::bulk_mode::read_only);

    // create a copy of the fileset where m_directory is empty
    // and the filenames in directories have been resolved
    auto tmp_files = std::move(fileset->m_files);
    auto tmp_dirs  = std::move(fileset->m_directories);
    auto tmp_root  = std::move(fileset->m_root);
    fileset->m_files = files;
    fileset->m_directories = decltype(fileset->m_directories)();
    fileset->m_root = remote_root;

    // call migrate_start RPC
    // the response is in the form <errorcode, userstatus, uuid>
    std::tuple<int32_t, int32_t, uuid> start_call_result 
        = ph->m_client->m_migrate_start_rpc.on(*ph)(*fileset, theSizes, theModes);
    int ret = std::get<0>(start_call_result);
    if(ret != REMI_SUCCESS) {
        cleanup();
        if(ret == REMI_ERR_USER)
            *status = std::get<1>(start_call_result);
        return ret;
    }
    auto& operation_id = std::get<2>(start_call_result);

    // send the migrate_mmap RPC
    ret = ph->m_client->m_migrate_mmap_rpc.on(*ph)(operation_id, localBulk);

    // put back the fileset's original members
    fileset->m_root        = std::move(tmp_root);
    fileset->m_files       = std::move(tmp_files);
    fileset->m_directories = std::move(tmp_dirs);

    if(ret != REMI_SUCCESS) {
        cleanup();
        return ret;
    }

    // xfer went ok, now send migrate_end rpc.
    // the response is in the form <errorcode, userstatus>
    std::pair<int32_t, int32_t> end_call_result =
        ph->m_client->m_migrate_end_rpc.on(*ph)(operation_id);

    cleanup();

    ret = end_call_result.first;
    if(ret == REMI_ERR_USER) {
        *status = end_call_result.second;
    } else {
        *status = 0;
    }

    return ret;
}

int migrate_using_abtio(
        remi_provider_handle_t ph,
        remi_fileset_t fileset,
        const std::set<std::string>& files,
        const std::string& remote_root,
        int* status)
{
    // expose the data
    std::vector<int> openedFileDescriptors;
    std::vector<std::pair<void*,std::size_t>> theData;
    std::vector<std::size_t> theSizes;
    std::vector<mode_t> theModes;

    auto cleanup = [&openedFileDescriptors]() {
        for(auto& fd : openedFileDescriptors) {
            close(fd);
        }
    };

    for(auto& filename : files) {
        // compose full name
        auto theFilename = fileset->m_root + filename;
        // open file
        int fd = open(theFilename.c_str(), O_RDONLY, 0);
        if(fd == -1) {
            cleanup();
            return REMI_ERR_UNKNOWN_FILE;
        }
        openedFileDescriptors.push_back(fd);
        // get file size
        struct stat st;
        if(0 != fstat(fd, &st)) {
            cleanup();
            return REMI_ERR_IO;
        }
        auto size = st.st_size;
        theSizes.push_back(size);
        auto mode = st.st_mode;
        theModes.push_back(mode);
    }

    // create a copy of the fileset where m_directory is empty
    // and the filenames in directories have been resolved
    auto tmp_files = std::move(fileset->m_files);
    auto tmp_dirs  = std::move(fileset->m_directories);
    auto tmp_root  = std::move(fileset->m_root);
    fileset->m_files = files;
    fileset->m_directories = decltype(fileset->m_directories)();
    fileset->m_root = remote_root;

    // call migrate_start RPC
    // the response is in the form <errorcode, userstatus, uuid>
    std::tuple<int32_t, int32_t, uuid> start_call_result 
        = ph->m_client->m_migrate_start_rpc.on(*ph)(*fileset, theSizes, theModes);
    int ret = std::get<0>(start_call_result);
    if(ret != REMI_SUCCESS) {
        cleanup();
        if(ret == REMI_ERR_USER)
            *status = std::get<1>(start_call_result);
        return ret;
    }
    // put back the fileset's original members
    fileset->m_root        = std::move(tmp_root);
    fileset->m_files       = std::move(tmp_files);
    fileset->m_directories = std::move(tmp_dirs);

    auto& operation_id = std::get<2>(start_call_result);

    auto abtio = ph->m_client->m_abtio;

    // send a series of migrate_write RPC, pipelined with abti-io pread calls
    size_t max_chunk_size = fileset->m_xfer_size;

    if(abtio == ABT_IO_INSTANCE_NULL) {

        std::vector<char> buffer(max_chunk_size);
        for(uint32_t i = 0; i < files.size(); i++) {
            size_t remaining_size = theSizes[i];
            int fd = openedFileDescriptors[i];
            size_t current_offset = 0;
            while(remaining_size != 0) {
                size_t chunk_size = remaining_size < max_chunk_size ? remaining_size : max_chunk_size;
                buffer.resize(chunk_size);
                auto sizeRead = read(fd, &buffer[0], chunk_size);
                ret = ph->m_client->m_migrate_write_rpc.on(*ph)(operation_id, i, current_offset, buffer);
                current_offset += chunk_size;
                remaining_size -= chunk_size;
            }
        }

    } else {

        size_t current_chunk_offset  = 0; // offset of the chunk that ABT-IO has to read in this iteration
        size_t previous_chunk_offset = 0; // offset of the chunk that should be send in this iteration
        size_t current_chunk_size    = 0; // size of the chunk that ABT-IO has to read in this iteration
        size_t previous_chunk_size = 0; // size of the chunk that should be send in this iteration
        std::vector<char> current_buffer(max_chunk_size);  // buffer for ABT-IO to place data
        std::vector<char> previous_buffer(max_chunk_size); // buffer to be sent

        for(uint32_t i = 0; i < files.size(); i++) {
            // reset variables
            current_chunk_offset  = 0;
            previous_chunk_offset = 0;
            current_chunk_size    = 0;
            previous_chunk_size   = 0;

            size_t remaining_size = theSizes[i];
            int fd = openedFileDescriptors[i];
            // read first chunk
            current_chunk_size = remaining_size < max_chunk_size ? remaining_size : max_chunk_size;
            current_buffer.resize(current_chunk_size);
            auto sizeRead = abt_io_pread(abtio, fd, &current_buffer[0], current_chunk_size, current_chunk_offset);
            // swap variables
            previous_chunk_offset = current_chunk_offset;
            previous_chunk_size = current_chunk_size;
            std::swap(current_buffer, previous_buffer);
            current_chunk_offset += current_chunk_size;
            remaining_size -= current_chunk_size;

            // read remaining chunks and send in parallel
            bool can_stop = false;
            while(!can_stop) {
                // issue RPC for the previous chunk
                auto async_req = ph->m_client->m_migrate_write_rpc.on(*ph).async(operation_id, i, previous_chunk_offset, previous_buffer);
                // read the current chunk
                if(remaining_size == 0) {
                    can_stop = true;
                } else {
                    current_chunk_size = remaining_size < max_chunk_size ? remaining_size : max_chunk_size;
                    current_buffer.resize(current_chunk_size);
                    auto sizeRead = abt_io_pread(abtio, fd, &current_buffer[0], current_chunk_size, current_chunk_offset);
                }
                // wait for the RPC to finish
                ret = async_req.wait();
                if(ret != REMI_SUCCESS)
                    can_stop = true;
                if(!can_stop) {
                    previous_chunk_offset = current_chunk_offset;
                    previous_chunk_size = current_chunk_size;
                    current_chunk_offset += current_chunk_size;
                    remaining_size -= current_chunk_size;
                    std::swap(current_buffer, previous_buffer);
                }
            }

            if(ret != REMI_SUCCESS) {
                break;
            }
        }

    }

    if(ret != REMI_SUCCESS) {
        cleanup();
        return ret;
    }

    // xfer went ok, now send migrate_end rpc.
    // the response is in the form <errorcode, userstatus>
    std::pair<int32_t, int32_t> end_call_result =
        ph->m_client->m_migrate_end_rpc.on(*ph)(operation_id);

    cleanup();

    ret = end_call_result.first;
    if(ret == REMI_ERR_USER) {
        *status = end_call_result.second;
    } else {
        *status = 0;
    }

    return ret;
}
