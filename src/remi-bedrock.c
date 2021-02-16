/*
 * (C) 2021 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <bedrock/module.h>
#include <remi/remi-client.h>
#include <remi/remi-server.h>
#include <string.h>

static struct bedrock_dependency remi_dependencies[] = {
    { "abt_io", "abt_io", 0 },
    BEDROCK_NO_MORE_DEPENDENCIES
};

static int remi_register_provider(
        bedrock_args_t args,
        bedrock_module_provider_t* provider)
{
    margo_instance_id mid = bedrock_args_get_margo_instance(args);
    uint16_t provider_id  = bedrock_args_get_provider_id(args);
    ABT_pool pool         = bedrock_args_get_pool(args);
    const char* config    = bedrock_args_get_config(args);
    const char* name      = bedrock_args_get_name(args);
    abt_io_instance_id abt_io = ABT_IO_INSTANCE_NULL;
    if(bedrock_args_get_num_dependencies(args, "abt_io")) {
        abt_io = (abt_io_instance_id)bedrock_args_get_dependency(args, "abt_io", 0);
    }

    remi_provider_t tmp = REMI_PROVIDER_NULL;
    int ret = remi_provider_register(mid, abt_io, provider_id, pool, &tmp);
    if(ret != REMI_SUCCESS)
        return ret;

    *provider = (bedrock_module_provider_t)tmp;

    return BEDROCK_SUCCESS;
}

static int remi_deregister_provider(
        bedrock_module_provider_t provider)
{
    return remi_provider_destroy((remi_provider_t)provider);
}

static char* remi_get_provider_config(
        bedrock_module_provider_t provider) {
    (void)provider;
    return strdup("{}");
}

static int remi_init_client(
        margo_instance_id mid,
        bedrock_module_client_t* client)
{
    return remi_client_init(mid, ABT_IO_INSTANCE_NULL, (remi_client_t*)client);
}

static int remi_finalize_client(
        bedrock_module_client_t client)
{
    return remi_client_finalize((remi_client_t)client);
}

static int remi_create_provider_handle(
        bedrock_module_client_t client,
        hg_addr_t address,
        uint16_t provider_id,
        bedrock_module_provider_handle_t* ph)
{
    return remi_provider_handle_create(
            (remi_client_t)client,
            address, provider_id,
            (remi_provider_handle_t*)ph);
}

static int remi_destroy_provider_handle(
        bedrock_module_provider_handle_t ph)
{
    return remi_provider_handle_release((remi_provider_handle_t)ph);
}

static struct bedrock_module remi = {
    .register_provider       = remi_register_provider,
    .deregister_provider     = remi_deregister_provider,
    .get_provider_config     = remi_get_provider_config,
    .init_client             = remi_init_client,
    .finalize_client         = remi_finalize_client,
    .create_provider_handle  = remi_create_provider_handle,
    .destroy_provider_handle = remi_destroy_provider_handle,
    .dependencies            = remi_dependencies
};

BEDROCK_REGISTER_MODULE(remi, remi)
