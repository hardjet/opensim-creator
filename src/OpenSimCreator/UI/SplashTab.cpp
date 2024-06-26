#include "SplashTab.h"

#include <OpenSimCreator/Documents/Model/UndoableModelActions.h>
#include <OpenSimCreator/Platform/RecentFile.h>
#include <OpenSimCreator/Platform/RecentFiles.h>
#include <OpenSimCreator/UI/IMainUIStateAPI.h>
#include <OpenSimCreator/UI/LoadingTab.h>
#include <OpenSimCreator/UI/FrameDefinition/FrameDefinitionTab.h>
#include <OpenSimCreator/UI/MeshImporter/MeshImporterTab.h>
#include <OpenSimCreator/UI/MeshWarper/MeshWarpingTab.h>
#include <OpenSimCreator/UI/Shared/MainMenu.h>

#include <IconsFontAwesome5.h>
#include <oscar/Formats/SVG.h>
#include <oscar/Graphics/Color.h>
#include <oscar/Graphics/Scene/SceneCache.h>
#include <oscar/Graphics/Scene/SceneRenderer.h>
#include <oscar/Graphics/Scene/SceneRendererParams.h>
#include <oscar/Graphics/Texture2D.h>
#include <oscar/Graphics/TextureFilterMode.h>
#include <oscar/Maths/MathHelpers.h>
#include <oscar/Maths/PolarPerspectiveCamera.h>
#include <oscar/Maths/Rect.h>
#include <oscar/Maths/Vec2.h>
#include <oscar/Platform/App.h>
#include <oscar/Platform/AppConfig.h>
#include <oscar/Platform/AppMetadata.h>
#include <oscar/Platform/os.h>
#include <oscar/UI/ImGuiHelpers.h>
#include <oscar/UI/oscimgui.h>
#include <oscar/UI/Tabs/ITabHost.h>
#include <oscar/UI/Widgets/LogViewer.h>
#include <oscar/Utils/Algorithms.h>
#include <oscar/Utils/CStringView.h>
#include <oscar/Utils/ParentPtr.h>
#include <SDL_events.h>

#include <filesystem>
#include <span>
#include <string>
#include <utility>

using namespace osc::literals;
using namespace osc;

namespace
{
    PolarPerspectiveCamera GetSplashScreenDefaultPolarCamera()
    {
        PolarPerspectiveCamera rv;
        rv.phi = 30_deg;
        rv.radius = 10.0f;
        rv.theta = 45_deg;
        return rv;
    }

    SceneRendererParams GetSplashScreenDefaultRenderParams(PolarPerspectiveCamera const& camera)
    {
        SceneRendererParams rv;
        rv.draw_rims = false;
        rv.view_matrix = camera.view_matrix();
        rv.near_clipping_plane = camera.znear;
        rv.far_clipping_plane = camera.zfar;
        rv.view_pos = camera.position();
        rv.light_direction = {-0.34f, -0.25f, 0.05f};
        rv.light_color = {248.0f / 255.0f, 247.0f / 255.0f, 247.0f / 255.0f, 1.0f};
        rv.background_color = {0.89f, 0.89f, 0.89f, 1.0f};
        return rv;
    }

    // helper: draws an ui::MenuItem for a given recent- or example-file-path
    void DrawRecentOrExampleFileMenuItem(
        std::filesystem::path const& path,
        ParentPtr<IMainUIStateAPI>& parent_,
        int& imguiID)
    {
        std::string const label = std::string{ICON_FA_FILE} + " " + path.filename().string();

        ui::PushID(++imguiID);
        if (ui::MenuItem(label))
        {
            parent_->addAndSelectTab<LoadingTab>(parent_, path);
        }
        // show the full path as a tooltip when the item is hovered (some people have
        // long file names (#784)
        if (ui::IsItemHovered())
        {
            ui::BeginTooltipNoWrap();
            ui::TextUnformatted(path.filename().string());
            ui::EndTooltipNoWrap();
        }
        ui::PopID();
    }
}

class osc::SplashTab::Impl final {
public:

    explicit Impl(ParentPtr<IMainUIStateAPI> const& parent_) :
        m_Parent{parent_}
    {
        m_MainAppLogo.set_filter_mode(TextureFilterMode::Linear);
        m_CziLogo.set_filter_mode(TextureFilterMode::Linear);
        m_TudLogo.set_filter_mode(TextureFilterMode::Linear);
    }

