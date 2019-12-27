#include "boost/archive/binary_iarchive.hpp"
#include "boost/archive/binary_oarchive.hpp"
#include "boost/serialization/export.hpp"

using iarchive = boost::archive::binary_iarchive;
using oarchive = boost::archive::binary_oarchive;

#define SERIALIZE_IMPL(A) template void A::serialize<iarchive>( \
    iarchive & ar,                                              \
    const unsigned int file_version                             \
);                                                              \
template void A::serialize<oarchive>(                           \
    oarchive & ar,                                              \
    const unsigned int file_version                             \
);

#define SERIALIZE_EXPORT_IMPL(A) \
BOOST_SERIALIZATION_REGISTER_ARCHIVE(iarchive) \
BOOST_SERIALIZATION_REGISTER_ARCHIVE(oarchive) \
BOOST_CLASS_EXPORT_IMPLEMENT(A)
