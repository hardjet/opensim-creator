#include <oscar/Graphics/Color.h>

#include <gtest/gtest.h>
#include <oscar/Maths/MathHelpers.h>
#include <oscar/Maths/Vec3.h>
#include <oscar/Maths/Vec4.h>

#include <algorithm>
#include <array>
#include <type_traits>
#include <utility>

using namespace osc;

namespace
{
    // these testing values were pulled out of inkscape, which is assumed to
    // have correct RGB-to-HSL relations
    struct KnownRGBAToHSLAConversions final {
        Color input;
        ColorHSLA expectedOutput;
    };
    constexpr auto c_RGBAToHSLACases = std::to_array<KnownRGBAToHSLAConversions>(
    {
         // RGBA                     // HSLA
         // r     g     b     a      // h (degrees) s     l     a
        {{  1.0f, 0.0f, 0.0f, 1.0f}, {  0.0f,       1.0f, 0.5f, 1.0f}},  // red
        {{  0.0f, 1.0f, 0.0f, 1.0f}, {  120.0f,     1.0f, 0.5f, 1.0f}},  // green
        {{  0.0f, 0.0f, 1.0f, 1.0f}, {  240.0f,     1.0f, 0.5f, 1.0f}},  // blue
    });

    std::ostream& operator<<(std::ostream& o, KnownRGBAToHSLAConversions const& tc)
    {
        return o << "rgba = " << tc.input << ", hsla = " << tc.expectedOutput;
    }

    constexpr float c_HLSLConversionTolerance = 0.0001f;
}

TEST(Color, DefaultConstructedIsClear)
{
    static_assert(Color{} == Color::clear());
}

TEST(Color, ConstructedWith1ArgFillsRGBWithTheArg)
{
    static_assert(Color{0.23f} == Color{0.23f, 0.23f, 0.23f, 1.0f});
}

TEST(Color, ConstructedWith2ArgsFillsRGBWithFirstAndAlphaWithSecond)
{
    static_assert(Color{0.83f, 0.4f} == Color{0.83f, 0.83f, 0.83f, 0.4f});
}

TEST(Color, ConstructedWithVec3AndAlphaRepacksCorrectly)
{
    static_assert(Color{Vec3{0.1f, 0.2f, 0.3f}, 0.7f} == Color{0.1f, 0.2f, 0.3f, 0.7f});
}
TEST(Color, CanConstructFromRGBAFloats)
{
    Color const color{5.0f, 4.0f, 3.0f, 2.0f};
    ASSERT_EQ(color.r, 5.0f);
    ASSERT_EQ(color.g, 4.0f);
    ASSERT_EQ(color.b, 3.0f);
    ASSERT_EQ(color.a, 2.0f);
}

TEST(Color, RGBAFloatConstructorIsConstexpr)
{
    // must compile
    [[maybe_unused]] constexpr Color color{0.0f, 0.0f, 0.0f, 0.0f};
}

TEST(Color, CanConstructFromRGBFloats)
{
    Color const color{5.0f, 4.0f, 3.0f};
    ASSERT_EQ(color.r, 5.0f);
    ASSERT_EQ(color.g, 4.0f);
    ASSERT_EQ(color.b, 3.0f);

    ASSERT_EQ(color.a, 1.0f);  // default value when given RGB
}

TEST(Color, RGBFloatConstructorIsConstexpr)
{
    // must compile
    [[maybe_unused]] constexpr Color color{0.0f, 0.0f, 0.0f};
}

TEST(Color, CanBeExplicitlyConstructedFromVec3)
{
    Vec3 const v = {0.25f, 0.387f, 0.1f};
    Color const color{v};

    // ensure vec3 ctor creates a solid color with a == 1.0f
    ASSERT_EQ(color.r, v.x);
    ASSERT_EQ(color.g, v.y);
    ASSERT_EQ(color.b, v.z);
    ASSERT_EQ(color.a, 1.0f);
}

TEST(Color, CanBeExplicitlyConstructedFromVec4)
{
    [[maybe_unused]] Color const color{Vec4{0.0f, 1.0f, 0.0f, 1.0f}};
}

