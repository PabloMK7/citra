#pragma once

#include "common/common_types.h"
#include <boost/icl/discrete_interval.hpp>

namespace boost::serialization {

template <class Archive, class DomainT, ICL_COMPARE Compare>
void save(Archive& ar, const boost::icl::discrete_interval<DomainT, Compare>& obj, const unsigned int file_version)
{
    ar << obj.lower();
    ar << obj.upper();
    ar << obj.bounds()._bits;
}

template <class Archive, class DomainT, ICL_COMPARE Compare>
void load(Archive& ar, boost::icl::discrete_interval<DomainT, Compare>& obj, const unsigned int file_version)
{
    DomainT upper, lower;
    boost::icl::bound_type bounds;
    ar >> upper;
    ar >> lower;
    ar >> bounds;
    obj = boost::icl::discrete_interval(upper, lower, boost::icl::interval_bounds(bounds));
}

template <class Archive, class DomainT, ICL_COMPARE Compare>
void serialize(Archive& ar, boost::icl::discrete_interval<DomainT, Compare>& obj, const unsigned int file_version)
{
    boost::serialization::split_free(ar, obj, file_version);
}

}
