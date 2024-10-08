#include "MandelbrotTab.h"

#include <oscar/oscar.h>

#include <limits>
#include <memory>

using namespace osc;

namespace
{
    constexpr CStringView c_tab_string_id = "Demos/Mandelbrot";

    Camera create_identity_camera()
    {
        Camera camera;
        camera.set_view_matrix_override(identity<Mat4>());
        camera.set_projection_matrix_override(identity<Mat4>());
        return camera;
    }
}

class osc::MandelbrotTab::Impl final : public StandardTabImpl {
public:
    Impl() : StandardTabImpl{c_tab_string_id}
    {}

private:
    bool on_keyup(const KeyEvent& e)
    {
        if (e.matches(Key::PageUp) and num_iterations_ < std::numeric_limits<decltype(num_iterations_)>::max()) {
            num_iterations_ *= 2;
            return true;
        }
        if (e.matches(Key::PageDown) and num_iterations_ > 1) {
            num_iterations_ /= 2;
            return true;
        }
        return false;
    }

    bool impl_on_event(const Event& ev) final
    {
        if (ev.type() == EventType::KeyRelease) {
            return on_keyup(dynamic_cast<const KeyEvent&>(ev));
        }
        else if (ev.type() == EventType::MouseWheel) {
            const float factor = dynamic_cast<const MouseWheelEvent&>(ev).delta().y > 0 ? 0.9f : 1.11111111f;
            apply_zoom_to_camera(ui::get_mouse_pos(), factor);
            return true;
        }
        else if (ev.type() == EventType::MouseMove) {
            apply_pan_to_camera(dynamic_cast<const MouseEvent&>(ev).relative_delta());
            return true;
        }
        return false;
    }

    void impl_on_draw() final
    {
        main_viewport_workspace_screenspace_rect_ = ui::get_main_viewport_workspace_screenspace_rect();

        material_.set("uRescale", Vec2{1.0f, 1.0f});
        material_.set("uOffset", Vec2{});
        material_.set("uNumIterations", num_iterations_);
        graphics::draw(quad_mesh_, identity<Transform>(), material_, camera_);
        camera_.set_pixel_rect(main_viewport_workspace_screenspace_rect_);
        camera_.render_to_screen();
    }

    void apply_zoom_to_camera(Vec2, float)
    {
        // TODO: zoom the mandelbrot viewport into the given screen-space location by the given factor
    }

    void apply_pan_to_camera(Vec2)
    {
        // TODO: pan the mandelbrot viewport by the given screen-space offset vector
    }

    ResourceLoader loader_ = App::resource_loader();
    int num_iterations_ = 16;
    Rect normalized_mandelbrot_viewport_ = {{}, {1.0f, 1.0f}};
    Rect main_viewport_workspace_screenspace_rect_ = {};
    Mesh quad_mesh_ = PlaneGeometry{{.width = 2.0f, .height = 2.0f}};
    Material material_{Shader{
        loader_.slurp("oscar_demos/shaders/Mandelbrot.vert"),
        loader_.slurp("oscar_demos/shaders/Mandelbrot.frag"),
    }};
    Camera camera_ = create_identity_camera();
};


CStringView osc::MandelbrotTab::id()
{
    return c_tab_string_id;
}

osc::MandelbrotTab::MandelbrotTab(const ParentPtr<ITabHost>&) :
    impl_{std::make_unique<Impl>()}
{}

osc::MandelbrotTab::MandelbrotTab(MandelbrotTab&&) noexcept = default;
osc::MandelbrotTab& osc::MandelbrotTab::operator=(MandelbrotTab&&) noexcept = default;
osc::MandelbrotTab::~MandelbrotTab() noexcept = default;

UID osc::MandelbrotTab::impl_get_id() const
{
    return impl_->id();
}

CStringView osc::MandelbrotTab::impl_get_name() const
{
    return impl_->name();
}

bool osc::MandelbrotTab::impl_on_event(const Event& e)
{
    return impl_->on_event(e);
}

void osc::MandelbrotTab::impl_on_draw()
{
    impl_->on_draw();
}