TEST(Color, CanBeImplicitlyConvertedToVec4)
{
    [[maybe_unused]] constexpr Vec4 v = Color{0.0f, 0.0f, 1.0f, 0.0f};
}

TEST(Color, BracketOpertatorOnConstColorWorksAsExpected)
{
    Color const color = {0.32f, 0.41f, 0.78f, 0.93f};

    ASSERT_EQ(color[0], color.r);
    ASSERT_EQ(color[1], color.g);
    ASSERT_EQ(color[2], color.b);
    ASSERT_EQ(color[3], color.a);
}

TEST(Color, Vec4ConstructorIsConstexpr)
{
    // must compile
    [[maybe_unused]] constexpr Color color{Vec4{0.0f, 1.0f, 0.0f, 1.0f}};
}

TEST(Color, ToVec4ExplicitlyConvertsToVec4)
{
    Color const c = {0.75, 0.75, 0.75, 1.0f};
    Vec4 const v = to_vec4(c);

    ASSERT_EQ(v.x, c.r);
    ASSERT_EQ(v.y, c.g);
    ASSERT_EQ(v.z, c.b);
    ASSERT_EQ(v.w, c.a);
}

TEST(Color, EqualityReturnsTrueForEquivalentColors)
{
    Color const a = {1.0f, 0.0f, 1.0f, 0.5f};
    Color const b = {1.0f, 0.0f, 1.0f, 0.5f};

    ASSERT_TRUE(a == b);
}

TEST(Color, EqualityReturnsFalseForInequivalentColors)
{
    Color const a = {0.0f, 0.0f, 1.0f, 0.5f};
    Color const b = {1.0f, 0.0f, 1.0f, 0.5f};

    ASSERT_FALSE(a == b);
}

TEST(Color, InequalityReturnsTrueForInequivalentColors)
{
    Color const a = {0.0f, 0.0f, 1.0f, 0.5f};
    Color const b = {1.0f, 0.0f, 1.0f, 0.5f};

    ASSERT_TRUE(a != b);
}

TEST(Color, InequalityReturnsFalseForEquivalentColors)
{
    Color const a = {0.0f, 0.0f, 1.0f, 0.5f};
    Color const b = {0.0f, 0.0f, 1.0f, 0.5f};

    ASSERT_FALSE(a != b);
}

TEST(Color, ConstIteratorsWorkAsExpected)
{
    Color const c = {1.0f, 0.25f, 0.1f, 0.3f};
    auto const expected = std::to_array({c.r, c.g, c.b, c.a});

    ASSERT_TRUE(std::equal(c.begin(), c.end(), expected.begin(), expected.end()));
}

TEST(Color, NonConstIteratorsWorkAsExpected)
{
    Color const c = {1.0f, 0.25f, 0.1f, 0.3f};
    auto expected = std::to_array({c.r, c.g, c.b, c.a});

    ASSERT_TRUE(std::equal(c.begin(), c.end(), expected.begin(), expected.end()));
}

TEST(Color, CanMultiplyColors)
{
    Color const a = {0.64f, 0.90f, 0.21f, 0.89f};
    Color const b = {0.12f, 0.10f, 0.23f, 0.01f};

    Color const rv = a * b;

    ASSERT_EQ(rv.r, a.r * b.r);
    ASSERT_EQ(rv.g, a.g * b.g);
    ASSERT_EQ(rv.b, a.b * b.b);
    ASSERT_EQ(rv.a, a.a * b.a);
}

TEST(Color, CanBeMutablyMultiplied)
{
    Color const a = {0.64f, 0.90f, 0.21f, 0.89f};
    Color const b = {0.12f, 0.10f, 0.23f, 0.01f};

    Color rv = a;
    rv *= b;

    ASSERT_EQ(rv.r, a.r * b.r);
    ASSERT_EQ(rv.g, a.g * b.g);
    ASSERT_EQ(rv.b, a.b * b.b);
    ASSERT_EQ(rv.a, a.a * b.a);
}

