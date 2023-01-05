#include "MuscleColoringStyle.hpp"

#include "src/Utils/Algorithms.hpp"

#include <nonstd/span.hpp>

#include <array>
#include <cstddef>

static constexpr auto c_ColorStyles = osc::MakeSizedArray<osc::MuscleColoringStyle, static_cast<size_t>(osc::MuscleColoringStyle::TOTAL)>
(
    osc::MuscleColoringStyle::OpenSim,
    osc::MuscleColoringStyle::Activation,
    osc::MuscleColoringStyle::Excitation,
    osc::MuscleColoringStyle::Force,
    osc::MuscleColoringStyle::FiberLength
);

static constexpr auto c_ColorStyleStrings = osc::MakeSizedArray<char const*, static_cast<size_t>(osc::MuscleColoringStyle::TOTAL)>
(
    "OpenSim",
    "Activation",
    "Excitation",
    "Force",
    "Fiber Length"
);

nonstd::span<osc::MuscleColoringStyle const> osc::GetAllMuscleColoringStyles()
{
    return c_ColorStyles;
}

nonstd::span<char const* const> osc::GetAllMuscleColoringStyleStrings()
{
    return c_ColorStyleStrings;
}

int osc::GetIndexOf(osc::MuscleColoringStyle s)
{
    return static_cast<int>(s);
}
