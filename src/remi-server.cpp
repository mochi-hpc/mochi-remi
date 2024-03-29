/*
 * (C) 2018 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <iostream>
#include <unordered_map>
#include <abt-io.h>
#include <thallium.hpp>
#include <thallium/serialization/stl/pair.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>
#include <thallium/serialization/stl/tuple.hpp>
#include "remi/remi-server.h"
#include "remi-fileset.hpp"
#include "fs-util.hpp"
#include "uuid-util.hpp"

namespace tl = thallium;

struct migration_class {
    remi_migration_callback_t m_before_callback;
    remi_migration_callback_t m_after_callback;
    remi_uarg_free_t          m_free;
    void*                     m_uargs;
};

struct device {
    int                    m_type;
    tl::mutex              m_mutex;

    void lock() {
        if(m_type != REMI_DEVICE_HDD)
            return;
        m_mutex.lock();
    }

    void unlock() {
        if(m_type != REMI_DEVICE_HDD)
            return;
        m_mutex.unlock();
    }
};

struct operation {
    remi_fileset             m_fileset;
    std::vector<std::size_t> m_filesizes;
    std::vector<mode_t>      m_modes;
    std::vector<int>         m_fds;
    tl::mutex                m_mutex;
    int                      m_error = REMI_SUCCESS;
};

struct class_key {
    std::string name;
    uint16_t provider_id;

    bool operator==(const class_key& other) const {
        return name == other.name && provider_id == other.provider_id;
    }
};

struct class_key_hash {
    inline size_t operator()(const class_key& x) const {
        return std::hash<std::string>()(x.name) ^ std::hash<uint16_t>()(x.provider_id);
    }
};

struct remi_provider : public tl::provider<remi_provider> {

    tl::engine                                                      m_engine;
    std::unordered_map<class_key, migration_class, class_key_hash>  m_migration_classes;
    tl::pool&                                                       m_pool;
    abt_io_instance_id                                              m_abtio;
    std::unordered_map<uuid, std::unique_ptr<operation>, uuid_hash> m_op_in_progress;
    tl::mutex                                                       m_op_in_progress_mtx;
    tl::auto_remote_procedure                                       m_migration_start_rpc;
    tl::auto_remote_procedure                                       m_migration_mmap_rpc;
    tl::auto_remote_procedure                                       m_migration_write_rpc;
    tl::auto_remote_procedure                                       m_migration_end_rpc;

    static std::unordered_map<uint16_t, remi_provider*> s_registered_providers;

    void migrate_start(
            const tl::request& req,
            remi_fileset& fileset,
            std::vector<std::size_t>& filesizes,
            std::vector<mode_t>& theModes)
    {
        // tuple of <returnvalue, userstatus, uuid>
        std::tuple<int32_t,int32_t,uuid> result;
        std::get<0>(result) = 0;
        std::get<1>(result) = 1;
        // uuid is initialized at random, which is what we want

        // check that the class of the fileset exists
        auto key = class_key{fileset.m_class, fileset.m_provider_id};
        if(m_migration_classes.count(key) == 0) {
            std::get<0>(result) = REMI_ERR_UNKNOWN_CLASS;
            req.respond(result);
            return;
        }

        // check if any of the target files already exist
        // (we don't want to overwrite)
        for(const auto& filename : fileset.m_files) {
            auto theFilename = fileset.m_root + filename;
            if(access(theFilename.c_str(), F_OK) != -1) {
                std::get<0>(result) = REMI_ERR_FILE_EXISTS;
                req.respond(result);
                return;
            }
        }
        // alright, none of the files already exist

        // call the "before migration" callback
        auto& klass = m_migration_classes[key];
        if(klass.m_before_callback != nullptr) {
            std::get<1>(result) = klass.m_before_callback(&fileset, klass.m_uargs);
        }
        if(std::get<1>(result) != 0) {
            std::get<0>(result) = REMI_ERR_USER;
            req.respond(result);
            return;
        }

        // create and open the files, record the device they belong to
        std::vector<int> openedFileDescriptors;
        unsigned i=0;
        for(const auto& filename : fileset.m_files) {
            auto theFilename = fileset.m_root + filename;
            auto p = theFilename.find_last_of('/');
            auto theDir = theFilename.substr(0, p);
            mkdirs(theDir.c_str());
            int fd = open(theFilename.c_str(), O_RDWR | O_CREAT | O_TRUNC, theModes[i]);
            if(fd == -1) {
                for(auto ffd : openedFileDescriptors)
                    close(ffd);
                std::get<0>(result) = REMI_ERR_IO;
                req.respond(result);
                return;
            }
            i += 1;
            openedFileDescriptors.push_back(fd);
        }
        // store the operation into the map of pending operations
        {
            std::lock_guard<tl::mutex> guard(m_op_in_progress_mtx);
            auto r = m_op_in_progress.insert(std::make_pair(std::get<2>(result), std::make_unique<operation>()));
            auto& op        = r.first->second;
            op->m_fileset   = std::move(fileset);
            op->m_filesizes = std::move(filesizes);
            op->m_modes     = std::move(theModes);
            op->m_fds       = std::move(openedFileDescriptors);
        }

        req.respond(result);
    }

    void migrate_end(const tl::request& req, const uuid& operation_id)
    {
        // the result of this RPC should be a pair <errorcode, userstatus>
        std::pair<int32_t, int32_t> result = {0,0};

        // get the operation associated with the operation id
        operation* op = nullptr;
        {
            std::lock_guard<tl::mutex> guard(m_op_in_progress_mtx);
            auto it = m_op_in_progress.find(operation_id);
            if(it == m_op_in_progress.end()) {
                result.first = REMI_ERR_INVALID_OPID;
                req.respond(result);
                return;
            }
            op = it->second.get();
        }

        {
            std::lock_guard<tl::mutex> guard(op->m_mutex);

            // close all the file descriptors
            for(int fd : op->m_fds) {
                close(fd);
            }

            if(op->m_error != REMI_SUCCESS) {
                result.first = op->m_error;
                req.respond(result);
                std::lock_guard<tl::mutex> guard(m_op_in_progress_mtx);
                m_op_in_progress.erase(operation_id);
                return;
            }

            // find the class of migration
            auto key = class_key{op->m_fileset.m_class, op->m_fileset.m_provider_id};
            auto& klass = m_migration_classes[key];

            // call the "after" migration callback associated with the class of fileset
            if(klass.m_after_callback != nullptr) {
                result.second = klass.m_after_callback(&(op->m_fileset), klass.m_uargs);
            }
            result.first = result.second == 0 ? REMI_SUCCESS : REMI_ERR_USER;
            req.respond(result);

        }

        {
            std::lock_guard<tl::mutex> guard(m_op_in_progress_mtx);
            m_op_in_progress.erase(operation_id);
        }

        return;
    }

    void migrate_mmap(
            const tl::request& req,
            const uuid& operation_id,
            tl::bulk& remote_bulk)
    {
        int ret;
        // get the operation associated with the operation id
        operation* op = nullptr;
        {
            std::lock_guard<tl::mutex> guard(m_op_in_progress_mtx);
            auto it = m_op_in_progress.find(operation_id);
            if(it == m_op_in_progress.end()) {
                ret = REMI_ERR_INVALID_OPID;
                req.respond(ret);
                return;
            }
            op = it->second.get();
        }
        // we found the operation, let's mmap some files!

        std::vector<std::pair<void*,std::size_t>> theData;

        // function to cleanup the segments
        auto cleanup = [this, &theData, &operation_id](bool error) {
            for(auto& seg : theData) {
                munmap(seg.first, seg.second);
            }
            if(error) {
                std::lock_guard<tl::mutex> guard(m_op_in_progress_mtx);
                m_op_in_progress.erase(operation_id);
            }
        };

        // compute total file size
        size_t totalSize = 0;
        for(auto& s : op->m_filesizes)
            totalSize += s;

        // create files, truncate them, and expose them with mmap
        unsigned i=0;
        for(int fd : op->m_fds) {
            if(op->m_filesizes[i] == 0) {
                i += 1;
                continue;
            }
            if(ftruncate(fd, op->m_filesizes[i]) == -1) {
                std::cerr << "remi-server.cpp: ftruncate() line "
                    << __LINE__ << " failed with errno " << errno << std::endl;
                cleanup(true);
                ret = REMI_ERR_IO;
                req.respond(ret);
                return;
            }
            void *segment = mmap(0, op->m_filesizes[i], PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
            if(segment == NULL) {
                std::cerr << "remi-server.cpp: mmap() line "
                    << __LINE__ << " failed with errno " << errno << std::endl;
                cleanup(true);
                ret = REMI_ERR_IO;
                req.respond(ret);
                return;
            }
            madvise(segment, op->m_filesizes[i], MADV_SEQUENTIAL);
            theData.emplace_back(segment, op->m_filesizes[i]);
            i += 1;
        }

        // create a local bulk handle to expose the segments
        auto localBulk = get_engine().expose(theData, tl::bulk_mode::write_only);

        // issue bulk transfer
        size_t transferred = remote_bulk.on(req.get_endpoint()) >> localBulk;

        if(transferred != totalSize) {
            // XXX we should cleanup the files that were created
            ret = REMI_ERR_MIGRATION;
            req.respond(ret);
            return;
        }

        for(auto& seg : theData) {
            if(msync(seg.first, seg.second, MS_SYNC) == -1) {
                cleanup(true);
                ret = REMI_ERR_IO;
                req.respond(ret);
                return;
            }
        }

        cleanup(false);
        ret = REMI_SUCCESS;
        req.respond(ret);
        return;
    }

    void migrate_write(
            const tl::request& req,
            const uuid& operation_id,
            uint32_t fileNumber,
            size_t writeOffset,
            const std::vector<char>& data)
    {
        int ret;
        // get the operation associated with the operation id
        operation* op = nullptr;
        {
            std::lock_guard<tl::mutex> guard(m_op_in_progress_mtx);
            auto it = m_op_in_progress.find(operation_id);
            if(it == m_op_in_progress.end()) {
                ret = REMI_ERR_INVALID_OPID;
                req.respond(ret);
                return;
            }
            op = it->second.get();
        }

        std::lock_guard<tl::mutex> guard(op->m_mutex);

        // we found the operation, let's open some files!

        std::vector<int> openedFileDescriptors;

        // function to cleanup everything in case of error
        auto cleanup = [this, &openedFileDescriptors, &operation_id](bool error) {
            for(auto& fd : openedFileDescriptors) {
                close(fd);
            }
            if(error) {
                std::lock_guard<tl::mutex> guard(m_op_in_progress_mtx);
                this->m_op_in_progress.erase(operation_id);
            }
        };

        // check the RPC's target file index
        // and the size of the file
        if(fileNumber >= op->m_fds.size()) {
            ret = REMI_ERR_IO;
            std::cerr << "remi-server.cpp: line "
                        << __LINE__ << " failed (fileNumber >= op->m_fds.size())" << std::endl;
            cleanup(true);
            req.respond(ret);
            return;
        }
        if(op->m_filesizes[fileNumber] < writeOffset + data.size()) {
            ret = REMI_ERR_IO;
            cleanup(true);
            req.respond(ret);
            return;
        }


        // write the chunk received
        int fd = op->m_fds[fileNumber];
        ssize_t s;
        {
            // send an early response so the client can start sending the next chunk
            // in parallel while this chunk is being written
            ret = REMI_SUCCESS;
            req.respond(ret);

            if(m_abtio == ABT_IO_INSTANCE_NULL) {
                s = pwrite(fd, data.data(), data.size(), writeOffset);
            } else {
                s = abt_io_pwrite(m_abtio, fd, data.data(), data.size(), writeOffset);
            }
            if(s != data.size()) {
                op->m_error = REMI_ERR_IO;
            }
        }

        return;
    }

    remi_provider(tl::engine e, abt_io_instance_id abtio, uint16_t provider_id, tl::pool& pool)
    : tl::provider<remi_provider>(e, provider_id, "remi"), m_engine(e), m_pool(pool), m_abtio(abtio)
    , m_migration_start_rpc(define("remi_migrate_start", &remi_provider::migrate_start, pool))
    , m_migration_mmap_rpc(define("remi_migrate_mmap", &remi_provider::migrate_mmap, pool))
    , m_migration_write_rpc(define("remi_migrate_write", &remi_provider::migrate_write, pool))
    , m_migration_end_rpc(define("remi_migrate_end", &remi_provider::migrate_end, pool))
    {
        s_registered_providers[provider_id] = this;
    }

    ~remi_provider() {
        s_registered_providers.erase(get_provider_id());
    }

};

std::unordered_map<uint16_t, remi_provider*> remi_provider::s_registered_providers;

static void remi_on_finalize(void* uargs) {
    auto provider = static_cast<remi_provider_t>(uargs);
    for(auto& klass : provider->m_migration_classes) {
        if(klass.second.m_free != nullptr) {
            klass.second.m_free(klass.second.m_uargs);
        }
    }
    delete provider;
}

extern "C" int remi_provider_register(
        margo_instance_id mid,
        abt_io_instance_id abtio,
        uint16_t provider_id,
        ABT_pool pool,
        remi_provider_t* provider)
{
    auto thePool   = tl::pool(pool);
    auto theProvider = new remi_provider(tl::engine(mid), abtio, provider_id, thePool);
    margo_provider_push_finalize_callback(mid, theProvider, remi_on_finalize, theProvider);
    *provider = theProvider;
    return REMI_SUCCESS;
}

extern "C" int remi_provider_destroy(
        remi_provider_t provider)
{
    margo_provider_pop_finalize_callback(provider->m_engine.get_margo_instance(), provider);
    remi_on_finalize(provider);
    return REMI_SUCCESS;
}

extern "C" int remi_provider_get_provider_id(
        remi_provider_t provider,
        uint16_t* provider_id)
{
    if(!provider) return REMI_ERR_INVALID_ARG;
    *provider_id = provider->get_provider_id();
    return REMI_SUCCESS;
}

extern "C" int remi_provider_registered(
        margo_instance_id mid,
        uint16_t provider_id,
        int* flag,
        abt_io_instance_id* abtio,
        ABT_pool* pool,
        remi_provider_t* provider)
{
    auto it = remi_provider::s_registered_providers.find(provider_id);
    if(it == remi_provider::s_registered_providers.end()) {
        if(pool) *pool = ABT_POOL_NULL;
        if(provider) *provider = REMI_PROVIDER_NULL;
        *flag = 0;
    } else {
        remi_provider* p = it->second;
        if(provider) *provider = p;
        if(abtio) *abtio = p->m_abtio;
        if(pool) *pool = p->m_pool.native_handle();
        *flag = 1;
    }
    return REMI_SUCCESS;
}

extern "C" int remi_provider_set_abt_io_instance(
        remi_provider_t provider,
        abt_io_instance_id abtio)
{
    provider->m_abtio = abtio;
    return REMI_SUCCESS;
}

extern "C" int remi_provider_register_migration_class(
        remi_provider_t provider,
        const char* class_name,
        remi_migration_callback_t before_callback,
        remi_migration_callback_t after_callback,
        remi_uarg_free_t free_fn,
        void* uargs)
{
    return remi_provider_register_provider_migration_class(
            provider, class_name, UINT16_MAX,
            before_callback, after_callback,
            free_fn, uargs);
}

extern "C" int remi_provider_deregister_migration_class(
        remi_provider_t provider,
        const char* class_name)
{
    return remi_provider_deregister_provider_migration_class(
            provider, class_name, UINT16_MAX);
}

extern "C" int remi_provider_register_provider_migration_class(
        remi_provider_t provider,
        const char* class_name,
        uint16_t provider_id,
        remi_migration_callback_t before_callback,
        remi_migration_callback_t after_callback,
        remi_uarg_free_t free_fn,
        void* uargs)
{
    if(provider == REMI_PROVIDER_NULL || class_name == NULL)
        return REMI_ERR_INVALID_ARG;
    auto key = class_key{class_name, provider_id};
    if(provider->m_migration_classes.count(key) != 0)
        return REMI_ERR_CLASS_EXISTS;
    auto& klass = provider->m_migration_classes[key];
    klass.m_before_callback = before_callback;
    klass.m_after_callback = after_callback;
    klass.m_uargs = uargs;
    klass.m_free = free_fn;
    return REMI_SUCCESS;
}

extern "C" int remi_provider_deregister_provider_migration_class(
        remi_provider_t provider,
        const char* class_name,
        uint16_t provider_id)
{
    if(provider == REMI_PROVIDER_NULL || class_name == NULL)
        return REMI_ERR_INVALID_ARG;
    auto key = class_key{class_name, provider_id};
    if(provider->m_migration_classes.count(key) == 0)
        return REMI_ERR_UNKNOWN_CLASS;
    auto& klass = provider->m_migration_classes[key];
    if(klass.m_free) klass.m_free(klass.m_uargs);
    provider->m_migration_classes.erase(key);
    return REMI_SUCCESS;
}

extern "C" int remi_set_device(
        const char* mount_point,
        int type)
{
    return REMI_SUCCESS;
}