TEST(Color, ToLinearReturnsLinearizedVersionOfOneColorChannel)
{
    float const sRGBColor = 0.02f;
    float const linearColor = to_linear_colorspace(sRGBColor);

    // we don't test what the actual equation is, just that low
    // sRGB colors map to higher linear colors (i.e. they are
    // "stretched out" from the bottom of the curve)
    ASSERT_GT(sRGBColor, linearColor);
}

TEST(Color, ToSRGBReturnsSRGBVersionOfOneColorChannel)
{
    float const linearColor = 0.4f;
    float const sRGBColor = to_srgb_colorspace(linearColor);

    // we don't test what the actual equation is, just that low-ish
    // linear colors are less than the equivalent sRGB color (because
    // sRGB will stretch lower colors out)
    ASSERT_LT(linearColor, sRGBColor);
}

TEST(Color, ToLinearReturnsLinearizedVersionOfColor)
{
    Color const sRGBColor = {0.5f, 0.5f, 0.5f, 0.5f};
    Color const linearColor = to_linear_colorspace(sRGBColor);

    ASSERT_EQ(linearColor.r, to_linear_colorspace(sRGBColor.r));
    ASSERT_EQ(linearColor.g, to_linear_colorspace(sRGBColor.g));
    ASSERT_EQ(linearColor.b, to_linear_colorspace(sRGBColor.b));
    ASSERT_EQ(linearColor.a, sRGBColor.a);
}

TEST(Color, ToSRGBReturnsColorWithGammaCurveApplied)
{
    Color const linearColor = {0.25f, 0.25f, 0.25f, 0.6f};
    Color const sRGBColor = to_srgb_colorspace(linearColor);

    ASSERT_EQ(sRGBColor.r, to_srgb_colorspace(linearColor.r));
    ASSERT_EQ(sRGBColor.g, to_srgb_colorspace(linearColor.g));
    ASSERT_EQ(sRGBColor.b, to_srgb_colorspace(linearColor.b));
    ASSERT_EQ(sRGBColor.a, linearColor.a);
}

TEST(Color, ToLinearFollowedByToSRGBEffectivelyReuturnsOriginalColor)
{
    Color const originalColor = {0.1f, 0.1f, 0.1f, 0.5f};
    Color const converted = to_srgb_colorspace(to_linear_colorspace(originalColor));

    constexpr float tolerance = 0.0001f;
    ASSERT_NEAR(originalColor.r, converted.r, tolerance);
    ASSERT_NEAR(originalColor.g, converted.g, tolerance);
    ASSERT_NEAR(originalColor.b, converted.b, tolerance);
    ASSERT_NEAR(originalColor.a, converted.a, tolerance);
}

TEST(Color, ToColor32ReturnsRgba32VersionOfTheColor)
{
    Color const color = {0.85f, 0.62f, 0.3f, 0.5f};
    Color32 const expected
    {
        static_cast<uint8_t>(color.r * static_cast<float>(0xff)),
        static_cast<uint8_t>(color.g * static_cast<float>(0xff)),
        static_cast<uint8_t>(color.b * static_cast<float>(0xff)),
        static_cast<uint8_t>(color.a * static_cast<float>(0xff)),
    };

    Color32 const got = to_color32(color);

    ASSERT_EQ(expected.r, got.r);
    ASSERT_EQ(expected.g, got.g);
    ASSERT_EQ(expected.b, got.b);
    ASSERT_EQ(expected.a, got.a);
}

TEST(Color, ToColor32ClampsHDRValues)
{
    Color const color = {1.5f, 0.0f, 2.0f, 1.0f};
    Color32 const expected = {0xff, 0x00, 0xff, 0xff};
    ASSERT_EQ(to_color32(color), expected);
}

TEST(Color, ToColor32ClampsNegativeValues)
{
    Color const color = {-1.0f, 0.0f, 1.0f, 1.0f};
    Color32 const expected = {0x00, 0x00, 0xff, 0xff};
    ASSERT_EQ(to_color32(color), expected);
}

