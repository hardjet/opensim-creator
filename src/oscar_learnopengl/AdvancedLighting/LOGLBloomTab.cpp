#include "LOGLBloomTab.h"

#include <oscar/oscar.h>
#include <SDL_events.h>

#include <array>
#include <memory>
#include <vector>

using namespace osc::literals;
using namespace osc;

namespace
{
    constexpr CStringView c_TabStringID = "LearnOpenGL/Bloom";

    constexpr auto c_SceneLightPositions = std::to_array<Vec3>({
        { 0.0f, 0.5f,  1.5f},
        {-4.0f, 0.5f, -3.0f},
        { 3.0f, 0.5f,  1.0f},
        {-0.8f, 2.4f, -1.0f},
    });

    std::array<Color, c_SceneLightPositions.size()> const& GetSceneLightColors()
    {
        static auto const s_SceneLightColors = std::to_array<Color>({
            to_srgb_colorspace({ 5.0f, 5.0f,  5.0f}),
            to_srgb_colorspace({10.0f, 0.0f,  0.0f}),
            to_srgb_colorspace({ 0.0f, 0.0f, 15.0f}),
            to_srgb_colorspace({ 0.0f, 5.0f,  0.0f}),
        });
        return s_SceneLightColors;
    }

    std::vector<Mat4> CreateCubeTransforms()
    {
        std::vector<Mat4> rv;
        rv.reserve(6);

        {
            Mat4 m = identity<Mat4>();
            m = translate(m, Vec3(0.0f, 1.5f, 0.0));
            m = scale(m, Vec3(0.5f));
            rv.push_back(m);
        }

        {
            Mat4 m = identity<Mat4>();
            m = translate(m, Vec3(2.0f, 0.0f, 1.0));
            m = scale(m, Vec3(0.5f));
            rv.push_back(m);
        }

        {
            Mat4 m = identity<Mat4>();
            m = translate(m, Vec3(-1.0f, -1.0f, 2.0));
            m = rotate(m, 60_deg, UnitVec3{1.0, 0.0, 1.0});
            rv.push_back(m);
        }

        {
            Mat4 m = identity<Mat4>();
            m = translate(m, Vec3(0.0f, 2.7f, 4.0));
            m = rotate(m, 23_deg, UnitVec3{1.0, 0.0, 1.0});
            m = scale(m, Vec3(1.25));
            rv.push_back(m);
        }

        {
            Mat4 m = identity<Mat4>();
            m = translate(m, Vec3(-2.0f, 1.0f, -3.0));
            m = rotate(m, 124_deg, UnitVec3{1.0, 0.0, 1.0});
            rv.push_back(m);
        }

        {
            Mat4 m = identity<Mat4>();
            m = translate(m, Vec3(-3.0f, 0.0f, 0.0));
            m = scale(m, Vec3(0.5f));
            rv.push_back(m);
        }

        return rv;
    }

    MouseCapturingCamera CreateCameraThatMatchesLearnOpenGL()
    {
        MouseCapturingCamera rv;
        rv.set_position({0.0f, 0.5f, 5.0f});
        rv.set_near_clipping_plane(0.1f);
        rv.set_far_clipping_plane(100.0f);
        rv.set_background_color({0.0f, 0.0f, 0.0f, 1.0f});
        return rv;
    }
}

class osc::LOGLBloomTab::Impl final : public StandardTabImpl {
public:

    Impl() : StandardTabImpl{c_TabStringID}
    {
        m_SceneMaterial.set_vec3_array("uLightPositions", c_SceneLightPositions);
        m_SceneMaterial.set_color_array("uLightColors", GetSceneLightColors());
    }

private:
    void implOnMount() final
    {
        App::upd().make_main_loop_polling();
        m_Camera.on_mount();
    }

    void implOnUnmount() final
    {
        m_Camera.on_unmount();
        App::upd().make_main_loop_waiting();
    }

    bool implOnEvent(SDL_Event const& e) final
    {
        return m_Camera.on_event(e);
    }

    void implOnDraw() final
    {
        m_Camera.on_draw();
        draw3DScene();
    }

    void draw3DScene()
    {
        Rect const viewportRect = ui::GetMainViewportWorkspaceScreenRect();

        reformatAllTextures(viewportRect);
        renderSceneMRT();
        renderBlurredBrightness();
        renderCombinedScene(viewportRect);
        drawOverlays(viewportRect);
    }

