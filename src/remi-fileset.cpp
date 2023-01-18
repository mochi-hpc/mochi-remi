/*
 * (C) 2018 The University of Chicago
 * 
 * See COPYRIGHT in top-level directory.
 */
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <cstring>
#include "remi/remi-common.h"
#include "fs-util.hpp"
#include "remi-fileset.hpp"

extern "C" int remi_fileset_create(
        const char* fileset_class,
        const char* fileset_root,
        remi_fileset_t* fileset) 
{
    if(fileset_class == NULL || fileset_root == NULL)
        return REMI_ERR_INVALID_ARG;
    if(fileset_root[0] != '/')
        return REMI_ERR_INVALID_ARG;
    auto fs = new remi_fileset;
    fs->m_class = fileset_class;
    fs->m_root  = fileset_root;
    if(fs->m_root[fs->m_root.size()-1] != '/') {
        fs->m_root += "/";
    }
    *fileset = fs;
    return REMI_SUCCESS;
}

extern "C" int remi_fileset_free(remi_fileset_t fileset)
{
    if(fileset == REMI_FILESET_NULL)
        return REMI_SUCCESS;
    delete fileset;
    return REMI_SUCCESS;
}

extern "C" int remi_fileset_set_xfer_size(
        remi_fileset_t fileset,
        size_t size)
{
    if(fileset == REMI_FILESET_NULL
    || size == 0)
        return REMI_ERR_INVALID_ARG;
    fileset->m_xfer_size = size;
    return REMI_SUCCESS;
}

extern "C" int remi_fileset_get_xfer_size(
        remi_fileset_t fileset,
        size_t* size)
{
    if(fileset == REMI_FILESET_NULL
    || size == nullptr)
        return REMI_ERR_INVALID_ARG;
    *size = fileset->m_xfer_size;
    return REMI_SUCCESS;
}

extern "C" int remi_fileset_get_class(
        remi_fileset_t fileset,
        char* buf,
        size_t* size)
{
    if(fileset == REMI_FILESET_NULL)
        return REMI_ERR_INVALID_ARG;
    if(buf == nullptr) {
        *size = fileset->m_class.size()+1;
        return REMI_SUCCESS;
    } else if(*size < fileset->m_class.size()+1) {
        return REMI_ERR_SIZE;
    } else {
        std::memcpy(buf, fileset->m_class.c_str(), fileset->m_class.size()+1);
        *size = fileset->m_class.size()+1;
    }
    return REMI_SUCCESS;
}

extern "C" int remi_fileset_get_root(
        remi_fileset_t fileset,
        char* buf,
        size_t* size)
{

    if(fileset == REMI_FILESET_NULL)
        return REMI_ERR_INVALID_ARG;
    if(buf == nullptr) {
        *size = fileset->m_root.size()+1;
        return REMI_SUCCESS;
    } else if(*size < fileset->m_root.size()+1) {
        return REMI_ERR_SIZE;
    } else {
        std::strcpy(buf, fileset->m_root.c_str());
        *size = fileset->m_root.size()+1;
    }
    return REMI_SUCCESS;
}

extern "C" int remi_fileset_set_root(
        remi_fileset_t fileset,
        const char* root)
{
    if(fileset == REMI_FILESET_NULL)
        return REMI_ERR_INVALID_ARG;
    fileset->m_root = root ? root : "";
    return REMI_SUCCESS;
}

extern "C" int remi_fileset_register_file(
        remi_fileset_t fileset,
        const char* filename)
{
    if(fileset == REMI_FILESET_NULL || filename == NULL)
        return REMI_ERR_INVALID_ARG;
    unsigned i = 0;
    while(filename[i] == '/') i += 1;
    fileset->m_files.insert(filename+i);
    return REMI_SUCCESS;
}

extern "C" int remi_fileset_deregister_file(
        remi_fileset_t fileset,
        const char* filename)
{
    if(fileset == REMI_FILESET_NULL || filename == NULL)
        return REMI_ERR_INVALID_ARG;
    unsigned i = 0;
    while(filename[i] == '/') i += 1;
    auto it = fileset->m_files.find(filename+i);
    if(it == fileset->m_files.end())
        return REMI_ERR_UNKNOWN_FILE;
    else
        fileset->m_files.erase(filename+i);
    return REMI_SUCCESS;
}

extern "C" int remi_fileset_foreach_file(
        remi_fileset_t fileset,
        remi_fileset_callback_t callback,
        void* uargs)
{
    if(fileset == REMI_FILESET_NULL || callback == NULL)
        return REMI_ERR_INVALID_ARG;
    for(auto& filename : fileset->m_files) {
        callback(filename.c_str(), uargs);
    }
    return REMI_SUCCESS;
}

