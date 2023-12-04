#pragma once

#include <oscar/Utils/Concepts.hpp>

#include <cstddef>
#include <span>

namespace osc
{
    template<ObjectRepresentationByte Byte = std::byte, BitCastable T>
    std::span<Byte const> ViewObjectRepresentation(T const& v)
    {
        // this is one of the few cases where `reinterpret_cast` is guaranteed to be safe
        // for _examination_ (i.e. reading)
        //
        // > from: https://en.cppreference.com/w/cpp/language/reinterpret_cast
        // >
        // > Whenever an attempt is made to read or modify the stored value of an object of
        // > type DynamicType through a glvalue of type AliasedType, the behavior is undefined
        // > unless one of the following is true:
        // >
        // > - AliasedType is std::byte,(since C++17) char, or unsigned char: this permits
        // >   examination of the object representation of any object as an array of bytes.
        return {reinterpret_cast<Byte const*>(&v), sizeof(T)};
    }

    template<ObjectRepresentationByte Byte = std::byte, ContiguousContainer Container>
    std::span<Byte const> ViewObjectRepresentations(Container const& c)
        requires BitCastable<typename Container::value_type>
    {
        // this is one of the few cases where `reinterpret_cast` is guaranteed to be safe
        // for _examination_ (i.e. reading)
        //
        // > from: https://en.cppreference.com/w/cpp/language/reinterpret_cast
        // >
        // > Whenever an attempt is made to read or modify the stored value of an object of
        // > type DynamicType through a glvalue of type AliasedType, the behavior is undefined
        // > unless one of the following is true:
        // >
        // > - AliasedType is std::byte,(since C++17) char, or unsigned char: this permits
        // >   examination of the object representation of any object as an array of bytes.
        return {reinterpret_cast<Byte const*>(c.data()), sizeof(typename Container::value_type) * c.size()};
    }
}