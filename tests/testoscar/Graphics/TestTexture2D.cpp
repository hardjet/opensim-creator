#include <oscar/Graphics/Texture2D.h>

#include <testoscar/TestingHelpers.h>

#include <gtest/gtest.h>
#include <oscar/Graphics/Color.h>
#include <oscar/Graphics/Color32.h>
#include <oscar/Graphics/ColorSpace.h>
#include <oscar/Graphics/TextureFilterMode.h>
#include <oscar/Graphics/TextureFormat.h>
#include <oscar/Graphics/TextureWrapMode.h>
#include <oscar/Maths/Vec2.h>
#include <oscar/Maths/Vec4.h>
#include <oscar/Utils/ObjectRepresentation.h>

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <utility>
#include <vector>

using namespace osc::testing;
using namespace osc;

namespace
{
    Texture2D GenerateTexture()
    {
        Texture2D rv{Vec2i{2, 2}};
        rv.setPixels(std::vector<Color>(4, Color::red()));
        return rv;
    }
}

TEST(Texture2D, Texture2DConstructorThrowsIfGivenZeroOrNegativeSizedDimensions)
{
    ASSERT_ANY_THROW({ Texture2D(Vec2i(0, 0)); });   // x and y are zero
    ASSERT_ANY_THROW({ Texture2D(Vec2i(0, 1)); });   // x is zero
    ASSERT_ANY_THROW({ Texture2D(Vec2i(1, 0)); });   // y is zero

    ASSERT_ANY_THROW({ Texture2D(Vec2i(-1, -1)); }); // x any y are negative
    ASSERT_ANY_THROW({ Texture2D(Vec2i(-1, 1)); });  // x is negative
    ASSERT_ANY_THROW({ Texture2D(Vec2i(1, -1)); });  // y is negative
}

TEST(Texture2D, DefaultConstructorCreatesRGBATextureWithExpectedColorSpaceEtc)
{
    Texture2D t{Vec2i(1, 1)};

    ASSERT_EQ(t.getDimensions(), Vec2i(1, 1));
    ASSERT_EQ(t.getTextureFormat(), TextureFormat::RGBA32);
    ASSERT_EQ(t.getColorSpace(), ColorSpace::sRGB);
    ASSERT_EQ(t.getWrapMode(), TextureWrapMode::Repeat);
    ASSERT_EQ(t.getFilterMode(), TextureFilterMode::Linear);
}

TEST(Texture2D, CanSetPixels32OnDefaultConstructedTexture)
{
    Vec2i const dimensions = {1, 1};
    std::vector<Color32> const pixels(static_cast<size_t>(dimensions.x * dimensions.y));

    Texture2D t{dimensions};
    t.setPixels32(pixels);

    ASSERT_EQ(t.getDimensions(), dimensions);
    ASSERT_EQ(t.getPixels32(), pixels);
}

TEST(Texture2D, SetPixelsThrowsIfNumberOfPixelsDoesNotMatchDimensions)
{
    Vec2i const dimensions = {1, 1};
    std::vector<Color> const incorrectPixels(dimensions.x * dimensions.y + 1);

    Texture2D t{dimensions};

    ASSERT_ANY_THROW({ t.setPixels(incorrectPixels); });
}

TEST(Texture2D, SetPixels32ThrowsIfNumberOfPixelsDoesNotMatchDimensions)
{
    Vec2i const dimensions = {1, 1};
    std::vector<Color32> const incorrectPixels(dimensions.x * dimensions.y + 1);

    Texture2D t{dimensions};
    ASSERT_ANY_THROW({ t.setPixels32(incorrectPixels); });
}

TEST(Texture2D, SetPixelDataThrowsIfNumberOfPixelBytesDoesNotMatchDimensions)
{
    Vec2i const dimensions = {1, 1};
    std::vector<Color32> const incorrectPixels(dimensions.x * dimensions.y + 1);

    Texture2D t{dimensions};

    ASSERT_EQ(t.getTextureFormat(), TextureFormat::RGBA32);  // sanity check
    ASSERT_ANY_THROW({ t.setPixelData(ViewObjectRepresentations<uint8_t>(incorrectPixels)); });
}

