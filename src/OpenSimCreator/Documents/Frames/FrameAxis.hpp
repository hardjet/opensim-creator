#pragma once

#include <iostream>
#include <optional>
#include <string_view>

namespace osc::frames
{
    enum class FrameAxis {
        PlusX = 0,
        PlusY,
        PlusZ,
        MinusX,
        MinusY,
        MinusZ,
        NUM_OPTIONS,
    };

    std::optional<FrameAxis> TryParseAsFrameAxis(std::string_view);
    bool AreOrthogonal(FrameAxis, FrameAxis);
    std::ostream& operator<<(std::ostream&, FrameAxis);
}