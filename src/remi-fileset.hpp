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
    std::string                       m_root;
    std::map<std::string,std::string> m_metadata;
    std::set<std::string>             m_files;
    std::set<std::string>             m_directories;

    template<typename A>
    void serialize(A& ar) {
        ar & m_class;
        ar & m_root;
        ar & m_metadata;
        ar & m_files;
        ar & m_directories;
    }
};

#endif