TEST(Texture2D, SetPixelDataDoesNotThrowWhenGivenValidNumberOfPixelBytes)
{
    Vec2i const dimensions = {1, 1};
    std::vector<Color32> const pixels(static_cast<size_t>(dimensions.x * dimensions.y));

    Texture2D t{dimensions};

    ASSERT_EQ(t.getTextureFormat(), TextureFormat::RGBA32);  // sanity check

    t.setPixelData(ViewObjectRepresentations<uint8_t>(pixels));
}

TEST(Texture2D, SetPixelDataWorksFineFor8BitSingleChannelData)
{
    Vec2i const dimensions = {1, 1};
    std::vector<uint8_t> const singleChannelPixels(static_cast<size_t>(dimensions.x * dimensions.y));

    Texture2D t{dimensions, TextureFormat::R8};
    t.setPixelData(singleChannelPixels);  // shouldn't throw
}

TEST(Texture2D, SetPixelDataWith8BitSingleChannelDataFollowedByGetPixelsBlanksOutGreenAndRed)
{
    uint8_t const color{0x88};
    float const colorFloat = static_cast<float>(color) / 255.0f;
    Vec2i const dimensions = {1, 1};
    std::vector<uint8_t> const singleChannelPixels(static_cast<size_t>(dimensions.x * dimensions.y), color);

    Texture2D t{dimensions, TextureFormat::R8};
    t.setPixelData(singleChannelPixels);

    for (Color const& c : t.getPixels())
    {
        ASSERT_EQ(c, Color(colorFloat, 0.0f, 0.0f, 1.0f));
    }
}

TEST(Texture2D, SetPixelDataWith8BitSingleChannelDataFollowedByGetPixels32BlanksOutGreenAndRed)
{
    uint8_t const color{0x88};
    Vec2i const dimensions = {1, 1};
    std::vector<uint8_t> const singleChannelPixels(static_cast<size_t>(dimensions.x * dimensions.y), color);

    Texture2D t{dimensions, TextureFormat::R8};
    t.setPixelData(singleChannelPixels);

    for (Color32 const& c : t.getPixels32())
    {
        Color32 expected{color, 0x00, 0x00, 0xff};
        ASSERT_EQ(c, expected);
    }
}

TEST(Texture2D, SetPixelDataWith32BitFloatingPointValuesFollowedByGetPixelDataReturnsSameSpan)
{
    Vec4 const color = GenerateVec4();
    Vec2i const dimensions = {1, 1};
    std::vector<Vec4> const rgbaFloatPixels(static_cast<size_t>(dimensions.x * dimensions.y), color);

    Texture2D t(dimensions, TextureFormat::RGBAFloat);
    t.setPixelData(ViewObjectRepresentations<uint8_t>(rgbaFloatPixels));

    ASSERT_TRUE(ContainersEqual(t.getPixelData(), ViewObjectRepresentations<uint8_t>(rgbaFloatPixels)));
}

TEST(Texture2D, SetPixelDataWith32BitFloatingPointValuesFollowedByGetPixelsReturnsSameValues)
{
    Color const hdrColor = {1.2f, 1.4f, 1.3f, 1.0f};
    Vec2i const dimensions = {1, 1};
    std::vector<Color> const rgbaFloatPixels(static_cast<size_t>(dimensions.x * dimensions.y), hdrColor);

    Texture2D t(dimensions, TextureFormat::RGBAFloat);
    t.setPixelData(ViewObjectRepresentations<uint8_t>(rgbaFloatPixels));

    ASSERT_EQ(t.getPixels(), rgbaFloatPixels);  // because the texture holds 32-bit floats
}

TEST(Texture2D, SetPixelsOnAn8BitTextureLDRClampsTheColorValues)
{
    Color const hdrColor = {1.2f, 1.4f, 1.3f, 1.0f};
    Vec2i const dimensions = {1, 1};
    std::vector<Color> const hdrPixels(static_cast<size_t>(dimensions.x * dimensions.y), hdrColor);

    Texture2D t(dimensions, TextureFormat::RGBA32);  // note: not HDR

    t.setPixels(hdrPixels);

    ASSERT_NE(t.getPixels(), hdrPixels);  // because the impl had to convert them
}

TEST(Texture2D, SetPixels32OnAn8BitTextureDoesntConvert)
{
    Color32 const color32 = {0x77, 0x63, 0x24, 0x76};
    Vec2i const dimensions = {1, 1};
    std::vector<Color32> const pixels32(static_cast<size_t>(dimensions.x * dimensions.y), color32);

    Texture2D t(dimensions, TextureFormat::RGBA32);  // note: matches pixel format

    t.setPixels32(pixels32);

    ASSERT_EQ(t.getPixels32(), pixels32);  // because no conversion was required
}

