#include "MainMenu.hpp"

#include "osc_config.hpp"

#include "src/Bindings/ImGuiHelpers.hpp"
#include "src/OpenSimBindings/MiddlewareAPIs/MainUIStateAPI.hpp"
#include "src/OpenSimBindings/Tabs/MeshImporterTab.hpp"
#include "src/OpenSimBindings/Tabs/SimulatorTab.hpp"
#include "src/OpenSimBindings/ActionFunctions.hpp"
#include "src/OpenSimBindings/Simulation.hpp"
#include "src/OpenSimBindings/StoFileSimulation.hpp"
#include "src/OpenSimBindings/OpenSimHelpers.hpp"
#include "src/OpenSimBindings/UndoableModelStatePair.hpp"
#include "src/Platform/App.hpp"
#include "src/Platform/Config.hpp"
#include "src/Platform/Log.hpp"
#include "src/Platform/os.hpp"
#include "src/Utils/Algorithms.hpp"
#include "src/Utils/Assertions.hpp"
#include "src/Utils/FilesystemHelpers.hpp"
#include "src/Utils/UID.hpp"

#include <imgui.h>
#include <IconsFontAwesome5.h>
#include <OpenSim/Common/PropertyObjArray.h>
#include <OpenSim/Common/Set.h>
#include <OpenSim/Simulation/Model/Model.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <typeinfo>
#include <utility>

static constexpr auto c_AntialiasingLevels = osc::MakeArray<char const*>("x1", "x2", "x4", "x8", "x16", "x32", "x64", "x128");


// public API

osc::MainMenuFileTab::MainMenuFileTab() :
    exampleOsimFiles{FindAllFilesWithExtensionsRecursively(App::resource("models"), ".osim")},
    recentlyOpenedFiles{App::get().getRecentFiles()}
{
    std::sort(exampleOsimFiles.begin(), exampleOsimFiles.end(), IsFilenameLexographicallyGreaterThan);
}

void osc::MainMenuFileTab::draw(MainUIStateAPI* api, UndoableModelStatePair* maybeModel)
{
    // handle hotkeys enabled by just drawing the menu
    {
        auto const& io = ImGui::GetIO();

        bool mod = osc::IsCtrlOrSuperDown();

        if (mod && ImGui::IsKeyPressed(ImGuiKey_N))
        {
            ActionNewModel(*api);
        }
        else if (mod && ImGui::IsKeyPressed(ImGuiKey_O))
        {
            ActionOpenModel(*api);
        }
        else if (maybeModel && mod && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S))
        {
            ActionSaveCurrentModelAs(*maybeModel);
        }
        else if (maybeModel && mod && ImGui::IsKeyPressed(ImGuiKey_S))
        {
            ActionSaveModel(*api, *maybeModel);
        }
    }

    // draw "save as", if necessary
    if (maybeSaveChangesPopup)
    {
        maybeSaveChangesPopup->draw();
    }

    if (!ImGui::BeginMenu("File"))
    {
        return;
    }

    if (ImGui::MenuItem(ICON_FA_FILE " New", "Ctrl+N"))
    {
        ActionNewModel(*api);
    }

    if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open", "Ctrl+O"))
    {
        ActionOpenModel(*api);
    }

    int imgui_id = 0;

    if (ImGui::BeginMenu(ICON_FA_FOLDER_OPEN " Open Recent", !recentlyOpenedFiles.empty()))
    {
        // iterate in reverse: recent files are stored oldest --> newest
        for (auto it = recentlyOpenedFiles.rbegin(); it != recentlyOpenedFiles.rend(); ++it)
        {
            RecentFile const& rf = *it;
            ImGui::PushID(++imgui_id);
            if (ImGui::MenuItem(rf.path.filename().string().c_str()))
            {
                ActionOpenModel(*api, rf.path);
            }
            ImGui::PopID();
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu(ICON_FA_FOLDER_OPEN " Open Example"))
    {
        for (std::filesystem::path const& ex : exampleOsimFiles)
        {
            ImGui::PushID(++imgui_id);
            if (ImGui::MenuItem(ex.filename().string().c_str()))
            {
                ActionOpenModel(*api, ex);
            }
            ImGui::PopID();
        }

        ImGui::EndMenu();
    }

    if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Load Motion", nullptr, false, maybeModel != nullptr))
    {
        std::optional<std::filesystem::path> maybePath = osc::PromptUserForFile("sto,mot");
        if (maybePath)
        {
            try
            {
                std::unique_ptr<OpenSim::Model> cpy = std::make_unique<OpenSim::Model>(maybeModel->getModel());
                osc::InitializeModel(*cpy);
                osc::InitializeState(*cpy);

                UID tabID = api->addTab<SimulatorTab>(api, std::make_shared<Simulation>(osc::StoFileSimulation{std::move(cpy), *maybePath, maybeModel->getFixupScaleFactor()}));
                api->selectTab(tabID);
            }
            catch (std::exception const& ex)
            {
                log::error("encountered error while trying to load an STO file against the model: %s", ex.what());
            }
        }
    }

    if (ImGui::MenuItem(ICON_FA_SAVE " Save", "Ctrl+S", false, maybeModel != nullptr))
    {
        ActionSaveModel(*api, *maybeModel);
    }

    if (ImGui::MenuItem(ICON_FA_SAVE " Save As", "Shift+Ctrl+S", false, maybeModel != nullptr))
    {
        ActionSaveCurrentModelAs(*maybeModel);
    }

    if (ImGui::MenuItem(ICON_FA_MAGIC " Import Meshes"))
    {
        UID tabID = api->addTab<MeshImporterTab>(api);
        api->selectTab(tabID);
    }
    osc::App::upd().addFrameAnnotation("MainMenu/ImportMeshesMenuItem", osc::GetItemRect());

    if (ImGui::MenuItem(ICON_FA_TIMES_CIRCLE " Quit", "Ctrl+Q"))
    {
        App::upd().requestQuit();
    }

    ImGui::EndMenu();
}


