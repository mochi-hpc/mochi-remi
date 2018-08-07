#ifndef __REMI_CLIENT_H
#define __REMI_CLIENT_H

#include <remi/remi-common.h>
#include <margo.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct remi_client* remi_client_t;
#define REMI_CLIENT_NULL ((remi_client_t)0)

typedef struct remi_provider_handle* remi_provider_handle_t;
#define REMI_PROVIDER_HANDLE_NULL ((remi_provider_handle_t)0)

int remi_client_init(margo_instance_id mid, remi_client_t* client);

int remi_client_finalize(remi_client_t client);

int remi_provider_handle_create(
        remi_client_t client,
        hg_addr_t addr,
        uint16_t provider_id,
        remi_provider_handle_t* handle);

int remi_provider_handle_ref_incr(remi_provider_handle_t handle);

int remi_provider_handle_release(remi_provider_handle_t handle);

int remi_shutdown_service(remi_client_t client, hg_addr_t addr);

int remi_fileset_migrate(
        remi_provider_handle_t handle,
        remi_fileset_t fileset,
        int flag);

#if defined(__cplusplus)
}
#endif

#endif
