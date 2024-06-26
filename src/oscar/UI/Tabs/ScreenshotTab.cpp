#include "ScreenshotTab.h"

#include <oscar/Formats/Image.h>
#include <oscar/Graphics/Camera.h>
#include <oscar/Graphics/Color.h>
#include <oscar/Graphics/ColorSpace.h>
#include <oscar/Graphics/Graphics.h>
#include <oscar/Graphics/Material.h>
#include <oscar/Graphics/Mesh.h>
#include <oscar/Graphics/RenderTexture.h>
#include <oscar/Graphics/Shader.h>
#include <oscar/Graphics/Texture2D.h>
#include <oscar/Graphics/TextureFormat.h>
#include <oscar/Maths/CollisionTests.h>
#include <oscar/Maths/MatFunctions.h>
#include <oscar/Maths/Mat4.h>
#include <oscar/Maths/MathHelpers.h>
#include <oscar/Maths/Vec2.h>
#include <oscar/Maths/Vec3.h>
#include <oscar/Maths/Vec4.h>
#include <oscar/Platform/App.h>
#include <oscar/Platform/os.h>
#include <oscar/Platform/Screenshot.h>
#include <oscar/UI/ImGuiHelpers.h>
#include <oscar/UI/oscimgui.h>
#include <oscar/UI/Tabs/StandardTabImpl.h>
#include <oscar/Utils/Assertions.h>

#include <IconsFontAwesome5.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace osc;

namespace
{
    constexpr Color c_UnselectedColor = {1.0f, 1.0f, 1.0f, 0.4f};
    constexpr Color c_SelectedColor = {1.0f, 0.0f, 0.0f, 0.8f};

    // returns a rect that fully spans at least one dimension of the target rect, but has
    // the given aspect ratio
    //
    // the returned rectangle is in the same space as the target rectangle
    Rect ShrinkToFit(Rect targetRect, float aspectRatio)
    {
        const float targetAspectRatio = aspect_ratio(targetRect);
        const float ratio = targetAspectRatio / aspectRatio;
        const Vec2 targetDims = dimensions_of(targetRect);

        if (ratio >= 1.0f) {
            // it will touch the top/bottom but may (ratio != 1.0f) fall short of the left/right
            const Vec2 rvDims = {targetDims.x/ratio, targetDims.y};
            const Vec2 rvTopLeft = {targetRect.p1.x + 0.5f*(targetDims.x - rvDims.x), targetRect.p1.y};
            return {rvTopLeft, rvTopLeft + rvDims};
        }
        else
        {
            // it will touch the left/right but will not touch the top/bottom
            const Vec2 rvDims = {targetDims.x, ratio*targetDims.y};
            const Vec2 rvTopLeft = {targetRect.p1.x, targetRect.p1.y + 0.5f*(targetDims.y - rvDims.y)};
            return {rvTopLeft, rvTopLeft + rvDims};
        }
    }

    Rect MapRect(const Rect& sourceRect, const Rect& targetRect, const Rect& rect)
    {
        const Vec2 scale = dimensions_of(targetRect) / dimensions_of(sourceRect);

        return Rect{
            targetRect.p1 + scale*(rect.p1 - sourceRect.p1),
            targetRect.p1 + scale*(rect.p2 - sourceRect.p1),
        };
    }
}

class osc::ScreenshotTab::Impl final : public StandardTabImpl {
public:
    explicit Impl(Screenshot&& screenshot) :
        StandardTabImpl{ICON_FA_COOKIE " ScreenshotTab"},
        m_Screenshot{std::move(screenshot)}
    {
        m_ImageTexture.set_filter_mode(TextureFilterMode::Mipmap);
    }

private:
    void implOnDrawMainMenu() final
    {
        if (ui::BeginMenu("File"))
        {
            if (ui::MenuItem("Save"))
            {
                actionSaveOutputImage();
            }
            ui::EndMenu();
        }
    }