extern "C" int remi_fileset_register_directory(
        remi_fileset_t fileset,
        const char* dirname)
{
    if(fileset == REMI_FILESET_NULL || dirname == NULL)
        return REMI_ERR_INVALID_ARG;
    unsigned i = 0;
    while(dirname[i] == '/') i += 1;
    fileset->m_directories.insert(dirname+i);
    return REMI_SUCCESS;
}

extern "C" int remi_fileset_deregister_directory(
        remi_fileset_t fileset,
        const char* dirname)
{
    if(fileset == REMI_FILESET_NULL || dirname == NULL)
        return REMI_ERR_INVALID_ARG;
    unsigned i = 0;
    while(dirname[i] == '/') i += 1;
    auto it = fileset->m_directories.find(dirname+i);
    if(it == fileset->m_directories.end())
        return REMI_ERR_UNKNOWN_FILE;
    else
        fileset->m_directories.erase(dirname+i);
    return REMI_SUCCESS;
}

extern "C" int remi_fileset_foreach_directory(
        remi_fileset_t fileset,
        remi_fileset_callback_t callback,
        void* uargs)
{
    if(fileset == REMI_FILESET_NULL || callback == NULL)
        return REMI_ERR_INVALID_ARG;
    for(auto& dirname : fileset->m_directories) {
        callback(dirname.c_str(), uargs);
    }
    return REMI_SUCCESS;
}

extern "C" int remi_fileset_walkthrough(
        remi_fileset_t fileset,
        remi_fileset_callback_t callback,
        void* uargs)
{
    if(fileset == REMI_FILESET_NULL || callback == NULL)
        return REMI_ERR_INVALID_ARG;
    std::set<std::string> files;
    for(const auto& filename : fileset->m_files) {
        files.insert(filename);
    }
    for(const auto& dirname : fileset->m_directories) {
        listFiles(fileset->m_root, dirname, [&files](const std::string& filename) {
            files.insert(filename);
        });
    }
    for(const auto& filename : files) {
        callback(filename.c_str(), uargs);
    }
    return REMI_SUCCESS;
}

extern "C" int remi_fileset_register_metadata(
        remi_fileset_t fileset,
        const char* key,
        const char* value)
{
    if(fileset == REMI_FILESET_NULL || key == NULL)
        return REMI_ERR_INVALID_ARG;
    fileset->m_metadata[key] = value;
    return REMI_SUCCESS;
}

extern "C" int remi_fileset_deregister_metadata(
        remi_fileset_t fileset,
        const char* key)
{
    if(fileset == REMI_FILESET_NULL || key == NULL)
        return REMI_ERR_INVALID_ARG;
    auto it = fileset->m_metadata.find(key);
    if(it == fileset->m_metadata.end())
        return REMI_ERR_UNKNOWN_META;
    fileset->m_metadata.erase(it);
    return REMI_SUCCESS;
}

extern "C" int remi_fileset_foreach_metadata(
        remi_fileset_t fileset,
        remi_metadata_callback_t callback,
        void* uargs)
{
    if(fileset == REMI_FILESET_NULL || callback == NULL)
        return REMI_ERR_INVALID_ARG;
    for(auto& meta : fileset->m_metadata) {
        callback(meta.first.c_str(), meta.second.c_str(), uargs);
    }
    return REMI_SUCCESS;
}

struct add_size_args {
    size_t         size;
    remi_fileset_t fileset;
    int            ret;
};

static void add_metadata_size(const char* key, const char* value, void* uargs) {
    add_size_args* args = static_cast<add_size_args*>(uargs);
    args->size += strlen(key) + strlen(value) + 2;
}

static void add_file_size(const char* filename, void* uargs) {
    add_size_args* args = static_cast<add_size_args*>(uargs);
    if(args->ret != 0) return;
    std::string theFilename = args->fileset->m_root + filename;
    int fd = open(theFilename.c_str(), O_RDONLY, 0);
    if(fd == -1) {
        args->ret = -1;
        return;
    }
    struct stat st;
    if(0 != fstat(fd, &st)) {
        close(fd);
        args->ret = -1;
        return;
    }
    close(fd);
    args->size += st.st_size;
}

extern "C" int remi_fileset_compute_size(
        remi_fileset_t fileset,
        int include_metadata,
        size_t* size)
{
    add_size_args args;
    args.size = 0;
    args.ret  = 0;
    args.fileset = fileset;
    int ret = remi_fileset_walkthrough(fileset, add_file_size, static_cast<void*>(&args));
    if(ret != REMI_SUCCESS)
        return ret;
    if(args.ret != 0) {
        return REMI_ERR_IO;
    }
    if(include_metadata) {
        ret = remi_fileset_foreach_metadata(fileset, add_metadata_size, static_cast<void*>(&args));
    }
    if(ret != REMI_SUCCESS)
        return ret;
    *size = args.size;
    return REMI_SUCCESS;
}
