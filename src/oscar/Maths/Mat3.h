#pragma once

#include <oscar/Maths/Mat.h>
#include <oscar/Maths/Vec3.h>

#include <cstddef>
#include <cstdint>

namespace osc
{
    // a 3x3 column-major matrix
    template<typename T>
    struct Mat<3, 3, T> {
        using col_type = Vec<3, T>;
        using row_type = Vec<3, T>;
        using transpose_type = Mat<3, 3, T>;
        using type = Mat<3, 3, T>;
        using value_type = col_type;
        using element_type = T;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using reference = col_type&;
        using const_reference = const col_type&;
        using pointer = col_type*;
        using const_pointer = const col_type*;
        using iterator = col_type*;
        using const_iterator = const col_type*;

        constexpr Mat() = default;

        explicit constexpr Mat(T s) :
            value{
                col_type{s, T{}, T{}},
                col_type{T{}, s, T{}},
                col_type{T{}, T{}, s}
            }
        {}

        constexpr Mat(
            T x0, T y0, T z0,
            T x1, T y1, T z1,
            T x2, T y2, T z2) :

            value{
                col_type{x0, y0, z0},
                col_type{x1, y1, z1},
                col_type{x2, y2, z2}
            }
        {}

        constexpr Mat(
            const col_type& v0,
            const col_type& v1,
            const col_type& v2) :

            value{v0, v1, v2}
        {}

        template<
            typename X0, typename Y0, typename Z0,
            typename X1, typename Y1, typename Z1,
            typename X2, typename Y2, typename Z2>
        constexpr Mat(
            X0 x0, Y0 y0, Z0 z0,
            X1 x1, Y1 y1, Z1 z1,
            X2 x2, Y2 y2, Z2 z2) :

            value{
                col_type{x0, y0, z0},
                col_type{x1, y1, z1},
                col_type{x2, y2, z2}
            }
        {}

        template<typename V1, typename V2, typename V3>
        constexpr Mat(
            const Vec<3, V1>& v1,
            const Vec<3, V2>& v2,
            const Vec<3, V3>& v3) :

            value{
                col_type{v1},
                col_type{v2},
                col_type{v3}
            }
        {}

        template<typename U>
        explicit constexpr Mat(const Mat<3, 3, U>& m) :
            value{
                col_type{m[0]},
                col_type{m[1]},
                col_type{m[2]}
            }
        {}

        explicit constexpr Mat(const Mat<4, 4, T>& m) :
            value{
                col_type{m[0]},
                col_type{m[1]},
                col_type{m[2]}
            }
        {}

        template<typename U>
        Mat<3, 3, T>& operator=(const Mat<3, 3, U>& m)
        {
            this->value[0] = m[0];
            this->value[1] = m[1];
            this->value[2] = m[2];
            return *this;
        }

        constexpr size_type size() const { return 3; }
        constexpr pointer data() { return value; }
        constexpr const_pointer data() const { return value; }
        constexpr iterator begin() { return data(); }
        constexpr const_iterator begin() const { return data(); }
        constexpr iterator end() { return data() + size(); }
        constexpr const_iterator end() const { return data() + size(); }
        constexpr reference operator[](size_type i) { return begin()[i]; }
        constexpr const_reference operator[](size_type i) const { return begin()[i]; }

        friend constexpr bool operator==(const Mat&, const Mat&) = default;

        template<typename U>
        Mat<3, 3, T>& operator+=(U s)
        {
            this->value[0] += s;
            this->value[1] += s;
            this->value[2] += s;
            return *this;
        }

        template<typename U>
        Mat<3, 3, T>& operator+=(const Mat<3, 3, U>& m)
        {
            this->value[0] += m[0];
            this->value[1] += m[1];
            this->value[2] += m[2];
            return *this;
        }

        template<typename U>
        Mat<3, 3, T>& operator-=(U s)
        {
            this->value[0] -= s;
            this->value[1] -= s;
            this->value[2] -= s;
            return *this;
        }