    UID getID() const
    {
        return m_TabID;
    }

    CStringView getName() const
    {
        return ICON_FA_HOME;
    }

    void on_mount()
    {
        // edge-case: reset the file tab whenever the splash screen is (re)mounted,
        // because actions within other tabs may have updated things like recently
        // used files etc. (#618)
        m_MainMenuFileTab = MainMenuFileTab{};

        App::upd().make_main_loop_waiting();
    }

    void on_unmount()
    {
        App::upd().make_main_loop_polling();
    }

    bool onEvent(SDL_Event const& e)
    {
        if (e.type == SDL_DROPFILE && e.drop.file != nullptr && std::string_view{e.drop.file}.ends_with(".osim"))
        {
            // if the user drops an osim file on this tab then it should be loaded
            m_Parent->addAndSelectTab<LoadingTab>(m_Parent, e.drop.file);
            return true;
        }
        return false;
    }

    void drawMainMenu()
    {
        m_MainMenuFileTab.onDraw(m_Parent);
        m_MainMenuAboutTab.onDraw();
    }

    void onDraw()
    {
        if (area_of(ui::GetMainViewportWorkspaceScreenRect()) <= 0.0f)
        {
            // edge-case: splash screen is the first rendered frame and ImGui
            //            is being unusual about it
            return;
        }

        drawBackground();
        drawLogo();
        drawAttributationLogos();
        drawVersionInfo();
        drawMenu();
    }

private:
    Rect calcMainMenuRect() const
    {
        Rect tabRect = ui::GetMainViewportWorkspaceScreenRect();
        // pretend the attributation bar isn't there (avoid it)
        tabRect.p2.y -= static_cast<float>(max(m_TudLogo.dimensions().y, m_CziLogo.dimensions().y)) - 2.0f*ui::GetStyleWindowPadding().y;

        Vec2 const menuAndTopLogoDims = elementwise_min(dimensions_of(tabRect), Vec2{m_SplashMenuMaxDims.x, m_SplashMenuMaxDims.y + m_MainAppLogoDims.y + m_TopLogoPadding.y});
        Vec2 const menuAndTopLogoTopLeft = tabRect.p1 + 0.5f*(dimensions_of(tabRect) - menuAndTopLogoDims);
        Vec2 const menuDims = {menuAndTopLogoDims.x, menuAndTopLogoDims.y - m_MainAppLogoDims.y - m_TopLogoPadding.y};
        Vec2 const menuTopLeft = Vec2{menuAndTopLogoTopLeft.x, menuAndTopLogoTopLeft.y + m_MainAppLogoDims.y + m_TopLogoPadding.y};

        return Rect{menuTopLeft, menuTopLeft + menuDims};
    }

    Rect calcLogoRect() const
    {
        Rect const mmr = calcMainMenuRect();
        Vec2 const topLeft
        {
            mmr.p1.x + dimensions_of(mmr).x/2.0f - m_MainAppLogoDims.x/2.0f,
            mmr.p1.y - m_TopLogoPadding.y - m_MainAppLogoDims.y,
        };

        return Rect{topLeft, topLeft + m_MainAppLogoDims};
    }

    void drawBackground()
    {
        Rect const screenRect = ui::GetMainViewportWorkspaceScreenRect();

        ui::SetNextWindowPos(screenRect.p1);
        ui::SetNextWindowSize(dimensions_of(screenRect));

        ui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
        ui::Begin("##splashscreenbackground", nullptr, ui::GetMinimalWindowFlags());
        ui::PopStyleVar();

        SceneRendererParams params{m_LastSceneRendererParams};
        params.dimensions = dimensions_of(screenRect);
        params.antialiasing_level = App::get().anti_aliasing_level();
        params.projection_matrix = m_Camera.projection_matrix(aspect_ratio(screenRect));

        if (params != m_LastSceneRendererParams)
        {
            scene_renderer_.render({}, params);
            m_LastSceneRendererParams = params;
        }

        ui::Image(scene_renderer_.upd_render_texture());

        ui::End();
    }

    void drawLogo()
    {
        Rect const logoRect = calcLogoRect();

        ui::SetNextWindowPos(logoRect.p1);
        ui::Begin("##osclogo", nullptr, ui::GetMinimalWindowFlags());
        ui::Image(m_MainAppLogo, dimensions_of(logoRect));
        ui::End();
    }

