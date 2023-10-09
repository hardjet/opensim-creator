#include "CustomRenderingOptionFlags.hpp"

#include <nonstd/span.hpp>
#include <oscar/Utils/Cpp20Shims.hpp>

namespace
{
    constexpr auto c_Metadata = osc::to_array<osc::CustomRenderingOptionFlagsMetadata>(
    {
        osc::CustomRenderingOptionFlagsMetadata
        {
            "show_floor",
            "Floor",
            osc::CustomRenderingOptionFlags::DrawFloor,
        },
        osc::CustomRenderingOptionFlagsMetadata
        {
            "show_mesh_normals",
            "Mesh Normals",
            osc::CustomRenderingOptionFlags::MeshNormals,
        },
        osc::CustomRenderingOptionFlagsMetadata
        {
            "show_shadows",
            "Shadows",
            osc::CustomRenderingOptionFlags::Shadows,
        },
        osc::CustomRenderingOptionFlagsMetadata
        {
            "show_selection_rims",
            "Selection Rims",
            osc::CustomRenderingOptionFlags::DrawSelectionRims,
        },
    });
}

nonstd::span<osc::CustomRenderingOptionFlagsMetadata const> osc::GetAllCustomRenderingOptionFlagsMetadata()
{
    return c_Metadata;
}