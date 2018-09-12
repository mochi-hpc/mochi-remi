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
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <iostream>
#include <unordered_map>
#include <thallium.hpp>
#include <thallium/serialization/stl/pair.hpp>
#include "remi/remi-server.h"
#include "remi-fileset.hpp"
#include "fs-util.hpp"

namespace tl = thallium;

struct migration_class {
    remi_migration_callback_t m_before_callback;
    remi_migration_callback_t m_after_callback;
    remi_uarg_free_t          m_free;
    void*                     m_uargs;
};

struct remi_provider : public tl::provider<remi_provider> {

    std::unordered_map<std::string, migration_class>    m_migration_classes;
    tl::engine*                                         m_engine;
    tl::pool&                                           m_pool;
    static std::unordered_map<uint16_t, remi_provider*> m_registered_providers;

    void migrate(
            const tl::request& req,
            remi_fileset& fileset,
            const std::vector<std::size_t>& filesizes,
            tl::bulk& remote_bulk) 
    {
        // pair of <returnvalue, status>
        std::pair<int32_t,int32_t> result = {0, 0};

        // check that the class of the fileset exists
        if(m_migration_classes.count(fileset.m_class) == 0) {
            result.first = REMI_ERR_UNKNOWN_CLASS;
            req.respond(result);
            return;
        }

        // check if any of the target files already exist
        // (we don't want to overwrite)
        for(const auto& filename : fileset.m_files) {
            auto theFilename = fileset.m_root + filename;
            if(access(theFilename.c_str(), F_OK) != -1) {
                result.first = REMI_ERR_FILE_EXISTS;
                req.respond(result);
                return;
            }
        }
        // alright, none of the files already exist

        // call the "before migration" callback
        auto& klass = m_migration_classes[fileset.m_class];
        if(klass.m_before_callback != nullptr) {
            result.second = klass.m_before_callback(&fileset, klass.m_uargs);
        }
        if(result.second != 0) {
            result.first = REMI_ERR_USER;
            req.respond(result);
            return;
        }

        std::vector<std::pair<void*,std::size_t>> theData;

        // function to cleanup the segments
        auto cleanup = [&theData]() {
            for(auto& seg : theData) {
                munmap(seg.first, seg.second);
            }
        };

        // create files, truncate them, and expose them with mmap
        unsigned i=0;
        size_t totalSize = 0;
        for(const auto& filename : fileset.m_files) {
            auto theFilename = fileset.m_root + filename;
            auto p = theFilename.find_last_of('/');
            auto theDir = theFilename.substr(0, p);
            mkdirs(theDir.c_str());
            totalSize += filesizes[i];
            int fd = open(theFilename.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0600);
            if(fd == -1) {
                cleanup();
                result.first = REMI_ERR_IO;
                req.respond(result);
                return;
            }
            if(filesizes[i] == 0) {
                i += 1;
                close(fd);
                continue;
            }
            if(ftruncate(fd, filesizes[i]) == -1) {
                cleanup();
                result.first = REMI_ERR_IO;
                req.respond(result);
                return;
            }
            void *segment = mmap(0, filesizes[i], PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
            if(segment == NULL) {
                close(fd);
                cleanup();
                result.first = REMI_ERR_IO;
                req.respond(result);
                return;
            }
            theData.emplace_back(segment, filesizes[i]);
            i += 1;
        }

        // create a local bulk handle to expose the segments
        auto localBulk = m_engine->expose(theData, tl::bulk_mode::write_only);

        // issue bulk transfer
        size_t transferred = remote_bulk.on(req.get_endpoint()) >> localBulk;

        if(transferred != totalSize) {
            // XXX we should cleanup the files that were created
            result.first = REMI_ERR_MIGRATION;
            req.respond(result);
            return;
        }

        for(auto& seg : theData) {
            if(msync(seg.first, seg.second, MS_SYNC) == -1) {
                // XXX we should cleanup the files that were created
                cleanup();
                result.first = REMI_ERR_IO;
                req.respond(result);
                return;
            }
        }

        cleanup();

        // call the "after" migration callback associated with the class of fileset
        if(klass.m_after_callback != nullptr) {
            result.second = klass.m_after_callback(&fileset, klass.m_uargs);
        }
        result.first = result.second == 0 ? REMI_SUCCESS : REMI_ERR_USER;
        req.respond(result);
        return;
    }

    remi_provider(tl::engine* e, uint16_t provider_id, tl::pool& pool)
    : tl::provider<remi_provider>(*e, provider_id), m_engine(e), m_pool(pool) {
        define("remi_migrate", &remi_provider::migrate, pool);
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
        uint16_t provider_id,
        ABT_pool pool,
        remi_provider_t* provider)
{
    auto thePool   = tl::pool(pool);
    auto theEngine = new tl::engine(mid, THALLIUM_SERVER_MODE);
    auto theProvider = new remi_provider(theEngine, provider_id, thePool);
    margo_push_finalize_callback(mid, on_finalize, theProvider);
    *provider = theProvider;
    return REMI_SUCCESS;
}

extern "C" int remi_provider_registered(
        margo_instance_id mid,
        uint16_t provider_id,
        int* flag,
        ABT_pool* pool,
        remi_provider_t* provider)
{
    auto it = remi_provider::m_registered_providers.find(provider_id);
    if(it == remi_provider::m_registered_providers.end()) {
        *pool = ABT_POOL_NULL;
        *provider = REMI_PROVIDER_NULL;
        *flag = 0;
    } else {
        remi_provider* p = it->second;
        if(provider) *provider = p;
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
