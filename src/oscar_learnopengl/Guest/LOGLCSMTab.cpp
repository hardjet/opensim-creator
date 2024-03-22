#include "LOGLCSMTab.h"

#include <oscar/oscar.h>

#include <SDL_events.h>

#include <array>
#include <cstddef>
#include <memory>
#include <random>
#include <vector>

using namespace osc;
using namespace osc::literals;

namespace
{
    constexpr CStringView c_TabStringID = "LearnOpenGL/CSM";

    struct TransformedMesh {
        Mesh mesh;
        Transform transform;
    };

    std::vector<TransformedMesh> generateDecorations()
    {
        auto const geoms = std::to_array<Mesh>({
            SphereGeometry{},
            TorusKnotGeometry{},
            IcosahedronGeometry{},
            BoxGeometry{},
        });

        auto rng = std::default_random_engine{std::random_device{}()};
        auto dist = std::normal_distribution{0.1f, 0.2f};
        AABB const bounds = {{-5.0f,  0.0f, -5.0f}, {5.0f, 0.0f, 5.0f}};
        Vec3 const dims = dimensions(bounds);
        Vec2uz const cells = {10, 10};

        std::vector<TransformedMesh> rv;
        rv.reserve(cells.x * cells.y);

        for (size_t x = 0; x < cells.x; ++x) {
            for (size_t y = 0; y < cells.y; ++y) {

                Vec3 const pos = bounds.min + dims * (Vec3{x, 0.0f, y} / Vec3{cells.x - 1_uz, 1, cells.y - 1_uz});
                Mesh mesh;
                sample(geoms, &mesh, 1, rng);

                rv.push_back(TransformedMesh{
                    .mesh = mesh,
                    .transform = {
                        .scale = Vec3{abs(dist(rng))},
                        .position = pos,
                    }
                });
            }
        }

        return rv;
    }
}

class osc::LOGLCSMTab::Impl final : public StandardTabImpl {
public:
    Impl() : StandardTabImpl{c_TabStringID}
    {
        m_UserCamera.setNearClippingPlane(0.1f);
        m_UserCamera.setFarClippingPlane(100.0f);
        m_Material.setLightPosition(Vec3{5.0f});
        m_Material.setDiffuseColor(Color::orange());
        m_Decorations.push_back(TransformedMesh{
            .mesh = PlaneGeometry{},
            .transform = {
                .scale = Vec3{10.0f, 10.0f, 1.0f},
                .rotation = angle_axis(-90_deg, CoordinateDirection::x()),
                .position = {0.0f, -1.0f, 0.0f},
            },
        });
    }

private:
    void implOnMount() final
    {
        App::upd().makeMainEventLoopPolling();
        m_UserCamera.onMount();
    }

    void implOnUnmount() final
    {
        m_UserCamera.onUnmount();
        App::upd().makeMainEventLoopWaiting();
    }

    bool implOnEvent(SDL_Event const& e) final
    {
        return m_UserCamera.onEvent(e);
    }

    void implOnDraw() final
    {
        m_UserCamera.onDraw();  // update from inputs etc.
        m_Material.setViewerPosition(m_UserCamera.getPosition());

        for (auto const& decoration : m_Decorations) {
            Graphics::DrawMesh(decoration.mesh, decoration.transform, m_Material, m_UserCamera);
        }

        m_UserCamera.setPixelRect(ui::GetMainViewportWorkspaceScreenRect());
        m_UserCamera.renderToScreen();
    }

    MouseCapturingCamera m_UserCamera;
    std::vector<TransformedMesh> m_Decorations = generateDecorations();
    MeshPhongMaterial m_Material;
};


// public API

osc::CStringView osc::LOGLCSMTab::id()
{
    return c_TabStringID;
}

osc::LOGLCSMTab::LOGLCSMTab(ParentPtr<ITabHost> const&) :
    m_Impl{std::make_unique<Impl>()}
{
}

osc::LOGLCSMTab::LOGLCSMTab(LOGLCSMTab&&) noexcept = default;
osc::LOGLCSMTab& osc::LOGLCSMTab::operator=(LOGLCSMTab&&) noexcept = default;
osc::LOGLCSMTab::~LOGLCSMTab() noexcept = default;

UID osc::LOGLCSMTab::implGetID() const
{
    return m_Impl->getID();
}

CStringView osc::LOGLCSMTab::implGetName() const
{
    return m_Impl->getName();
}

void osc::LOGLCSMTab::implOnMount()
{
    m_Impl->onMount();
}

void osc::LOGLCSMTab::implOnUnmount()
{
    m_Impl->onUnmount();
}

bool osc::LOGLCSMTab::implOnEvent(SDL_Event const& e)
{
    return m_Impl->onEvent(e);
}

void osc::LOGLCSMTab::implOnDraw()
{
    m_Impl->onDraw();
}