        template<typename U>
        Mat<3, 3, T>& operator-=(const Mat<3, 3, U>& m)
        {
            this->value[0] -= m[0];
            this->value[1] -= m[1];
            this->value[2] -= m[2];
            return *this;
        }

        template<typename U>
        Mat<3, 3, T>& operator*=(U s)
        {
            this->value[0] *= s;
            this->value[1] *= s;
            this->value[2] *= s;
            return *this;
        }

        template<typename U>
        Mat<3, 3, T>& operator*=(const Mat<3, 3, U>& m)
        {
            return (*this = *this * m);
        }

        template<typename U>
        Mat<3, 3, T>& operator/=(U s)
        {
            this->value[0] /= s;
            this->value[1] /= s;
            this->value[2] /= s;
            return *this;
        }

        template<typename U>
        Mat<3, 3, T>& operator/=(const Mat<3, 3, U>& m)
        {
            return *this /= inverse(m);
        }

        Mat<3, 3, T>& operator++()
        {
            ++this->value[0];
            ++this->value[1];
            ++this->value[2];
            return *this;
        }

        Mat<3, 3, T>& operator--()
        {
            --this->value[0];
            --this->value[1];
            --this->value[2];
            return *this;
        }

        Mat<3, 3, T> operator++(int)
        {
            Mat<3, 3, T> copy{*this};
            ++*this;
            return copy;
        }

        Mat<3, 3, T> operator--(int)
        {
            Mat<3, 3, T> copy{*this};
            --*this;
            return copy;
        }

    private:
        col_type value[3];
    };

    template<typename T>
    Mat<3, 3, T> operator+(const Mat<3, 3, T>& m)
    {
        return m;
    }

    template<typename T>
    Mat<3, 3, T> operator-(const Mat<3, 3, T>& m)
    {
        return Mat<3, 3, T>{-m[0], -m[1], -m[2]};
    }

    template<typename T>
    Mat<3, 3, T> operator+(const Mat<3, 3, T>& m, T scalar)
    {
        return Mat<3, 3, T>{m[0] + scalar, m[1] + scalar, m[2] + scalar};
    }

    template<typename T>
    Mat<3, 3, T> operator+(T scalar, const Mat<3, 3, T>& m)
    {
        return Mat<3, 3, T>{scalar + m[0], scalar + m[1], scalar + m[2]};
    }

    template<typename T>
    Mat<3, 3, T> operator+(const Mat<3, 3, T>& m1, const Mat<3, 3, T>& m2)
    {
        return Mat<3, 3, T>{m1[0] + m2[0], m1[1] + m2[1], m1[2] + m2[2]};
    }

    template<typename T>
    Mat<3, 3, T> operator-(const Mat<3, 3, T>& m, T scalar)
    {
        return Mat<3, 3, T>{m[0] - scalar, m[1] - scalar, m[2] - scalar};
    }

    template<typename T>
    Mat<3, 3, T> operator-(T scalar, const Mat<3, 3, T>& m)
    {
        return Mat<3, 3, T>{scalar - m[0], scalar - m[1], scalar - m[2]};
    }

    template<typename T>
    Mat<3, 3, T> operator-(const Mat<3, 3, T>& m1, const Mat<3, 3, T>& m2)
    {
        return Mat<3, 3, T>{m1[0] - m2[0], m1[1] - m2[1], m1[2] - m2[2]};
    }

    template<typename T>
    Mat<3, 3, T> operator*(const Mat<3, 3, T>& m, T scalar)
    {
        return Mat<3, 3, T>{m[0] * scalar, m[1] * scalar, m[2] * scalar};
    }

    template<typename T>
    Mat<3, 3, T> operator*(T scalar, const Mat<3, 3, T>& m)
    {
        return Mat<3, 3, T>{scalar * m[0], scalar * m[1], scalar * m[2]};
    }

    template<typename T>
    typename Mat<3, 3, T>::col_type operator*(const Mat<3, 3, T>& m, const typename Mat<3, 3, T>::row_type& v)
    {
        return typename Mat<3, 3, T>::col_type(
            m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z,
            m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z,
            m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z
        );
    }

