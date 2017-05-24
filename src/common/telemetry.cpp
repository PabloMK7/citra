// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "common/telemetry.h"

namespace Telemetry {

void FieldCollection::Accept(VisitorInterface& visitor) const {
    for (const auto& field : fields) {
        field.second->Accept(visitor);
    }
}

void FieldCollection::AddField(std::unique_ptr<FieldInterface> field) {
    fields[field->GetName()] = std::move(field);
}

template <class T>
void Field<T>::Accept(VisitorInterface& visitor) const {
    visitor.Visit(*this);
}

template class Field<bool>;
template class Field<double>;
template class Field<float>;
template class Field<u8>;
template class Field<u16>;
template class Field<u32>;
template class Field<u64>;
template class Field<s8>;
template class Field<s16>;
template class Field<s32>;
template class Field<s64>;
template class Field<std::string>;
template class Field<const char*>;
template class Field<std::chrono::microseconds>;

} // namespace Telemetry
