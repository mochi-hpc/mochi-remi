/*
 * (C) 2018 The University of Chicago
 * 
 * See COPYRIGHT in top-level directory.
 */
#ifndef __REMI_SERVER_H
#define __REMI_SERVER_H

#include <remi/remi-common.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define REMI_ABT_POOL_DEFAULT ABT_POOL_NULL /* Default Argobots pool for REMI */
#define REMI_PROVIDER_ID_DEFAULT 0          /* Default provider id for REMI */


/**
 * @brief REMI provider type.
 */
typedef struct remi_provider* remi_provider_t;
#define REMI_PROVIDER_NULL ((remi_provider_t)0)

/**
 * @brief Callback called when a migration completes. This callback
 * takes the fileset that was newly created on the provider, as well
 * as a pointer to user-provided arguments.
 */
typedef void (*remi_migration_callback_t)(remi_fileset_t, void*);
#define REMI_MIGRATION_CALLBACK_NULL ((remi_migration_callback_t)0)

/**
 * @brief Registers a new REMI provider. The provider will be
 * automatically destroyed upon finalizing the margo instance.
 *
 * @param[in] mid Margo instance.
 * @param[in] provider_id Provider id.
 * @param[in] pool Argobots pool.
 * @param[out] provider Resulting provider.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_provider_register(
        margo_instance_id mid,
        uint16_t provider_id,
        ABT_pool pool,
        remi_provider_t* provider);

/**
 * @brief Registers a migration class by providing a callback
 * to call when a fileset of that class is migrated.
 *
 * @param[in] provider Provider in which to register a migration class.
 * @param[in] class_name Migration class name.
 * @param[in] callback Callback to call after migration of a fileset of this class.
 * @param[in] uargs User-argument to pass to the callback.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_provider_register_migration_class(
        remi_provider_t provider,
        const char* class_name,
        remi_migration_callback_t callback,
        void* uargs);

#if defined(__cplusplus)
}
#endif

#endif