TEST(Texture2D, SetPixels32OnA32BitTextureDoesntDetectablyChangeValues)
{
    Color32 const color32 = {0x77, 0x63, 0x24, 0x76};
    Vec2i const dimensions = {1, 1};
    std::vector<Color32> const pixels32(static_cast<size_t>(dimensions.x * dimensions.y), color32);

    Texture2D t(dimensions, TextureFormat::RGBAFloat);  // note: higher precision than input

    t.setPixels32(pixels32);

    ASSERT_EQ(t.getPixels32(), pixels32);  // because, although conversion happened, it was _from_ a higher precision
}

TEST(Texture2D, CanCopyConstruct)
{
    Texture2D t = GenerateTexture();
    Texture2D{t};
}

TEST(Texture2D, CanMoveConstruct)
{
    Texture2D t = GenerateTexture();
    Texture2D copy{std::move(t)};
}

TEST(Texture2D, CanCopyAssign)
{
    Texture2D t1 = GenerateTexture();
    Texture2D t2 = GenerateTexture();

    t1 = t2;
}

TEST(Texture2D, CanMoveAssign)
{
    Texture2D t1 = GenerateTexture();
    Texture2D t2 = GenerateTexture();

    t1 = std::move(t2);
}

TEST(Texture2D, GetWidthReturnsSuppliedWidth)
{
    int width = 2;
    int height = 6;

    Texture2D t{{width, height}};

    ASSERT_EQ(t.getDimensions().x, width);
}

TEST(Texture2D, GetHeightReturnsSuppliedHeight)
{
    int width = 2;
    int height = 6;

    Texture2D t{{width, height}};

    ASSERT_EQ(t.getDimensions().y, height);
}

TEST(Texture2D, GetColorSpaceReturnsProvidedColorSpaceIfSRGB)
{
    Texture2D t{{1, 1}, TextureFormat::RGBA32, ColorSpace::sRGB};

    ASSERT_EQ(t.getColorSpace(), ColorSpace::sRGB);
}

TEST(Texture2D, GetColorSpaceReturnsProvidedColorSpaceIfLinear)
{
    Texture2D t{{1, 1}, TextureFormat::RGBA32, ColorSpace::Linear};

    ASSERT_EQ(t.getColorSpace(), ColorSpace::Linear);
}

TEST(Texture2D, GetWrapModeReturnsRepeatedByDefault)
{
    Texture2D t = GenerateTexture();

    ASSERT_EQ(t.getWrapMode(), TextureWrapMode::Repeat);
}

TEST(Texture2D, SetWrapModeMakesSubsequentGetWrapModeReturnNewWrapMode)
{
    Texture2D t = GenerateTexture();

    TextureWrapMode wm = TextureWrapMode::Mirror;

    ASSERT_NE(t.getWrapMode(), wm);

    t.setWrapMode(wm);

    ASSERT_EQ(t.getWrapMode(), wm);
}

TEST(Texture2D, SetWrapModeCausesGetWrapModeUToAlsoReturnNewWrapMode)
{
    Texture2D t = GenerateTexture();

    TextureWrapMode wm = TextureWrapMode::Mirror;

    ASSERT_NE(t.getWrapMode(), wm);
    ASSERT_NE(t.getWrapModeU(), wm);

    t.setWrapMode(wm);

    ASSERT_EQ(t.getWrapModeU(), wm);
}

TEST(Texture2D, SetWrapModeUCausesGetWrapModeUToReturnValue)
{
    Texture2D t = GenerateTexture();

    TextureWrapMode wm = TextureWrapMode::Mirror;

    ASSERT_NE(t.getWrapModeU(), wm);

    t.setWrapModeU(wm);

    ASSERT_EQ(t.getWrapModeU(), wm);
}

TEST(Texture2D, SetWrapModeVCausesGetWrapModeVToReturnValue)
{
    Texture2D t = GenerateTexture();

    TextureWrapMode wm = TextureWrapMode::Mirror;

    ASSERT_NE(t.getWrapModeV(), wm);

    t.setWrapModeV(wm);

    ASSERT_EQ(t.getWrapModeV(), wm);
}

