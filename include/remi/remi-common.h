/*
 * (C) 2018 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __REMI_COMMON_H
#define __REMI_COMMON_H

#include <margo.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define REMI_KEEP_SOURCE   0    /* Keep the source files/directories */
#define REMI_REMOVE_SOURCE 1    /* Remove the source files/directories */

#define REMI_USE_MMAP  2 /* Use mmap-ed files to issue transfers (good for memory-based storage) */
#define REMI_USE_ABTIO 4 /* Use ABT-IO to pipeline read/write with data transfers (good for disks) */

#define REMI_SUCCESS             0 /* Success */
#define REMI_ERR_ALLOCATION     -1 /* Error allocating something */
#define REMI_ERR_INVALID_ARG    -2 /* An argument is invalid */
#define REMI_ERR_MERCURY        -3 /* An error happened calling a Mercury function */
#define REMI_ERR_UNKNOWN_CLASS  -4 /* Database refered to by id is not known to provider */
#define REMI_ERR_UNKNOWN_FILE   -5 /* File not found in fileset or in file system */
#define REMI_ERR_UNKNOWN_PR     -6 /* Provider id could not be matched with a provider */
#define REMI_ERR_UNKNOWN_META   -7 /* Unknown metadata entry */
#define REMI_ERR_SIZE           -8 /* Client did not allocate enough for the requested data */
#define REMI_ERR_MIGRATION      -9 /* Error during data migration */
#define REMI_ERR_CLASS_EXISTS  -10 /* Migration class already registered */
#define REMI_ERR_FILE_EXISTS   -11 /* File already exists */
#define REMI_ERR_IO            -12 /* Error in I/O (stat, open, etc.) call */
#define REMI_ERR_USER          -13 /* User-defined error reported in "status" argument */
#define REMI_ERR_INVALID_OPID  -14 /* Invalid UUID operation identifier received */

/**
 * @brief Fileset type.
 */
typedef struct remi_fileset* remi_fileset_t;
#define REMI_FILESET_NULL ((remi_fileset_t)0)

/**
 * @brief Fileset callback type. This callback takes a file's name
 * as first parameter and a pointer to user-provided arguments as the
 * second parameter. It is used by the remi_fileset_foreach_file()
 * function.
 */
typedef void (*remi_fileset_callback_t)(const char*, void*);
#define REMI_FILESET_CALLBACK_NULL ((remi_fileset_callback_t)0)

/**
 * @brief Fileset metadata callback type. This callback takes a key
 * (string), a value (string) and a pointer to user-provided arguments.
 * It is used by the remi_fileset_foreach_metadata() function.
 */
typedef void (*remi_metadata_callback_t)(const char*, const char*, void*);
#define REMI_METADATA_CALLBACK_NULL ((remi_metadata_callback_t)0)

/**
 * @brief Creates a new empty fileset. The fileset's root directory
 * must be an absolute path.
 *
 * @param[in] fileset_class Class of the fileset.
 * @param[in] fileset_root Root directory.
 * @param[out] fileset Resulting fileset.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_fileset_create(
        const char* fileset_class,
        const char* fileset_root,
        remi_fileset_t* fileset);

/**
 * @brief Set a provider ID for the fileset.
 *
 * @param fileset Fileset.
 * @param provider_id provider ID.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_fileset_set_provider_id(
        remi_fileset_t fileset, uint16_t provider_id);

/**
 * @brief Frees a fileset. Passing a REMI_FILESET_NULL fileset
 * to this function is valid.
 *
 * @param fileset Fileset to free.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_fileset_free(remi_fileset_t fileset);

/**
 * @brief Gets the class of the fileset. If buf is NULL, this
 * function will set the size to the size required to hold the
 * class name. If buf is not NULL, size should contain the
 * maximum number of bytes available in the buffer. After the call,
 * the buffer will contain the fileset's class name, and size will
 * be set to the size of this class name (including null-terminator).
 *
 * @param[in] fileset Fileset from which to get the class name.
 * @param[out] buf Buffer to hold the name.
 * @param[inout] size Size of the buffer/name.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_fileset_get_class(
        remi_fileset_t fileset,
        char* buf,
        size_t* size);

/**
 * @brief Get the provider ID associated with the the fileset.
 */
int remi_fileset_get_provider_id(
        remi_fileset_t fileset,
        uint16_t* provider_id);

