#pragma once

#include <SDL_events.h>

#include <memory>

namespace OpenSim {
    class Model;
    class Component;
    class ModelDisplayHints;
}

namespace SimTK {
    class State;
}

namespace osc {

    // flags that toggle the viewer's behavior
    using Component3DViewerFlags = int;
    enum Component3DViewerFlags_ {

        // no flags: a basic-as-possible render
        Component3DViewerFlags_None = 0,

        // draw dynamic decorations, as defined by OpenSim (e.g. muscles)
        Component3DViewerFlags_DrawDynamicDecorations = 1 << 0,

        // draw static decorations, as defined by OpenSim (e.g. meshes)
        Component3DViewerFlags_DrawStaticDecorations = 1 << 1,

        // draw scene floor
        Component3DViewerFlags_DrawFloor = 1 << 2,

        // draw model "frames", as defined by OpenSim (e.g. body frames)
        Component3DViewerFlags_DrawFrames = 1 << 3,

        // draw debug geometry, as defined by OpenSim
        Component3DViewerFlags_DrawDebugGeometry = 1 << 4,

        // draw labels, as defined by OpenSim
        Component3DViewerFlags_DrawLabels = 1 << 5,

        // draw a 2D XZ grid
        Component3DViewerFlags_DrawXZGrid = 1 << 6,

        // draw a 2D XY grid
        Component3DViewerFlags_DrawXYGrid = 1 << 7,

        // draw a 2D YZ grid
        Component3DViewerFlags_DrawYZGrid = 1 << 8,

        // draw alignment axes
        //
        // these are little red+green+blue demo axes in corner of the viewer that
        // show the user how the world axes align relative to the current view location
        Component3DViewerFlags_DrawAlignmentAxes = 1 << 9,

        Component3DViewerFlags_Default = Component3DViewerFlags_DrawDynamicDecorations |
                                         Component3DViewerFlags_DrawStaticDecorations |
                                         Component3DViewerFlags_DrawFloor,
    };

    // viewer response
    //
    // this lets higher-level callers know of any potentially-relevant state
    // changes the viewer has detected
    struct Component3DViewerResponse final {
        OpenSim::Component const* hovertest_result = nullptr;
        bool is_moused_over = false;
        bool is_left_clicked = false;
        bool is_right_clicked = false;
    };

    // a 3D viewer for a single OpenSim::Component or OpenSim::Model
    //
    // internally handles rendering, hit testing, etc. and exposes and API that lets
    // callers only have to handle `OpenSim::Model`s, `OpenSim::Component`s, etc.
    class Component_3d_viewer final {
    public:
        struct Impl;
    private:
        std::unique_ptr<Impl> m_Impl;

    public:
        Component_3d_viewer(Component3DViewerFlags = Component3DViewerFlags_Default);
        ~Component_3d_viewer() noexcept;

        bool is_moused_over() const noexcept;

        bool on_event(SDL_Event const&);

        Component3DViewerResponse draw(
            char const* panel_name,
            OpenSim::Component const&,
            OpenSim::ModelDisplayHints const&,
            SimTK::State const&,
            OpenSim::Component const* current_selection,
            OpenSim::Component const* current_hover);

        Component3DViewerResponse draw(
            char const* panel_name,
            OpenSim::Model const&,
            SimTK::State const&,
            OpenSim::Component const* current_selection = nullptr,
            OpenSim::Component const* current_hover = nullptr);
    };
}
