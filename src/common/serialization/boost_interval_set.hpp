// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <boost/icl/interval_set.hpp>
#include "common/serialization/boost_discrete_interval.hpp"

namespace boost::serialization {

template <class Archive, class T>
void serialize(Archive& ar, boost::icl::interval_set<T>& obj, const unsigned int file_version) {
    using IntervalSet = boost::icl::interval_set<T>;
    // This works because interval_set has exactly one member of type ImplSetT
    static_assert(std::is_standard_layout_v<IntervalSet>);
    ar&*(reinterpret_cast<typename IntervalSet::ImplSetT*>(&obj));
}

} // namespace boost::serialization
