#include "CoordinateEditorPanel.hpp"

#include "src/Bindings/ImGuiHelpers.hpp"
#include "src/OpenSimBindings/MiddlewareAPIs/EditorAPI.hpp"
#include "src/OpenSimBindings/Widgets/ComponentContextMenu.hpp"
#include "src/OpenSimBindings/ActionFunctions.hpp"
#include "src/OpenSimBindings/OpenSimHelpers.hpp"
#include "src/OpenSimBindings/UndoableModelStatePair.hpp"
#include "src/Panels/StandardPanel.hpp"
#include "src/Platform/Styling.hpp"
#include "src/Utils/Algorithms.hpp"

#include <OpenSim/Common/Component.h>
#include <OpenSim/Simulation/SimbodyEngine/Coordinate.h>
#include <imgui.h>
#include <IconsFontAwesome5.h>

#include <string>
#include <string_view>
#include <sstream>
#include <utility>
#include <vector>

class osc::CoordinateEditorPanel::Impl final : public osc::StandardPanel {
public:

    Impl(
        std::string_view panelName,
        MainUIStateAPI* mainUIStateAPI,
        EditorAPI* editorAPI,
        std::shared_ptr<UndoableModelStatePair> uum) :

        StandardPanel{std::move(panelName)},
        m_MainUIStateAPI{std::move(mainUIStateAPI)},
        m_EditorAPI{std::move(editorAPI)},
        m_Uum{std::move(uum)}
    {
    }

private:

    void implDrawContent() final
    {
        // load coords
        std::vector<OpenSim::Coordinate const*> coordPtrs = GetCoordinatesInModel(m_Uum->getModel());

        // if there's no coordinates in the model, show a warning message and stop drawing
        if (coordPtrs.empty())
        {
            char const* const msg = "(there are no coordinates in the model)";
            float const w = ImGui::CalcTextSize(msg).x;
            ImGui::SetCursorPosX(0.5f * (ImGui::GetContentRegionAvail().x - w));  // center align
            ImGui::TextDisabled(msg);
            return;
        }

        // else: there's coordinates, which should be shown in a table
        ImGuiTableFlags flags =
            ImGuiTableFlags_NoSavedSettings |
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_Sortable |
            ImGuiTableFlags_SortTristate |
            ImGuiTableFlags_BordersInner;
        if (ImGui::BeginTable("##coordinatestable", 3, flags))
        {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_NoSort, 1.65f);
            ImGui::TableSetupColumn("Speed", ImGuiTableColumnFlags_NoSort, 0.5f);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsDirty)
            {
                // HACK: we know the user can only sort one column (name) so we don't need to permute
                //       through the entire specs structure
                if (specs->SpecsCount == 1 && specs->Specs[0].ColumnIndex == 0 && specs->Specs[0].SortOrder == 0)
                {
                    ImGuiTableColumnSortSpecs const& spec = specs->Specs[0];
                    switch (spec.SortDirection)
                    {
                    case ImGuiSortDirection_Ascending:
                        Sort(coordPtrs, osc::IsNameLexographicallyLowerThan<OpenSim::Component const*>);
                        break;
                    case ImGuiSortDirection_Descending:
                        Sort(coordPtrs, osc::IsNameLexographicallyGreaterThan<OpenSim::Component const*>);
                        break;
                    case ImGuiSortDirection_None:
                    default:
                        break;  // just use them as-is
                    }
                }
            }

            int id = 0;
            for (OpenSim::Coordinate const* coordPtr : coordPtrs)
            {
                ImGui::PushID(id++);
                drawRow(*coordPtr);
                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }

    void drawRow(OpenSim::Coordinate const& c)
    {
        ImGui::TableNextRow();

        int column = 0;
        ImGui::TableSetColumnIndex(column++);
        drawNameCell(c);
        ImGui::TableSetColumnIndex(column++);
        drawDataCell(c);
        ImGui::TableSetColumnIndex(column++);
        drawSpeedCell(c);
    }

