#include "ImGuizmoDemoTab.hpp"

#include <imgui.h>
#include <ImGuizmo.h>
#include <oscar/Bindings/ImGuiHelpers.hpp>
#include <oscar/Maths/Mat4.hpp>
#include <oscar/Maths/MathHelpers.hpp>
#include <oscar/Maths/PolarPerspectiveCamera.hpp>
#include <oscar/Maths/Rect.hpp>
#include <oscar/Maths/Vec2.hpp>
#include <oscar/UI/Tabs/StandardTabBase.hpp>
#include <oscar/Utils/CStringView.hpp>

#include <memory>

namespace
{
    constexpr osc::CStringView c_TabStringID = "Demos/ImGuizmo";
}

class osc::ImGuizmoDemoTab::Impl final : public osc::StandardTabBase {
public:
    Impl() : StandardTabBase{c_TabStringID}
    {
    }

private:
    void implOnDraw() final
    {
        // ImGuizmo::BeginFrame();  already done by MainUIScreen

        Mat4 view = m_SceneCamera.getViewMtx();
        Rect viewportRect = GetMainViewportWorkspaceScreenRect();
        Vec2 dims = Dimensions(viewportRect);
        Mat4 projection = m_SceneCamera.getProjMtx(AspectRatio(dims));

        ImGuizmo::SetRect(viewportRect.p1.x, viewportRect.p1.y, dims.x, dims.y);
        Mat4 identity{1.0f};
        ImGuizmo::DrawGrid(ValuePtr(view), ValuePtr(projection), ValuePtr(identity), 100.f);
        ImGuizmo::DrawCubes(ValuePtr(view), ValuePtr(projection), ValuePtr(m_ModelMatrix), 1);

        ImGui::Checkbox("translate", &m_IsInTranslateMode);

        ImGuizmo::Manipulate(
            ValuePtr(view),
            ValuePtr(projection),
            m_IsInTranslateMode ? ImGuizmo::TRANSLATE : ImGuizmo::ROTATE,
            ImGuizmo::LOCAL,
            ValuePtr(m_ModelMatrix),
            nullptr,
            nullptr, //&snap[0],   // snap
            nullptr, // bound sizing?
            nullptr  // bound sizing snap
        );
    }

    PolarPerspectiveCamera m_SceneCamera = []()
    {
        PolarPerspectiveCamera rv;
        rv.focusPoint = {0.0f, 0.0f, 0.0f};
        rv.phi = 1.0f;
        rv.theta = 0.0f;
        rv.radius = 5.0f;
        return rv;
    }();

    bool m_IsInTranslateMode = false;
    Mat4 m_ModelMatrix{1.0f};
};


// public API

osc::CStringView osc::ImGuizmoDemoTab::id() noexcept
{
    return "Demos/ImGuizmo";
}

osc::ImGuizmoDemoTab::ImGuizmoDemoTab(ParentPtr<TabHost> const&) :
    m_Impl{std::make_unique<Impl>()}
{
}

osc::ImGuizmoDemoTab::ImGuizmoDemoTab(ImGuizmoDemoTab&&) noexcept = default;
osc::ImGuizmoDemoTab& osc::ImGuizmoDemoTab::operator=(ImGuizmoDemoTab&&) noexcept = default;
osc::ImGuizmoDemoTab::~ImGuizmoDemoTab() noexcept = default;

osc::UID osc::ImGuizmoDemoTab::implGetID() const
{
    return m_Impl->getID();
}

osc::CStringView osc::ImGuizmoDemoTab::implGetName() const
{
    return m_Impl->getName();
}

void osc::ImGuizmoDemoTab::implOnDraw()
{
    m_Impl->onDraw();
}
