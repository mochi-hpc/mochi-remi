#include <thallium.hpp>
#include "remi/remi-server.h"
#include "remi-fileset.hpp"

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
        // TODO
        return REMI_SUCCESS;
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
