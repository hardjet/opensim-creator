#include "CustomWidgetsTab.hpp"

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <oscar/Bindings/ImGuiHelpers.hpp>
#include <oscar/Graphics/Color.hpp>
#include <oscar/UI/Tabs/StandardTabBase.hpp>
#include <oscar/Utils/CStringView.hpp>
#include <SDL_events.h>

#include <array>
#include <cmath>
#include <string>
#include <utility>

namespace
{
    constexpr osc::CStringView c_TabStringID = "Experiments/CustomWidgets";

    void WidgetTitle(osc::CStringView title, glm::vec2 pos)
    {
        glm::vec2 const textTopLeft = pos + glm::vec2{ImGui::GetStyle().FramePadding};
        ImGui::GetWindowDrawList()->AddText(textTopLeft, ImGui::GetColorU32(ImGuiCol_Text), title.c_str());
    }
}

// toggle
namespace
{
    void DrawToggle(bool enabled, bool hovered, ImVec2 pos, ImVec2 size)
    {
        ImDrawList& draw_list = *ImGui::GetWindowDrawList();

        float const radius = size.y * 0.5f;
        float const rounding = size.y * 0.25f;
        float const slot_half_height = size.y * 0.5f;
        bool const circular_grab = false;

        ImU32 const bgColor = hovered ?
            ImGui::GetColorU32(enabled ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBgHovered) :
            ImGui::GetColorU32(enabled ? ImGuiCol_CheckMark : ImGuiCol_FrameBg);

        glm::vec2 const pmid
        {
            pos.x + radius + (enabled ? 1.0f : 0.0f) * (size.x - radius * 2),
            pos.y + size.y / 2.0f,
        };
        ImVec2 const smin = {pos.x, pmid.y - slot_half_height};
        ImVec2 const smax = {pos.x + size.x, pmid.y + slot_half_height};

        draw_list.AddRectFilled(smin, smax, bgColor, rounding);

        if (circular_grab)
        {
            draw_list.AddCircleFilled(pmid, radius * 0.8f, ImGui::GetColorU32(ImGuiCol_SliderGrab));
        }
        else
        {
            glm::vec2 const offs = {radius*0.8f, radius*0.8f};
            draw_list.AddRectFilled(pmid - offs, pmid + offs, ImGui::GetColorU32(ImGuiCol_SliderGrab), rounding);
        }
    }

    bool Toggle(osc::CStringView label, bool* v)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32_BLACK_TRANS);

        ImGuiStyle const& style = ImGui::GetStyle();

        float const titleHeight = ImGui::GetTextLineHeight();

        ImVec2 const p = ImGui::GetCursorScreenPos();
        ImVec2 const bb(ImGui::GetColumnWidth(), ImGui::GetFrameHeight());
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));
        ImGui::PushID(label.c_str());
        bool const status = ImGui::Button("###toggle_button", bb);

        if (status)
        {
            *v = !*v;
        }

        ImGui::PopID();
        ImGui::PopStyleVar();
        ImVec2 const pMin = ImGui::GetItemRectMin();
        ImVec2 const pMax = ImGui::GetItemRectMax();

        WidgetTitle(label, p);

        float const toggleHeight = titleHeight * 0.9f;
        ImVec2 const toggleSize = {toggleHeight * 1.75f, toggleHeight};
        ImVec2 const togglePos
        {
            pMax.x - toggleSize.x - style.FramePadding.x,
            pMin.y + (titleHeight - toggleSize.y)/2.0f + style.FramePadding.y,
        };
        DrawToggle(*v, ImGui::IsItemHovered(), togglePos, toggleSize);

        ImGui::PopStyleColor();

        return status;
    }
}

class osc::CustomWidgetsTab::Impl final : public osc::StandardTabBase {
public:
    Impl() : StandardTabBase{c_TabStringID}
    {
    }

private:
    void implOnDraw() final
    {
        ImGui::Begin("window");
        ImGui::InputFloat("standardinput", &m_Value);
        CircularSliderFloat("custom slider", &m_Value, 15.0f, 5.0f);
        ImGui::Text("%f", m_Value);
        Toggle("custom toggle", &m_Toggle);
        ImGui::End();
    }

    float m_Value = 10.0f;
    bool m_Toggle = false;
};


// public API

osc::CStringView osc::CustomWidgetsTab::id() noexcept
{
    return c_TabStringID;
}

osc::CustomWidgetsTab::CustomWidgetsTab(ParentPtr<TabHost> const&) :
    m_Impl{std::make_unique<Impl>()}
{
}

osc::CustomWidgetsTab::CustomWidgetsTab(CustomWidgetsTab&&) noexcept = default;
osc::CustomWidgetsTab& osc::CustomWidgetsTab::operator=(CustomWidgetsTab&&) noexcept = default;
osc::CustomWidgetsTab::~CustomWidgetsTab() noexcept = default;

osc::UID osc::CustomWidgetsTab::implGetID() const
{
    return m_Impl->getID();
}

osc::CStringView osc::CustomWidgetsTab::implGetName() const
{
    return m_Impl->getName();
}

void osc::CustomWidgetsTab::implOnDraw()
{
    m_Impl->onDraw();
}