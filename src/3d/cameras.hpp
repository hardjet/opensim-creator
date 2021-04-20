#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace osc {
    // not included in the camera structs themselves because callers should
    // handle state caller-side.
    //
    // this is because transitioning the state requires integration with the
    // caller's event system, 3D state, etc.
    enum class Camera_state { Viewing, Dragging, Panning };

    struct Polar_perspective_camera final {
        // polar coords
        float radius = 5.0f;
        float theta = 0.88f;
        float phi = 0.4f;

        // how much to pan the scene by, relative to worldspace
        glm::vec3 pan = {0.3f, -0.5f, 0.0f};

        // clipspace properties
        float fov = 120.0f;
        float znear = 0.1f;
        float zfar = 100.0f;
    };

    // note: relative deltas here are relative to whatever "screen" the camera
    // is handling.
    //
    // e.g. moving a mouse 400px in X in a screen that is 800px wide should
    //      have a delta.x of 0.5f

    // pan: pan along the current view plane
    void pan(Polar_perspective_camera&, float aspect_ratio, glm::vec2 delta) noexcept;

    // drag: spin the view around the origin, such that the distance between
    //       the camera and the origin remains constant
    void drag(Polar_perspective_camera& cam, glm::vec2 delta) noexcept;

    [[nodiscard]] glm::mat4 view_matrix(Polar_perspective_camera const&) noexcept;
    [[nodiscard]] glm::mat4 projection_matrix(Polar_perspective_camera const&, float aspect_ratio) noexcept;
    [[nodiscard]] glm::vec3 pos(Polar_perspective_camera const&) noexcept;
}