TEST(Color, ToColorFromColor32ReturnsExpectedOutputs)
{
    ASSERT_EQ(to_color(Color32(0xff, 0x00, 0x00, 0xff)), Color(1.0f, 0.0f, 0.0f, 1.0f));
    ASSERT_EQ(to_color(Color32(0x00, 0xff, 0x00, 0xff)), Color(0.0f, 1.0f, 0.0f, 1.0f));
    ASSERT_EQ(to_color(Color32(0x00, 0x00, 0xff, 0xff)), Color(0.0f, 0.0f, 1.0f, 1.0f));
    ASSERT_EQ(to_color(Color32(0x00, 0xff, 0xff, 0x00)), Color(0.0f, 1.0f, 1.0f, 0.0f));
}

TEST(Color, CanGetBlackColor)
{
    ASSERT_EQ(Color::black(), Color(0.0f, 0.0f, 0.0f, 1.0f));
}

TEST(Color, CanGetBlueColor)
{
    ASSERT_EQ(Color::blue(), Color(0.0f, 0.0f, 1.0f, 1.0f));
}

TEST(Color, CanGetClearColor)
{
    ASSERT_EQ(Color::clear(), Color(0.0f, 0.0f, 0.0f, 0.0f));
}

TEST(Color, CanGetGreenColor)
{
    ASSERT_EQ(Color::green(), Color(0.0f, 1.0f, 0.0f, 1.0f));
}

TEST(Color, CanGetRedColor)
{
    ASSERT_EQ(Color::red(), Color(1.0f, 0.0f, 0.0f, 1.0f));
}

TEST(Color, CanGetWhiteColor)
{
    ASSERT_EQ(Color::white(), Color(1.0f, 1.0f, 1.0f, 1.0f));
}

TEST(Color, CanGetYellowColor)
{
    ASSERT_EQ(Color::yellow(), Color(1.0f, 1.0f, 0.0f, 1.0f));
}

TEST(Color, WithAlphaWorksAsExpected)
{
    static_assert(Color::white().with_alpha(0.33f) == Color{1.0f, 1.0f, 1.0f, 0.33f});
}

TEST(Color, ValuePtrConstVersionReturnsAddressOfColor)
{
    Color const color = Color::red();
    ASSERT_EQ(&color.r, value_ptr(color));
}

TEST(Color, ValuePtrMutatingVersionReturnsAddressOfColor)
{
    Color color = Color::red();
    ASSERT_EQ(&color.r, value_ptr(color));
}

TEST(Color, LerpWithZeroReturnsFirstColor)
{
    Color const a = Color::red();
    Color const b = Color::blue();

    ASSERT_EQ(lerp(a, b, 0.0f), a);
}

TEST(Color, LerpWithOneReturnsSecondColor)
{
    Color const a = Color::red();
    Color const b = Color::blue();

    ASSERT_EQ(lerp(a, b, 1.0f), b);
}

TEST(Color, LerpBelowZeroReturnsFirstColor)
{
    // tests that `t` is appropriately clamped

    Color const a = Color::red();
    Color const b = Color::blue();

    ASSERT_EQ(lerp(a, b, -1.0f), a);
}

TEST(Color, LerpAboveOneReturnsSecondColor)
{
    // tests that `t` is appropriately clamped

    Color const a = Color::red();
    Color const b = Color::blue();

    ASSERT_EQ(lerp(a, b, 2.0f), b);
}

TEST(Color, LerpBetweenTheTwoColorsReturnsExpectedResult)
{
    Color const a = Color::red();
    Color const b = Color::blue();
    float const t = 0.5f;
    float const tolerance = 0.0001f;

    Color const rv = lerp(a, b, t);

    for (size_t i = 0; i < 4; ++i)
    {
        ASSERT_NEAR(rv[i], (1.0f-t)*a[i] + t*b[i], tolerance);
    }
}

TEST(Color, CanBeHashed)
{
    Color const a = Color::red();
    Color const b = Color::blue();

    ASSERT_NE(std::hash<Color>{}(a), std::hash<Color>{}(b));
}

