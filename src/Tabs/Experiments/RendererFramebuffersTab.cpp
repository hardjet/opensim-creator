#include "RendererFramebuffersTab.hpp"

#include "src/Bindings/ImGuiHelpers.hpp"
#include "src/Graphics/Camera.hpp"
#include "src/Graphics/Graphics.hpp"
#include "src/Graphics/GraphicsHelpers.hpp"
#include "src/Graphics/Material.hpp"
#include "src/Graphics/MaterialPropertyBlock.hpp"
#include "src/Graphics/MeshGen.hpp"
#include "src/Maths/MathHelpers.hpp"
#include "src/Maths/Transform.hpp"
#include "src/Panels/LogViewerPanel.hpp"
#include "src/Panels/PerfPanel.hpp"
#include "src/Platform/App.hpp"
#include "src/Utils/Assertions.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <SDL_events.h>

#include <cstdint>
#include <memory>

static glm::vec3 constexpr c_PlaneVertices[] =
{
    { 5.0f, -0.5f,  5.0f},
    {-5.0f, -0.5f,  5.0f},
    {-5.0f, -0.5f, -5.0f},

    { 5.0f, -0.5f,  5.0f},
    {-5.0f, -0.5f, -5.0f},
    { 5.0f, -0.5f, -5.0f},
};

static glm::vec2 constexpr c_PlaneTexCoords[] =
{
    {2.0f, 0.0f},
    {0.0f, 0.0f},
    {0.0f, 2.0f},

    {2.0f, 0.0f},
    {0.0f, 2.0f},
    {2.0f, 2.0f},
};

static uint16_t constexpr  c_PlaneIndices[] = {0, 2, 1, 3, 5, 4};

namespace
{
    osc::Mesh GeneratePlane()
    {
        osc::Mesh rv;
        rv.setVerts(c_PlaneVertices);
        rv.setTexCoords(c_PlaneTexCoords);
        rv.setIndices(c_PlaneIndices);
        return rv;
    }
}

class osc::RendererFramebuffersTab::Impl final {
public:

    Impl()
    {
        m_SceneCamera.setPosition({0.0f, 0.0f, 3.0f});
        m_SceneCamera.setCameraFOV(glm::radians(45.0f));
        m_SceneCamera.setNearClippingPlane(0.1f);
        m_SceneCamera.setFarClippingPlane(100.0f);
        m_ScreenCamera.setViewMatrixOverride(glm::mat4{1.0f});
        m_ScreenCamera.setProjectionMatrixOverride(glm::mat4{1.0f});
    }

    UID getID() const
    {
        return m_TabID;
    }

    CStringView getName() const
    {
        return "Frame Buffers (LearnOpenGL)";
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

    void onDraw()
    {
        // handle mouse capturing
        if (m_IsMouseCaptured)
        {
            UpdateEulerCameraFromImGuiUserInput(m_SceneCamera, m_CameraEulers);
            ImGui::SetMouseCursor(ImGuiMouseCursor_None);
            App::upd().setShowCursor(false);
        }
        else
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
            App::upd().setShowCursor(true);
        }

        // setup render texture
        osc::Rect viewportRect = osc::GetMainViewportWorkspaceScreenRect();
        glm::vec2 viewportRectDims = osc::Dimensions(viewportRect);
        m_RenderTexture.setDimensions(viewportRectDims);
        m_RenderTexture.setAntialiasingLevel(osc::App::get().getMSXAASamplesRecommended());

        // render scene
        {
            // cubes
            m_SceneRenderMaterial.setTexture("uTexture1", m_ContainerTexture);
            Transform t;
            t.position = { -1.0f, 0.0f, -1.0f };
            osc::Graphics::DrawMesh(m_CubeMesh, t, m_SceneRenderMaterial, m_SceneCamera);
            t.position = { 1.0f, 0.0f, -1.0f };
            osc::Graphics::DrawMesh(m_CubeMesh, t, m_SceneRenderMaterial, m_SceneCamera);

            // floor
            m_SceneRenderMaterial.setTexture("uTexture1", m_MetalTexture);
            osc::Graphics::DrawMesh(m_PlaneMesh, Transform{}, m_SceneRenderMaterial, m_SceneCamera);
        }
        m_SceneCamera.renderTo(m_RenderTexture);

        // render via a effect sampler
        Graphics::BlitToScreen(m_RenderTexture, viewportRect, m_ScreenMaterial);

        // auxiliary UI
        m_LogViewer.draw();
        m_PerfPanel.draw();
    }

private:
    UID m_TabID;

    Material m_SceneRenderMaterial
    {
        Shader
        {
            App::slurp("shaders/ExperimentFrameBuffers.vert"),
            App::slurp("shaders/ExperimentFrameBuffers.frag"),
        }
    };

    Camera m_SceneCamera;
    bool m_IsMouseCaptured = false;
    glm::vec3 m_CameraEulers = { 0.0f, 0.0f, 0.0f };

    Texture2D m_ContainerTexture = LoadTexture2DFromImage(App::resource("textures/container.jpg"));
    Texture2D m_MetalTexture = LoadTexture2DFromImage(App::resource("textures/metal.png"));

    Mesh m_CubeMesh = GenLearnOpenGLCube();
    Mesh m_PlaneMesh = GeneratePlane();
    Mesh m_QuadMesh = GenTexturedQuad();

    RenderTexture m_RenderTexture;
    Camera m_ScreenCamera;
    Material m_ScreenMaterial
    {
        Shader
        {
            App::slurp("shaders/ExperimentFrameBuffersScreen.vert"),
            App::slurp("shaders/ExperimentFrameBuffersScreen.frag"),
        }
    };

    LogViewerPanel m_LogViewer{"log"};
    PerfPanel m_PerfPanel{"perf"};
};


// public API (PIMPL)

osc::CStringView osc::RendererFramebuffersTab::id() noexcept
{
    return "Renderer/Framebuffers";
}

osc::RendererFramebuffersTab::RendererFramebuffersTab(TabHost*) :
    m_Impl{std::make_unique<Impl>()}
{
}

osc::RendererFramebuffersTab::RendererFramebuffersTab(RendererFramebuffersTab&&) noexcept = default;
osc::RendererFramebuffersTab& osc::RendererFramebuffersTab::operator=(RendererFramebuffersTab&&) noexcept = default;
osc::RendererFramebuffersTab::~RendererFramebuffersTab() noexcept = default;

osc::UID osc::RendererFramebuffersTab::implGetID() const
{
    return m_Impl->getID();
}

osc::CStringView osc::RendererFramebuffersTab::implGetName() const
{
    return m_Impl->getName();
}

void osc::RendererFramebuffersTab::implOnMount()
{
    m_Impl->onMount();
}

void osc::RendererFramebuffersTab::implOnUnmount()
{
    m_Impl->onUnmount();
}

bool osc::RendererFramebuffersTab::implOnEvent(SDL_Event const& e)
{
    return m_Impl->onEvent(e);
}

void osc::RendererFramebuffersTab::implOnDraw()
{
    m_Impl->onDraw();
}
