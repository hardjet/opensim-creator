#include <oscar/Graphics/Rgba.h>

#include <gtest/gtest.h>
#include <oscar/Graphics/Unorm8.h>

#include <sstream>

using namespace osc;

TEST(Rgba, can_be_instantiated_with_float_template_arg)
{
    [[maybe_unused]] Rgba<float> this_should_compile;
}

TEST(Rgba, can_be_instantiated_with_Unorm8_template_arg)
{
    [[maybe_unused]] Rgba<Unorm8> this_should_compile;
}

TEST(Rgba, can_write_float_channels_to_stream)
{
    std::stringstream ss;
    ss << Rgba<float>{};
    ASSERT_FALSE(ss.str().empty());
}

TEST(Rgba, can_write_Unorm8_channels_to_stream)
{
    std::stringstream ss;
    ss << Rgba<Unorm8>{};
    ASSERT_FALSE(ss.str().empty());
}

TEST(Rgba, can_be_decomposed_into_component_parts)
{
    const Rgba<float> rgba{1.0f, 2.0f, 3.0f, 0.5f};
    const auto& [r, g, b, a] = rgba;

    ASSERT_EQ(rgba.r, 1.0f);
    ASSERT_EQ(rgba.g, 2.0f);
    ASSERT_EQ(rgba.b, 3.0f);
    ASSERT_EQ(rgba.a, 0.5f);
}

TEST(Rgba, can_be_decomposed_into_mutable_component_parts)
{
    Rgba<float> rgba{1.0f, 2.0f, 3.0f, 0.5f};
    auto& [r, g, b, a] = rgba;

    r *= 2.0f;
    g *= 2.0f;
    b *= 2.0f;
    a *= 2.0f;

    ASSERT_EQ(rgba.r, 2.0f);
    ASSERT_EQ(rgba.g, 4.0f);
    ASSERT_EQ(rgba.b, 6.0f);
    ASSERT_EQ(rgba.a, 0.25f);
}

TEST(Rgba, value_ptr_works_for_float_type)
{
    const Rgba<float> rgba;
    ASSERT_EQ(value_ptr(rgba), &rgba.r);
}

TEST(Rgba, value_ptr_works_for_Unorm8)
{
    const Rgba<Unorm8> rgba;
    ASSERT_EQ(value_ptr(rgba), &rgba.r);
}

TEST(Rgba, float_rgba_can_be_hashed)
{
    Rgba<float> rgba{0.125f, 0.25f, 0.5f, 1.0f};
    std::hash<Rgba<float>> hasher{};

    size_t last_hash = 0;
    for (auto& component : rgba) {
        component *= 0.5f;
        const size_t hash = hasher(rgba);
        ASSERT_NE(hash, last_hash);
        last_hash = hash;
    }
}

TEST(Rgba, Unorm8_rgba_can_be_hashed)
{
    Rgba<Unorm8> rgba{0.125f, 0.25f, 0.5f, 1.0f};
    std::hash<Rgba<Unorm8>> hasher{};

    size_t last_hash = 0;
    for (auto& component : rgba) {
        component = component.normalized_value() * 0.5f;
        const size_t hash = hasher(rgba);
        ASSERT_NE(hash, last_hash);
        last_hash = hash;
    }
}