#include "boost/serialization/split_member.hpp"

#define SERIALIZE_AS_POD                                             \
    private:                                                         \
    friend class boost::serialization::access;                       \
    template<typename Archive>                                       \
    void save(Archive & ar, const unsigned int file_version) const { \
        ar.save_binary(this, sizeof(*this));                         \
    }                                                                \
    template<typename Archive>                                       \
    void load(Archive & ar, const unsigned int file_version) {       \
        ar.load_binary(this, sizeof(*this));                         \
    }                                                                \
    template<class Archive>                                          \
    void serialize(                                                  \
        Archive &ar,                                                 \
        const unsigned int file_version                              \
    ){                                                               \
        boost::serialization::split_member(ar, *this, file_version); \
    }
