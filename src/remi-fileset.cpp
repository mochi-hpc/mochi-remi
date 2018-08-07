#include <cstring>
#include "remi/remi-common.h"
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
