/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include "remi/remi-client.h"
#include "remi/remi-server.h"
#include <bedrock/AbstractComponent.hpp>

namespace tl = thallium;

class RemiReceiverComponent : public bedrock::AbstractComponent {

    remi_provider_t m_provider;

    public:

    RemiReceiverComponent(const tl::engine& engine,
                          uint16_t  provider_id,
                          const tl::pool& pool,
                          abt_io_instance_id abtio)
    {
        int ret = remi_provider_register(
                engine.get_margo_instance(),
                abtio,
                provider_id,
                pool.native_handle(),
                &m_provider);
        if(ret != REMI_SUCCESS)
            throw bedrock::Exception{
                "Could not create REMI provider: remi_provider_register returned {}", ret};
    }

    ~RemiReceiverComponent() {
        remi_provider_destroy(m_provider);
    }

    void* getHandle() override {
        return static_cast<void*>(m_provider);
    }

    std::string getConfig() override {
        return "{}";
    }

    static std::shared_ptr<bedrock::AbstractComponent>
        Register(const bedrock::ComponentArgs& args) {
            tl::pool pool;
            auto it = args.dependencies.find("pool");
            if(it != args.dependencies.end() && !it->second.empty()) {
                pool = it->second[0]->getHandle<tl::pool>();
            }
            it = args.dependencies.find("abt_io");
            abt_io_instance_id abt_io = ABT_IO_INSTANCE_NULL;
            if(it != args.dependencies.end() && !it->second.empty()) {
                auto component = it->second[0]->getHandle<bedrock::ComponentPtr>();
                abt_io = reinterpret_cast<abt_io_instance_id>(component->getHandle());
            }
            return std::make_shared<RemiReceiverComponent>(
                args.engine, args.provider_id, pool, abt_io);
        }

    static std::vector<bedrock::Dependency>
        GetDependencies(const bedrock::ComponentArgs& args) {
            (void)args;
            std::vector<bedrock::Dependency> dependencies{
                bedrock::Dependency{
                    /* name */ "pool",
                    /* type */ "pool",
                    /* is_required */ false,
                    /* is_array */ false,
                    /* is_updatable */ false
                },
                bedrock::Dependency{
                    /* name */ "abt_io",
                    /* type */ "abt_io",
                    /* is_required */ false,
                    /* is_array */ false,
                    /* is_updatable */ false
                }
            };
            return dependencies;
        }
};

BEDROCK_REGISTER_COMPONENT_TYPE(remi_receiver, RemiReceiverComponent)

class RemiSenderComponent : public bedrock::AbstractComponent {

    remi_client_t m_client;

    public:

    RemiSenderComponent(const tl::engine& engine,
                        uint16_t  provider_id,
                        abt_io_instance_id abtio)
    {
        int ret = remi_client_init(
                engine.get_margo_instance(),
                abtio,
                &m_client);
        if(ret != REMI_SUCCESS)
            throw bedrock::Exception{
                "Could not create REMI provider: remi_client_init returned {}", ret};
    }

    ~RemiSenderComponent() {
        remi_client_finalize(m_client);
    }

    void* getHandle() override {
        return static_cast<void*>(m_client);
    }

    std::string getConfig() override {
        return "{}";
    }

    static std::shared_ptr<bedrock::AbstractComponent>
        Register(const bedrock::ComponentArgs& args) {
            auto it = args.dependencies.find("abt_io");
            abt_io_instance_id abt_io = ABT_IO_INSTANCE_NULL;
            if(it != args.dependencies.end() && !it->second.empty()) {
                auto component = it->second[0]->getHandle<bedrock::ComponentPtr>();
                abt_io = reinterpret_cast<abt_io_instance_id>(component->getHandle());
            }
            return std::make_shared<RemiSenderComponent>(
                args.engine, args.provider_id, abt_io);
        }

    static std::vector<bedrock::Dependency>
        GetDependencies(const bedrock::ComponentArgs& args) {
            (void)args;
            std::vector<bedrock::Dependency> dependencies{
                bedrock::Dependency{
                    /* name */ "abt_io",
                    /* type */ "abt_io",
                    /* is_required */ false,
                    /* is_array */ false,
                    /* is_updatable */ false
                }
            };
            return dependencies;
        }
};

BEDROCK_REGISTER_COMPONENT_TYPE(remi_sender, RemiSenderComponent)