TEST(Color, ToHtmlStringRGBAReturnsExpectedValues)
{
    ASSERT_EQ(to_html_string_rgba(Color::red()), "#ff0000ff");
    ASSERT_EQ(to_html_string_rgba(Color::green()), "#00ff00ff");
    ASSERT_EQ(to_html_string_rgba(Color::blue()), "#0000ffff");
    ASSERT_EQ(to_html_string_rgba(Color::black()), "#000000ff");
    ASSERT_EQ(to_html_string_rgba(Color::clear()), "#00000000");
    ASSERT_EQ(to_html_string_rgba(Color::white()), "#ffffffff");
    ASSERT_EQ(to_html_string_rgba(Color::yellow()), "#ffff00ff");
    ASSERT_EQ(to_html_string_rgba(Color::cyan()), "#00ffffff");
    ASSERT_EQ(to_html_string_rgba(Color::magenta()), "#ff00ffff");

    // ... and HDR values are LDR clamped
    ASSERT_EQ(to_html_string_rgba(Color(1.5f, 1.5f, 0.0f, 1.0f)), "#ffff00ff");

    // ... and negative values are clamped
    ASSERT_EQ(to_html_string_rgba(Color(-1.0f, 0.0f, 0.0f, 1.0f)), "#000000ff");
}

TEST(Color, TryParseHtmlStringReturnsExpectedValues)
{
    // when caller specifies all channels
    ASSERT_EQ(try_parse_html_color_string("#ff0000ff"), Color::red());
    ASSERT_EQ(try_parse_html_color_string("#00ff00ff"), Color::green());
    ASSERT_EQ(try_parse_html_color_string("#0000ffff"), Color::blue());
    ASSERT_EQ(try_parse_html_color_string("#000000ff"), Color::black());
    ASSERT_EQ(try_parse_html_color_string("#ffff00ff"), Color::yellow());
    ASSERT_EQ(try_parse_html_color_string("#00000000"), Color::clear());

    // no colorspace conversion occurs on intermediate values (e.g. no sRGB-to-linear)
    ASSERT_EQ(try_parse_html_color_string("#110000ff"), Color((1.0f*16.0f + 1.0f)/255.0f, 0.0f, 0.0f, 1.0f));

    // when caller specifies 3 channels, assume alpha == 1.0
    ASSERT_EQ(try_parse_html_color_string("#ff0000"), Color::red());
    ASSERT_EQ(try_parse_html_color_string("#000000"), Color::black());

    // unparseable input
    ASSERT_EQ(try_parse_html_color_string("not a color"), std::nullopt);
    ASSERT_EQ(try_parse_html_color_string(" #ff0000ff"), std::nullopt);  // caller handles whitespace
    ASSERT_EQ(try_parse_html_color_string("ff0000ff"), std::nullopt);  // caller must put the # prefix before the string
    ASSERT_EQ(try_parse_html_color_string("red"), std::nullopt);  // literal color strings (e.g. as in Unity) aren't supported (yet)
}

TEST(Color, ToHSLAWorksAsExpected)
{
    for (auto const& [rgba, expected] : c_RGBAToHSLACases)
    {
        auto const got = to_hsla_color(rgba);
        ASSERT_NEAR(got.h, expected.h/360.0f, c_HLSLConversionTolerance);
        ASSERT_NEAR(got.s, expected.s, c_HLSLConversionTolerance);
        ASSERT_NEAR(got.l, expected.l, c_HLSLConversionTolerance);
        ASSERT_NEAR(got.a, expected.a, c_HLSLConversionTolerance);
    }
}

TEST(Color, HSLAToColorWorksAsExpected)
{
    for (auto const& tc : c_RGBAToHSLACases)
    {
        auto normalized = tc.expectedOutput;
        normalized.h /= 360.0f;

        auto const got = to_color(normalized);
        ASSERT_NEAR(got.r, tc.input.r, c_HLSLConversionTolerance) << tc << ", got = " << got;
        ASSERT_NEAR(got.g, tc.input.g, c_HLSLConversionTolerance) << tc << ", got = " << got;
        ASSERT_NEAR(got.b, tc.input.b, c_HLSLConversionTolerance) << tc << ", got = " << got;
        ASSERT_NEAR(got.a, tc.input.a, c_HLSLConversionTolerance) << tc << ", got = " << got;
    }
}
