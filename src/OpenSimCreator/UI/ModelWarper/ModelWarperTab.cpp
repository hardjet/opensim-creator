#include "ModelWarperTab.h"

#include <OpenSimCreator/UI/ModelWarper/ChecklistPanel.h>
#include <OpenSimCreator/UI/ModelWarper/MainMenu.h>
#include <OpenSimCreator/UI/ModelWarper/ResultModelViewerPanel.h>
#include <OpenSimCreator/UI/ModelWarper/SourceModelViewerPanel.h>
#include <OpenSimCreator/UI/ModelWarper/Toolbar.h>
#include <OpenSimCreator/UI/ModelWarper/UIState.h>

#include <SDL_events.h>
#include <oscar/Platform/App.h>
#include <oscar/UI/Panels/LogViewerPanel.h>
#include <oscar/UI/Panels/PanelManager.h>
#include <oscar/UI/Tabs/StandardTabImpl.h>
#include <oscar/Utils/CStringView.h>

#include <functional>
#include <string_view>

using namespace osc;

namespace
{
    constexpr CStringView c_TabStringID = "OpenSim/ModelWarper";
}

class osc::mow::ModelWarperTab::Impl final : public StandardTabImpl {
public:
    explicit Impl(ParentPtr<ITabHost> const& tabHost) :
        StandardTabImpl{c_TabStringID},
        m_TabHost{tabHost}
    {
        m_PanelManager->register_toggleable_panel(
            "Checklist",
            [state = m_State](std::string_view panelName)
            {
                return std::make_shared<ChecklistPanel>(panelName, state);
            }
        );

        m_PanelManager->register_toggleable_panel(
            "Source Model",
            [state = m_State](std::string_view panelName)
            {
                return std::make_shared<SourceModelViewerPanel>(panelName, state);
            }
        );

        m_PanelManager->register_toggleable_panel(
            "Result Model",
            [state = m_State](std::string_view panelName)
            {
                return std::make_shared<ResultModelViewerPanel>(panelName, state);
            }
        );

        m_PanelManager->register_toggleable_panel(
            "Log",
            [](std::string_view panelName)
            {
                return std::make_shared<LogViewerPanel>(panelName);
            }
        );
    }

private:
    void implOnMount() final
    {
        App::upd().make_main_loop_waiting();
        m_PanelManager->on_mount();
    }

    void implOnUnmount() final
    {
        m_PanelManager->on_unmount();
        App::upd().make_main_loop_waiting();
    }

    bool implOnEvent(SDL_Event const&) final
    {
        return false;
    }

    void implOnTick() final
    {
        m_PanelManager->on_tick();
    }

    void implOnDrawMainMenu() final
    {
        m_MainMenu.onDraw();
    }

    void implOnDraw() final
    {
        ui::DockSpaceOverViewport(ui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
        m_Toolbar.onDraw();
        m_PanelManager->on_draw();
    }

    ParentPtr<ITabHost> m_TabHost;
    std::shared_ptr<UIState> m_State = std::make_shared<UIState>(m_TabHost);
    std::shared_ptr<PanelManager> m_PanelManager = std::make_shared<PanelManager>();
    MainMenu m_MainMenu{m_State, m_PanelManager};
    Toolbar m_Toolbar{"##ModelWarperToolbar", m_State};
};


// public API

CStringView osc::mow::ModelWarperTab::id()
{
    return c_TabStringID;
}

osc::mow::ModelWarperTab::ModelWarperTab(ParentPtr<ITabHost> const& tabHost) :
    m_Impl{std::make_unique<Impl>(tabHost)}
{}

osc::mow::ModelWarperTab::ModelWarperTab(ModelWarperTab&&) noexcept = default;
osc::mow::ModelWarperTab& osc::mow::ModelWarperTab::operator=(ModelWarperTab&&) noexcept = default;
osc::mow::ModelWarperTab::~ModelWarperTab() noexcept = default;

UID osc::mow::ModelWarperTab::implGetID() const
{
    return m_Impl->getID();
}

CStringView osc::mow::ModelWarperTab::implGetName() const
{
    return m_Impl->getName();
}

void osc::mow::ModelWarperTab::implOnMount()
{
    m_Impl->on_mount();
}

void osc::mow::ModelWarperTab::implOnUnmount()
{
    m_Impl->on_unmount();
}

bool osc::mow::ModelWarperTab::implOnEvent(SDL_Event const& e)
{
    return m_Impl->onEvent(e);
}

void osc::mow::ModelWarperTab::implOnTick()
{
    m_Impl->on_tick();
}

void osc::mow::ModelWarperTab::implOnDrawMainMenu()
{
    m_Impl->onDrawMainMenu();
}

void osc::mow::ModelWarperTab::implOnDraw()
{
    m_Impl->onDraw();
}
