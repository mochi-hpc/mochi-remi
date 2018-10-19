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

struct operation {
    remi_fileset             m_fileset;
    std::vector<std::size_t> m_filesizes;
    std::vector<mode_t>      m_modes;
    std::vector<int>         m_fds;
};

struct remi_provider : public tl::provider<remi_provider> {

    std::unordered_map<std::string, migration_class>    m_migration_classes;
    tl::engine*                                         m_engine;
    tl::pool&                                           m_pool;
    abt_io_instance_id                                  m_abtio;
    std::unordered_map<uuid, operation, uuid_hash>      m_op_in_progress;

    static std::unordered_map<uint16_t, remi_provider*> m_registered_providers;

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
        if(m_migration_classes.count(fileset.m_class) == 0) {
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
        auto& klass = m_migration_classes[fileset.m_class];
        if(klass.m_before_callback != nullptr) {
            std::get<1>(result) = klass.m_before_callback(&fileset, klass.m_uargs);
        }
        if(std::get<1>(result) != 0) {
            std::get<0>(result) = REMI_ERR_USER;
            req.respond(result);
            return;
        }

        // create and open the files
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
        auto& op       = m_op_in_progress[std::get<2>(result)];
        op.m_fileset   = std::move(fileset);
        op.m_filesizes = std::move(filesizes);
        op.m_modes     = std::move(theModes);
        op.m_fds       = std::move(openedFileDescriptors);

        req.respond(result);
    }

