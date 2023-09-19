#pragma once

#include <nonstd/span.hpp>
#include <oscar/Utils/CStringView.hpp>

#include <cstddef>

namespace osc
{
    enum class MuscleColoringStyle {
        OpenSimAppearanceProperty = 0,
        OpenSim,
        Activation,
        Excitation,
        Force,
        FiberLength,
        NUM_OPTIONS,

        Default = OpenSim,
    };

    nonstd::span<MuscleColoringStyle const> GetAllMuscleColoringStyles();
    nonstd::span<CStringView const> GetAllMuscleColoringStyleStrings();
    ptrdiff_t GetIndexOf(MuscleColoringStyle);
}