    template<typename T>
    typename Mat<3, 3, T>::row_type operator*(const typename Mat<3, 3, T>::col_type& v, const Mat<3, 3, T>& m)
    {
        return typename Mat<3, 3, T>::row_type(
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
        );
    }

    template<typename T>
    Mat<3, 3, T> operator*(const Mat<3, 3, T>& m1, const Mat<3, 3, T>& m2)
    {
        const T SrcA00 = m1[0][0];
        const T SrcA01 = m1[0][1];
        const T SrcA02 = m1[0][2];
        const T SrcA10 = m1[1][0];
        const T SrcA11 = m1[1][1];
        const T SrcA12 = m1[1][2];
        const T SrcA20 = m1[2][0];
        const T SrcA21 = m1[2][1];
        const T SrcA22 = m1[2][2];

        const T SrcB00 = m2[0][0];
        const T SrcB01 = m2[0][1];
        const T SrcB02 = m2[0][2];
        const T SrcB10 = m2[1][0];
        const T SrcB11 = m2[1][1];
        const T SrcB12 = m2[1][2];
        const T SrcB20 = m2[2][0];
        const T SrcB21 = m2[2][1];
        const T SrcB22 = m2[2][2];

        Mat<3, 3, T> Result;
        Result[0][0] = SrcA00 * SrcB00 + SrcA10 * SrcB01 + SrcA20 * SrcB02;
        Result[0][1] = SrcA01 * SrcB00 + SrcA11 * SrcB01 + SrcA21 * SrcB02;
        Result[0][2] = SrcA02 * SrcB00 + SrcA12 * SrcB01 + SrcA22 * SrcB02;
        Result[1][0] = SrcA00 * SrcB10 + SrcA10 * SrcB11 + SrcA20 * SrcB12;
        Result[1][1] = SrcA01 * SrcB10 + SrcA11 * SrcB11 + SrcA21 * SrcB12;
        Result[1][2] = SrcA02 * SrcB10 + SrcA12 * SrcB11 + SrcA22 * SrcB12;
        Result[2][0] = SrcA00 * SrcB20 + SrcA10 * SrcB21 + SrcA20 * SrcB22;
        Result[2][1] = SrcA01 * SrcB20 + SrcA11 * SrcB21 + SrcA21 * SrcB22;
        Result[2][2] = SrcA02 * SrcB20 + SrcA12 * SrcB21 + SrcA22 * SrcB22;
        return Result;
    }

    template<typename T>
    Mat<3, 3, T> operator/(const Mat<3, 3, T>& m, T scalar)
    {
        return Mat<3, 3, T>{m[0] / scalar, m[1] / scalar, m[2] / scalar};
    }

    template<typename T>
    Mat<3, 3, T> operator/(T scalar, const Mat<3, 3, T>& m)
    {
        return Mat<3, 3, T>{scalar / m[0], scalar / m[1], scalar / m[2]};
    }

    template<typename T>
    typename Mat<3, 3, T>::col_type operator/(const Mat<3, 3, T>& m, const typename Mat<3, 3, T>::row_type& v)
    {
        return inverse(m) * v;
    }

    template<typename T>
    typename Mat<3, 3, T>::row_type operator/(const typename Mat<3, 3, T>::col_type& v, const Mat<3, 3, T>& m)
    {
        return v * inverse(m);
    }

    template<typename T>
    Mat<3, 3, T> operator/(const Mat<3, 3, T>& m1, const Mat<3, 3, T>& m2)
    {
        Mat<3, 3, T> m1_copy{m1};
        return m1_copy /= m2;
    }

    using Mat3 = Mat<3, 3, float>;
    using Mat3f = Mat<3, 3, float>;
    using Mat3d = Mat<3, 3, double>;
    using Mat3i = Mat<3, 3, int>;
    using Mat3z = Mat<3, 3, ptrdiff_t>;
    using Mat3zu = Mat<3, 3, size_t>;
    using Mat3u32 = Mat<3, 3, uint32_t>;

    template<typename T>
    constexpr T identity();

    template<>
    constexpr Mat3 identity<Mat3>()
    {
        return Mat3{1.0f};
    }
}