    void migrate_mmap(
            const tl::request& req,
            const uuid& operation_id,
            tl::bulk& remote_bulk) 
    {
        int ret;
        // get the operation associated with the operation id
        auto it = m_op_in_progress.find(operation_id);
        if(it == m_op_in_progress.end()) {
            ret = REMI_ERR_INVALID_OPID;
            req.respond(ret);
            return;
        }
        auto& op = it->second;
        // we found the operation, let's mmap some files!

        std::vector<std::pair<void*,std::size_t>> theData;

        // function to cleanup the segments
        auto cleanup = [this, &theData, &it](bool error) {
            for(auto& seg : theData) {
                munmap(seg.first, seg.second);
            }
            if(error) {
                this->m_op_in_progress.erase(it);
            }
        };

        // compute total file size
        size_t totalSize = 0;
        for(auto& s : op.m_filesizes)
            totalSize += s;

        // create files, truncate them, and expose them with mmap
        unsigned i=0;
        for(int fd : op.m_fds) {
            if(op.m_filesizes[i] == 0) {
                i += 1;
                continue;
            }
            if(ftruncate(fd, op.m_filesizes[i]) == -1) {
                cleanup(true);
                ret = REMI_ERR_IO;
                req.respond(ret);
                return;
            }
            void *segment = mmap(0, op.m_filesizes[i], PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
            if(segment == NULL) {
                cleanup(true);
                ret = REMI_ERR_IO;
                req.respond(ret);
                return;
            }
            madvise(segment, op.m_filesizes[i], MADV_SEQUENTIAL);
            theData.emplace_back(segment, op.m_filesizes[i]);
            i += 1;
        }

        // create a local bulk handle to expose the segments
        auto localBulk = m_engine->expose(theData, tl::bulk_mode::write_only);

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
                // XXX we should cleanup the files that were created
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

    void migrate_end(const tl::request& req, const uuid& operation_id) 
    {
        // the result of this RPC should be a pair <errorcode, userstatus>
        std::pair<int32_t, int32_t> result = {0,0};

        // get the operation associated with the operation id
        auto it = m_op_in_progress.find(operation_id);
        if(it == m_op_in_progress.end()) {
            result.first = REMI_ERR_INVALID_OPID;
            req.respond(result);
            return;
        }
        auto& op = it->second;

        // close all the file descriptors
        for(int fd : op.m_fds) {
            close(fd);
        }

        // find the class of migration
        auto& klass = m_migration_classes[op.m_fileset.m_class];

        // call the "after" migration callback associated with the class of fileset
        if(klass.m_after_callback != nullptr) {
            result.second = klass.m_after_callback(&(op.m_fileset), klass.m_uargs);
        }
        result.first = result.second == 0 ? REMI_SUCCESS : REMI_ERR_USER;
        req.respond(result);

        m_op_in_progress.erase(it);

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
        auto it = m_op_in_progress.find(operation_id);
        if(it == m_op_in_progress.end()) {
            ret = REMI_ERR_INVALID_OPID;
            req.respond(ret);
            return;
        }
        auto& op = it->second;
        // we found the operation, let's open some files!

        std::vector<int> openedFileDescriptors;

        // function to cleanup everything in case of error
        auto cleanup = [this, &openedFileDescriptors, &it](bool error) {
            for(auto& fd : openedFileDescriptors) {
                close(fd);
            }
            if(error) {
                this->m_op_in_progress.erase(it);
            }
        };

        // check the RPC's target file index
        // and the size of the file
        if(fileNumber >= op.m_fds.size()) {
            ret = REMI_ERR_IO;
            cleanup(true);
            req.respond(ret);
            return;
        }
        if(op.m_filesizes[fileNumber] < writeOffset + data.size()) {
            ret = REMI_ERR_IO;
            cleanup(true);
            req.respond(ret);
            return;
        }
        // write the chunk received
        int fd = op.m_fds[fileNumber];
        auto s = pwrite(fd, data.data(), data.size(), writeOffset);

        if(s != data.size()) {
            ret = REMI_ERR_IO;
            cleanup(true);
            req.respond(ret);
            return;
        }

        ret = REMI_SUCCESS;
        req.respond(ret);
        return;
    }

    remi_provider(tl::engine* e, abt_io_instance_id abtio, uint16_t provider_id, tl::pool& pool)
    : tl::provider<remi_provider>(*e, provider_id), m_engine(e), m_pool(pool), m_abtio(abtio) {
        define("remi_migrate_start", &remi_provider::migrate_start, pool);
        define("remi_migrate_mmap", &remi_provider::migrate_mmap, pool);
        define("remi_migrate_write", &remi_provider::migrate_write, pool);
        define("remi_migrate_end", &remi_provider::migrate_end, pool);
        m_registered_providers[provider_id] = this;
    }

    ~remi_provider() {
        m_registered_providers.erase(get_provider_id());
    }

};

std::unordered_map<uint16_t, remi_provider*> remi_provider::m_registered_providers;

static void on_finalize(void* uargs) {
    auto provider = static_cast<remi_provider_t>(uargs);
    for(auto& klass : provider->m_migration_classes) {
        if(klass.second.m_free != nullptr) {
            klass.second.m_free(klass.second.m_uargs);
        }
    }
    delete provider->m_engine;
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
    auto theEngine = new tl::engine(mid, THALLIUM_SERVER_MODE);
    auto theProvider = new remi_provider(theEngine, abtio, provider_id, thePool);
    margo_push_finalize_callback(mid, on_finalize, theProvider);
    *provider = theProvider;
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
    auto it = remi_provider::m_registered_providers.find(provider_id);
    if(it == remi_provider::m_registered_providers.end()) {
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

extern "C" int remi_provider_register_migration_class(
        remi_provider_t provider,
        const char* class_name,
        remi_migration_callback_t before_callback,
        remi_migration_callback_t after_callback,
        remi_uarg_free_t free_fn,
        void* uargs)
{
    if(provider == REMI_PROVIDER_NULL || class_name == NULL)
        return REMI_ERR_INVALID_ARG;
    if(provider->m_migration_classes.count(class_name) != 0)
        return REMI_ERR_CLASS_EXISTS;
    auto& klass = provider->m_migration_classes[class_name];
    klass.m_before_callback = before_callback;
    klass.m_after_callback = after_callback;
    klass.m_uargs = uargs;
    klass.m_free = free_fn;
    return REMI_SUCCESS;
}
