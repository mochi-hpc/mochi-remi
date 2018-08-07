#ifndef __FS_UTIL
#define __FS_UTIL

#include <sys/stat.h>
#include <limits.h>

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

#endif