    void implOnDraw() final
    {
        ui::DockSpaceOverViewport(ui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        // draw screenshot window
        {
            ui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
            ui::Begin("Screenshot");
            ui::PopStyleVar();

            Rect imageRect = drawScreenshot();
            drawOverlays(*ui::GetWindowDrawList(), imageRect, c_UnselectedColor, c_SelectedColor);

            ui::End();
        }

        // draw controls window
        {
            int id = 0;
            ui::Begin("Controls");
            for (const ScreenshotAnnotation& annotation : m_Screenshot.annotations) {
                ui::PushID(id++);
                ui::TextUnformatted(annotation.label);
                ui::PopID();
            }
            ui::End();
        }
    }

    // returns screenspace rect of the screenshot within the UI
    Rect drawScreenshot()
    {
        const Vec2 screenTopLeft = ui::GetCursorScreenPos();
        const Rect windowRect = {screenTopLeft, screenTopLeft + Vec2{ui::GetContentRegionAvail()}};
        const Rect imageRect = ShrinkToFit(windowRect, aspect_ratio(m_Screenshot.image.dimensions()));
        ui::SetCursorScreenPos(imageRect.p1);
        ui::Image(m_ImageTexture, dimensions_of(imageRect));
        return imageRect;
    }

    void drawOverlays(
        ImDrawList& drawlist,
        const Rect& imageRect,
        const Color& unselectedColor,
        const Color& selectedColor)
    {
        const Vec2 mousePos = ui::GetMousePos();
        const bool leftClickReleased = ui::IsMouseReleased(ImGuiMouseButton_Left);
        const Rect imageSourceRect = {{0.0f, 0.0f}, m_Screenshot.image.dimensions()};

        for (const ScreenshotAnnotation& annotation : m_Screenshot.annotations) {
            const Rect annotationRectScreenSpace = MapRect(imageSourceRect, imageRect, annotation.rect);
            const bool selected = m_SelectedAnnotations.contains(annotation.label);
            const bool hovered = is_intersecting(annotationRectScreenSpace, mousePos);

            Vec4 color = selected ? selectedColor : unselectedColor;
            if (hovered) {
                color.w = saturate(color.w + 0.3f);
            }

            if (hovered && leftClickReleased) {
                if (selected) {
                    m_SelectedAnnotations.erase(annotation.label);
                }
                else {
                    m_SelectedAnnotations.insert(annotation.label);
                }
            }

            drawlist.AddRect(
                annotationRectScreenSpace.p1,
                annotationRectScreenSpace.p2,
                ui::ColorConvertFloat4ToU32(color),
                3.0f,
                0,
                3.0f
            );
        }
    }

    void actionSaveOutputImage()
    {
        const std::optional<std::filesystem::path> maybeImagePath = PromptUserForFileSaveLocationAndAddExtensionIfNecessary("png");
        if (maybeImagePath) {
            std::ofstream fout{*maybeImagePath, std::ios_base::binary};
            if (!fout) {
                throw std::runtime_error{maybeImagePath->string() + ": cannot open for writing"};
            }
            Texture2D outputImage = renderOutputImage();
            write_to_png(outputImage, fout);
            OpenPathInOSDefaultApplication(*maybeImagePath);
        }
    }

    Texture2D renderOutputImage()
    {
        std::optional<RenderTexture> rt;
        rt.emplace(RenderTextureDescriptor{m_ImageTexture.dimensions()});

        // blit the screenshot into the output
        graphics::blit(m_ImageTexture, *rt);

        // draw overlays to a local ImGui drawlist
        ImDrawList drawlist{ui::GetDrawListSharedData()};
        drawlist.Flags |= ImDrawListFlags_AntiAliasedLines;
        drawlist.AddDrawCmd();
        Color outlineColor = c_SelectedColor;
        outlineColor.a = 1.0f;
        drawOverlays(
            drawlist,
            Rect{{0.0f, 0.0f}, m_ImageTexture.dimensions()},
            {0.0f, 0.0f, 0.0f, 0.0f},
            outlineColor
        );

        // render drawlist to output
        {
            // upload vertex positions/colors
            Mesh mesh;
            {
                // vertices
                {
                    std::vector<Vec3> vertices;
                    vertices.reserve(drawlist.VtxBuffer.size());
                    for (const ImDrawVert& vert : drawlist.VtxBuffer) {
                        vertices.emplace_back(vert.pos.x, vert.pos.y, 0.0f);
                    }
                    mesh.set_vertices(vertices);
                }

                // colors
                {
                    std::vector<Color> colors;
                    colors.reserve(drawlist.VtxBuffer.size());
                    for (const ImDrawVert& vert : drawlist.VtxBuffer) {
                        const Color linearColor = ui::to_color(vert.col);
                        colors.push_back(linearColor);
                    }
                    mesh.set_colors(colors);
                }
            }

            // solid color material
            Material material
            {
                Shader
                {
                    App::slurp("oscar/shaders/PerVertexColor.vert"),
                    App::slurp("oscar/shaders/PerVertexColor.frag"),
                }
            };

            Camera c;
            c.set_view_matrix_override(identity<Mat4>());

            {
                // project screenspace overlays into NDC
                float L = 0.0f;
                float R = static_cast<float>(m_ImageTexture.dimensions().x);
                float T = 0.0f;
                float B = static_cast<float>(m_ImageTexture.dimensions().y);
                const Mat4 proj = {
                    { 2.0f/(R-L),   0.0f,         0.0f,   0.0f },
                    { 0.0f,         2.0f/(T-B),   0.0f,   0.0f },
                    { 0.0f,         0.0f,        -1.0f,   0.0f },
                    { (R+L)/(L-R),  (T+B)/(B-T),  0.0f,   1.0f },
                };
                c.set_projection_matrix_override(proj);
            }
            c.set_clear_flags(CameraClearFlags::Nothing);

            for (int cmdIdx = 0; cmdIdx < drawlist.CmdBuffer.Size; ++cmdIdx)
            {
                const ImDrawCmd& cmd = drawlist.CmdBuffer[cmdIdx];
                {
                    // upload indices
                    std::vector<ImDrawIdx> indices;
                    indices.reserve(cmd.ElemCount);
                    for (unsigned int i = cmd.IdxOffset; i < cmd.IdxOffset + cmd.ElemCount; ++i)
                    {
                        indices.push_back(drawlist.IdxBuffer[static_cast<int>(i)]);
                    }
                    mesh.set_indices(indices);
                }
                graphics::draw(mesh, Transform{}, material, c);
            }

            OSC_ASSERT(rt.has_value());
            c.render_to(*rt);
        }

        Texture2D t
        {
            rt->dimensions(),
            TextureFormat::RGB24,
            ColorSpace::sRGB,
        };
        graphics::copy_texture(*rt, t);
        return t;
    }

    Screenshot m_Screenshot;
    Texture2D m_ImageTexture = m_Screenshot.image;
    std::unordered_set<std::string> m_SelectedAnnotations;
};


// public API

osc::ScreenshotTab::ScreenshotTab(const ParentPtr<ITabHost>&, Screenshot&& screenshot) :
    m_Impl{std::make_unique<Impl>(std::move(screenshot))}
{}

osc::ScreenshotTab::ScreenshotTab(ScreenshotTab&&) noexcept = default;
osc::ScreenshotTab& osc::ScreenshotTab::operator=(ScreenshotTab&&) noexcept = default;
osc::ScreenshotTab::~ScreenshotTab() noexcept = default;

UID osc::ScreenshotTab::implGetID() const
{
    return m_Impl->getID();
}

CStringView osc::ScreenshotTab::implGetName() const
{
    return m_Impl->getName();
}

void osc::ScreenshotTab::implOnDrawMainMenu()
{
    m_Impl->onDrawMainMenu();
}

void osc::ScreenshotTab::implOnDraw()
{
    m_Impl->onDraw();
}