    void reformatAllTextures(Rect const& viewportRect)
    {
        Vec2 const viewportDims = dimensions_of(viewportRect);
        AntiAliasingLevel const msxaaSamples = App::get().anti_aliasing_level();

        RenderTextureDescriptor textureDescription{viewportDims};
        textureDescription.set_anti_aliasing_level(msxaaSamples);
        textureDescription.set_color_format(RenderTextureFormat::DefaultHDR);

        // direct render targets are multisampled HDR textures
        m_SceneHDRColorOutput.reformat(textureDescription);
        m_SceneHDRThresholdedOutput.reformat(textureDescription);

        // intermediate buffers are single-sampled HDR textures
        textureDescription.set_anti_aliasing_level(AntiAliasingLevel::none());
        for (RenderTexture& pingPongBuffer : m_PingPongBlurOutputBuffers) {
            pingPongBuffer.reformat(textureDescription);
        }
    }

    void renderSceneMRT()
    {
        drawSceneCubesToCamera();
        drawLightBoxesToCamera();
        flushCameraRenderQueueToMRT();
    }

    void drawSceneCubesToCamera()
    {
        m_SceneMaterial.set_vec3("uViewWorldPos", m_Camera.position());

        // draw floor
        {
            Mat4 floorTransform = identity<Mat4>();
            floorTransform = translate(floorTransform, Vec3(0.0f, -1.0f, 0.0));
            floorTransform = scale(floorTransform, Vec3(12.5f, 0.5f, 12.5f));

            MaterialPropertyBlock floorProps;
            floorProps.set_texture("uDiffuseTexture", m_WoodTexture);

            graphics::draw(
                m_CubeMesh,
                floorTransform,
                m_SceneMaterial,
                m_Camera,
                floorProps
            );
        }

        MaterialPropertyBlock cubeProps;
        cubeProps.set_texture("uDiffuseTexture", m_ContainerTexture);
        for (auto const& cubeTransform : CreateCubeTransforms()) {
            graphics::draw(
                m_CubeMesh,
                cubeTransform,
                m_SceneMaterial,
                m_Camera,
                cubeProps
            );
        }
    }

    void drawLightBoxesToCamera()
    {
        std::array<Color, c_SceneLightPositions.size()> const& sceneLightColors = GetSceneLightColors();

        for (size_t i = 0; i < c_SceneLightPositions.size(); ++i) {
            Mat4 lightTransform = identity<Mat4>();
            lightTransform = translate(lightTransform, Vec3(c_SceneLightPositions[i]));
            lightTransform = scale(lightTransform, Vec3(0.25f));

            MaterialPropertyBlock lightProps;
            lightProps.set_color("uLightColor", sceneLightColors[i]);

            graphics::draw(
                m_CubeMesh,
                lightTransform,
                m_LightboxMaterial,
                m_Camera,
                lightProps
            );
        }
    }

    void flushCameraRenderQueueToMRT()
    {
        RenderTarget mrt{
            {
                RenderTargetColorAttachment{
                    m_SceneHDRColorOutput.upd_color_buffer(),
                    RenderBufferLoadAction::Clear,
                    RenderBufferStoreAction::Resolve,
                    Color::clear(),
                },
                RenderTargetColorAttachment{
                    m_SceneHDRThresholdedOutput.upd_color_buffer(),
                    RenderBufferLoadAction::Clear,
                    RenderBufferStoreAction::Resolve,
                    Color::clear(),
                },
            },
            RenderTargetDepthAttachment{
                m_SceneHDRThresholdedOutput.upd_depth_buffer(),
                RenderBufferLoadAction::Clear,
                RenderBufferStoreAction::DontCare,
            },
        };
        m_Camera.render_to(mrt);
    }

    void renderBlurredBrightness()
    {
        m_BlurMaterial.set_render_texture("uInputImage", m_SceneHDRThresholdedOutput);

        bool horizontal = false;
        for (RenderTexture& pingPongBuffer : m_PingPongBlurOutputBuffers) {
            m_BlurMaterial.set_bool("uHorizontal", horizontal);
            Camera camera;
            graphics::draw(m_QuadMesh, identity<Transform>(), m_BlurMaterial, camera);
            camera.render_to(pingPongBuffer);
            m_BlurMaterial.clear_render_texture("uInputImage");

            horizontal = !horizontal;
        }
    }

    void renderCombinedScene(Rect const& viewportRect)
    {
        m_FinalCompositingMaterial.set_render_texture("uHDRSceneRender", m_SceneHDRColorOutput);
        m_FinalCompositingMaterial.set_render_texture("uBloomBlur", m_PingPongBlurOutputBuffers[0]);
        m_FinalCompositingMaterial.set_bool("uBloom", true);
        m_FinalCompositingMaterial.set_float("uExposure", 1.0f);

        Camera camera;
        graphics::draw(m_QuadMesh, identity<Transform>(), m_FinalCompositingMaterial, camera);
        camera.set_pixel_rect(viewportRect);
        camera.render_to_screen();

        m_FinalCompositingMaterial.clear_render_texture("uBloomBlur");
        m_FinalCompositingMaterial.clear_render_texture("uHDRSceneRender");
    }

