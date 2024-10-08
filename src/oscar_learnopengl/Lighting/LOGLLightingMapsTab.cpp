#include "LOGLLightingMapsTab.h"

#include <oscar/oscar.h>

#include <memory>

using namespace osc::literals;
using namespace osc;

namespace
{
    constexpr CStringView c_tab_string_id = "LearnOpenGL/LightingMaps";

    MouseCapturingCamera create_camera()
    {
        MouseCapturingCamera rv;
        rv.set_position({0.0f, 0.0f, 3.0f});
        rv.set_vertical_fov(45_deg);
        rv.set_clipping_planes({0.1f, 100.0f});
        return rv;
    }

    Material create_light_mapping_material(IResourceLoader& loader)
    {
        const Texture2D diffuse_map = load_texture2D_from_image(
            loader.open("oscar_learnopengl/textures/container2.png"),
            ColorSpace::sRGB,
            ImageLoadingFlag::FlipVertically
        );

        const Texture2D specular_map = load_texture2D_from_image(
            loader.open("oscar_learnopengl/textures/container2_specular.png"),
            ColorSpace::sRGB,
            ImageLoadingFlag::FlipVertically
        );

        Material rv{Shader{
            loader.slurp("oscar_learnopengl/shaders/Lighting/LightingMaps.vert"),
            loader.slurp("oscar_learnopengl/shaders/Lighting/LightingMaps.frag"),
        }};
        rv.set("uMaterialDiffuse", diffuse_map);
        rv.set("uMaterialSpecular", specular_map);
        return rv;
    }
}

class osc::LOGLLightingMapsTab::Impl final : public StandardTabImpl {
public:
    Impl() : StandardTabImpl{c_tab_string_id}
    {}

private:
    void impl_on_mount() final
    {
        App::upd().make_main_loop_polling();
        camera_.on_mount();
    }

    void impl_on_unmount() final
    {
        camera_.on_unmount();
        App::upd().make_main_loop_waiting();
    }

    bool impl_on_event(const Event& e) final
    {
        return camera_.on_event(e);
    }

    void impl_on_draw() final
    {
        camera_.on_draw();

        // clear screen and ensure camera has correct pixel rect
        App::upd().clear_screen(Color::dark_grey());

        // draw cube
        lighting_maps_material_.set("uViewPos", camera_.position());
        lighting_maps_material_.set("uLightPos", light_transform_.position);
        lighting_maps_material_.set("uLightAmbient", light_ambient_);
        lighting_maps_material_.set("uLightDiffuse", light_diffuse_);
        lighting_maps_material_.set("uLightSpecular", light_specular_);
        lighting_maps_material_.set("uMaterialShininess", material_shininess_);
        graphics::draw(mesh_, identity<Transform>(), lighting_maps_material_, camera_);

        // draw lamp
        light_cube_material_.set("uLightColor", Color::white());
        graphics::draw(mesh_, light_transform_, light_cube_material_, camera_);

        // render 3D scene
        camera_.set_pixel_rect(ui::get_main_viewport_workspace_screenspace_rect());
        camera_.render_to_screen();

        // render 2D UI
        ui::begin_panel("controls");
        ui::draw_vec3_input("uLightPos", light_transform_.position);
        ui::draw_float_input("uLightAmbient", &light_ambient_);
        ui::draw_float_input("uLightDiffuse", &light_diffuse_);
        ui::draw_float_input("uLightSpecular", &light_specular_);
        ui::draw_float_input("uMaterialShininess", &material_shininess_);
        ui::end_panel();
    }

    ResourceLoader loader_ = App::resource_loader();
    Material lighting_maps_material_ = create_light_mapping_material(loader_);
    Material light_cube_material_{Shader{
        loader_.slurp("oscar_learnopengl/shaders/LightCube.vert"),
        loader_.slurp("oscar_learnopengl/shaders/LightCube.frag"),
    }};
    Mesh mesh_ = BoxGeometry{};
    MouseCapturingCamera camera_ = create_camera();

    Transform light_transform_ = {
        .scale = Vec3{0.2f},
        .position = {0.4f, 0.4f, 2.0f},
    };
    float light_ambient_ = 0.02f;
    float light_diffuse_ = 0.4f;
    float light_specular_ = 1.0f;
    float material_shininess_ = 64.0f;
};


CStringView osc::LOGLLightingMapsTab::id()
{
    return c_tab_string_id;
}

osc::LOGLLightingMapsTab::LOGLLightingMapsTab(const ParentPtr<ITabHost>&) :
    impl_{std::make_unique<Impl>()}
{}
osc::LOGLLightingMapsTab::LOGLLightingMapsTab(LOGLLightingMapsTab&&) noexcept = default;
osc::LOGLLightingMapsTab& osc::LOGLLightingMapsTab::operator=(LOGLLightingMapsTab&&) noexcept = default;
osc::LOGLLightingMapsTab::~LOGLLightingMapsTab() noexcept = default;

UID osc::LOGLLightingMapsTab::impl_get_id() const
{
    return impl_->id();
}

CStringView osc::LOGLLightingMapsTab::impl_get_name() const
{
    return impl_->name();
}

void osc::LOGLLightingMapsTab::impl_on_mount()
{
    impl_->on_mount();
}

void osc::LOGLLightingMapsTab::impl_on_unmount()
{
    impl_->on_unmount();
}

bool osc::LOGLLightingMapsTab::impl_on_event(const Event& e)
{
    return impl_->on_event(e);
}

void osc::LOGLLightingMapsTab::impl_on_draw()
{
    impl_->on_draw();
}
