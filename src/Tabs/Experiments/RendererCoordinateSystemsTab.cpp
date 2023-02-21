#include "RendererCoordinateSystemsTab.hpp"

#include "src/Bindings/ImGuiHelpers.hpp"
#include "src/Graphics/Camera.hpp"
#include "src/Graphics/Graphics.hpp"
#include "src/Graphics/GraphicsHelpers.hpp"
#include "src/Graphics/Material.hpp"
#include "src/Graphics/MeshGen.hpp"
#include "src/Graphics/Shader.hpp"
#include "src/Graphics/Texture2D.hpp"
#include "src/Maths/Constants.hpp"
#include "src/Maths/Transform.hpp"
#include "src/Panels/PerfPanel.hpp"
#include "src/Platform/App.hpp"
#include "src/Utils/CStringView.hpp"
#include "src/Utils/UID.hpp"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <SDL_events.h>

#include <cstdint>
#include <memory>

// worldspace positions of each cube (step 2)
static glm::vec3 constexpr c_CubePositions[] =
{
    { 0.0f,  0.0f,  0.0f },
    { 2.0f,  5.0f, -15.0f},
    {-1.5f, -2.2f, -2.5f },
    {-3.8f, -2.0f, -12.3f},
    { 2.4f, -0.4f, -3.5f },
    {-1.7f,  3.0f, -7.5f },
    { 1.3f, -2.0f, -2.5f },
    { 1.5f,  2.0f, -2.5f },
    { 1.5f,  0.2f, -1.5f },
    {-1.3f,  1.0f, -1.5  },
};

class osc::RendererCoordinateSystemsTab::Impl final {
public:

    Impl()
    {
        m_Camera.setPosition({0.0f, 0.0f, 3.0f});
        m_Camera.setCameraFOV(glm::radians(45.0f));
        m_Camera.setNearClippingPlane(0.1f);
        m_Camera.setFarClippingPlane(100.0f);
        m_Camera.setBackgroundColor({0.2f, 0.3f, 0.3f, 1.0f});
        m_Material.setTexture("uTexture1", LoadTexture2DFromImage(App::resource("textures/container.jpg"), ImageFlags_FlipVertically));
        m_Material.setTexture("uTexture2", LoadTexture2DFromImage(App::resource("textures/awesomeface.png"), ImageFlags_FlipVertically));
    }

    UID getID() const
    {
        return m_TabID;
    }

    CStringView getName() const
    {
        return "Coordinate Systems (LearnOpenGL)";
    }

    void onMount()
    {
        App::upd().makeMainEventLoopPolling();
        m_IsMouseCaptured = true;
    }

    void onUnmount()
    {
        m_IsMouseCaptured = false;
        App::upd().setShowCursor(true);
        App::upd().makeMainEventLoopWaiting();
    }

    bool onEvent(SDL_Event const& e)
    {
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
        {
            m_IsMouseCaptured = false;
            return true;
        }
        else if (e.type == SDL_MOUSEBUTTONDOWN && osc::IsMouseInMainViewportWorkspaceScreenRect())
        {
            m_IsMouseCaptured = true;
            return true;
        }
        return false;
    }

    void onTick()
    {
        float constexpr rotationSpeed = glm::radians(50.0f);
        float const dt = App::get().getDeltaSinceAppStartup().count();
        float const angle = rotationSpeed * dt;
        glm::vec3 const axis = glm::normalize(glm::vec3{0.5f, 1.0f, 0.0f});

        m_Step1.rotation = glm::angleAxis(angle, axis);
    }

    void onDraw()
    {
        // handle mouse capturing
        if (m_IsMouseCaptured)
        {
            UpdateEulerCameraFromImGuiUserInput(m_Camera, m_CameraEulers);
            ImGui::SetMouseCursor(ImGuiMouseCursor_None);
            App::upd().setShowCursor(false);
        }
        else
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
            App::upd().setShowCursor(true);
        }

        // clear screen and ensure camera has correct pixel rect
        m_Camera.setPixelRect(osc::GetMainViewportWorkspaceScreenRect());

        // draw 3D scene
        if (m_ShowStep1)
        {
            Graphics::DrawMesh(m_Mesh, m_Step1, m_Material, m_Camera);
        }
        else
        {
            int32_t i = 0;
            glm::vec3 const axis = glm::normalize(glm::vec3{1.0f, 0.3f, 0.5f});
            for (glm::vec3 const& pos : c_CubePositions)
            {
                float const angle = glm::radians(i++ * 20.0f);

                Transform t;
                t.rotation = glm::angleAxis(angle, axis);
                t.position = pos;

                Graphics::DrawMesh(m_Mesh, t, m_Material, m_Camera);
            }
        }

        m_Camera.renderToScreen();

        // draw UI extras
        {
            ImGui::Begin("Tutorial Step");
            ImGui::Checkbox("step1", &m_ShowStep1);
            if (m_IsMouseCaptured)
            {
                ImGui::Text("mouse captured (esc to uncapture)");
            }

            glm::vec3 cameraPos = m_Camera.getPosition();
            ImGui::Text("camera pos = (%f, %f, %f)", cameraPos.x, cameraPos.y, cameraPos.z);
            glm::vec3 cameraEulers = glm::degrees(m_CameraEulers);
            ImGui::Text("camera eulers = (%f, %f, %f)", cameraEulers.x, cameraEulers.y, cameraEulers.z);
            ImGui::End();

            m_PerfPanel.draw();
        }
    }

private:
    UID m_TabID;

    Material m_Material
    {
        Shader
        {
            App::slurp("shaders/ExperimentCoordinateSystems.vert"),
            App::slurp("shaders/ExperimentCoordinateSystems.frag"),
        },
    };
    Mesh m_Mesh = GenLearnOpenGLCube();
    Camera m_Camera;
    bool m_IsMouseCaptured = false;
    glm::vec3 m_CameraEulers = {0.0f, 0.0f, 0.0f};

    bool m_ShowStep1 = false;
    Transform m_Step1;

    PerfPanel m_PerfPanel{"perf"};
};


// public API (PIMPL)

osc::CStringView osc::RendererCoordinateSystemsTab::id() noexcept
{
    return "Renderer/CoordinateSystems";
}

osc::RendererCoordinateSystemsTab::RendererCoordinateSystemsTab(TabHost*) :
    m_Impl{std::make_unique<Impl>()}
{
}

osc::RendererCoordinateSystemsTab::RendererCoordinateSystemsTab(RendererCoordinateSystemsTab&&) noexcept = default;
osc::RendererCoordinateSystemsTab& osc::RendererCoordinateSystemsTab::operator=(RendererCoordinateSystemsTab&&) noexcept = default;
osc::RendererCoordinateSystemsTab::~RendererCoordinateSystemsTab() noexcept = default;

osc::UID osc::RendererCoordinateSystemsTab::implGetID() const
{
    return m_Impl->getID();
}

osc::CStringView osc::RendererCoordinateSystemsTab::implGetName() const
{
    return m_Impl->getName();
}

void osc::RendererCoordinateSystemsTab::implOnMount()
{
    m_Impl->onMount();
}

void osc::RendererCoordinateSystemsTab::implOnUnmount()
{
    m_Impl->onUnmount();
}

bool osc::RendererCoordinateSystemsTab::implOnEvent(SDL_Event const& e)
{
    return m_Impl->onEvent(e);
}

void osc::RendererCoordinateSystemsTab::implOnTick()
{
    m_Impl->onTick();
}

void osc::RendererCoordinateSystemsTab::implOnDraw()
{
    m_Impl->onDraw();
}
