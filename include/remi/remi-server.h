#ifndef __REMI_SERVER_H
#define __REMI_SERVER_H

#include <remi/remi-common.h>
#include <margo.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct remi_provider* remi_provider_t;
#define REMI_PROVIDER_NULL ((remi_provider_t)0)

typedef void (*remi_migration_callback_t)(remi_provider_t, remi_fileset_t, void*);
#define REMI_MIGRATION_CALLBACK_NULL ((remi_migration_callback_t)0)

#if defined(__cplusplus)
}
#endif

#endif