    void drawMenu()
    {
        // center the menu window
        Rect const mmr = calcMainMenuRect();
        ui::SetNextWindowPos(mmr.p1);
        ui::SetNextWindowSize({dimensions_of(mmr).x, -1.0f});
        ui::SetNextWindowSizeConstraints(dimensions_of(mmr), dimensions_of(mmr));

        if (ui::Begin("Splash screen", nullptr, ImGuiWindowFlags_NoTitleBar))
        {
            drawMenuContent();
        }
        ui::End();
    }

    void drawMenuContent()
    {
        // de-dupe imgui IDs because these lists may contain duplicate
        // names
        int imguiID = 0;

        ui::Columns(2, nullptr, false);
        drawMenuLeftColumnContent(imguiID);
        ui::NextColumn();
        drawMenuRightColumnContent(imguiID);
        ui::NextColumn();
        ui::Columns();
    }

    void drawActionsMenuSectionContent()
    {
        if (ui::MenuItem(ICON_FA_FILE " New Model"))
        {
            ActionNewModel(m_Parent);
        }
        if (ui::MenuItem(ICON_FA_FOLDER_OPEN " Open Model"))
        {
            ActionOpenModel(m_Parent);
        }
        if (ui::MenuItem(ICON_FA_MAGIC " Import Meshes"))
        {
            m_Parent->addAndSelectTab<mi::MeshImporterTab>(m_Parent);
        }
        App::upd().add_frame_annotation("SplashTab/ImportMeshesMenuItem", ui::GetItemRect());
        if (ui::MenuItem(ICON_FA_BOOK " Open Documentation"))
        {
            OpenPathInOSDefaultApplication(App::config().html_docs_directory() / "index.html");
        }
    }

    void drawWorkflowsMenuSectionContent()
    {
        if (ui::MenuItem(ICON_FA_ARROWS_ALT " Frame Definition"))
        {
            m_Parent->addAndSelectTab<FrameDefinitionTab>(m_Parent);
        }
        if (ui::MenuItem(ICON_FA_MAGIC " Mesh Importer"))
        {
            m_Parent->addAndSelectTab<mi::MeshImporterTab>(m_Parent);
        }
        if (ui::MenuItem(ICON_FA_CUBE " Mesh Warping"))
        {
            m_Parent->addAndSelectTab<MeshWarpingTab>(m_Parent);
        }
    }

    void drawRecentlyOpenedFilesMenuSectionContent(int& imguiID)
    {
        auto const recentFiles = App::singleton<RecentFiles>();
        if (!recentFiles->empty())
        {
            for (RecentFile const& rf : *recentFiles)
            {
                DrawRecentOrExampleFileMenuItem(
                    rf.path,
                    m_Parent,
                    imguiID
                );
            }
        }
        else
        {
            ui::PushStyleColor(ImGuiCol_Text, Color::half_grey());
            ui::TextWrapped("No files opened recently. Try:");
            ui::BulletText("Creating a new model (Ctrl+N)");
            ui::BulletText("Opening an existing model (Ctrl+O)");
            ui::BulletText("Opening an example (right-side)");
            ui::PopStyleColor();
        }
    }

    void drawMenuLeftColumnContent(int& imguiID)
    {
        ui::TextDisabled("Actions");
        ui::Dummy({0.0f, 2.0f});

        drawActionsMenuSectionContent();

        ui::Dummy({0.0f, 1.0f*ui::GetTextLineHeight()});
        ui::TextDisabled("Workflows");
        ui::Dummy({0.0f, 2.0f});

        drawWorkflowsMenuSectionContent();

        ui::Dummy({0.0f, 1.0f*ui::GetTextLineHeight()});
        ui::TextDisabled("Recent Models");
        ui::Dummy({0.0f, 2.0f});

        drawRecentlyOpenedFilesMenuSectionContent(imguiID);
    }

    void drawMenuRightColumnContent(int& imguiID)
    {
        if (!m_MainMenuFileTab.exampleOsimFiles.empty())
        {
            ui::TextDisabled("Example Models");
            ui::Dummy({0.0f, 2.0f});

            for (std::filesystem::path const& examplePath : m_MainMenuFileTab.exampleOsimFiles)
            {
                DrawRecentOrExampleFileMenuItem(
                    examplePath,
                    m_Parent,
                    imguiID
                );
            }
        }
    }

