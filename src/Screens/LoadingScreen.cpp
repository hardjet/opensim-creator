#include "LoadingScreen.hpp"

#include "src/3D/Gl.hpp"
#include "src/OpenSimBindings/UndoableUiModel.hpp"
#include "src/Screens/ModelEditorScreen.hpp"
#include "src/Screens/SplashScreen.hpp"
#include "src/App.hpp"
#include "src/Assertions.hpp"
#include "src/Log.hpp"

#include <imgui.h>
#include <OpenSim/Simulation/Model/Model.h>
#include <SDL_events.h>

#include <chrono>
#include <filesystem>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

using namespace osc;

// the function that loads the OpenSim model
static std::unique_ptr<osc::UndoableUiModel> loadOpenSimModel(std::string path) {
    auto model = std::make_unique<OpenSim::Model>(path);
    return std::make_unique<osc::UndoableUiModel>(std::move(model));
}

struct LoadingScreen::Impl final {

    // filesystem path to the osim being loaded
    std::filesystem::path path;

    // future that lets the UI thread poll the loading thread for
    // the loaded model
    std::future<std::unique_ptr<osc::UndoableUiModel>> result;

    // if not empty, any error encountered by the loading thread
    std::string error;

    // a main state that should be recycled by this screen when
    // transitioning into the editor
    std::shared_ptr<MainEditorState> mes;

    // a fake progress indicator that never quite reaches 100 %
    //
    // this might seem evil, but its main purpose is to ensure the
    // user that *something* is happening - even if that "something"
    // is "the background thread is deadlocked" ;)
    float progress;

    Impl(std::filesystem::path _path, std::shared_ptr<MainEditorState> _mes) :

        // save the path being loaded
        path{std::move(_path)},

        // immediately start loading the model file on a background thread
        result{std::async(std::launch::async, loadOpenSimModel, path.string())},

        // error is blank until the UI thread encounters an error polling `result`
        error{},

        // save the editor state (if any): it will be forwarded after loading the model
        mes{std::move(_mes)},

        progress{0.0f}
    {
        if (!mes)
        {
            mes = std::make_shared<MainEditorState>();
        }

        OSC_ASSERT(mes);
    }
};

// public API

osc::LoadingScreen::LoadingScreen(
        std::shared_ptr<MainEditorState> _st,
        std::filesystem::path _path) :

    m_Impl{new Impl{std::move(_path), std::move(_st)}} {
}

osc::LoadingScreen::~LoadingScreen() noexcept = default;

void osc::LoadingScreen::onMount() {
    osc::ImGuiInit();
}

void osc::LoadingScreen::onUnmount() {
    osc::ImGuiShutdown();
}

void osc::LoadingScreen::onEvent(SDL_Event const& e) {
    if (osc::ImGuiOnEvent(e)) {
        return;
    }

    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
        App::cur().requestTransition<SplashScreen>();
        return;
    }
}

void osc::LoadingScreen::tick(float dt) {
    Impl& impl = *m_Impl;

    // tick the progress bar up a little bit
    impl.progress += (dt * (1.0f - impl.progress))/2.0f;

    // if there's an error, then the result came through (it's an error)
    // and this screen should just continuously show the error until the
    // user decides to transition back
    if (!impl.error.empty()) {
        return;
    }

    // otherwise, poll for the result and catch any exceptions that bubble
    // up from the background thread
    std::unique_ptr<UndoableUiModel> result = nullptr;
    try {
        if (impl.result.wait_for(std::chrono::seconds{0}) == std::future_status::ready) {
            result = impl.result.get();
        }
    } catch (std::exception const& ex) {
        impl.error = ex.what();
        return;
    } catch (...) {
        impl.error = "an unknown exception (does not inherit from std::exception) occurred when loading the file";
        return;
    }

    // if there was a result (a newly-loaded model), handle it
    if (result) {

        // add newly-loaded model to the "Recent Files" list
        App::cur().addRecentFile(impl.path);

        if (impl.mes) {
            // there is an existing editor state
            //
            // recycle it so that users can keep their running sims, local edits, etc.
            impl.mes->editedModel = std::move(*result);
            App::cur().requestTransition<ModelEditorScreen>(impl.mes);
            for (auto& viewer : impl.mes->viewers)
            {
                if (viewer)
                {
                    viewer->requestAutoFocus();
                }
            }
        } else {
            // there is no existing editor state
            //
            // transitiong into "fresh" editor
            auto mes = std::make_shared<MainEditorState>(std::move(*result));
            App::cur().requestTransition<ModelEditorScreen>(mes);
            for (auto& viewer : mes->viewers)
            {
                if (viewer)
                {
                    viewer->requestAutoFocus();
                }
            }
        }
    }
}

void osc::LoadingScreen::draw() {
    osc::ImGuiNewFrame();

    Impl& impl = *m_Impl;
    constexpr glm::vec2 menu_dims = {512.0f, 512.0f};

    gl::ClearColor(0.99f, 0.98f, 0.96f, 1.0f);
    gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::vec2 window_dims = App::cur().dims();

    // center the menu
    {
        glm::vec2 menu_pos = (window_dims - menu_dims) / 2.0f;
        ImGui::SetNextWindowPos(menu_pos);
        ImGui::SetNextWindowSize(ImVec2(menu_dims.x, -1));
    }

    if (impl.error.empty()) {
        if (ImGui::Begin("Loading Message", nullptr, ImGuiWindowFlags_NoTitleBar)) {
            ImGui::Text("loading: %s", impl.path.string().c_str());
            ImGui::ProgressBar(impl.progress);
        }
        ImGui::End();
    } else {
        if (ImGui::Begin("Error Message", nullptr, ImGuiWindowFlags_NoTitleBar)) {
            ImGui::TextWrapped("An error occurred while loading the file:");
            ImGui::Dummy(ImVec2{0.0f, 5.0f});
            ImGui::TextWrapped("%s", impl.error.c_str());
            ImGui::Dummy(ImVec2{0.0f, 5.0f});

            if (ImGui::Button("back to splash screen (ESC)")) {
                App::cur().requestTransition<SplashScreen>();
            }
            ImGui::SameLine();
            if (ImGui::Button("try again")) {
                App::cur().requestTransition<LoadingScreen>(impl.mes, impl.path);
            }
        }
        ImGui::End();
    }
    osc::ImGuiRender();
}
