#ifndef __UUID_UTIL_HPP
#define __UUID_UTIL_HPP

#include <string>
#include <array>
#include <uuid/uuid.h>
#include <cereal/types/base_class.hpp>
#include <thallium/serialization/stl/array.hpp>

class uuid : public std::array<unsigned char,16> {

    public:

    uuid() {
        uuid_t id;
        uuid_generate(id);
        for(int i=0; i<16; i++) {
            (*this)[i] = id[i];
        }
    }

    uuid(const uuid& other) = default;
    uuid(uuid&& other) = default;
    uuid& operator=(const uuid& other) = default;
    uuid& operator=(uuid&& other) = default;

    template<typename T>
    friend T& operator<<(T& stream, const uuid& id);
};

template<typename T>
T& operator<<(T& stream, const uuid& id) {
    for(unsigned i=0; i<16; i++) {
        stream << (int)id[i] << " ";
    }
    return stream;
}

struct uuid_hash {

    std::size_t operator()(uuid const& id) const noexcept
    {
        std::string str((char*)(&id[0]), 16);
        return std::hash<std::string>()(str);
    }
};

#endif
