#pragma once

#include <oscar/Graphics/Mesh.h>
#include <oscar/Utils/CStringView.h>

#include <cstddef>

namespace osc
{
    class DodecahedronGeometry final : public Mesh {
    public:
        static constexpr CStringView name() { return "Dodecahedron"; }

        struct Params final {
            float radius = 1.0f;
            size_t detail = 0;
        };

        explicit DodecahedronGeometry(const Params& = {});
    };
}