/**
 * @brief Gets the root of the fileset. If buf is NULL, this
 * function will set the size to the size required to hold the
 * root. If buf is not NULL, size should contain the
 * maximum number of bytes available in the buffer. After the call,
 * the buffer will contain the fileset's root, and size will
 * be set to the size of this root (including null-terminator).
 *
 * @param[in] fileset Fileset from which to get the root.
 * @param[out] buf Buffer to hold the root.
 * @param[inout] size Size of the buffer/root.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_fileset_get_root(
        remi_fileset_t fileset,
        char* buf,
        size_t* size);

/**
 * @brief Change the root of the fileset.
 *
 * @param fileset Fileset in which to change the root.
 * @param path New root path.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_fileset_set_root(
        remi_fileset_t fileset,
        const char* path);

/**
 * @brief Sets the transfer size for this fileset. This attribute
 * has an effect only if the fileset is migrated with the REMI_USE_ABTIO
 * option. It determins the maximum size of data an RPC is allowed to
 * transfer at once.
 *
 * @param[in] fileset Fileset for which to set the xfer size.
 * @param[in] size New size.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_fileset_set_xfer_size(
        remi_fileset_t fileset,
        size_t size);

/**
 * @brief Gets the transfer size for this fileset.
 *
 * @param[in] fileset Fileset for which to get the xfer size.
 * @param[out] size resulting size.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_fileset_get_xfer_size(
        remi_fileset_t fileset,
        size_t* size);

/**
 * @brief Registers a file in the fileset. The provided path
 * should be relative to the fileset's root. The file does not need
 * to exist at the moment it is being registered (it needs to exist
 * when migrating the fileset).
 *
 * @param fileset Fileset in which to register the file.
 * @param filename File name.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_fileset_register_file(
        remi_fileset_t fileset,
        const char* filename);

/**
 * @brief Registers a directory in the fileset. The provided path
 * should be relative to the fileset's root. The directory does not need
 * to exist at the moment it is being registered (it needs to exist when
 * migrating the fileset). When migrating the fileset, all the files and
 * subdirectories in this directory will be migrated.
 *
 * @param fileset Fileset in which to register the directory.
 * @param dirname Directory path.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_fileset_register_directory(
        remi_fileset_t fileset,
        const char* dirname);

/**
 * @brief Deregisters a file from the fileset. This deregisters
 * the file only if the file has been added using remi_fileset_register_file.
 * If an entire directory has been registered, individual files in the
 * directory cannot be deregistered.
 *
 * @param fileset Fileset from which to deregister the file.
 * @param filename File name.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_fileset_deregister_file(
        remi_fileset_t fileset,
        const char* filename);

/**
 * @brief Deregisters a directory from the fileset. This deregisters
 * the directory only if the directory has been added using
 * remi_fileset_register_directory. If individual files in the directory
 * have been added independently, this will not deregister them.
 *
 * @param fileset Fileset from which to deregister the directory.
 * @param dirname Directory name.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_fileset_deregister_directory(
        remi_fileset_t fileset,
        const char* dirname);

/**
 * @brief Iterates over all the files in a fileset in alphabetical
 * order and call the provided callback on the file's name and the
 * provided user-arguments.
 *
 * @param fileset Fileset on which to iterate.
 * @param callback Callback to call on each file.
 * @param uargs User-provided arguments (may be NULL).
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_fileset_foreach_file(
        remi_fileset_t fileset,
        remi_fileset_callback_t callback,
        void* uargs);

/**
 * @brief Iterates over all the directories registered in a fileset
 * in alphabetical order and call the provided callback on the directory's
 * name and the provided user-arguments.
 *
 * @param fileset Fileset on which to iterate.
 * @param callback Callback to call on each directory.
 * @param uargs User-provided arguments (may be NULL).
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_fileset_foreach_directory(
        remi_fileset_t fileset,
        remi_fileset_callback_t callback,
        void* uargs);

/**
 * @brief Iterate over all the files and directories registered in
 * a fileset and call the callback on all files that actually exist.
 *
 * @param fileset Fileset on which to iterate.
 * @param callback Callback to call on each existing file.
 * @param uargs User-provided arguments (may be NULL).
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_fileset_walkthrough(
        remi_fileset_t fileset,
        remi_fileset_callback_t callback,
        void* uargs);

/**
 * @brief Registers a metadata (key/value pair of strings) in the
 * fileset. If a metadata already exists with the provided key,
 * its value is overwritten.
 *
 * @param fileset Fileset in which to set the metadata.
 * @param key Key.
 * @param value Value.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_fileset_register_metadata(
        remi_fileset_t fileset,
        const char* key,
        const char* value);

/**
 * @brief Deregisters a metadata from the fileset.
 *
 * @param fileset Fileset in which to deregister the metadata.
 * @param key Key.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_fileset_deregister_metadata(
        remi_fileset_t fileset,
        const char* key);

/**
 * @brief Get the metadata associated with a given key.
 * The caller should NOT free the resulting pointer.
 *
 * @param fileset Fileset.
 * @param key Metadata key.
 * @param value Resultig value.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_fileset_get_metadata(
        remi_fileset_t fileset,
        const char* key,
        const char** value);

/**
 * @brief Iterates over the key/value pairs and call the
 * provided callback on each of them and on the user-provided argument.
 *
 * @param fileset Fileset on which to iterate over the metadata.
 * @param callback Callback to call on the key/value pairs.
 * @param uargs User-provided argument.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_fileset_foreach_metadata(
        remi_fileset_t fileset,
        remi_metadata_callback_t callback,
        void* uargs);


/**
 * @brief Walk through the files pointed to by this fileset and
 * compute the total size of the fileset.
 *
 * @param fileset Fileset on which to compute the size.
 * @param include_metadata Whether to include metadata or not.
 * @param size Resulting size.
 *
 * @return REMI_SUCCESS or error code defined in remi-common.h.
 */
int remi_fileset_compute_size(
        remi_fileset_t fileset,
        int include_metadata,
        size_t* size);

#if defined(__cplusplus)
}
#endif

#endif