    void drawOverlays(Rect const& viewportRect)
    {
        constexpr float w = 200.0f;

        auto const textures = std::to_array<RenderTexture const*>({
            &m_SceneHDRColorOutput,
            &m_SceneHDRThresholdedOutput,
            m_PingPongBlurOutputBuffers.data(),
            m_PingPongBlurOutputBuffers.data() + 1,
        });

        for (size_t i = 0; i < textures.size(); ++i) {
            Vec2 const offset = {static_cast<float>(i)*w, 0.0f};
            Rect const overlayRect{
                viewportRect.p1 + offset,
                viewportRect.p1 + offset + w,
            };

            graphics::blit_to_screen(*textures[i], overlayRect);
        }
    }

    ResourceLoader m_Loader = App::resource_loader();

    Material m_SceneMaterial{Shader{
        m_Loader.slurp("oscar_learnopengl/shaders/AdvancedLighting/bloom/Bloom.vert"),
        m_Loader.slurp("oscar_learnopengl/shaders/AdvancedLighting/bloom/Bloom.frag"),
    }};

    Material m_LightboxMaterial{Shader{
        m_Loader.slurp("oscar_learnopengl/shaders/AdvancedLighting/bloom/LightBox.vert"),
        m_Loader.slurp("oscar_learnopengl/shaders/AdvancedLighting/bloom/LightBox.frag"),
    }};

    Material m_BlurMaterial{Shader{
        m_Loader.slurp("oscar_learnopengl/shaders/AdvancedLighting/bloom/Blur.vert"),
        m_Loader.slurp("oscar_learnopengl/shaders/AdvancedLighting/bloom/Blur.frag"),
    }};

    Material m_FinalCompositingMaterial{Shader{
        m_Loader.slurp("oscar_learnopengl/shaders/AdvancedLighting/bloom/Final.vert"),
        m_Loader.slurp("oscar_learnopengl/shaders/AdvancedLighting/bloom/Final.frag"),
    }};

    Texture2D m_WoodTexture = load_texture2D_from_image(
        m_Loader.open("oscar_learnopengl/textures/wood.png"),
        ColorSpace::sRGB
    );
    Texture2D m_ContainerTexture = load_texture2D_from_image(
        m_Loader.open("oscar_learnopengl/textures/container2.png"),
        ColorSpace::sRGB
    );
    Mesh m_CubeMesh = BoxGeometry{2.0f, 2.0f, 2.0f};
    Mesh m_QuadMesh = PlaneGeometry{2.0f, 2.0f};

    RenderTexture m_SceneHDRColorOutput;
    RenderTexture m_SceneHDRThresholdedOutput;
    std::array<RenderTexture, 2> m_PingPongBlurOutputBuffers;

    MouseCapturingCamera m_Camera = CreateCameraThatMatchesLearnOpenGL();
};


// public API

CStringView osc::LOGLBloomTab::id()
{
    return c_TabStringID;
}

osc::LOGLBloomTab::LOGLBloomTab(ParentPtr<ITabHost> const&) :
    m_Impl{std::make_unique<Impl>()}
{
}

osc::LOGLBloomTab::LOGLBloomTab(LOGLBloomTab&&) noexcept = default;
osc::LOGLBloomTab& osc::LOGLBloomTab::operator=(LOGLBloomTab&&) noexcept = default;
osc::LOGLBloomTab::~LOGLBloomTab() noexcept = default;

UID osc::LOGLBloomTab::implGetID() const
{
    return m_Impl->getID();
}

CStringView osc::LOGLBloomTab::implGetName() const
{
    return m_Impl->getName();
}

void osc::LOGLBloomTab::implOnMount()
{
    m_Impl->on_mount();
}

void osc::LOGLBloomTab::implOnUnmount()
{
    m_Impl->on_unmount();
}

bool osc::LOGLBloomTab::implOnEvent(SDL_Event const& e)
{
    return m_Impl->onEvent(e);
}

void osc::LOGLBloomTab::implOnTick()
{
    m_Impl->on_tick();
}

void osc::LOGLBloomTab::implOnDrawMainMenu()
{
    m_Impl->onDrawMainMenu();
}

void osc::LOGLBloomTab::implOnDraw()
{
    m_Impl->onDraw();
}