    void drawNameCell(OpenSim::Coordinate const& c)
    {
        int stylesPushed = 0;
        if (&c == m_Uum->getHovered())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, OSC_HOVERED_COMPONENT_RGBA);
            ++stylesPushed;
        }
        if (&c == m_Uum->getSelected())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, OSC_SELECTED_COMPONENT_RGBA);
            ++stylesPushed;
        }

        ImGui::TextUnformatted(c.getName().c_str());
        ImGui::PopStyleColor(std::exchange(stylesPushed, 0));

        if (ImGui::IsItemHovered())
        {
            m_Uum->setHovered(&c);

            std::stringstream ss;
            ss << "    motion type = " << osc::GetMotionTypeDisplayName(c) << '\n';
            ss << "    owner = " << (c.hasOwner() ? c.getOwner().getName().c_str() : "(no owner)");

            osc::DrawTooltip(c.getName(), ss.str());
        }

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            m_Uum->setSelected(&c);
        }
        else if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            auto popup = std::make_unique<ComponentContextMenu>("##componentcontextmenu", m_MainUIStateAPI, m_EditorAPI, m_Uum, c.getAbsolutePath());
            popup->open();
            m_EditorAPI->pushPopup(std::move(popup));
        }
    }

    void drawDataCell(OpenSim::Coordinate const& c)
    {
        int stylesPushed = 0;

        if (c.getLocked(m_Uum->getState()))
        {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, {0.6f, 0.0f, 0.0f, 1.0f});
            ++stylesPushed;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0.0f, ImGui::GetStyle().FramePadding.y});
        if (ImGui::Button(c.getLocked(m_Uum->getState()) ? ICON_FA_LOCK : ICON_FA_UNLOCK))
        {
            bool newValue = !c.getLocked(m_Uum->getState());
            ActionSetCoordinateLockedAndSave(*m_Uum, c, newValue);
        }
        ImGui::PopStyleVar();
        osc::DrawTooltipIfItemHovered("Toggle Coordinate Lock", "Lock/unlock the coordinate's value.\n\nLocking a coordinate indicates whether the coordinate's value should be constrained to this value during the simulation.");

        ImGui::SameLine(0.0f, 1.0f);

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

        float minValue = ConvertCoordValueToDisplayValue(c, c.getRangeMin());
        float maxValue = ConvertCoordValueToDisplayValue(c, c.getRangeMax());
        float displayedValue = ConvertCoordValueToDisplayValue(c, c.getValue(m_Uum->getState()));
        if (ImGui::SliderFloat("##coordinatevalueeditor", &displayedValue, minValue, maxValue))
        {
            double storedValue = ConvertCoordDisplayValueToStorageValue(c, displayedValue);
            ActionSetCoordinateValue(*m_Uum, c, storedValue);
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            double storedValue = ConvertCoordDisplayValueToStorageValue(c, displayedValue);
            ActionSetCoordinateValueAndSave(*m_Uum, c, storedValue);
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("Ctrl-click the slider to edit");
            ImGui::EndTooltip();
        }

        ImGui::PopStyleColor(stylesPushed);
    }

    void drawSpeedCell(OpenSim::Coordinate const& c)
    {
        float displayedSpeed = ConvertCoordValueToDisplayValue(c, c.getSpeedValue(m_Uum->getState()));

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (InputMetersFloat("##coordinatespeededitor", displayedSpeed))
        {
            double storedSpeed = ConvertCoordDisplayValueToStorageValue(c, displayedSpeed);
            osc::ActionSetCoordinateSpeed(*m_Uum, c, storedSpeed);
        }

        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            double storedSpeed = ConvertCoordDisplayValueToStorageValue(c, displayedSpeed);
            osc::ActionSetCoordinateSpeedAndSave(*m_Uum, c, storedSpeed);
        }
    }

    MainUIStateAPI* m_MainUIStateAPI;
    EditorAPI* m_EditorAPI;
    std::shared_ptr<UndoableModelStatePair> m_Uum;
};


// public API

osc::CoordinateEditorPanel::CoordinateEditorPanel(
    std::string_view panelName,
    MainUIStateAPI* mainUIStateAPI,
    EditorAPI* editorAPI,
    std::shared_ptr<UndoableModelStatePair> uum) :

    m_Impl{std::make_unique<Impl>(std::move(panelName), std::move(mainUIStateAPI), std::move(editorAPI), std::move(uum))}
{
}

osc::CoordinateEditorPanel::CoordinateEditorPanel(CoordinateEditorPanel&&) noexcept = default;
osc::CoordinateEditorPanel& osc::CoordinateEditorPanel::operator=(CoordinateEditorPanel&&) noexcept = default;
osc::CoordinateEditorPanel::~CoordinateEditorPanel() noexcept = default;

osc::CStringView osc::CoordinateEditorPanel::implGetName() const
{
    return m_Impl->getName();
}

bool osc::CoordinateEditorPanel::implIsOpen() const
{
    return m_Impl->isOpen();
}

void osc::CoordinateEditorPanel::implOpen()
{
    m_Impl->open();
}

void osc::CoordinateEditorPanel::implClose()
{
    m_Impl->close();
}

void osc::CoordinateEditorPanel::implDraw()
{
    m_Impl->draw();
}