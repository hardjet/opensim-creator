#pragma once

#include <nonstd/span.hpp>
#include <oscar/Utils/CStringView.hpp>

#include <cstddef>

namespace osc
{
    enum class MuscleSizingStyle {
        OpenSim = 0,
        PcsaDerived,
        NUM_OPTIONS,

        Default = OpenSim,
    };

    nonstd::span<MuscleSizingStyle const> GetAllMuscleSizingStyles();
    nonstd::span<CStringView const> GetAllMuscleSizingStyleStrings();
    ptrdiff_t GetIndexOf(MuscleSizingStyle);
}
