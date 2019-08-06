#include "boost/archive/binary_iarchive.hpp"
#include "boost/archive/binary_oarchive.hpp"

#define SERIALIZE_IMPL(A) template void A::serialize<boost::archive::binary_iarchive>( \
    boost::archive::binary_iarchive & ar, \
    const unsigned int file_version \
); \
template void A::serialize<boost::archive::binary_oarchive>( \
    boost::archive::binary_oarchive & ar, \
    const unsigned int file_version \
);