void osc::MainMenuAboutTab::draw()
{
    if (!ImGui::BeginMenu("About"))
    {
        return;
    }

    static constexpr float menuWidth = 400;
    ImGui::Dummy({menuWidth, 0});

    ImGui::TextUnformatted("graphics");
    ImGui::SameLine();
    DrawHelpMarker("OSMV's global graphical settings");
    ImGui::Separator();
    ImGui::Dummy({0.0f, 0.5f});
    {
        ImGui::Columns(2);

        ImGui::TextUnformatted("FPS");
        ImGui::NextColumn();
        ImGui::Text("%.0f", static_cast<double>(ImGui::GetIO().Framerate));
        ImGui::NextColumn();

        ImGui::TextUnformatted("MSXAA");
        ImGui::SameLine();
        DrawHelpMarker("the level of MultiSample Anti-Aliasing to use. This only affects 3D renders *within* the UI, not the whole UI (panels etc. will not be affected)");
        ImGui::NextColumn();
        {
            int samplesIdx = LeastSignificantBitIndex(App::get().getMSXAASamplesRecommended());
            int maxSamplesIdx = LeastSignificantBitIndex(App::get().getMSXAASamplesMax());
            OSC_ASSERT(static_cast<size_t>(maxSamplesIdx) < c_AntialiasingLevels.size());

            if (ImGui::Combo("##msxaa", &samplesIdx, c_AntialiasingLevels.data(), maxSamplesIdx + 1))
            {
                App::upd().setMSXAASamplesRecommended(1 << samplesIdx);
            }
        }
        ImGui::NextColumn();

        ImGui::TextUnformatted("window");
        ImGui::NextColumn();

        if (ImGui::Button(ICON_FA_EXPAND " fullscreen"))
        {
            App::upd().makeFullscreen();
        }
        if (ImGui::Button(ICON_FA_EXPAND " windowed fullscreen"))
        {
            App::upd().makeWindowedFullscreen();
        }
        if (ImGui::Button(ICON_FA_WINDOW_RESTORE " windowed"))
        {
            App::upd().makeWindowed();
        }
        ImGui::NextColumn();

        ImGui::TextUnformatted("VSYNC");
        ImGui::SameLine();
        DrawHelpMarker("whether the backend uses vertical sync (VSYNC), which will cap the rendering FPS to your monitor's refresh rate");
        ImGui::NextColumn();

        bool enabled = App::get().isVsyncEnabled();
        if (ImGui::Checkbox("##vsynccheckbox", &enabled)) {
            if (enabled) {
                App::upd().enableVsync();
            } else {
                App::upd().disableVsync();
            }
        }
        ImGui::NextColumn();

        ImGui::Columns();
    }

    ImGui::Dummy({0.0f, 2.0f});
    ImGui::TextUnformatted("properties");
    ImGui::SameLine();
    DrawHelpMarker("general software properties: useful information for bug reporting etc.");
    ImGui::Separator();
    ImGui::Dummy({0.0f, 0.5f});
    {
        ImGui::Columns(2);

        ImGui::TextUnformatted("OSC_VERSION");
        ImGui::NextColumn();
        ImGui::TextUnformatted(OSC_VERSION_STRING);
        ImGui::NextColumn();

        ImGui::TextUnformatted("OSC_BUILD_ID");
        ImGui::NextColumn();
        ImGui::TextUnformatted(OSC_BUILD_ID);
        ImGui::NextColumn();

        ImGui::TextUnformatted("Graphics vendor");
        ImGui::NextColumn();
        ImGui::Text("%s", App::get().getGraphicsBackendVendorString().c_str());
        ImGui::NextColumn();

        ImGui::TextUnformatted("Graphics renderer");
        ImGui::NextColumn();
        ImGui::Text("%s", App::get().getGraphicsBackendRendererString().c_str());
        ImGui::NextColumn();

        ImGui::TextUnformatted("Graphics renderer version");
        ImGui::NextColumn();
        ImGui::Text("%s", App::get().getGraphicsBackendVersionString().c_str());
        ImGui::NextColumn();

        ImGui::TextUnformatted("Graphics shader version");
        ImGui::NextColumn();
        ImGui::Text("%s", App::get().getGraphicsBackendShadingLanguageVersionString().c_str());
        ImGui::NextColumn();

        ImGui::Columns(1);
    }

    ImGui::Dummy({0.0f, 2.5f});
    ImGui::TextUnformatted("debugging utilities:");
    ImGui::SameLine();
    DrawHelpMarker("standard utilities that can help with development, debugging, etc.");
    ImGui::Separator();
    ImGui::Dummy({0.0f, 0.5f});
    int id = 0;
    {
        ImGui::Columns(2);

        ImGui::TextUnformatted("OSC Install Location");
        ImGui::SameLine();
        DrawHelpMarker("opens OSC's installation location in your OS's default file browser");
        ImGui::NextColumn();
        ImGui::PushID(id++);
        if (ImGui::Button(ICON_FA_FOLDER " open"))
        {
            OpenPathInOSDefaultApplication(CurrentExeDir());
        }
        ImGui::PopID();
        ImGui::NextColumn();

        ImGui::TextUnformatted("User Data Dir");
        ImGui::SameLine();
        DrawHelpMarker("opens your OSC user data directory in your OS's default file browser");
        ImGui::NextColumn();
        ImGui::PushID(id++);
        if (ImGui::Button(ICON_FA_FOLDER " open")) {
            OpenPathInOSDefaultApplication(GetUserDataDir());
        }
        ImGui::PopID();
        ImGui::NextColumn();

        ImGui::TextUnformatted("Debug mode");
        ImGui::SameLine();
        DrawHelpMarker("Toggles whether the application is in debug mode or not: enabling this can reveal more inforamtion about bugs");
        ImGui::NextColumn();
        {
            bool appIsInDebugMode = App::get().isInDebugMode();
            if (ImGui::Checkbox("##opengldebugmodecheckbox", &appIsInDebugMode))
            {
                if (appIsInDebugMode)
                {
                    App::upd().enableDebugMode();
                }
                else
                {
                    App::upd().disableDebugMode();
                }
            }
        }

        ImGui::Columns();
    }

    ImGui::Dummy({0.0f, 2.5f});
    ImGui::TextUnformatted("useful links:");
    ImGui::SameLine();
    DrawHelpMarker("links to external sites that might be useful");
    ImGui::Separator();
    ImGui::Dummy({0.0f, 0.5f});
    {
        ImGui::Columns(2);

        ImGui::TextUnformatted("OpenSim Creator Documentation");
        ImGui::NextColumn();
        ImGui::PushID(id++);
        if (ImGui::Button(ICON_FA_LINK " open")) {
            OpenPathInOSDefaultApplication(App::get().getConfig().getHTMLDocsDir() / "index.html");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted("this will open the (locally installed) documentation in a separate browser window");
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::PopID();
        ImGui::NextColumn();

        ImGui::TextUnformatted("OpenSim Creator GitHub");
        ImGui::NextColumn();
        ImGui::PushID(id++);
        if (ImGui::Button(ICON_FA_LINK " open")) {
            OpenPathInOSDefaultApplication(OSC_REPO_URL);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted("this will open the GitHub homepage in a separate browser window");
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::PopID();
        ImGui::NextColumn();

        ImGui::TextUnformatted("OpenSim Documentation");
        ImGui::NextColumn();
        ImGui::PushID(id++);
        if (ImGui::Button(ICON_FA_LINK " open")) {
            OpenPathInOSDefaultApplication("https://simtk-confluence.stanford.edu/display/OpenSim/Documentation");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted("this will open the documentation in a separate browser window");
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::PopID();
        ImGui::NextColumn();

        ImGui::Columns();
    }

    ImGui::EndMenu();
}
