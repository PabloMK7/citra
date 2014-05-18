// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

// Copyright 2014 Tony Wasserka
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the owner nor the names of its contributors may
//       be used to endorse or promote products derived from this software
//       without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/*
 * Standardized way to define a group of registers and corresponding data structures. To define
 * a new register set, first define struct containing an enumeration called "Id" containing
 * all register IDs and a template union called "Struct". Specialize the Struct union for any
 * register ID which needs to be accessed in a specialized way. You can then declare the object
 * containing all register values using the RegisterSet<BaseType, DefiningStruct> type, where
 * BaseType is the underlying type of each register (e.g. u32).
 * Of course, you'll usually want to implement the Struct template such that they are of the same
 * size as BaseType. However, it's also possible to make it larger, e.g. when you want to describe
 * multiple registers with the same structure.
 *
 * Example:
 *
 *     struct Regs {
 *         enum Id : u32 {
 *             Value1 = 0,
 *             Value2 = 1,
 *             Value3 = 2,
 *             NumIds = 3
 *         };
 *
 *         // declare register definition structures
 *         template<Id id>
 *         union Struct;
 *     };
 *
 *     // Define register set object
 *     RegisterSet<u32, CommandIds> registers;
 *
 *     // define register definition structures
 *     template<>
 *     union Regs::Struct<Regs::Value1> {
 *         BitField<0, 4, u32> some_field;
 *         BitField<4, 3, u32> some_other_field;
 *     };
 *
 * Usage in external code (within SomeNamespace scope):
 *
 *     For a register which maps to a single index:
 *     registers.Get<Regs::Value1>().some_field = some_value;
 *
 *      For a register which maps to different indices, e.g. a group of similar registers
 *     registers.Get<Regs::Value1>(index).some_field = some_value;
 *
 *
 * @tparam BaseType Base type used for storing individual registers, e.g. u32
 * @tparam RegDefinition Class defining an enumeration called "Id" and a template<Id id> union, as described above.
 * @note RegDefinition::Id needs to have an enum value called NumIds defining the number of registers to be allocated.
 */
template<typename BaseType, typename RegDefinition>
struct RegisterSet {
    // Register IDs
    using Id = typename RegDefinition::Id;

    // type used for *this
    using ThisType = RegisterSet<BaseType, RegDefinition>;

    // Register definition structs, defined in RegDefinition
    template<Id id>
    using Struct = typename RegDefinition::template Struct<id>;


    /*
     * Lookup register with the given id and return it as the corresponding structure type.
     * @note This just forwards the arguments to Get(Id).
     */
    template<Id id>
    const Struct<id>& Get() const {
        return Get<id>(id);
    }

    /*
     * Lookup register with the given id and return it as the corresponding structure type.
     * @note This just forwards the arguments to Get(Id).
     */
    template<Id id>
    Struct<id>& Get() {
        return Get<id>(id);
    }

    /*
     * Lookup register with the given index and return it as the corresponding structure type.
     * @todo Is this portable with regards to structures larger than BaseType?
     * @note if index==id, you don't need to specify the function parameter.
     */
    template<Id id>
    const Struct<id>& Get(const Id& index) const {
        const int idx = static_cast<size_t>(index);
        return *reinterpret_cast<const Struct<id>*>(&raw[idx]);
    }

    /*
     * Lookup register with the given index and return it as the corresponding structure type.
     * @note This just forwards the arguments to the const version of Get(Id).
     * @note if index==id, you don't need to specify the function parameter.
     */
    template<Id id>
    Struct<id>& Get(const Id& index) {
        return const_cast<Struct<id>&>(GetThis().Get<id>(index));
    }

    /*
     * Plain array access.
     * @note If you want to have this casted to a register defininition struct, use Get() instead.
     */
    const BaseType& operator[] (const Id& id) const {
        return raw[static_cast<size_t>(id)];
    }

    /*
     * Plain array access.
     * @note If you want to have this casted to a register defininition struct, use Get() instead.
     * @note This operator just forwards its argument to the const version.
     */
    BaseType& operator[] (const Id& id) {
        return const_cast<BaseType&>(GetThis()[id]);
    }

private:
    /*
     * Returns a const reference to "this".
     */
    const ThisType& GetThis() const {
        return static_cast<const ThisType&>(*this);
    }

    BaseType raw[Id::NumIds];
};
