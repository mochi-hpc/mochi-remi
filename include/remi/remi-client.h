/*
 * (C) 2018 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __REMI_CLIENT_H
#define __REMI_CLIENT_H

#include <remi/remi-common.h>
#include <abt-io.h>
#include <margo.h>

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief REMI client type.
 */
typedef struct remi_client* remi_client_t;
#define REMI_CLIENT_NULL ((remi_client_t)0)

/**
 * @brief REMI provider handle type.
 */
typedef struct remi_provider_handle* remi_provider_handle_t;
#define REMI_PROVIDER_HANDLE_NULL ((remi_provider_handle_t)0)

/**
 * @brief Initializes a REMI client.
 *
 * @param[in] mid Margo instance.
 * @param[in] abtio ABT-IO instance.
 * @param[out] client Resulting client.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_client_init(
        margo_instance_id mid,
        abt_io_instance_id abtio,
        remi_client_t* client);

/**
 * @brief Finalizes a REMI client.
 *
 * @param client REMI client.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_client_finalize(remi_client_t client);

/**
 * @brief Creates a REMI provider handle.
 *
 * @param[in] client REMI client.
 * @param[in] addr Mercury address of the provider.
 * @param[in] provider_id Provider id.
 * @param[out] handle Resulting provider handle.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_provider_handle_create(
        remi_client_t client,
        hg_addr_t addr,
        uint16_t provider_id,
        remi_provider_handle_t* handle);

/**
 * @brief Increments the reference counter of the provider handle.
 *
 * @param[in] handle Provider handle.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_provider_handle_ref_incr(remi_provider_handle_t handle);

/**
 * @brief Releases the provider handle. This function will decrement
 * the reference count and free the provider handle if the reference
 * count reaches 0.
 *
 * @param[in] handle Provider handle.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_provider_handle_release(remi_provider_handle_t handle);

/**
 * @brief Helper function wrapping margo_shutdown_remote_instance.
 *
 * @param[in] client REMI client.
 * @param[in] addr Address of the remote instance to shutdown.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_shutdown_service(remi_client_t client, hg_addr_t addr);

/**
 * @brief Migrates a fileset to a remote provider. All the files
 * of the local fileset will be transfered over RDMA to the destination
 * provider and a remote fileset will be created with the provided
 * remote root. If flag is set to REMI_REMOVE_SOURCE, the original
 * files will be destroyed.
 *
 * @param handle Provider handle of the target provider.
 * @param fileset Fileset to migrate.
 * @param remote_root Root of the fileset when migrated.
 * @param remove_source REMI_REMOVE_SOURCE or REMI_KEEP_SOURCE.
 * @param mode REMI_USE_MMAP or REMI_USE_ABTIO.
 * @param status Value returned by the user-defined migration callbacks.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_fileset_migrate(
        remi_provider_handle_t handle,
        remi_fileset_t fileset,
        const char* remote_root,
        int remove_source,
        int mode,
        int* status);

/**
 * @brief Sets the ABT-IO instance to use for I/O.
 *
 * @param client Client.
 * @param abtio ABT-IO instance.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_client_set_abt_io_instance(
        remi_client_t client,
        abt_io_instance_id abtio);

#if defined(__cplusplus)
}
#endif

#endif
