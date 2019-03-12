/*
 * (C) 2018 The University of Chicago
 * 
 * See COPYRIGHT in top-level directory.
 */
#ifndef __REMI_SERVER_H
#define __REMI_SERVER_H

#include <remi/remi-common.h>
#include <abt-io.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define REMI_ABT_POOL_DEFAULT ABT_POOL_NULL /* Default Argobots pool for REMI */
#define REMI_PROVIDER_ID_DEFAULT 0          /* Default provider id for REMI */

#define REMI_DEVICE_MEM 1 
#define REMI_DEVICE_HDD 2
#define REMI_DEVICE_SSD 3

/**
 * @brief REMI provider type.
 */
typedef struct remi_provider* remi_provider_t;
#define REMI_PROVIDER_NULL ((remi_provider_t)0)

/**
 * @brief Callback called when a migration starts or completes.
 * This callback takes the fileset that will or has been migrated, as well
 * as a pointer to user-provided arguments corresponding to the void*
 * argument given when registering the migration class.
 * The function should return 0 when successful, a non-zero value
 * otherwise. If a callback called before migration returns a non-zero
 * value, this will abort the migration. If a callback called after the
 * migration returns a non-zero value, the source of the migration will
 * not erase its original files. In all cases, the return value of this
 * function is propagated back to the source.
 */
typedef int32_t (*remi_migration_callback_t)(remi_fileset_t, void*);
#define REMI_MIGRATION_CALLBACK_NULL ((remi_migration_callback_t)0)


/**
 * @brief Type of callback called on void* user arguments when the
 * provider is destroyed.
 */
typedef void (*remi_uarg_free_t)(void*);

/**
 * @brief Registers a new REMI provider. The provider will be
 * automatically destroyed upon finalizing the margo instance.
 *
 * @param[in] mid Margo instance.
 * @param[in] abtio ABT-IO instance. May be ABT_IO_INSTANCE_NULL.
 * @param[in] provider_id Provider id.
 * @param[in] pool Argobots pool.
 * @param[out] provider Resulting provider.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_provider_register(
        margo_instance_id mid,
        abt_io_instance_id abtio,
        uint16_t provider_id,
        ABT_pool pool,
        remi_provider_t* provider);

/**
 * @brief Checks if a REMI provider with the given provider id
 * is registered. If yes, flag will be set to 1, provider
 * will be set to the registered provider, and pool will be set
 * to its associated pool. If not, flag will be
 * set to 0, pool will be set to ABT_POOL_NULL, and the provider
 * parameter will be set to REMI_PROVIDER_NULL.
 *
 * @param[in]  mid Margo instance.
 * @param[in]  provider_id Provider id.
 * @param[out] flag 1 if provider is registered, 0 otherwise.
 * @param[out] abtio ABT-IO instance used by the provider.
 * @param[out] pool Pool used to register the provider.
 * @param[out] provider Registered provider (if it exists).
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_provider_registered(
        margo_instance_id mid,
        uint16_t provider_id,
        int* flag,
        abt_io_instance_id* abtio,
        ABT_pool* pool,
        remi_provider_t* provider);

/**
 * @brief Sets the ABT-IO instance to use for I/O.
 *
 * @param provider Provider.
 * @param abtio ABT-IO instance.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_provider_set_abt_io_instance(
        remi_provider_t provider,
        abt_io_instance_id abtio);

/**
 * @brief Registers a migration class by providing a callback
 * to call when a fileset of that class is migrated.
 *
 * @param[in] provider Provider in which to register a migration class.
 * @param[in] class_name Migration class name.
 * @param[in] before_migration_cb Callback to call before migration of a fileset of this class.
 * @param[in] after_migration_cb Callback to call after migration of a fileset of this class.
 * @param[in] free_fn Function to call on uargs when the provider is destroyed.
 * @param[in] uargs User-argument to pass to the callback.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_provider_register_migration_class(
        remi_provider_t provider,
        const char* class_name,
        remi_migration_callback_t before_migration_cb,
        remi_migration_callback_t after_migration_cb,
        remi_uarg_free_t free_fn,
        void* uargs);

/**
 * @brief Set the type of device for a given mount point. Calling this function
 * gives an opportunity for REMI to optimize transfers to files in this device,
 * .e.g by using locks that are specific to this device (not shared with other
 * devices), by restraining concurrency, etc.
 *
 * @param mount_point Mount point of the device.
 * @param type Type of device (REMI_DEVICE_MEM, REMI_DEVICE_HDD, REMI_DEVICE_SSD).
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_set_device(
        const char* mount_point,
        int type);

#if defined(__cplusplus)
}
#endif

#endif
