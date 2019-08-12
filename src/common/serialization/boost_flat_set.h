#pragma once

#include "common/common_types.h"
#include <boost/container/flat_set.hpp>
#include <boost/serialization/split_free.hpp>

namespace boost::serialization {

template <class Archive, class T>
void save(Archive& ar, const boost::container::flat_set<T>& set, const unsigned int file_version)
{
    ar << static_cast<u64>(set.size());
    for (auto &v : set) {
        ar << v;
    }
}

template <class Archive, class T>
void load(Archive& ar, boost::container::flat_set<T>& set, const unsigned int file_version)
{
    u64 count{};
    ar >> count;
    set.clear();
    for (auto i = 0; i < count; i++) {
        T value{};
        ar >> value;
        set.insert(value);
    }
}

template <class Archive, class T>
void serialize(Archive& ar, boost::container::flat_set<T>& set, const unsigned int file_version)
{
    boost::serialization::split_free(ar, set, file_version);
}

}
