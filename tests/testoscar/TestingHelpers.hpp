#pragma once

#include <oscar/Graphics/Color.hpp>
#include <oscar/Graphics/Color32.hpp>
#include <oscar/Maths/Mat3.hpp>
#include <oscar/Maths/Mat4.hpp>
#include <oscar/Maths/Transform.hpp>
#include <oscar/Maths/Triangle.hpp>
#include <oscar/Maths/Vec2.hpp>
#include <oscar/Maths/Vec3.hpp>
#include <oscar/Maths/Vec4.hpp>
#include <oscar/Utils/Concepts.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <random>
#include <vector>

namespace osc::testing
{
    std::default_random_engine& GetRngEngine();

    float GenerateFloat();
    int GenerateInt();
    bool GenerateBool();
    uint8_t GenerateUint8();
    Color GenerateColor();
    Color32 GenerateColor32();
    Vec2 GenerateVec2();
    Vec3 GenerateVec3();
    Vec4 GenerateVec4();
    Mat3 GenerateMat3x3();
    Mat4 GenerateMat4x4();
    Triangle GenerateTriangle();
    std::vector<Vec3> GenerateTriangleVerts();
    std::vector<Vec3> GenerateVertices(size_t);
    std::vector<Vec3> GenerateNormals(size_t);
    std::vector<Vec2> GenerateTexCoords(size_t);
    std::vector<Color> GenerateColors(size_t);
    std::vector<Vec4> GenerateTangents(size_t);
    std::vector<uint16_t> GenerateIndices(size_t start, size_t end);

    template<ContiguousContainer T, ContiguousContainer U>
    bool ContainersEqual(T const& a, U const& b)
    {
        using std::begin;
        using std::end;
        return std::equal(begin(a), end(a), begin(b), end(b));
    }

    template<class Container, class UnaryOperation>
    auto MapToVector(Container const& src, UnaryOperation op)
    {
        using std::begin;
        using std::end;

        std::vector<std::remove_const_t<std::remove_reference_t<decltype(op(std::declval<decltype(*begin(src))>()))>>> rv;
        rv.reserve(std::distance(begin(src), end(src)));
        for (auto const& el : src)
        {
            rv.push_back(op(el));
        }
        return rv;
    }

    template<class T>
    std::vector<T> ResizedVectorCopy(std::vector<T> const& v, size_t newSize, T const& filler = {})
    {
        std::vector<T> rv;
        rv.reserve(newSize);
        rv.insert(rv.end(), v.begin(), v.begin() + std::min(v.size(), newSize));
        rv.resize(newSize, filler);
        return rv;
    }
}

