/*
 * (C) 2018 The University of Chicago
 * 
 * See COPYRIGHT in top-level directory.
 */
#ifndef __FS_UTIL
#define __FS_UTIL

#include <sys/stat.h>
#include <limits.h>
#include <string>
#include <iostream>
#include <functional>
#include <dirent.h>

inline void mkdirs(const char *dir) {
    std::string tmp(dir);
    char *p = NULL;
    size_t len = tmp.size();

    if(tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for(p = (char*)tmp.c_str() + 1; *p; p++)
        if(*p == '/') {
            *p = 0;
            mkdir(tmp.c_str(), S_IRWXU);
            *p = '/';
        }
    mkdir(tmp.c_str(), S_IRWXU);
}

template<typename F>
void listFiles(const std::string& root, const std::string &path, F&& cb) {
    auto fullpath = root + "/" + path;
    if(auto dir = opendir(fullpath.c_str())) {
        while(auto f = readdir(dir)) {
            if(!f->d_name || f->d_name[0] == '.') continue;
            if(f->d_type == DT_DIR) 
                listFiles(root, path + f->d_name + "/", std::forward<F>(cb));

            if(f->d_type == DT_REG)
                cb(path + f->d_name);
        }
        closedir(dir);
    }
}

inline void removeRec(const std::string &path) {
    if(auto dir = opendir(path.c_str())) {
        while(auto f = readdir(dir)) {
            if(!f->d_name || f->d_name[0] == '.') continue;
            if(f->d_type == DT_DIR) {
                removeRec(path + f->d_name + "/");
            } else {
                remove((path+f->d_name).c_str());
            }
        }
        closedir(dir);
    }
    remove(path.c_str());
}
#endif
