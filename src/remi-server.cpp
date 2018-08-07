#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <thallium.hpp>
#include "remi/remi-server.h"
#include "remi-fileset.hpp"
#include "fs-util.h"

namespace tl = thallium;

struct migration_class {
    remi_migration_callback_t m_callback;
    void*                     m_uargs;
};

struct remi_provider : public tl::provider<remi_provider> {

    std::unordered_map<std::string, migration_class> m_migration_classes;
    tl::engine*                                      m_engine;

    int32_t migrate(
            const tl::request& req,
            remi_fileset& fileset,
            const std::vector<std::size_t>& filesizes,
            tl::bulk& remote_bulk) 
    {
        // check that the class of the fileset exists
        if(m_migration_classes.count(fileset.m_class) == 0) {
            return REMI_ERR_UNKNOWN_CLASS;
        }

        // check if any of the target files already exist
        // (we don't want to overwrite)
        for(const auto& filename : fileset.m_files) {
            auto theFilename = fileset.m_root + filename;
            if(access(theFilename.c_str(), F_OK) != -1)
                return REMI_ERR_FILE_EXISTS;
        }
        // alright, none of the files already exist

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
                return REMI_ERR_IO;
            }
            if(filesizes[i] == 0) {
                i += 1;
                close(fd);
                continue;
            }
            if(ftruncate(fd, filesizes[i]) == -1) {
                cleanup();
                return REMI_ERR_IO;
            }
            void *segment = mmap(0, filesizes[i], PROT_WRITE, MAP_PRIVATE, fd, 0);
            if(segment == NULL) {
                close(fd);
                cleanup();
                return REMI_ERR_IO;
            }
            theData.emplace_back(segment, filesizes[i]);
            i += 1;
        }
        return REMI_SUCCESS;

        // create a local bulk handle to expose the segments
        auto localBulk = m_engine->expose(theData, tl::bulk_mode::write_only);

        // issue bulk transfer
        size_t transferred = remote_bulk.on(req.get_endpoint()) >> localBulk;

        cleanup();

        if(transferred != totalSize) {
            // XXX we should cleanup the files that were created
            return REMI_ERR_MIGRATION;
        }

        for(auto& seg : theData) {
            if(msync(seg.first, seg.second, MS_SYNC) == -1) {
                // XXX we should cleanup the files that were created
                cleanup();
                return REMI_ERR_IO;
            }
        }

        // call the migration callback associated with the class of fileset
        auto& klass = m_migration_classes[fileset.m_class];
        if(klass.m_callback != nullptr) {
            klass.m_callback(&fileset, klass.m_uargs);
        }
    }

    remi_provider(tl::engine* e, uint16_t provider_id, tl::pool& pool)
    : tl::provider<remi_provider>(*e, provider_id) {
        define("remi_migrate", &remi_provider::migrate);
    } 

};

static void on_finalize(void* uargs) {
    auto provider = static_cast<remi_provider_t>(uargs);
    delete provider->m_engine;
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

extern "C" int remi_provider_register_migration_class(
        remi_provider_t provider,
        const char* class_name,
        remi_migration_callback_t callback,
        void* uargs)
{
    if(provider == REMI_PROVIDER_NULL || class_name == NULL)
        return REMI_ERR_INVALID_ARG;
    if(provider->m_migration_classes.count(class_name) != 0)
        return REMI_ERR_CLASS_EXISTS;
    auto& klass = provider->m_migration_classes[class_name];
    klass.m_callback = callback;
    klass.m_uargs = uargs;
    return REMI_SUCCESS;
}
