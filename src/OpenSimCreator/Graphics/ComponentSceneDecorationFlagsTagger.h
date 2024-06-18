#pragma once

#include <oscar/Graphics/Scene/SceneDecorationFlags.h>

namespace OpenSim { class Component; }
namespace osc { struct SceneDecoration; }

namespace osc
{
    // functor class that sets a decoration's flags based on selection logic
    class ComponentSceneDecorationFlagsTagger final {
    public:
        ComponentSceneDecorationFlagsTagger(
            OpenSim::Component const* selected_,
            OpenSim::Component const* hovered_
        );

        void operator()(const OpenSim::Component&, SceneDecoration&);
    private:
        SceneDecorationFlags computeFlags(const OpenSim::Component&) const;

        OpenSim::Component const* m_Selected;
        OpenSim::Component const* m_Hovered;
        OpenSim::Component const* m_LastComponent = nullptr;
        SceneDecorationFlags m_Flags = SceneDecorationFlags::None;
    };
}
