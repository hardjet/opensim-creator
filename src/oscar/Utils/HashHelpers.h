#pragma once

#include <oscar/Utils/Concepts.h>

#include <cstddef>
#include <functional>
#include <ranges>

namespace osc
{
    // combines hash of `T` into the seed value
    template<Hashable T>
    size_t HashCombine(size_t seed, const T& v)
    {
        return seed ^ (std::hash<T>{}(v) + 0x9e3779b9 + (seed<<6) + (seed>>2));
    }

    template<Hashable T>
    size_t hash_of(const T& v)
    {
        return std::hash<T>{}(v);
    }

    template<Hashable T, Hashable... Ts>
    size_t hash_of(const T& v, const Ts&... vs)
    {
        return HashCombine(hash_of(v), hash_of(vs...));
    }

    template<typename Range>
    size_t HashRange(const Range& range)
    {
        size_t rv = 0;
        for (const auto& el : range) {
            rv = HashCombine(rv, el);
        }
        return rv;
    }
}