    void drawAttributationLogos()
    {
        Rect const viewportRect = ui::GetMainViewportWorkspaceScreenRect();
        Vec2 loc = viewportRect.p2;
        loc.x = loc.x - 2.0f*ui::GetStyleWindowPadding().x - static_cast<float>(m_CziLogo.dimensions().x) - 2.0f*ui::GetStyleItemSpacing().x - static_cast<float>(m_TudLogo.dimensions().x);
        loc.y = loc.y - 2.0f*ui::GetStyleWindowPadding().y - static_cast<float>(max(m_CziLogo.dimensions().y, m_TudLogo.dimensions().y));

        ui::SetNextWindowPos(loc);
        ui::Begin("##czlogo", nullptr, ui::GetMinimalWindowFlags());
        ui::Image(m_CziLogo);
        ui::End();

        loc.x += static_cast<float>(m_CziLogo.dimensions().x) + 2.0f*ui::GetStyleItemSpacing().x;
        ui::SetNextWindowPos(loc);
        ui::Begin("##tudlogo", nullptr, ui::GetMinimalWindowFlags());
        ui::Image(m_TudLogo);
        ui::End();
    }

    void drawVersionInfo()
    {
        Rect const tabRect = ui::GetMainViewportWorkspaceScreenRect();
        float const h = ui::GetTextLineHeightWithSpacing();
        float const padding = 5.0f;

        Vec2 const pos
        {
            tabRect.p1.x + padding,
            tabRect.p2.y - h - padding,
        };

        ImDrawList* const dl = ui::GetForegroundDrawList();
        ImU32 const color = ui::ToImU32(Color::black());
        std::string const text = calc_full_application_name_with_version_and_build_id(App::get().metadata());
        dl->AddText(pos, color, text.c_str());
    }

    // tab data
    UID m_TabID;
    ParentPtr<IMainUIStateAPI> m_Parent;

    // for rendering the 3D scene
    PolarPerspectiveCamera m_Camera = GetSplashScreenDefaultPolarCamera();
    SceneRenderer scene_renderer_{
        *App::singleton<SceneCache>(App::resource_loader()),
    };
    SceneRendererParams m_LastSceneRendererParams = GetSplashScreenDefaultRenderParams(m_Camera);

    Texture2D m_MainAppLogo = load_texture2D_from_svg(App::load_resource("textures/banner.svg"));
    Texture2D m_CziLogo = load_texture2D_from_svg(App::load_resource("textures/chanzuckerberg_logo.svg"), 0.5f);
    Texture2D m_TudLogo = load_texture2D_from_svg(App::load_resource("textures/tudelft_logo.svg"), 0.5f);

    // dimensions of stuff
    Vec2 m_SplashMenuMaxDims = {640.0f, 512.0f};
    Vec2 m_MainAppLogoDims =  m_MainAppLogo.dimensions();
    Vec2 m_TopLogoPadding = {25.0f, 35.0f};

    // UI state
    MainMenuFileTab m_MainMenuFileTab;
    MainMenuAboutTab m_MainMenuAboutTab;
    LogViewer m_LogViewer;
};


// public API (PIMPL)

osc::SplashTab::SplashTab(ParentPtr<IMainUIStateAPI> const& parent_) :
    m_Impl{std::make_unique<Impl>(parent_)}
{
}

osc::SplashTab::SplashTab(SplashTab&&) noexcept = default;
osc::SplashTab& osc::SplashTab::operator=(SplashTab&&) noexcept = default;
osc::SplashTab::~SplashTab() noexcept = default;

UID osc::SplashTab::implGetID() const
{
    return m_Impl->getID();
}

CStringView osc::SplashTab::implGetName() const
{
    return m_Impl->getName();
}

void osc::SplashTab::implOnMount()
{
    m_Impl->on_mount();
}

void osc::SplashTab::implOnUnmount()
{
    m_Impl->on_unmount();
}

bool osc::SplashTab::implOnEvent(SDL_Event const& e)
{
    return m_Impl->onEvent(e);
}

void osc::SplashTab::implOnDrawMainMenu()
{
    m_Impl->drawMainMenu();
}

void osc::SplashTab::implOnDraw()
{
    m_Impl->onDraw();
}
