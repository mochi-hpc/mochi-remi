#ifndef __REMI_COMMON_H
#define __REMI_COMMON_H

#if defined(__cplusplus)
extern "C" {
#endif

#define REMI_KEEP_SOURCE 0
#define REMI_REMOBE_SOURCE 1

#define REMI_SUCCESS             0 /* Success */
#define REMI_ERR_ALLOCATION     -1 /* Error allocating something */
#define REMI_ERR_INVALID_ARG    -2 /* An argument is invalid */
#define REMI_ERR_MERCURY        -3 /* An error happened calling a Mercury function */
#define REMI_ERR_UNKNOWN_CLASS  -4 /* Database refered to by id is not known to provider */
#define REMI_ERR_UNKNOWN_PR     -5 /* Mplex id could not be matched with a provider */
#define REMI_ERR_SIZE           -6 /* Client did not allocate enough for the requested data */
#define REMI_ERR_MIGRATION      -7 /* Error during data migration */

typedef struct remi_fileset* remi_fileset_t;
#define REMI_FILESET_NULL ((remi_fileset_t)0)

#if defined(__cplusplus)
}
#endif

#endif
