#pragma once

#include <oscar/Graphics/Scene/SceneCollision.h>
#include <oscar/Maths/Vec3.h>

#include <optional>

namespace osc { struct PolarPerspectiveCamera; }
namespace osc { struct Rect; }

namespace osc
{
    class GuiRuler final {
    public:
        void onDraw(
            const PolarPerspectiveCamera&,
            const Rect& renderRect,
            std::optional<SceneCollision>
        );
        void startMeasuring();
        void stopMeasuring();
        void toggleMeasuring();
        bool isMeasuring() const;

    private:
        enum class State { Inactive, WaitingForFirstPoint, WaitingForSecondPoint };
        State m_State = State::Inactive;
        Vec3 m_StartWorldPos = {};
    };
}