TEST(Texture2D, SetWrapModeWCausesGetWrapModeWToReturnValue)
{
    Texture2D t = GenerateTexture();

    TextureWrapMode wm = TextureWrapMode::Mirror;

    ASSERT_NE(t.getWrapModeW(), wm);

    t.setWrapModeW(wm);

    ASSERT_EQ(t.getWrapModeW(), wm);
}

TEST(Texture2D, SetFilterModeCausesGetFilterModeToReturnValue)
{
    Texture2D t = GenerateTexture();

    TextureFilterMode tfm = TextureFilterMode::Nearest;

    ASSERT_NE(t.getFilterMode(), tfm);

    t.setFilterMode(tfm);

    ASSERT_EQ(t.getFilterMode(), tfm);
}

TEST(Texture2D, SetFilterModeMipmapReturnsMipmapOnGetFilterMode)
{
    Texture2D t = GenerateTexture();

    TextureFilterMode tfm = TextureFilterMode::Mipmap;

    ASSERT_NE(t.getFilterMode(), tfm);

    t.setFilterMode(tfm);

    ASSERT_EQ(t.getFilterMode(), tfm);
}

TEST(Texture2D, CanBeComparedForEquality)
{
    Texture2D t1 = GenerateTexture();
    Texture2D t2 = GenerateTexture();

    (void)(t1 == t2);  // just ensure it compiles + runs
}

TEST(Texture2D, CopyConstructingComparesEqual)
{
    Texture2D t = GenerateTexture();
    Texture2D tcopy{t};  // NOLINT(performance-unnecessary-copy-initialization)

    ASSERT_EQ(t, tcopy);
}

TEST(Texture2D, CopyAssignmentMakesEqualityReturnTrue)
{
    Texture2D t1 = GenerateTexture();
    Texture2D t2 = GenerateTexture();

    t1 = t2;

    ASSERT_EQ(t1, t2);
}

TEST(Texture2D, CanBeComparedForNotEquals)
{
    Texture2D t1 = GenerateTexture();
    Texture2D t2 = GenerateTexture();

    (void)(t1 != t2);  // just ensure this expression compiles
}

TEST(Texture2D, ChangingWrapModeMakesCopyUnequal)
{
    Texture2D t1 = GenerateTexture();
    Texture2D t2{t1};
    TextureWrapMode wm = TextureWrapMode::Clamp;

    ASSERT_EQ(t1, t2);
    ASSERT_NE(t2.getWrapMode(), wm);

    t2.setWrapMode(wm);

    ASSERT_NE(t1, t2);
}

TEST(Texture2D, ChangingWrapModeUMakesCopyUnequal)
{
    Texture2D t1 = GenerateTexture();
    Texture2D t2{t1};
    TextureWrapMode wm = TextureWrapMode::Clamp;

    ASSERT_EQ(t1, t2);
    ASSERT_NE(t2.getWrapModeU(), wm);

    t2.setWrapModeU(wm);

    ASSERT_NE(t1, t2);
}

TEST(Texture2D, ChangingWrapModeVMakesCopyUnequal)
{
    Texture2D t1 = GenerateTexture();
    Texture2D t2{t1};
    TextureWrapMode wm = TextureWrapMode::Clamp;

    ASSERT_EQ(t1, t2);
    ASSERT_NE(t2.getWrapModeV(), wm);

    t2.setWrapModeV(wm);

    ASSERT_NE(t1, t2);
}

TEST(Texture2D, ChangingWrapModeWMakesCopyUnequal)
{
    Texture2D t1 = GenerateTexture();
    Texture2D t2{t1};
    TextureWrapMode wm = TextureWrapMode::Clamp;

    ASSERT_EQ(t1, t2);
    ASSERT_NE(t2.getWrapModeW(), wm);

    t2.setWrapModeW(wm);

    ASSERT_NE(t1, t2);
}

TEST(Texture2D, ChangingFilterModeMakesCopyUnequal)
{
    Texture2D t1 = GenerateTexture();
    Texture2D t2{t1};
    TextureFilterMode fm = TextureFilterMode::Nearest;

    ASSERT_EQ(t1, t2);
    ASSERT_NE(t2.getFilterMode(), fm);

    t2.setFilterMode(fm);

    ASSERT_NE(t1, t2);
}

TEST(Texture2D, CanBeWrittenToOutputStream)
{
    Texture2D t = GenerateTexture();

    std::stringstream ss;
    ss << t;

    ASSERT_FALSE(ss.str().empty());
}
