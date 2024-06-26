#include "LOGLCoordinateSystemsTab.h"

#include <oscar/oscar.h>
#include <SDL_events.h>

#include <array>
#include <memory>

using namespace osc::literals;
using namespace osc;

namespace
{
    // worldspace positions of each cube (step 2)
    constexpr auto c_CubePositions = std::to_array<Vec3>({
        { 0.0f,  0.0f,  0.0f },
        { 2.0f,  5.0f, -15.0f},
        {-1.5f, -2.2f, -2.5f },
        {-3.8f, -2.0f, -12.3f},
        { 2.4f, -0.4f, -3.5f },
        {-1.7f,  3.0f, -7.5f },
        { 1.3f, -2.0f, -2.5f },
        { 1.5f,  2.0f, -2.5f },
        { 1.5f,  0.2f, -1.5f },
        {-1.3f,  1.0f, -1.5f },
    });

    constexpr CStringView c_TabStringID = "LearnOpenGL/CoordinateSystems";

    MouseCapturingCamera CreateCameraThatMatchesLearnOpenGL()
    {
        MouseCapturingCamera rv;
        rv.set_position({0.0f, 0.0f, 3.0f});
        rv.set_vertical_fov(45_deg);
        rv.set_near_clipping_plane(0.1f);
        rv.set_far_clipping_plane(100.0f);
        rv.set_background_color({0.2f, 0.3f, 0.3f, 1.0f});
        return rv;
    }

    Material MakeBoxMaterial(IResourceLoader& rl)
    {
        Material rv{Shader{
            rl.slurp("oscar_learnopengl/shaders/GettingStarted/CoordinateSystems.vert"),
            rl.slurp("oscar_learnopengl/shaders/GettingStarted/CoordinateSystems.frag"),
        }};

        rv.set_texture(
            "uTexture1",
            load_texture2D_from_image(
                rl.open("oscar_learnopengl/textures/container.jpg"),
                ColorSpace::sRGB,
                ImageLoadingFlags::FlipVertically
            )
        );

        rv.set_texture(
            "uTexture2",
            load_texture2D_from_image(
                rl.open("oscar_learnopengl/textures/awesomeface.png"),
                ColorSpace::sRGB,
                ImageLoadingFlags::FlipVertically
            )
        );

        return rv;
    }
}

class osc::LOGLCoordinateSystemsTab::Impl final : public StandardTabImpl {
public:

    Impl() : StandardTabImpl{c_TabStringID}
    {}

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

    void implOnTick() final
    {
        double const dt = App::get().frame_delta_since_startup().count();
        m_Step1Transform.rotation = angle_axis(50_deg * dt, UnitVec3{0.5f, 1.0f, 0.0f});
    }

    void implOnDraw() final
    {
        m_Camera.on_draw();
        draw3DScene();
        draw2DUI();
    }

    void draw3DScene()
    {
        // clear screen and ensure camera has correct pixel rect
        m_Camera.set_pixel_rect(ui::GetMainViewportWorkspaceScreenRect());

        // draw 3D scene
        if (m_ShowStep1) {
            graphics::draw(m_Mesh, m_Step1Transform, m_Material, m_Camera);
        }
        else {
            UnitVec3 const axis{1.0f, 0.3f, 0.5f};

            for (size_t i = 0; i < c_CubePositions.size(); ++i) {
                graphics::draw(
                    m_Mesh,
                    Transform{
                        .rotation = angle_axis(i * 20_deg, axis),
                        .position = c_CubePositions[i],
                    },
                    m_Material,
                    m_Camera
                );
            }
        }

        m_Camera.render_to_screen();
    }

    void draw2DUI()
    {
        ui::Begin("Tutorial Step");
        ui::Checkbox("step1", &m_ShowStep1);
        if (m_Camera.is_capturing_mouse()) {
            ui::Text("mouse captured (esc to uncapture)");
        }

        Vec3 const cameraPos = m_Camera.position();
        ui::Text("camera pos = (%f, %f, %f)", cameraPos.x, cameraPos.y, cameraPos.z);
        Eulers const cameraEulers = m_Camera.eulers();
        ui::Text("camera eulers = (%f, %f, %f)", cameraEulers.x.count(), cameraEulers.y.count(), cameraEulers.z.count());
        ui::End();

        m_PerfPanel.on_draw();
    }

    ResourceLoader m_Loader = App::resource_loader();
    Material m_Material = MakeBoxMaterial(m_Loader);
    Mesh m_Mesh = BoxGeometry{};
    MouseCapturingCamera m_Camera = CreateCameraThatMatchesLearnOpenGL();
    bool m_ShowStep1 = false;
    Transform m_Step1Transform;
    PerfPanel m_PerfPanel{"perf"};
};


// public API

CStringView osc::LOGLCoordinateSystemsTab::id()
{
    return c_TabStringID;
}

osc::LOGLCoordinateSystemsTab::LOGLCoordinateSystemsTab(ParentPtr<ITabHost> const&) :
    m_Impl{std::make_unique<Impl>()}
{
}

osc::LOGLCoordinateSystemsTab::LOGLCoordinateSystemsTab(LOGLCoordinateSystemsTab&&) noexcept = default;
osc::LOGLCoordinateSystemsTab& osc::LOGLCoordinateSystemsTab::operator=(LOGLCoordinateSystemsTab&&) noexcept = default;
osc::LOGLCoordinateSystemsTab::~LOGLCoordinateSystemsTab() noexcept = default;

UID osc::LOGLCoordinateSystemsTab::implGetID() const
{
    return m_Impl->getID();
}

CStringView osc::LOGLCoordinateSystemsTab::implGetName() const
{
    return m_Impl->getName();
}

void osc::LOGLCoordinateSystemsTab::implOnMount()
{
    m_Impl->on_mount();
}

void osc::LOGLCoordinateSystemsTab::implOnUnmount()
{
    m_Impl->on_unmount();
}

bool osc::LOGLCoordinateSystemsTab::implOnEvent(SDL_Event const& e)
{
    return m_Impl->onEvent(e);
}

void osc::LOGLCoordinateSystemsTab::implOnTick()
{
    m_Impl->on_tick();
}

void osc::LOGLCoordinateSystemsTab::implOnDraw()
{
    m_Impl->onDraw();
}
