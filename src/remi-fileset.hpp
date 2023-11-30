/*
 * (C) 2018 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __REMI_FILESET_HPP
#define __REMI_FILESET_HPP

#include <string>
#include <map>
#include <set>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/set.hpp>
#include <thallium/serialization/stl/map.hpp>

struct remi_fileset {

    std::string                       m_class;
    uint16_t                          m_provider_id;
    std::string                       m_root;
    std::map<std::string,std::string> m_metadata;
    std::set<std::string>             m_files;
    std::set<std::string>             m_directories;
    size_t                            m_xfer_size = 1048576;

    template<typename A>
    void serialize(A& ar) {
        ar & m_class;
        ar & m_provider_id;
        ar & m_root;
        ar & m_metadata;
        ar & m_files;
        ar & m_directories;
        ar & m_xfer_size;
    }
};

#endif
