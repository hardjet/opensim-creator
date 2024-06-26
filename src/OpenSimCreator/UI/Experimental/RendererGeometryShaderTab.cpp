#include "RendererGeometryShaderTab.h"

#include <OpenSimCreator/Graphics/SimTKMeshLoader.h>

#include <SDL_events.h>
#include <oscar/Graphics/Camera.h>
#include <oscar/Graphics/Color.h>
#include <oscar/Graphics/Graphics.h>
#include <oscar/Graphics/Material.h>
#include <oscar/Graphics/Mesh.h>
#include <oscar/Graphics/Shader.h>
#include <oscar/Maths/Angle.h>
#include <oscar/Maths/Eulers.h>
#include <oscar/Maths/MathHelpers.h>
#include <oscar/Maths/Transform.h>
#include <oscar/Maths/Vec3.h>
#include <oscar/Platform/App.h>
#include <oscar/UI/ImGuiHelpers.h>

#include <memory>

using namespace osc::literals;
using namespace osc;

class osc::RendererGeometryShaderTab::Impl final {
public:

    Impl()
    {
        m_SceneCamera.set_position({0.0f, 0.0f, 3.0f});
        m_SceneCamera.set_vertical_fov(45_deg);
        m_SceneCamera.set_near_clipping_plane(0.1f);
        m_SceneCamera.set_far_clipping_plane(100.0f);
    }

    UID getID() const
    {
        return m_TabID;
    }

    CStringView getName() const
    {
        return "GeometryShader";
    }

    void on_mount()
    {
        App::upd().make_main_loop_polling();
        m_IsMouseCaptured = true;
    }

    void on_unmount()
    {
        m_IsMouseCaptured = false;
        App::upd().set_show_cursor(true);
        App::upd().make_main_loop_waiting();
    }

    bool onEvent(SDL_Event const& e)
    {
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
        {
            m_IsMouseCaptured = false;
            return true;
        }
        else if (e.type == SDL_MOUSEBUTTONDOWN && ui::IsMouseInMainViewportWorkspaceScreenRect())
        {
            m_IsMouseCaptured = true;
            return true;
        }
        return false;
    }

    void onDraw()
    {
        // handle mouse capturing
        if (m_IsMouseCaptured)
        {
            ui::UpdateCameraFromInputs(m_SceneCamera, m_CameraEulers);
            ui::SetMouseCursor(ImGuiMouseCursor_None);
            App::upd().set_show_cursor(false);
        }
        else
        {
            ui::SetMouseCursor(ImGuiMouseCursor_Arrow);
            App::upd().set_show_cursor(true);
        }
        m_SceneCamera.set_pixel_rect(ui::GetMainViewportWorkspaceScreenRect());

        m_SceneMaterial.set_color("uDiffuseColor", m_MeshColor);
        graphics::draw(m_Mesh, identity<Transform>(), m_SceneMaterial, m_SceneCamera);
        graphics::draw(m_Mesh, identity<Transform>(), m_NormalsMaterial, m_SceneCamera);
        m_SceneCamera.render_to_screen();
    }

private:
    UID m_TabID;

    Material m_SceneMaterial
    {
        Shader
        {
            App::slurp("shaders/GeometryShaderTab/Scene.vert"),
            App::slurp("shaders/GeometryShaderTab/Scene.frag"),
        },
    };

    Material m_NormalsMaterial
    {
        Shader
        {
            App::slurp("shaders/GeometryShaderTab/DrawNormals.vert"),
            App::slurp("shaders/GeometryShaderTab/DrawNormals.geom"),
            App::slurp("shaders/GeometryShaderTab/DrawNormals.frag"),
        },
    };

    Mesh m_Mesh = LoadMeshViaSimTK(App::resource_filepath("geometry/hat_ribs_scap.vtp"));
    Camera m_SceneCamera;
    bool m_IsMouseCaptured = false;
    Eulers m_CameraEulers = {};
    Color m_MeshColor = Color::white();
};


// public API (PIMPL)

CStringView osc::RendererGeometryShaderTab::id()
{
    return "OpenSim/Experimental/GeometryShader";
}

osc::RendererGeometryShaderTab::RendererGeometryShaderTab(ParentPtr<ITabHost> const&) :
    m_Impl{std::make_unique<Impl>()}
{
}

osc::RendererGeometryShaderTab::RendererGeometryShaderTab(RendererGeometryShaderTab&&) noexcept = default;
osc::RendererGeometryShaderTab& osc::RendererGeometryShaderTab::operator=(RendererGeometryShaderTab&&) noexcept = default;
osc::RendererGeometryShaderTab::~RendererGeometryShaderTab() noexcept = default;

UID osc::RendererGeometryShaderTab::implGetID() const
{
    return m_Impl->getID();
}

CStringView osc::RendererGeometryShaderTab::implGetName() const
{
    return m_Impl->getName();
}

void osc::RendererGeometryShaderTab::implOnMount()
{
    m_Impl->on_mount();
}

void osc::RendererGeometryShaderTab::implOnUnmount()
{
    m_Impl->on_unmount();
}

bool osc::RendererGeometryShaderTab::implOnEvent(SDL_Event const& e)
{
    return m_Impl->onEvent(e);
}

void osc::RendererGeometryShaderTab::implOnDraw()
{
    m_Impl->onDraw();
}
