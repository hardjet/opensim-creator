#include <oscar/Graphics/AntiAliasingLevel.h>
#include <oscar/Graphics/Camera.h>
#include <oscar/Graphics/CameraClearFlags.h>
#include <oscar/Graphics/CameraProjection.h>
#include <oscar/Graphics/Color32.h>
#include <oscar/Graphics/ColorSpace.h>
#include <oscar/Graphics/Cubemap.h>
#include <oscar/Graphics/DepthStencilFormat.h>
#include <oscar/Graphics/Detail/CPUDataType.h>
#include <oscar/Graphics/Detail/CPUImageFormat.h>
#include <oscar/Graphics/Detail/ShaderPropertyTypeList.h>
#include <oscar/Graphics/Detail/ShaderPropertyTypeTraits.h>
#include <oscar/Graphics/Detail/TextureFormatList.h>
#include <oscar/Graphics/Detail/TextureFormatTraits.h>
#include <oscar/Graphics/Detail/VertexAttributeFormatHelpers.h>
#include <oscar/Graphics/Detail/VertexAttributeFormatList.h>
#include <oscar/Graphics/Detail/VertexAttributeFormatTraits.h>
#include <oscar/Graphics/Detail/VertexAttributeHelpers.h>
#include <oscar/Graphics/Detail/VertexAttributeList.h>
#include <oscar/Graphics/Geometries/PlaneGeometry.h>
#include <oscar/Graphics/Graphics.h>
#include <oscar/Graphics/GraphicsContext.h>
#include <oscar/Graphics/Material.h>
#include <oscar/Graphics/Mesh.h>
#include <oscar/Graphics/MeshFunctions.h>
#include <oscar/Graphics/MeshTopology.h>
#include <oscar/Graphics/OpenGL/CPUDataTypeOpenGLTraits.h>
#include <oscar/Graphics/OpenGL/CPUImageFormatOpenGLTraits.h>
#include <oscar/Graphics/OpenGL/Gl.h>
#include <oscar/Graphics/OpenGL/TextureFormatOpenGLTraits.h>
#include <oscar/Graphics/RenderBuffer.h>
#include <oscar/Graphics/RenderBufferLoadAction.h>
#include <oscar/Graphics/RenderBufferStoreAction.h>
#include <oscar/Graphics/RenderTarget.h>
#include <oscar/Graphics/RenderTargetColorAttachment.h>
#include <oscar/Graphics/RenderTargetDepthAttachment.h>
#include <oscar/Graphics/RenderTexture.h>
#include <oscar/Graphics/RenderTextureDescriptor.h>
#include <oscar/Graphics/RenderTextureFormat.h>
#include <oscar/Graphics/Shader.h>
#include <oscar/Graphics/ShaderPropertyType.h>
#include <oscar/Graphics/SubMeshDescriptor.h>
#include <oscar/Graphics/Texture2D.h>
#include <oscar/Graphics/TextureFilterMode.h>
#include <oscar/Graphics/TextureFormat.h>
#include <oscar/Graphics/TextureWrapMode.h>
#include <oscar/Graphics/Unorm8.h>
#include <oscar/Graphics/VertexAttribute.h>
#include <oscar/Graphics/VertexAttributeDescriptor.h>
#include <oscar/Graphics/VertexAttributeFormat.h>
#include <oscar/Graphics/VertexFormat.h>
#include <oscar/Maths/AABB.h>
#include <oscar/Maths/Angle.h>
#include <oscar/Maths/Mat3.h>
#include <oscar/Maths/Mat4.h>
#include <oscar/Maths/MatFunctions.h>
#include <oscar/Maths/MathHelpers.h>
#include <oscar/Maths/Quat.h>
#include <oscar/Maths/Transform.h>
#include <oscar/Maths/Triangle.h>
#include <oscar/Maths/TriangleFunctions.h>
#include <oscar/Maths/Vec2.h>
#include <oscar/Maths/Vec3.h>
#include <oscar/Maths/Vec4.h>
#include <oscar/Maths/VecFunctions.h>
#include <oscar/Platform/App.h>
#include <oscar/Platform/Detail/SDL2Helpers.h>
#include <oscar/Platform/Log.h>
#include <oscar/Utils/Algorithms.h>
#include <oscar/Utils/Assertions.h>
#include <oscar/Utils/Concepts.h>
#include <oscar/Utils/CStringView.h>
#include <oscar/Utils/DefaultConstructOnCopy.h>
#include <oscar/Utils/EnumHelpers.h>
#include <oscar/Utils/ObjectRepresentation.h>
#include <oscar/Utils/Perf.h>
#include <oscar/Utils/StdVariantHelpers.h>
#include <oscar/Utils/UID.h>

#include <GL/glew.h>
#include <ankerl/unordered_dense.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <iterator>
#include <ranges>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

using namespace osc::detail;
using namespace osc::literals;
using namespace osc;
namespace cpp20 = osc::cpp20;
namespace gl = osc::gl;
namespace sdl = osc::sdl;

// shader source
namespace
{
    // vertex shader source used for blitting a textured quad (common use-case)
    //
    // it's here, rather than in an external resource file, because it is eagerly
    // loaded while the graphics backend is initialized (i.e. potentially before
    // the application is fully loaded)
    constexpr CStringView c_quad_vertex_shader_src = R"(
        #version 330 core

        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec2 aTexCoord;

        out vec2 TexCoord;

        void main()
        {
            TexCoord = aTexCoord;
            gl_Position = vec4(aPos, 1.0);
        }
    )";

    // fragment shader source used for blitting a textured quad
    //
    // it's here, rather than in an external resource file, because it is eagerly
    // loaded while the graphics backend is initialized (i.e. potentially before
    // the application is fully loaded)
    constexpr CStringView c_quad_fragment_shader_src = R"(
        #version 330 core

        uniform sampler2D uTexture;

        in vec2 TexCoord;
        out vec4 FragColor;

        void main()
        {
            FragColor = texture(uTexture, TexCoord);
        }
    )";

    CStringView opengl_string_to_cstringview(const GLubyte* string_ptr)
    {
        using value_type = CStringView::value_type;

        static_assert(sizeof(GLubyte) == sizeof(value_type));
        static_assert(alignof(GLubyte) == alignof(value_type));
        if (string_ptr) {
            return CStringView{std::launder(reinterpret_cast<const value_type*>(string_ptr))};
        }
        else {
            return CStringView{};
        }
    }

    CStringView opengl_get_cstringview(GLenum name)
    {
        return opengl_string_to_cstringview(glGetString(name));
    }

    CStringView opengl_get_cstringviewi(GLenum name, GLuint index)
    {
        return opengl_string_to_cstringview(glGetStringi(name, index));
    }

    bool is_aligned_at_least(const void* ptr, GLint required_alignment)
    {
        return cpp20::bit_cast<intptr_t>(ptr) % required_alignment == 0;
    }

    // returns the name strings of all extensions that the OpenGL backend may use
    std::vector<CStringView> get_all_opengl_extensions_used_by_opengl_backend()
    {
        // most entries in this list were initially from a mixture of:
        //
        // - https://www.khronos.org/opengl/wiki/History_of_OpenGL (lists historical extension changes)
        // - Khronos official pages

        // this list isn't comprehensive, it's just things that I reakon the OpenGL backend
        // wants, so that, at runtime, the graphics backend can emit user-facing warning
        // messages so that it's a little bit easier to spot production bugs

        return {
            // framebuffer objects, blitting, multisampled renderbuffer objects, and
            // packed depth+stencil image formats
            //
            // core in OpenGL 3.0
            "GL_ARB_framebuffer_object",

            // VAOs
            //
            // core in OpenGL 3.0
            "GL_ARB_vertex_array_object",

            // GL_HALF_FLOAT as a texture pixel format (e.g. HDR textures)
            //
            // core in OpenGL 3.0
            "GL_ARB_half_float_pixel",

            // floating point color and depth internal formats for textures
            // and render buffers
            //
            // core in OpenGL 3.0
            "GL_ARB_color_buffer_float",
            "GL_ARB_texture_float",

            // hardware support for automatic sRGB/linear color conversion via
            // framebuffers and GL_FRAMEBUFFER_SRGB
            //
            // core in OpenGL 3.0
            "GL_EXT_framebuffer_sRGB",

            "GL_EXT_texture_sRGB",

            // shaders
            //
            // core in OpenGL 2.0
            "GL_ARB_shader_objects",
            "GL_ARB_vertex_shader",
            "GL_ARB_fragment_shader",

            // multi-render target (MRT) support
            //
            // core in OpenGL 2.0
            "GL_ARB_draw_buffers",

            // non-power-of-2 texture sizes
            //
            // core in OpenGL 2.0
            "GL_ARB_texture_non_power_of_two",

            // VBOs
            //
            // core in OpenGL 1.5
            "GL_ARB_vertex_buffer_object",

            // mipmap generation
            //
            // core in OpenGL 1.4
            "GL_SGIS_generate_mipmap",

            // depth textures
            //
            // core in OpenGL 1.4
            "GL_ARB_depth_texture",

            // separate blend functions (might be handy with premultiplied alpha at some point)
            //
            // core in OpenGL 1.4
            "GL_EXT_blend_func_separate",

            // mirrored repeating of textures
            //
            // core in OpenGL 1.4
            "GL_ARB_texture_mirrored_repeat",

            // cubemap support
            //
            // core in OpenGL 1.3
            "GL_ARB_texture_cube_map",

            // MSXAA support
            //
            // core in OpenGL 1.3
            "GL_ARB_multisample",

            // core in OpenGL 1.3
            "GL_ARB_texture_border_clamp",

            // core in OpenGL 1.2
            "GL_EXT_texture3D",

            // core in OpenGL 1.1
            "GL_EXT_vertex_array",
            "GL_EXT_texture_object",

            // also from OpenGL 1.1, but don't seem to be reported
            // by the NVIDIA backend?
            //
            // "GL_EXT_blend_logic_op",
            // "GL_EXT_texture",
            // "GL_EXT_copy_texture",
            // "GL_EXT_subtexture",
        };
    }

    size_t get_num_extensions_supported_by_opengl_backend()
    {
        GLint rv = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &rv);
        return rv >= 0 ? static_cast<size_t>(rv) : 0;
    }

    std::vector<CStringView> get_extensions_supported_by_opengl_backend()
    {
        const size_t num_extensions = get_num_extensions_supported_by_opengl_backend();

        std::vector<CStringView> rv;
        rv.reserve(num_extensions);
        for (size_t i = 0; i < num_extensions; ++i) {
            rv.emplace_back(opengl_get_cstringviewi(GL_EXTENSIONS, static_cast<GLuint>(i)));
        }
        return rv;
    }

    void validate_opengl_backend_extension_support(LogLevel logging_level)
    {
        // note: the OpenGL specification _requires_ that a backend supports
        // (effectively) RGBA, RG, and RED textures with the following data
        // formats for each channel:
        //
        // - uint8 (normalized)
        // - int8 (normalized)
        // - float32
        // - uint8/uint16/uint32 (non-normalized)
        // - int8/int16/int32 (non-normalized)
        //
        // see "Required Formats" in: https://www.khronos.org/opengl/wiki/Image_Format

        if (logging_level < log_level()) {
            return;
        }

        std::vector<CStringView> extensions_needed = get_all_opengl_extensions_used_by_opengl_backend();
        std::sort(extensions_needed.begin(), extensions_needed.end());

        std::vector<CStringView> extensions_available = get_extensions_supported_by_opengl_backend();
        std::sort(extensions_available.begin(), extensions_available.end());

        std::vector<CStringView> extensions_missing;
        extensions_missing.reserve(extensions_needed.size());  // pessimistic guess
        std::set_difference(
            extensions_needed.begin(),
            extensions_needed.end(),
            extensions_available.begin(),
            extensions_available.end(),
            std::back_inserter(extensions_missing)
        );

        if (not extensions_missing.empty()) {
            log_message(logging_level, "OpenGL: the following OpenGL extensions may be missing from the graphics backend: ");
            for (const auto& extension : extensions_missing) {
                log_message(logging_level, "OpenGL:  - %s", extension.c_str());
            }
            log_message(logging_level, "OpenGL: because extensions may be missing, rendering may behave abnormally");
            log_message(logging_level, "OpenGL: note: some graphics engines can mis-report an extension as missing");
        }

        log_message(logging_level, "OpenGL: here is a list of all of the extensions supported by the graphics backend:");
        for (const auto& extension : extensions_available) {
            log_message(logging_level, "OpenGL:  - %s", extension.c_str());
        }
    }
}


// generic utility functions
namespace
{
    template<BitCastable T>
    void push_as_bytes(const T& v, std::vector<uint8_t>& out)
    {
        const auto bytes = view_object_representation<uint8_t>(v);
        out.insert(out.end(), bytes.begin(), bytes.end());
    }

    template<typename VecOrMat>
    requires BitCastable<typename VecOrMat::element_type>
    std::span<const typename VecOrMat::element_type> to_float_span(const VecOrMat& v)
    {
        return {value_ptr(v), sizeof(VecOrMat)/sizeof(typename VecOrMat::element_type)};
    }
}

// material value storage
//
// materials can store a variety of stuff (colors, positions, offsets, textures, etc.). This
// code defines how it's actually stored at runtime
namespace
{
    using MaterialValue = std::variant<
        Color,
        std::vector<Color>,
        float,
        std::vector<float>,
        Vec2,
        Vec3,
        std::vector<Vec3>,
        Vec4,
        Mat3,
        Mat4,
        std::vector<Mat4>,
        int32_t,
        bool,
        Texture2D,
        RenderTexture,
        Cubemap
    >;

    ShaderPropertyType get_shader_type(const MaterialValue& material_val)
    {
        switch (material_val.index()) {
        case variant_index<MaterialValue, Color>():
        case variant_index<MaterialValue, std::vector<Color>>():
            return ShaderPropertyType::Vec4;
        case variant_index<MaterialValue, Vec2>():
            return ShaderPropertyType::Vec2;
        case variant_index<MaterialValue, float>():
        case variant_index<MaterialValue, std::vector<float>>():
            return ShaderPropertyType::Float;
        case variant_index<MaterialValue, Vec3>():
        case variant_index<MaterialValue, std::vector<Vec3>>():
            return ShaderPropertyType::Vec3;
        case variant_index<MaterialValue, Vec4>():
            return ShaderPropertyType::Vec4;
        case variant_index<MaterialValue, Mat3>():
            return ShaderPropertyType::Mat3;
        case variant_index<MaterialValue, Mat4>():
        case variant_index<MaterialValue, std::vector<Mat4>>():
            return ShaderPropertyType::Mat4;
        case variant_index<MaterialValue, int32_t>():
            return ShaderPropertyType::Int;
        case variant_index<MaterialValue, bool>():
            return ShaderPropertyType::Bool;
        case variant_index<MaterialValue, Texture2D>():
            return ShaderPropertyType::Sampler2D;
        case variant_index<MaterialValue, RenderTexture>(): {

            static_assert(num_options<TextureDimensionality>() == 2);
            return std::get<RenderTexture>(material_val).getDimensionality() == TextureDimensionality::Tex2D ?
                ShaderPropertyType::Sampler2D :
                ShaderPropertyType::SamplerCube;
        }
        case variant_index<MaterialValue, Cubemap>():
            return ShaderPropertyType::SamplerCube;
        default:
            return ShaderPropertyType::Unknown;
        }
    }
}

// shader (backend stuff)
namespace
{
    // convert a GL shader type to an internal shader type
    ShaderPropertyType opengl_shader_type_to_osc_shader_type(GLenum e)
    {
        static_assert(num_options<ShaderPropertyType>() == 11);

        switch (e) {
        case GL_FLOAT:        return ShaderPropertyType::Float;
        case GL_FLOAT_VEC2:   return ShaderPropertyType::Vec2;
        case GL_FLOAT_VEC3:   return ShaderPropertyType::Vec3;
        case GL_FLOAT_VEC4:   return ShaderPropertyType::Vec4;
        case GL_FLOAT_MAT3:   return ShaderPropertyType::Mat3;
        case GL_FLOAT_MAT4:   return ShaderPropertyType::Mat4;
        case GL_INT:          return ShaderPropertyType::Int;
        case GL_BOOL:         return ShaderPropertyType::Bool;
        case GL_SAMPLER_2D:   return ShaderPropertyType::Sampler2D;
        case GL_SAMPLER_CUBE: return ShaderPropertyType::SamplerCube;

        case GL_INT_VEC2:
        case GL_INT_VEC3:
        case GL_INT_VEC4:
        case GL_UNSIGNED_INT:
        case GL_UNSIGNED_INT_VEC2:
        case GL_UNSIGNED_INT_VEC3:
        case GL_UNSIGNED_INT_VEC4:
        case GL_DOUBLE:
        case GL_DOUBLE_VEC2:
        case GL_DOUBLE_VEC3:
        case GL_DOUBLE_VEC4:
        case GL_DOUBLE_MAT2:
        case GL_DOUBLE_MAT3:
        case GL_DOUBLE_MAT4:
        case GL_DOUBLE_MAT2x3:
        case GL_DOUBLE_MAT2x4:
        case GL_FLOAT_MAT2x3:
        case GL_FLOAT_MAT2x4:
        case GL_FLOAT_MAT3x2:
        case GL_FLOAT_MAT3x4:
        case GL_FLOAT_MAT4x2:
        case GL_FLOAT_MAT4x3:
        case GL_FLOAT_MAT2:
        default:
            return ShaderPropertyType::Unknown;
        }
    }

    std::string normalize_shader_element_name(std::string_view opengl_name)
    {
        std::string s{opengl_name};
        auto loc = s.find('[');
        if (loc != std::string::npos) {
            s.erase(loc);
        }
        return s;
    }

    // parsed-out description of a shader "element" (uniform/attribute)
    struct ShaderElement final {
        ShaderElement(
            int32_t location_,
            ShaderPropertyType shader_type_,
            int32_t size_) :

            location{location_},
            shader_type{shader_type_},
            size{size_}
        {}

        int32_t location;
        ShaderPropertyType shader_type;
        int32_t size;
    };

    void print_shader_element(std::ostream& o, std::string_view name, const ShaderElement& se)
    {
        o << "ShadeElement(name = " << name << ", location = " << se.location << ", shader_type = " << se.shader_type << ", size = " << se.size << ')';
    }

    // see: ankerl/unordered_dense documentation for heterogeneous lookups
    struct transparent_string_hash final {
        using is_transparent = void;
        using is_avalanching = void;

        [[nodiscard]] auto operator()(std::string_view str) const -> uint64_t {
            return ankerl::unordered_dense::hash<std::string_view>{}(str);
        }
    };

    template<typename Value>
    using FastStringHashtable = ankerl::unordered_dense::map<
        std::string,
        Value,
        transparent_string_hash,
        std::equal_to<>
    >;
}

namespace
{
    // transform storage: either as a matrix or a transform
    //
    // calling code is allowed to submit transforms as either a `Transform` (preferred) or a
    // `Mat4` (can be handier)
    //
    // these need to be stored as-is, because that's the smallest possible representation and
    // the drawing algorithm needs to traverse + sort the render objects at runtime (so size
    // is important)
    using Mat4OrTransform = std::variant<Mat4, Transform>;

    Mat4 mat4_cast(const Mat4OrTransform& mat4_or_transform)
    {
        return std::visit(Overload{
            [](const Mat4& mat4) { return mat4; },
            [](const Transform& transform) { return mat4_cast(transform); }
        }, mat4_or_transform);
    }

    Mat4 normal_matrix4(const Mat4OrTransform& mat4_or_transform)
    {
        return std::visit(Overload{
            [](const Mat4& mat4) { return normal_matrix4(mat4); },
            [](const Transform& transform) { return normal_matrix_4x4(transform); }
        }, mat4_or_transform);
    }

    Mat3 normal_matrix(const Mat4OrTransform& mat4_or_transform)
    {
        return std::visit(Overload{
            [](const Mat4& mat4) { return normal_matrix(mat4); },
            [](const Transform& transform) { return normal_matrix(transform); }
        }, mat4_or_transform);
    }

    // this is what is stored in the renderer's render queue
    struct RenderObject final {

        RenderObject(
            Mesh mesh_,
            const Transform& transform_,
            Material material_,
            std::optional<MaterialPropertyBlock> maybe_prop_block_,
            std::optional<size_t> maybe_submesh_index_) :

            material{std::move(material_)},
            mesh{std::move(mesh_)},
            maybe_prop_block{std::move(maybe_prop_block_)},
            transform{transform_},
            world_centroid{material.getTransparent() ? transform_point(transform_, centroid_of(mesh.getBounds())) : Vec3{}},
            maybe_submesh_index{maybe_submesh_index_}
        {}

        RenderObject(
            Mesh mesh_,
            const Mat4& transform_,
            Material material_,
            std::optional<MaterialPropertyBlock> maybe_prop_block_,
            std::optional<size_t> maybe_submesh_index_) :

            material{std::move(material_)},
            mesh{std::move(mesh_)},
            maybe_prop_block{std::move(maybe_prop_block_)},
            transform{transform_},
            world_centroid{material.getTransparent() ? transform_ * Vec4{centroid_of(mesh.getBounds()), 1.0f} : Vec3{}},
            maybe_submesh_index{maybe_submesh_index_}
        {}

        friend void swap(RenderObject& a, RenderObject& b) noexcept
        {
            using std::swap;

            swap(a.material, b.material);
            swap(a.mesh, b.mesh);
            swap(a.maybe_prop_block, b.maybe_prop_block);
            swap(a.transform, b.transform);
            swap(a.world_centroid, b.world_centroid);
            swap(a.maybe_submesh_index, b.maybe_submesh_index);
        }

        friend bool operator==(const RenderObject&, const RenderObject&) = default;

        Material material;
        Mesh mesh;
        std::optional<MaterialPropertyBlock> maybe_prop_block;
        Mat4OrTransform transform;
        Vec3 world_centroid;
        std::optional<size_t> maybe_submesh_index;
    };

    // returns true if the render object is opaque
    bool is_opaque(const RenderObject& ro)
    {
        return !ro.material.getTransparent();
    }

    bool is_depth_tested(const RenderObject& ro)
    {
        return ro.material.getDepthTested();
    }

    Mat4 model_mat4(const RenderObject& ro)
    {
        return mat4_cast(ro.transform);
    }

    Mat3 normal_matrix(const RenderObject& ro)
    {
        return normal_matrix(ro.transform);
    }

    Mat4 normal_matrix4(const RenderObject& ro)
    {
        return normal_matrix4(ro.transform);
    }

    Vec3 const& worldspace_centroid(const RenderObject& ro)
    {
        return ro.world_centroid;
    }

    // function object that returns true if the first argument is farther from the second
    //
    // (handy for depth sorting)
    class RenderObjectIsFartherFrom final {
    public:
        explicit RenderObjectIsFartherFrom(const Vec3& pos) : pos_{pos} {}

        bool operator()(const RenderObject& a, const RenderObject& b) const
        {
            const Vec3 centroid_a = worldspace_centroid(a);
            const Vec3 centroid_b = worldspace_centroid(b);
            const Vec3 pos_to_a = centroid_a - pos_;
            const Vec3 pos_to_b = centroid_b - pos_;
            return length2(pos_to_a) > length2(pos_to_b);
        }
    private:
        Vec3 pos_;
    };

    class RenderObjectHasMaterial final {
    public:
        explicit RenderObjectHasMaterial(const Material* material) :
            material_{material}
        {
            OSC_ASSERT(material_ != nullptr);
        }

        bool operator()(const RenderObject& ro) const
        {
            return ro.material == *material_;
        }
    private:
        const Material* material_;
    };

    class RenderObjectHasMaterialPropertyBlock final {
    public:
        explicit RenderObjectHasMaterialPropertyBlock(const std::optional<MaterialPropertyBlock>* mpb) :
            mpb_{mpb}
        {
            OSC_ASSERT(mpb_ != nullptr);
        }

        bool operator()(const RenderObject& ro) const
        {
            return ro.maybe_prop_block == *mpb_;
        }

    private:
        const std::optional<MaterialPropertyBlock>* mpb_;
    };

    class RenderObjectHasMesh final {
    public:
        explicit RenderObjectHasMesh(const Mesh* mesh) :
            mesh_{mesh}
        {
            OSC_ASSERT(mesh_ != nullptr);
        }

        bool operator()(const RenderObject& ro) const
        {
            return ro.mesh == *mesh_;
        }
    private:
        const Mesh* mesh_;
    };

    class RenderObjectHasSubMeshIndex final {
    public:
        explicit RenderObjectHasSubMeshIndex(std::optional<size_t> maybe_submesh_index) :
            maybe_submesh_index_{maybe_submesh_index}
        {}

        bool operator()(const RenderObject& ro) const
        {
            return ro.maybe_submesh_index == maybe_submesh_index_;
        }
    private:
        std::optional<size_t> maybe_submesh_index_;
    };

    // sort a sequence of `RenderObject`s for optimal drawing
    std::vector<RenderObject>::iterator sort_render_queue(
        std::vector<RenderObject>::iterator queue_begin,
        std::vector<RenderObject>::iterator queue_end,
        Vec3 camera_pos)
    {
        // partition the render queue into `[opaque_objs | transparent_objs]`
        const auto opaque_objs_end = std::partition(queue_begin, queue_end, is_opaque);

        // optimize the `opaque_objs` partition (it can be reordered safely)
        //
        // first, batch `opaque_objs` into `RenderObject`s that have the same `Material`
        auto material_batch_start = queue_begin;
        while (material_batch_start != opaque_objs_end) {

            const auto material_batch_end = std::partition(
                material_batch_start,
                opaque_objs_end,
                RenderObjectHasMaterial{&material_batch_start->material}
            );

            // second, batch `RenderObject`s with the same `Material` into sub-batches
            // with the same `MaterialPropertyBlock`
            auto props_batch_start = material_batch_start;
            while (props_batch_start != material_batch_end) {

                const auto props_batch_end = std::partition(
                    props_batch_start,
                    material_batch_end,
                    RenderObjectHasMaterialPropertyBlock{&props_batch_start->maybe_prop_block}
                );

                // third, batch `RenderObject`s with the same `Material` and `MaterialPropertyBlock`s
                // into sub-batches with the same `Mesh`
                auto mesh_batch_start = props_batch_start;
                while (mesh_batch_start != props_batch_end) {

                    const auto mesh_batch_end = std::partition(
                        mesh_batch_start,
                        props_batch_end,
                        RenderObjectHasMesh{&mesh_batch_start->mesh}
                    );

                    // fourth, batch `RenderObject`s with the same `Material`, `MaterialPropertyBlock`,
                    // and `Mesh` into sub-batches with the same sub-mesh index
                    auto submesh_batch_start = mesh_batch_start;
                    while (submesh_batch_start != mesh_batch_end) {

                        const auto submesh_batch_end = std::partition(
                            submesh_batch_start,
                            mesh_batch_end,
                            RenderObjectHasSubMeshIndex{submesh_batch_start->maybe_submesh_index}
                        );

                        submesh_batch_start = submesh_batch_end;
                    }
                    mesh_batch_start = mesh_batch_end;
                }
                props_batch_start = props_batch_end;
            }
            material_batch_start = material_batch_end;
        }

        // sort the transparent partition by distance from camera (back-to-front)
        std::sort(opaque_objs_end, queue_end, RenderObjectIsFartherFrom{camera_pos});

        return opaque_objs_end;
    }

    // top-level state for a single call to `render`
    struct RenderPassState final {

        RenderPassState(
            const Vec3& camera_pos_,
            const Mat4& view_matrix_,
            const Mat4& projection_matrix_) :

            camera_pos{camera_pos_},
            view_matrix{view_matrix_},
            projection_matrix{projection_matrix_}
        {}

        Vec3 camera_pos;
        Mat4 view_matrix;
        Mat4 projection_matrix;
        Mat4 view_projection_matrix = projection_matrix * view_matrix;
    };

    // the OpenGL data associated with a `Texture2D`
    struct Texture2DOpenGLData final {
        gl::Texture2D texture;
        UID texture_params_version;
    };


    // the OpenGL data associated with a `RenderBuffer`
    struct SingleSampledTexture final {
        gl::Texture2D texture2D;
    };
    struct MultisampledRBOAndResolvedTexture final {
        gl::RenderBuffer multisampled_rbo;
        gl::Texture2D single_sampled_texture2D;
    };
    struct SingleSampledCubemap final {
        gl::TextureCubemap cubemap;
    };
    using RenderBufferOpenGLData = std::variant<
        SingleSampledTexture,
        MultisampledRBOAndResolvedTexture,
        SingleSampledCubemap
    >;

    // the OpenGL data associated with a `Mesh`
    struct MeshOpenGLData final {
        UID data_version;
        gl::TypedBufferHandle<GL_ARRAY_BUFFER> array_buffer;
        gl::TypedBufferHandle<GL_ELEMENT_ARRAY_BUFFER> indices_buffer;
        gl::VertexArray vao;
    };

    struct InstancingState final {
        InstancingState(
            gl::ArrayBuffer<float, GL_STREAM_DRAW>& buf_,
            size_t stride_) :

            buffer{buf_},
            stride{stride_}
        {}

        gl::ArrayBuffer<float, GL_STREAM_DRAW>& buffer;
        size_t stride = 0;
        size_t base_offset = 0;
    };
}

namespace osc
{
    class GraphicsBackend final {
    public:
        // internal methods

        static void bind_to_instanced_attributes(
            const Shader::Impl&,
            InstancingState&
        );

        static void unbind_from_instanced_attributes(
            const Shader::Impl&,
            InstancingState&
        );

        static std::optional<InstancingState> upload_instance_data(
            std::span<const RenderObject>,
            const Shader::Impl&
        );

        static void try_bind_material_value_to_shader_element(
            const ShaderElement&,
            const MaterialValue&,
            int32_t& texture_slot
        );

        static void handle_batch_with_same_submesh(
            std::span<const RenderObject>,
            std::optional<InstancingState>& instancing_state
        );

        static void handle_batch_with_same_mesh(
            std::span<const RenderObject>,
            std::optional<InstancingState>& instancing_state
        );

        static void handle_batch_with_same_material_property_block(
            std::span<const RenderObject>,
            int32_t& texture_slot,
            std::optional<InstancingState>& instancing_state
        );

        static void handle_batch_with_same_material(
            const RenderPassState&,
            std::span<const RenderObject>
        );

        static void draw_render_objects(
            const RenderPassState&,
            std::span<const RenderObject>
        );

        static void draw_batched_by_opaqueness(
            const RenderPassState&,
            std::span<const RenderObject>
        );

        static void validate_render_target(
            RenderTarget&
        );
        static Rect calc_viewport_bounds(
            Camera::Impl&,
            RenderTarget* maybe_custom_render_target
        );
        static Rect setup_top_level_pipeline_state(
            Camera::Impl&,
            RenderTarget* maybe_custom_render_target
        );
        static std::optional<gl::FrameBuffer> bind_and_clear_render_buffers(
            Camera::Impl&,
            RenderTarget* maybe_custom_render_target
        );
        static void resolve_render_buffers(
            RenderTarget& maybe_custom_render_target
        );
        static void flush_render_queue(
            Camera::Impl& camera,
            float aspect_ratio
        );
        static void teardown_top_level_pipeline_state(
            Camera::Impl&,
            RenderTarget* maybe_custom_render_target
        );
        static void render_camera_queue(
            Camera::Impl& camera,
            RenderTarget* maybe_custom_render_target = nullptr
        );


        // public (forwarded) API

        static void draw(
            const Mesh&,
            const Transform&,
            const Material&,
            Camera&,
            const std::optional<MaterialPropertyBlock>&,
            std::optional<size_t>
        );

        static void draw(
            const Mesh&,
            const Mat4&,
            const Material&,
            Camera&,
            const std::optional<MaterialPropertyBlock>&,
            std::optional<size_t>
        );

        static void blit(
            const Texture2D&,
            RenderTexture&
        );

        static void blit_to_screen(
            const RenderTexture&,
            const Rect&,
            BlitFlags
        );

        static void blit_to_screen(
            const RenderTexture&,
            const Rect&,
            const Material&,
            BlitFlags
        );

        static void blit_to_screen(
            const Texture2D&,
            const Rect&
        );

        static void copy_texture(
            const RenderTexture&,
            Texture2D&
        );

        static void copy_texture(
            const RenderTexture&,
            Texture2D&,
            CubemapFace
        );

        static void copy_texture(
            const RenderTexture&,
            Cubemap&,
            size_t
        );
    };
}

namespace
{
    // returns the memory alignment of data that is to be copied from the
    // CPU (packed) to the GPU (unpacked)
    constexpr GLint opengl_unpack_alignment_of(TextureFormat texture_format)
    {
        constexpr auto lut = []<TextureFormat... Formats>(OptionList<TextureFormat, Formats...>)
        {
            return std::to_array({ TextureFormatOpenGLTraits<Formats>::unpack_alignment... });
        }(TextureFormatList{});

        return lut.at(to_index(texture_format));
    }

    // returns the format OpenGL will use internally (i.e. on the GPU) to
    // represent the given format+colorspace combo
    constexpr GLenum opengl_internal_format_of(TextureFormat texture_format, ColorSpace color_space)
    {
        constexpr auto srgb_lut = []<TextureFormat... Formats>(OptionList<TextureFormat, Formats...>)
        {
            return std::to_array({ TextureFormatOpenGLTraits<Formats>::internal_format_srgb... });
        }(TextureFormatList{});

        constexpr auto linear_lut = []<TextureFormat... Formats>(OptionList<TextureFormat, Formats...>)
        {
            return std::to_array({ TextureFormatOpenGLTraits<Formats>::internal_format_linear... });
        }(TextureFormatList{});

        static_assert(num_options<ColorSpace>() == 2);
        if (color_space == ColorSpace::sRGB) {
            return srgb_lut.at(to_index(texture_format));
        }
        else {
            return linear_lut.at(to_index(texture_format));
        }
    }

    constexpr GLenum opengl_data_type_of(CPUDataType cpu_datatype)
    {
        constexpr auto lut = []<CPUDataType... DataTypes>(OptionList<CPUDataType, DataTypes...>)
        {
            return std::to_array({ CPUDataTypeOpenGLTraits<DataTypes>::opengl_data_type... });
        }(CPUDataTypeList{});

        return lut.at(to_index(cpu_datatype));
    }

    constexpr CPUDataType equivalent_cpu_datatype_of(TextureFormat texture_format)
    {
        constexpr auto lut = []<TextureFormat... Formats>(OptionList<TextureFormat, Formats...>)
        {
            return std::to_array({ TextureFormatTraits<Formats>::equivalent_cpu_datatype... });
        }(TextureFormatList{});

        return lut.at(to_index(texture_format));
    }

    constexpr CPUImageFormat equivalent_cpu_image_format_of(TextureFormat texture_format)
    {
        constexpr auto lut = []<TextureFormat... Formats>(OptionList<TextureFormat, Formats...>)
        {
            return std::to_array({ TextureFormatTraits<Formats>::equivalent_cpu_image_format... });
        }(TextureFormatList{});

        return lut.at(to_index(texture_format));
    }

    constexpr GLenum opengl_format_of(CPUImageFormat cpu_format)
    {
        constexpr auto lut = []<CPUImageFormat... Formats>(OptionList<CPUImageFormat, Formats...>)
        {
            return std::to_array({ CPUImageFormatOpenGLTraits<Formats>::opengl_format... });
        }(CPUImageFormatList{});

        return lut.at(to_index(cpu_format));
    }

    constexpr GLenum to_opengl_texture_cubemap_enum(CubemapFace cubemap_face)
    {
        static_assert(num_options<CubemapFace>() == 6);
        static_assert(static_cast<GLenum>(CubemapFace::PositiveX) == 0);
        static_assert(static_cast<GLenum>(CubemapFace::NegativeZ) == 5);
        static_assert(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z - GL_TEXTURE_CUBE_MAP_POSITIVE_X == 5);

        return GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(cubemap_face);
    }

    GLint to_opengl_texturewrap_enum(TextureWrapMode texture_wrap_mode)
    {
        static_assert(num_options<TextureWrapMode>() == 3);

        switch (texture_wrap_mode) {
        case TextureWrapMode::Repeat:
            return GL_REPEAT;
        case TextureWrapMode::Clamp:
            return GL_CLAMP_TO_EDGE;
        case TextureWrapMode::Mirror:
            return GL_MIRRORED_REPEAT;
        default:
            return GL_REPEAT;
        }
    }

    constexpr auto c_texture_wrap_mode_strings = std::to_array<CStringView>(
    {
        "Repeat",
        "Clamp",
        "Mirror",
    });
    static_assert(c_texture_wrap_mode_strings.size() == num_options<TextureWrapMode>());

    constexpr auto c_texture_filter_mode_strings = std::to_array<CStringView>(
    {
        "Nearest",
        "Linear",
        "Mipmap",
    });
    static_assert(c_texture_filter_mode_strings.size() == num_options<TextureFilterMode>());

    GLint to_opengl_texture_min_filter_param(TextureFilterMode texture_filter_mode)
    {
        static_assert(num_options<TextureFilterMode>() == 3);

        switch (texture_filter_mode) {
        case TextureFilterMode::Nearest: return GL_NEAREST;
        case TextureFilterMode::Linear:  return GL_LINEAR;
        case TextureFilterMode::Mipmap:  return GL_LINEAR_MIPMAP_LINEAR;
        default:                         return GL_LINEAR;
        }
    }

    GLint to_opengl_texture_mag_filter_param(TextureFilterMode texture_filter_mode)
    {
        static_assert(num_options<TextureFilterMode>() == 3);

        switch (texture_filter_mode) {
        case TextureFilterMode::Nearest: return GL_NEAREST;
        case TextureFilterMode::Linear:  return GL_LINEAR;
        case TextureFilterMode::Mipmap:  return GL_LINEAR;
        default:                         return GL_LINEAR;
        }
    }
}

namespace
{
    // the OpenGL data associated with a `Texture2D`
    struct CubemapOpenGLData final {
        gl::TextureCubemap texture;
        UID source_data_version;
        UID source_params_version;
    };
}

class osc::Cubemap::Impl final {
public:
    Impl(int32_t width, TextureFormat format) :
        width_{width},
        format_{format}
    {
        OSC_ASSERT(width_ > 0 && "the width of a cubemap must be a positive number");

        const size_t num_bytes_per_pixel = num_bytes_per_pixel_in(format_);
        const size_t num_pixels_per_face = static_cast<size_t>(width_*width_);
        const size_t num_bytes_per_face = num_bytes_per_pixel * num_pixels_per_face;
        data_.resize(num_options<CubemapFace>() * num_bytes_per_face);
    }

    int32_t width() const
    {
        return width_;
    }

    TextureFormat texture_format() const
    {
        return format_;
    }

    TextureWrapMode wrap_mode() const
    {
        return wrap_mode_u_;
    }

    void set_wrap_mode(TextureWrapMode wrap_mode)
    {
        wrap_mode_u_ = wrap_mode;
        wrap_mode_v_ = wrap_mode;
        wrap_mode_w_ = wrap_mode;
        texture_params_version_.reset();
    }

    TextureWrapMode get_wrap_mode_u() const
    {
        return wrap_mode_u_;
    }

    void set_wrap_mode_u(TextureWrapMode wrap_mode_u)
    {
        wrap_mode_u_ = wrap_mode_u;
        texture_params_version_.reset();
    }

    TextureWrapMode get_wrap_mode_v() const
    {
        return wrap_mode_v_;
    }

    void set_wrap_mode_v(TextureWrapMode wrap_mode_v)
    {
        wrap_mode_v_ = wrap_mode_v;
        texture_params_version_.reset();
    }

    TextureWrapMode wrap_mode_w() const
    {
        return wrap_mode_w_;
    }

    void set_wrap_mode_w(TextureWrapMode wrap_mode_w)
    {
        wrap_mode_w_ = wrap_mode_w;
        texture_params_version_.reset();
    }

    TextureFilterMode filter_mode() const
    {
        return filter_mode_;
    }

    void set_filter_mode(TextureFilterMode filter_mode)
    {
        filter_mode_ = filter_mode;
        texture_params_version_.reset();
    }

    void set_pixel_data(CubemapFace face, std::span<const uint8_t> data)
    {
        const size_t face_index = to_index(face);
        const auto num_pixels_per_face = static_cast<size_t>(width_) * static_cast<size_t>(width_);
        const size_t num_bytes_per_face = num_pixels_per_face * num_bytes_per_pixel_in(format_);
        const size_t destination_data_begin = face_index * num_bytes_per_face;
        const size_t destination_data_end = destination_data_begin + num_bytes_per_face;

        OSC_ASSERT(face_index < num_options<CubemapFace>() && "invalid cubemap face passed to Cubemap::set_pixel_data");
        OSC_ASSERT(data.size() == num_bytes_per_face && "incorrect amount of data passed to Cubemap::set_pixel_data: the data must match the dimensions and texture format of the cubemap");
        OSC_ASSERT(destination_data_end <= data_.size() && "out of range assignment detected: this should be handled in the constructor");

        copy(data, data_.begin() + destination_data_begin);
        data_version_.reset();
    }

    gl::TextureCubemap& upd_cubemap()
    {
        if (not *maybe_gpu_texture_) {
            *maybe_gpu_texture_ = CubemapOpenGLData{};
        }

        CubemapOpenGLData& opengl_data = **maybe_gpu_texture_;

        if (opengl_data.source_data_version != data_version_) {
            upload_to_gpu(opengl_data);
        }

        if (opengl_data.source_params_version != texture_params_version_) {
            update_opengl_texture_params(opengl_data);
        }

        return opengl_data.texture;
    }
private:
    void upload_to_gpu(CubemapOpenGLData& opengl_data)
    {
        // calculate CPU-to-GPU data transfer parameters
        const size_t num_bytes_per_pixel = num_bytes_per_pixel_in(format_);
        const size_t num_bytes_per_row = width_ * num_bytes_per_pixel;
        const size_t num_bytes_per_face = width_ * num_bytes_per_row;
        const size_t num_bytes_in_cubemap = num_options<CubemapFace>() * num_bytes_per_face;
        const CPUDataType cpu_data_type = equivalent_cpu_datatype_of(format_);  // TextureFormat's datatype == CPU format's datatype for cubemaps
        const CPUImageFormat cpu_channel_layout = equivalent_cpu_image_format_of(format_);  // TextureFormat's layout == CPU formats's layout for cubemaps
        const GLint opengl_unpack_alignment = opengl_unpack_alignment_of(format_);

        // sanity-check before doing anything with OpenGL
        OSC_ASSERT(num_bytes_per_row % opengl_unpack_alignment == 0 && "the memory alignment of each horizontal line in an OpenGL texture must match the GL_UNPACK_ALIGNMENT arg (see: https://www.khronos.org/opengl/wiki/Common_Mistakes)");
        OSC_ASSERT(is_aligned_at_least(data_.data(), opengl_unpack_alignment) && "the memory alignment of the supplied pixel memory must match the GL_UNPACK_ALIGNMENT arg (see: https://www.khronos.org/opengl/wiki/Common_Mistakes)");
        OSC_ASSERT(num_bytes_in_cubemap <= data_.size() && "the number of bytes in the cubemap (CPU-side) is less than expected: this is an implementation bug that should be reported");
        static_assert(num_options<TextureFormat>() == 7, "careful here, glTexImage2D will not accept some formats (e.g. GL_RGBA16F) as the externally-provided format (must be GL_RGBA format with GL_HALF_FLOAT type)");

        // upload cubemap to GPU
        static_assert(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z - GL_TEXTURE_CUBE_MAP_POSITIVE_X == 5);
        gl::bind_texture(opengl_data.texture);
        gl::pixel_store_i(GL_UNPACK_ALIGNMENT, opengl_unpack_alignment);
        for (GLint face_index = 0; face_index < static_cast<GLint>(num_options<CubemapFace>()); ++face_index) {

            const size_t face_bytes_begin = face_index * num_bytes_per_face;

            gl::tex_image2D(
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_index,
                0,
                opengl_internal_format_of(format_, ColorSpace::sRGB),  // cubemaps are always sRGB
                width_,
                width_,
                0,
                opengl_format_of(cpu_channel_layout),
                opengl_data_type_of(cpu_data_type),
                data_.data() + face_bytes_begin
            );
        }

        // generate mips (care: they can be uploaded to with graphics::copy_texture)
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

        gl::bind_texture();

        opengl_data.source_data_version = data_version_;
    }

    void update_opengl_texture_params(CubemapOpenGLData& opengl_data)
    {
        gl::bind_texture(opengl_data.texture);

        // set texture parameters
        gl::tex_parameter_i(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, to_opengl_texture_mag_filter_param(filter_mode_));
        gl::tex_parameter_i(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, to_opengl_texture_min_filter_param(filter_mode_));
        gl::tex_parameter_i(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, to_opengl_texturewrap_enum(wrap_mode_u_));
        gl::tex_parameter_i(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, to_opengl_texturewrap_enum(wrap_mode_v_));
        gl::tex_parameter_i(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, to_opengl_texturewrap_enum(wrap_mode_w_));

        // cleanup OpenGL binding state
        gl::bind_texture();

        opengl_data.source_params_version = texture_params_version_;
    }

    int32_t width_;
    TextureFormat format_;
    std::vector<uint8_t> data_;
    UID data_version_;

    TextureWrapMode wrap_mode_u_ = TextureWrapMode::Repeat;
    TextureWrapMode wrap_mode_v_ = TextureWrapMode::Repeat;
    TextureWrapMode wrap_mode_w_ = TextureWrapMode::Repeat;
    TextureFilterMode filter_mode_ = TextureFilterMode::Mipmap;
    UID texture_params_version_;

    DefaultConstructOnCopy<std::optional<CubemapOpenGLData>> maybe_gpu_texture_;
};

osc::Cubemap::Cubemap(int32_t width, TextureFormat format) :
    impl_{make_cow<Impl>(width, format)}
{}

int32_t osc::Cubemap::width() const
{
    return impl_->width();
}

TextureWrapMode osc::Cubemap::wrap_mode() const
{
    return impl_->wrap_mode();
}

void osc::Cubemap::set_wrap_mode(TextureWrapMode wm)
{
    impl_.upd()->set_wrap_mode(wm);
}

TextureWrapMode osc::Cubemap::wrap_mode_u() const
{
    return impl_->get_wrap_mode_u();
}

void osc::Cubemap::set_wrap_mode_u(TextureWrapMode wm)
{
    impl_.upd()->set_wrap_mode_u(wm);
}

TextureWrapMode osc::Cubemap::wrap_mode_v() const
{
    return impl_->get_wrap_mode_v();
}

void osc::Cubemap::set_wrap_mode_v(TextureWrapMode wm)
{
    impl_.upd()->set_wrap_mode_v(wm);
}

TextureWrapMode osc::Cubemap::wrap_mode_w() const
{
    return impl_->wrap_mode_w();
}

void osc::Cubemap::set_wrap_mode_w(TextureWrapMode wm)
{
    impl_.upd()->set_wrap_mode_w(wm);
}

TextureFilterMode osc::Cubemap::filter_mode() const
{
    return impl_->filter_mode();
}

void osc::Cubemap::set_filter_mode(TextureFilterMode fm)
{
    impl_.upd()->set_filter_mode(fm);
}

TextureFormat osc::Cubemap::texture_format() const
{
    return impl_->texture_format();
}

void osc::Cubemap::set_pixel_data(CubemapFace face, std::span<const uint8_t> channelsRowByRow)
{
    impl_.upd()->set_pixel_data(face, channelsRowByRow);
}

namespace
{
    std::vector<Color> convert_pixel_bytes_to_color(
        std::span<const uint8_t> pixel_bytes,
        TextureFormat pixel_format)
    {
        const TextureChannelFormat channel_format = channel_format_of(pixel_format);

        const size_t num_channels = num_channels_in(pixel_format);
        const size_t num_bytes_per_channel = num_bytes_per_channel_in(channel_format);
        const size_t num_bytes_per_pixel = num_bytes_per_channel * num_channels;
        const size_t num_pixels = pixel_bytes.size() / num_bytes_per_pixel;

        OSC_ASSERT(pixel_bytes.size() % num_bytes_per_pixel == 0);

        std::vector<Color> rv;
        rv.reserve(num_pixels);

        static_assert(num_options<TextureChannelFormat>() == 2);
        if (channel_format == TextureChannelFormat::Uint8) {

            // unpack 8-bit channel bytes into floating-point Color channels
            for (size_t pixel = 0; pixel < num_pixels; ++pixel) {
                const size_t pixel_begin = num_bytes_per_pixel * pixel;

                Color color = Color::black();
                for (size_t channel = 0; channel < num_channels; ++channel) {
                    const size_t channel_begin = pixel_begin + channel;
                    color[channel] = Unorm8{pixel_bytes[channel_begin]}.normalized_value();
                }
                rv.push_back(color);
            }
        }
        else if (channel_format == TextureChannelFormat::Float32 and num_bytes_per_channel == sizeof(float)) {

            // read 32-bit channel floats into Color channels
            for (size_t pixel = 0; pixel < num_pixels; ++pixel) {
                const size_t pixel_begin = num_bytes_per_pixel * pixel;

                Color color = Color::black();
                for (size_t channel = 0; channel < num_channels; ++channel)
                {
                    const size_t channel_begin = pixel_begin + channel*num_bytes_per_channel;

                    std::span<const uint8_t> channel_span{pixel_bytes.data() + channel_begin, sizeof(float)};
                    std::array<uint8_t, sizeof(float)> tmp_array{};
                    copy(channel_span, tmp_array.begin());

                    color[channel] = cpp20::bit_cast<float>(tmp_array);
                }
                rv.push_back(color);
            }
        }
        else {
            OSC_ASSERT(false && "unsupported texture channel format or bytes per channel detected");
        }

        return rv;
    }

    std::vector<Color32> convert_pixel_bytes_to_color32(
        std::span<const uint8_t> pixel_bytes,
        TextureFormat pixel_format)
    {
        const TextureChannelFormat channel_format = channel_format_of(pixel_format);

        const size_t num_channels = num_channels_in(pixel_format);
        const size_t num_bytes_per_channel = num_bytes_per_channel_in(channel_format);
        const size_t num_bytes_per_pixel = num_bytes_per_channel * num_channels;
        const size_t num_pixels = pixel_bytes.size() / num_bytes_per_pixel;

        std::vector<Color32> rv;
        rv.reserve(num_pixels);

        static_assert(num_options<TextureChannelFormat>() == 2);
        if (channel_format == TextureChannelFormat::Uint8) {

            // read 8-bit channel bytes into 8-bit Color32 color channels
            for (size_t pixel = 0; pixel < num_pixels; ++pixel) {
                const size_t pixel_begin = num_bytes_per_pixel * pixel;

                Color32 color = {0x00, 0x00, 0x00, 0xff};
                for (size_t channel = 0; channel < num_channels; ++channel) {
                    const size_t channel_begin = pixel_begin + channel;
                    color[channel] = pixel_bytes[channel_begin];
                }
                rv.push_back(color);
            }
        }
        else {
            static_assert(std::is_same_v<Color::value_type, float>);
            OSC_ASSERT(num_bytes_per_channel == sizeof(float));

            // pack 32-bit channel floats into 8-bit Color32 color channels
            for (size_t pixel = 0; pixel < num_pixels; ++pixel) {
                const size_t pixel_begin = num_bytes_per_pixel * pixel;

                Color32 color = {0x00, 0x00, 0x00, 0xff};
                for (size_t channel = 0; channel < num_channels; ++channel) {
                    const size_t channel_begin = pixel_begin + channel*sizeof(float);

                    std::span<const uint8_t> channel_span{pixel_bytes.data() + channel_begin, sizeof(float)};
                    std::array<uint8_t, sizeof(float)> tmp_array{};
                    copy(channel_span, tmp_array.begin());
                    const auto channelFloat = cpp20::bit_cast<float>(tmp_array);

                    color[channel] = Unorm8{channelFloat};
                }
                rv.push_back(color);
            }
        }

        return rv;
    }

    void convert_colors_to_pixel_bytes(
        std::span<const Color> colors,
        TextureFormat desired_pixel_format,
        std::vector<uint8_t>& pixel_bytes_out)
    {
        const TextureChannelFormat channel_format = channel_format_of(desired_pixel_format);

        const size_t num_channels = num_channels_in(desired_pixel_format);
        const size_t num_bytes_per_channel = num_bytes_per_channel_in(channel_format);
        const size_t num_bytes_per_pixel = num_bytes_per_channel * num_channels;
        const size_t num_pixels = colors.size();
        const size_t num_output_bytes = num_bytes_per_pixel * num_pixels;

        pixel_bytes_out.clear();
        pixel_bytes_out.reserve(num_output_bytes);

        OSC_ASSERT(num_channels <= std::tuple_size_v<Color>);
        static_assert(num_options<TextureChannelFormat>() == 2);
        if (channel_format == TextureChannelFormat::Uint8) {

            // clamp pixels, convert them to bytes, add them to pixel data buffer
            for (const Color& color : colors) {
                for (size_t channel = 0; channel < num_channels; ++channel) {
                    pixel_bytes_out.push_back(Unorm8{color[channel]}.raw_value());
                }
            }
        }
        else {
            // write pixels to pixel data buffer as-is (they're floats already)
            for (const Color& color : colors) {
                for (size_t channel = 0; channel < num_channels; ++channel) {
                    push_as_bytes(color[channel], pixel_bytes_out);
                }
            }
        }
    }

    void convert_color32s_to_pixel_bytes(
        std::span<const Color32> colors,
        TextureFormat desired_pixel_format,
        std::vector<uint8_t>& pixel_data_out)
    {
        const TextureChannelFormat channel_format = channel_format_of(desired_pixel_format);

        const size_t num_channels = num_channels_in(desired_pixel_format);
        const size_t num_bytes_per_channel = num_bytes_per_channel_in(channel_format);
        const size_t num_bytes_per_pixel = num_bytes_per_channel * num_channels;
        const size_t num_pixels = colors.size();
        const size_t num_output_bytes = num_bytes_per_pixel * num_pixels;

        pixel_data_out.clear();
        pixel_data_out.reserve(num_output_bytes);

        OSC_ASSERT(num_channels <= Color32::length());
        static_assert(num_options<TextureChannelFormat>() == 2);
        if (channel_format == TextureChannelFormat::Uint8) {
            // write pixels to pixel data buffer as-is (they're bytes already)
            for (const Color32& color : colors) {
                for (size_t channel = 0; channel < num_channels; ++channel) {
                    pixel_data_out.push_back(color[channel].raw_value());
                }
            }
        }
        else
        {
            // upscale pixels to float32s and write the floats to the pixel buffer
            for (const Color32& color : colors) {
                for (size_t channel = 0; channel < num_channels; ++channel) {
                    const float pixelFloatVal = Unorm8{color[channel]}.normalized_value();
                    push_as_bytes(pixelFloatVal, pixel_data_out);
                }
            }
        }
    }
}

class osc::Texture2D::Impl final {
public:
    Impl(
        Vec2i dimensions,
        TextureFormat format,
        ColorSpace color_space,
        TextureWrapMode wrap_mode,
        TextureFilterMode filter_mode) :

        dimensions_{dimensions},
        format_{format},
        color_space_{color_space},
        wrap_mode_u_{wrap_mode},
        wrap_mode_v_{wrap_mode},
        wrap_mode_w_{wrap_mode},
        filter_mode_{filter_mode}
    {
        OSC_ASSERT(dimensions_.x > 0 and dimensions_.y > 0);
    }

    Vec2i getDimensions() const
    {
        return dimensions_;
    }

    TextureFormat texture_format() const
    {
        return format_;
    }

    ColorSpace getColorSpace() const
    {
        return color_space_;
    }

    TextureWrapMode wrap_mode() const
    {
        return get_wrap_mode_u();
    }

    void set_wrap_mode(TextureWrapMode wrap_mode)
    {
        set_wrap_mode_u(wrap_mode);
        set_wrap_mode_v(wrap_mode);
        set_wrap_mode_w(wrap_mode);
        texture_params_version_.reset();
    }

    TextureWrapMode get_wrap_mode_u() const
    {
        return wrap_mode_u_;
    }

    void set_wrap_mode_u(TextureWrapMode wrap_mode_u)
    {
        wrap_mode_u_ = wrap_mode_u;
        texture_params_version_.reset();
    }

    TextureWrapMode get_wrap_mode_v() const
    {
        return wrap_mode_v_;
    }

    void set_wrap_mode_v(TextureWrapMode wrap_mode_v)
    {
        wrap_mode_v_ = wrap_mode_v;
        texture_params_version_.reset();
    }

    TextureWrapMode wrap_mode_w() const
    {
        return wrap_mode_w_;
    }

    void set_wrap_mode_w(TextureWrapMode wrap_mode_w)
    {
        wrap_mode_w_ = wrap_mode_w;
        texture_params_version_.reset();
    }

    TextureFilterMode filter_mode() const
    {
        return filter_mode_;
    }

    void set_filter_mode(TextureFilterMode filter_mode)
    {
        filter_mode_ = filter_mode;
        texture_params_version_.reset();
    }

    std::vector<Color> getPixels() const
    {
        return convert_pixel_bytes_to_color(pixel_data_, format_);
    }

    void setPixels(std::span<const Color> pixels)
    {
        OSC_ASSERT(std::ssize(pixels) == static_cast<ptrdiff_t>(dimensions_.x*dimensions_.y));
        convert_colors_to_pixel_bytes(pixels, format_, pixel_data_);
    }

    std::vector<Color32> getPixels32() const
    {
        return convert_pixel_bytes_to_color32(pixel_data_, format_);
    }

    void setPixels32(std::span<const Color32> pixels)
    {
        OSC_ASSERT(std::ssize(pixels) == static_cast<ptrdiff_t>(dimensions_.x*dimensions_.y));
        convert_color32s_to_pixel_bytes(pixels, format_, pixel_data_);
    }

    std::span<const uint8_t> getPixelData() const
    {
        return pixel_data_;
    }

    void set_pixel_data(std::span<const uint8_t> pixelData)
    {
        OSC_ASSERT(pixelData.size() == num_bytes_per_pixel_in(format_)*dimensions_.x*dimensions_.y && "incorrect number of bytes passed to Texture2D::set_pixel_data");
        OSC_ASSERT(pixelData.size() == pixel_data_.size());

        copy(pixelData, pixel_data_.begin());
    }

    // non PIMPL method

    gl::Texture2D& updTexture()
    {
        if (not *maybe_opengl_data_) {
            upload_to_gpu();
        }
        OSC_ASSERT(*maybe_opengl_data_);

        Texture2DOpenGLData& bufs = **maybe_opengl_data_;

        if (bufs.texture_params_version != texture_params_version_)
        {
            update_opengl_texture_params(bufs);
        }

        return bufs.texture;
    }

private:
    void upload_to_gpu()
    {
        *maybe_opengl_data_ = Texture2DOpenGLData{};

        const size_t num_bytes_per_pixel = num_bytes_per_pixel_in(format_);
        const size_t num_bytes_per_row = dimensions_.x * num_bytes_per_pixel;
        const GLint unpack_alignment = opengl_unpack_alignment_of(format_);
        const CPUDataType cpu_data_type = equivalent_cpu_datatype_of(format_);  // TextureFormat's datatype == CPU format's datatype for cubemaps
        const CPUImageFormat cpu_channel_layout = equivalent_cpu_image_format_of(format_);  // TextureFormat's layout == CPU formats's layout for cubemaps

        static_assert(num_options<TextureFormat>() == 7, "careful here, glTexImage2D will not accept some formats (e.g. GL_RGBA16F) as the externally-provided format (must be GL_RGBA format with GL_HALF_FLOAT type)");
        OSC_ASSERT(num_bytes_per_row % unpack_alignment == 0 && "the memory alignment of each horizontal line in an OpenGL texture must match the GL_UNPACK_ALIGNMENT arg (see: https://www.khronos.org/opengl/wiki/Common_Mistakes)");
        OSC_ASSERT(is_aligned_at_least(pixel_data_.data(), unpack_alignment) && "the memory alignment of the supplied pixel memory must match the GL_UNPACK_ALIGNMENT arg (see: https://www.khronos.org/opengl/wiki/Common_Mistakes)");

        // one-time upload, because pixels cannot be altered
        gl::bind_texture((*maybe_opengl_data_)->texture);
        gl::pixel_store_i(GL_UNPACK_ALIGNMENT, unpack_alignment);
        gl::tex_image2D(
            GL_TEXTURE_2D,
            0,
            opengl_internal_format_of(format_, color_space_),
            dimensions_.x,
            dimensions_.y,
            0,
            opengl_format_of(cpu_channel_layout),
            opengl_data_type_of(cpu_data_type),
            pixel_data_.data()
        );
        glGenerateMipmap(GL_TEXTURE_2D);
        gl::bind_texture();
    }

    void update_opengl_texture_params(Texture2DOpenGLData& bufs)
    {
        gl::bind_texture(bufs.texture);
        gl::tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, to_opengl_texturewrap_enum(wrap_mode_u_));
        gl::tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, to_opengl_texturewrap_enum(wrap_mode_v_));
        gl::tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, to_opengl_texturewrap_enum(wrap_mode_w_));
        gl::tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, to_opengl_texture_min_filter_param(filter_mode_));
        gl::tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, to_opengl_texture_mag_filter_param(filter_mode_));
        gl::bind_texture();
        bufs.texture_params_version = texture_params_version_;
    }

    friend class GraphicsBackend;

    Vec2i dimensions_;
    TextureFormat format_;
    ColorSpace color_space_;
    TextureWrapMode wrap_mode_u_ = TextureWrapMode::Repeat;
    TextureWrapMode wrap_mode_v_ = TextureWrapMode::Repeat;
    TextureWrapMode wrap_mode_w_ = TextureWrapMode::Repeat;
    TextureFilterMode filter_mode_ = TextureFilterMode::Nearest;
    std::vector<uint8_t> pixel_data_ = std::vector<uint8_t>(num_bytes_per_pixel_in(format_) * dimensions_.x * dimensions_.y, 0xff);
    UID texture_params_version_;
    DefaultConstructOnCopy<std::optional<Texture2DOpenGLData>> maybe_opengl_data_;
};

std::ostream& osc::operator<<(std::ostream& o, TextureWrapMode twm)
{
    return o << c_texture_wrap_mode_strings.at(static_cast<size_t>(twm));
}

std::ostream& osc::operator<<(std::ostream& o, TextureFilterMode twm)
{
    return o << c_texture_filter_mode_strings.at(static_cast<size_t>(twm));
}

size_t osc::num_channels_in(TextureFormat format)
{
    constexpr auto lut = []<TextureFormat... Formats>(OptionList<TextureFormat, Formats...>)
    {
        return std::to_array({ TextureFormatTraits<Formats>::num_channels... });
    }(TextureFormatList{});

    return lut.at(to_index(format));
}

TextureChannelFormat osc::channel_format_of(TextureFormat f)
{
    constexpr auto lut = []<TextureFormat... Formats>(OptionList<TextureFormat, Formats...>)
    {
        return std::to_array({ TextureFormatTraits<Formats>::channel_format... });
    }(TextureFormatList{});

    return lut.at(to_index(f));
}

size_t osc::num_bytes_per_pixel_in(TextureFormat format)
{
    return num_channels_in(format) * num_bytes_per_channel_in(channel_format_of(format));
}

std::optional<TextureFormat> osc::to_texture_format(size_t num_channels, TextureChannelFormat channel_format)
{
    static_assert(num_options<TextureChannelFormat>() == 2);
    const bool format_is_byte_oriented = channel_format == TextureChannelFormat::Uint8;

    static_assert(num_options<TextureFormat>() == 7);
    switch (num_channels) {
    case 1: return format_is_byte_oriented ? TextureFormat::R8     : std::optional<TextureFormat>{};
    case 2: return format_is_byte_oriented ? TextureFormat::RG16   : TextureFormat::RGFloat;
    case 3: return format_is_byte_oriented ? TextureFormat::RGB24  : TextureFormat::RGBFloat;
    case 4: return format_is_byte_oriented ? TextureFormat::RGBA32 : TextureFormat::RGBAFloat;
    default: return std::nullopt;
    }
}

size_t osc::num_bytes_per_channel_in(TextureChannelFormat channel_format)
{
    static_assert(num_options<TextureChannelFormat>() == 2);
    switch (channel_format) {
    case TextureChannelFormat::Uint8:   return 1;
    case TextureChannelFormat::Float32: return 4;
    default:                            return 1;
    }
}

osc::Texture2D::Texture2D(
    Vec2i dimensions,
    TextureFormat format,
    ColorSpace color_space,
    TextureWrapMode wrap_mode,
    TextureFilterMode filter_mode) :

    m_Impl{make_cow<Impl>(dimensions, format, color_space, wrap_mode, filter_mode)}
{}

Vec2i osc::Texture2D::getDimensions() const
{
    return m_Impl->getDimensions();
}

TextureFormat osc::Texture2D::texture_format() const
{
    return m_Impl->texture_format();
}

ColorSpace osc::Texture2D::getColorSpace() const
{
    return m_Impl->getColorSpace();
}

TextureWrapMode osc::Texture2D::wrap_mode() const
{
    return m_Impl->wrap_mode();
}

void osc::Texture2D::set_wrap_mode(TextureWrapMode wrap_mode)
{
    m_Impl.upd()->set_wrap_mode(wrap_mode);
}

TextureWrapMode osc::Texture2D::wrap_mode_u() const
{
    return m_Impl->get_wrap_mode_u();
}

void osc::Texture2D::set_wrap_mode_u(TextureWrapMode wrap_mode_u)
{
    m_Impl.upd()->set_wrap_mode_u(wrap_mode_u);
}

TextureWrapMode osc::Texture2D::wrap_mode_v() const
{
    return m_Impl->get_wrap_mode_v();
}

void osc::Texture2D::set_wrap_mode_v(TextureWrapMode wrap_mode_v)
{
    m_Impl.upd()->set_wrap_mode_v(wrap_mode_v);
}

TextureWrapMode osc::Texture2D::wrap_mode_w() const
{
    return m_Impl->wrap_mode_w();
}

void osc::Texture2D::set_wrap_mode_w(TextureWrapMode wrap_mode_w)
{
    m_Impl.upd()->set_wrap_mode_w(wrap_mode_w);
}

TextureFilterMode osc::Texture2D::filter_mode() const
{
    return m_Impl->filter_mode();
}

void osc::Texture2D::set_filter_mode(TextureFilterMode filter_mode)
{
    m_Impl.upd()->set_filter_mode(filter_mode);
}

std::vector<Color> osc::Texture2D::getPixels() const
{
    return m_Impl->getPixels();
}

void osc::Texture2D::setPixels(std::span<const Color> pixels)
{
    m_Impl.upd()->setPixels(pixels);
}

std::vector<Color32> osc::Texture2D::getPixels32() const
{
    return m_Impl->getPixels32();
}

void osc::Texture2D::setPixels32(std::span<Color32 const> pixels)
{
    m_Impl.upd()->setPixels32(pixels);
}

std::span<const uint8_t> osc::Texture2D::getPixelData() const
{
    return m_Impl->getPixelData();
}

void osc::Texture2D::set_pixel_data(std::span<const uint8_t> pixel_data)
{
    m_Impl.upd()->set_pixel_data(pixel_data);
}

std::ostream& osc::operator<<(std::ostream& o, const Texture2D&)
{
    return o << "Texture2D()";
}

namespace
{
    constexpr auto c_render_texture_format_strings = std::to_array<CStringView>({
        "Red8",
        "ARGB32",

        "RGFloat16",
        "RGBFloat16",
        "ARGBFloat16",

        "Depth",
    });
    static_assert(c_render_texture_format_strings.size() == num_options<RenderTextureFormat>());

    constexpr auto c_depth_stencil_format_strings = std::to_array<CStringView>({
        "D24_UNorm_S8_UInt",
    });
    static_assert(c_depth_stencil_format_strings.size() == num_options<DepthStencilFormat>());

    GLenum to_opengl_internal_color_format_enum(
        RenderBufferType buffer_type,
        const RenderTextureDescriptor& descriptor)
    {
        static_assert(num_options<RenderBufferType>() == 2, "review code below, which treats RenderBufferType as a bool");
        if (buffer_type == RenderBufferType::Depth) {
            return GL_DEPTH24_STENCIL8;
        }
        else {
            static_assert(num_options<RenderTextureFormat>() == 6);
            static_assert(num_options<RenderTextureReadWrite>() == 2);

            switch (descriptor.getColorFormat()) {
            case RenderTextureFormat::Red8:        return GL_RED;
            case RenderTextureFormat::ARGB32:      return descriptor.getReadWrite() == RenderTextureReadWrite::sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
            case RenderTextureFormat::RGFloat16:   return GL_RG16F;
            case RenderTextureFormat::RGBFloat16:  return GL_RGB16F;
            case RenderTextureFormat::ARGBFloat16: return GL_RGBA16F;
            case RenderTextureFormat::Depth:       return GL_R32F;
            default:                               return descriptor.getReadWrite() == RenderTextureReadWrite::sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
            }
        }
    }

    constexpr CPUImageFormat equivalent_cpu_image_format_of(
        RenderBufferType type,
        const RenderTextureDescriptor& desc)
    {
        static_assert(num_options<RenderBufferType>() == 2);
        static_assert(num_options<DepthStencilFormat>() == 1);
        static_assert(num_options<RenderTextureFormat>() == 6);
        static_assert(num_options<CPUImageFormat>() == 5);

        if (type == RenderBufferType::Depth) {
            return CPUImageFormat::DepthStencil;
        }
        else {
            switch (desc.getColorFormat()) {
            case RenderTextureFormat::Red8:        return CPUImageFormat::R8;
            case RenderTextureFormat::ARGB32:      return CPUImageFormat::RGBA;
            case RenderTextureFormat::RGFloat16:   return CPUImageFormat::RG;
            case RenderTextureFormat::RGBFloat16:  return CPUImageFormat::RGB;
            case RenderTextureFormat::ARGBFloat16: return CPUImageFormat::RGBA;
            case RenderTextureFormat::Depth:       return CPUImageFormat::R8;
            default:                               return CPUImageFormat::RGBA;
            }
        }
    }

    constexpr CPUDataType equivalent_cpu_datatype_of(
        RenderBufferType buffer_type,
        const RenderTextureDescriptor& desc)
    {
        static_assert(num_options<RenderBufferType>() == 2);
        static_assert(num_options<DepthStencilFormat>() == 1);
        static_assert(num_options<RenderTextureFormat>() == 6);
        static_assert(num_options<CPUDataType>() == 4);

        if (buffer_type == RenderBufferType::Depth) {
            return CPUDataType::UnsignedInt24_8;
        }
        else {
            switch (desc.getColorFormat()) {
            case RenderTextureFormat::Red8:        return CPUDataType::UnsignedByte;
            case RenderTextureFormat::ARGB32:      return CPUDataType::UnsignedByte;
            case RenderTextureFormat::RGFloat16:   return CPUDataType::HalfFloat;
            case RenderTextureFormat::RGBFloat16:  return CPUDataType::HalfFloat;
            case RenderTextureFormat::ARGBFloat16: return CPUDataType::HalfFloat;
            case RenderTextureFormat::Depth:       return CPUDataType::Float;
            default:                               return CPUDataType::UnsignedByte;
            }
        }
    }

    constexpr GLenum to_opengl_image_color_format_enum(TextureFormat format)
    {
        constexpr auto lut = []<TextureFormat... Formats>(OptionList<TextureFormat, Formats...>)
        {
            return std::to_array({ TextureFormatOpenGLTraits<Formats>::image_color_format... });
        }(TextureFormatList{});

        return lut.at(to_index(format));
    }

    constexpr GLint to_opengl_image_pixel_pack_alignment(TextureFormat format)
    {
        constexpr auto lut = []<TextureFormat... Formats>(OptionList<TextureFormat, Formats...>)
        {
            return std::to_array({ TextureFormatOpenGLTraits<Formats>::pixel_pack_alignment... });
        }(TextureFormatList{});

        return lut.at(to_index(format));
    }

    constexpr GLenum to_opengl_image_data_type_enum(TextureFormat)
    {
        static_assert(num_options<TextureFormat>() == 7);
        return GL_UNSIGNED_BYTE;
    }
}

std::ostream& osc::operator<<(std::ostream& o, RenderTextureFormat f)
{
    return o << c_render_texture_format_strings.at(static_cast<size_t>(f));
}

std::ostream& osc::operator<<(std::ostream& o, DepthStencilFormat f)
{
    return o << c_depth_stencil_format_strings.at(static_cast<size_t>(f));
}

osc::RenderTextureDescriptor::RenderTextureDescriptor(Vec2i dimensions) :
    dimensions_{elementwise_max(dimensions, Vec2i{0, 0})},
    dimensionality_{TextureDimensionality::Tex2D},
    antialiasing_level_{1},
    color_format_{RenderTextureFormat::ARGB32},
    depth_stencil_format_{DepthStencilFormat::D24_UNorm_S8_UInt},
    read_write_{RenderTextureReadWrite::Default}
{}

Vec2i osc::RenderTextureDescriptor::getDimensions() const
{
    return dimensions_;
}

void osc::RenderTextureDescriptor::setDimensions(Vec2i dimensions)
{
    OSC_ASSERT(dimensions.x >= 0 and dimensions.y >= 0);
    dimensions_ = dimensions;
}

TextureDimensionality osc::RenderTextureDescriptor::getDimensionality() const
{
    return dimensionality_;
}

void osc::RenderTextureDescriptor::setDimensionality(TextureDimensionality dimensionality)
{
    dimensionality_ = dimensionality;
}

AntiAliasingLevel osc::RenderTextureDescriptor::getAntialiasingLevel() const
{
    return antialiasing_level_;
}

void osc::RenderTextureDescriptor::setAntialiasingLevel(AntiAliasingLevel aa_level)
{
    antialiasing_level_ = aa_level;
}

RenderTextureFormat osc::RenderTextureDescriptor::getColorFormat() const
{
    return color_format_;
}

void osc::RenderTextureDescriptor::setColorFormat(RenderTextureFormat color_format)
{
    color_format_ = color_format;
}

DepthStencilFormat osc::RenderTextureDescriptor::getDepthStencilFormat() const
{
    return depth_stencil_format_;
}

void osc::RenderTextureDescriptor::setDepthStencilFormat(DepthStencilFormat depth_stencil_format)
{
    depth_stencil_format_ = depth_stencil_format;
}

RenderTextureReadWrite osc::RenderTextureDescriptor::getReadWrite() const
{
    return read_write_;
}

void osc::RenderTextureDescriptor::setReadWrite(RenderTextureReadWrite read_write)
{
    read_write_ = read_write;
}

std::ostream& osc::operator<<(std::ostream& o, const RenderTextureDescriptor& descriptor)
{
    return o <<
        "RenderTextureDescriptor(width = " << descriptor.dimensions_.x
        << ", height = " << descriptor.dimensions_.y
        << ", aa = " << descriptor.antialiasing_level_
        << ", color_format = " << descriptor.color_format_
        << ", depth_stencil_format = " << descriptor.depth_stencil_format_
        << ")";
}

class osc::RenderBuffer::Impl final {
public:
    Impl(
        const RenderTextureDescriptor& descriptor,
        RenderBufferType type) :

        descriptor_{descriptor},
        buffer_type_{type}
    {
        OSC_ASSERT((getDimensionality() != TextureDimensionality::Cube or getDimensions().x == getDimensions().y) && "cannot construct a Cube renderbuffer with non-square dimensions");
        OSC_ASSERT((getDimensionality() != TextureDimensionality::Cube or getAntialiasingLevel() == AntiAliasingLevel::none()) && "cannot construct a Cube renderbuffer that is anti-aliased (not supported by backends like OpenGL)");
    }

    void reformat(const RenderTextureDescriptor& descriptor)
    {
        OSC_ASSERT((descriptor.getDimensionality() != TextureDimensionality::Cube or descriptor.getDimensions().x == descriptor.getDimensions().y) && "cannot reformat a render buffer to a Cube dimensionality with non-square dimensions");
        OSC_ASSERT((descriptor.getDimensionality() != TextureDimensionality::Cube or descriptor.getAntialiasingLevel() == AntiAliasingLevel::none()) && "cannot reformat a renderbuffer to a Cube dimensionality with is anti-aliased (not supported by backends like OpenGL)");

        if (descriptor_ != descriptor) {
            descriptor_ = descriptor;
            maybe_opengl_data_->reset();
        }
    }

    const RenderTextureDescriptor& getDescriptor() const
    {
        return descriptor_;
    }

    Vec2i getDimensions() const
    {
        return descriptor_.getDimensions();
    }

    void setDimensions(Vec2i dimensions)
    {
        OSC_ASSERT((getDimensionality() != TextureDimensionality::Cube or dimensions.x == dimensions.y) && "cannot set a cubemap to have non-square dimensions");

        if (dimensions != getDimensions()) {
            descriptor_.setDimensions(dimensions);
            maybe_opengl_data_->reset();
        }
    }

    TextureDimensionality getDimensionality() const
    {
        return descriptor_.getDimensionality();
    }

    void setDimensionality(TextureDimensionality dimensionality)
    {
        OSC_ASSERT((dimensionality != TextureDimensionality::Cube or getDimensions().x == getDimensions().y) && "cannot set dimensionality to Cube for non-square render buffer");
        OSC_ASSERT((dimensionality != TextureDimensionality::Cube or getAntialiasingLevel() == AntiAliasingLevel{1}) && "cannot set dimensionality to Cube for an anti-aliased render buffer (not supported by backends like OpenGL)");

        if (dimensionality != getDimensionality()) {
            descriptor_.setDimensionality(dimensionality);
            maybe_opengl_data_->reset();
        }
    }

    RenderTextureFormat getColorFormat() const
    {
        return descriptor_.getColorFormat();
    }

    void setColorFormat(RenderTextureFormat format)
    {
        if (format != getColorFormat()) {
            descriptor_.setColorFormat(format);
            maybe_opengl_data_->reset();
        }
    }

    AntiAliasingLevel getAntialiasingLevel() const
    {
        return descriptor_.getAntialiasingLevel();
    }

    void setAntialiasingLevel(AntiAliasingLevel aa_level)
    {
        OSC_ASSERT((getDimensionality() != TextureDimensionality::Cube or aa_level == AntiAliasingLevel{1}) && "cannot set anti-aliasing level >1 on a cube render buffer (it is not supported by backends like OpenGL)");

        if (aa_level != getAntialiasingLevel()) {
            descriptor_.setAntialiasingLevel(aa_level);
            maybe_opengl_data_->reset();
        }
    }

    DepthStencilFormat getDepthStencilFormat() const
    {
        return descriptor_.getDepthStencilFormat();
    }

    void setDepthStencilFormat(DepthStencilFormat depth_stencil_format)
    {
        if (depth_stencil_format != getDepthStencilFormat()) {
            descriptor_.setDepthStencilFormat(depth_stencil_format);
            maybe_opengl_data_->reset();
        }
    }

    RenderTextureReadWrite getReadWrite() const
    {
        return descriptor_.getReadWrite();
    }

    void setReadWrite(RenderTextureReadWrite read_write)
    {
        if (read_write != descriptor_.getReadWrite()) {
            descriptor_.setReadWrite(read_write);
            maybe_opengl_data_->reset();
        }
    }

    RenderBufferOpenGLData& upd_opengl_data()
    {
        if (not *maybe_opengl_data_) {
            upload_to_gpu();
        }
        return **maybe_opengl_data_;
    }

    void upload_to_gpu()
    {
        // dispatch _which_ texture handles are created based on render buffer params

        static_assert(num_options<TextureDimensionality>() == 2);

        if (getDimensionality() == TextureDimensionality::Tex2D) {
            if (descriptor_.getAntialiasingLevel() <= AntiAliasingLevel{1}) {
                auto& t = std::get<SingleSampledTexture>((*maybe_opengl_data_).emplace(SingleSampledTexture{}));
                configure_texture(t);
            }
            else {
                auto& t = std::get<MultisampledRBOAndResolvedTexture>((*maybe_opengl_data_).emplace(MultisampledRBOAndResolvedTexture{}));
                configure_texture(t);
            }
        }
        else {
            auto& t = std::get<SingleSampledCubemap>((*maybe_opengl_data_).emplace(SingleSampledCubemap{}));
            configure_texture(t);
        }
    }

    void configure_texture(SingleSampledTexture& t)
    {
        const Vec2i dimensions = descriptor_.getDimensions();

        // setup resolved texture
        gl::bind_texture(t.texture2D);
        gl::tex_image2D(
            GL_TEXTURE_2D,
            0,
            to_opengl_internal_color_format_enum(buffer_type_, descriptor_),
            dimensions.x,
            dimensions.y,
            0,
            opengl_format_of(equivalent_cpu_image_format_of(buffer_type_, descriptor_)),
            opengl_data_type_of(equivalent_cpu_datatype_of(buffer_type_, descriptor_)),
            nullptr
        );
        gl::tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl::tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl::tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl::tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl::tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        gl::bind_texture();
    }

    void configure_texture(MultisampledRBOAndResolvedTexture& data)
    {
        const Vec2i dimensions = descriptor_.getDimensions();

        // setup multisampled RBO
        gl::bind_renderbuffer(data.multisampled_rbo);
        glRenderbufferStorageMultisample(
            GL_RENDERBUFFER,
            descriptor_.getAntialiasingLevel().get_as<GLsizei>(),
            to_opengl_internal_color_format_enum(buffer_type_, descriptor_),
            dimensions.x,
            dimensions.y
        );
        gl::bind_renderbuffer();

        // setup resolved texture
        gl::bind_texture(data.single_sampled_texture2D);
        gl::tex_image2D(
            GL_TEXTURE_2D,
            0,
            to_opengl_internal_color_format_enum(buffer_type_, descriptor_),
            dimensions.x,
            dimensions.y,
            0,
            opengl_format_of(equivalent_cpu_image_format_of(buffer_type_, descriptor_)),
            opengl_data_type_of(equivalent_cpu_datatype_of(buffer_type_, descriptor_)),
            nullptr
        );
        gl::tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl::tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl::tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl::tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl::tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        gl::bind_texture();
    }

    void configure_texture(SingleSampledCubemap& t)
    {
        const Vec2i dimensions = descriptor_.getDimensions();

        // setup resolved texture
        gl::bind_texture(t.cubemap);
        for (int i = 0; i < 6; ++i)
        {
            gl::tex_image2D(
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                0,
                to_opengl_internal_color_format_enum(buffer_type_, descriptor_),
                dimensions.x,
                dimensions.y,
                0,
                opengl_format_of(equivalent_cpu_image_format_of(buffer_type_, descriptor_)),
                opengl_data_type_of(equivalent_cpu_datatype_of(buffer_type_, descriptor_)),
                nullptr
            );
        }
        gl::tex_parameter_i(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl::tex_parameter_i(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl::tex_parameter_i(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl::tex_parameter_i(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl::tex_parameter_i(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    }

    bool has_been_rendered_to() const
    {
        return maybe_opengl_data_->has_value();
    }

private:
    RenderTextureDescriptor descriptor_;
    RenderBufferType buffer_type_;
    DefaultConstructOnCopy<std::optional<RenderBufferOpenGLData>> maybe_opengl_data_;
};

osc::RenderBuffer::RenderBuffer(
    const RenderTextureDescriptor& descriptor_,
    RenderBufferType type_) :

    m_Impl{std::make_unique<Impl>(descriptor_, type_)}
{}

osc::RenderBuffer::~RenderBuffer() noexcept = default;

class osc::RenderTexture::Impl final {
public:
    Impl() : Impl{Vec2i{1, 1}}
    {}

    explicit Impl(Vec2i dimensions) :
        Impl{RenderTextureDescriptor{dimensions}}
    {}

    explicit Impl(const RenderTextureDescriptor& descriptor) :
        color_buffer_{std::make_shared<RenderBuffer>(descriptor, RenderBufferType::Color)},
        depth_buffer_{std::make_shared<RenderBuffer>(descriptor, RenderBufferType::Depth)}
    {}

    Vec2i getDimensions() const
    {
        return color_buffer_->m_Impl->getDimensions();
    }

    void setDimensions(Vec2i dimensions)
    {
        if (dimensions != getDimensions()) {
            color_buffer_->m_Impl->setDimensions(dimensions);
            depth_buffer_->m_Impl->setDimensions(dimensions);
        }
    }

    TextureDimensionality getDimensionality() const
    {
        return color_buffer_->m_Impl->getDimensionality();
    }

    void setDimensionality(TextureDimensionality dimensionality)
    {
        if (dimensionality != getDimensionality()) {
            color_buffer_->m_Impl->setDimensionality(dimensionality);
            depth_buffer_->m_Impl->setDimensionality(dimensionality);
        }
    }

    RenderTextureFormat getColorFormat() const
    {
        return color_buffer_->m_Impl->getColorFormat();
    }

    void setColorFormat(RenderTextureFormat color_format)
    {
        if (color_format != getColorFormat()) {
            color_buffer_->m_Impl->setColorFormat(color_format);
            depth_buffer_->m_Impl->setColorFormat(color_format);
        }
    }

    AntiAliasingLevel getAntialiasingLevel() const
    {
        return color_buffer_->m_Impl->getAntialiasingLevel();
    }

    void setAntialiasingLevel(AntiAliasingLevel aa_level)
    {
        if (aa_level != getAntialiasingLevel()) {
            color_buffer_->m_Impl->setAntialiasingLevel(aa_level);
            depth_buffer_->m_Impl->setAntialiasingLevel(aa_level);
        }
    }

    DepthStencilFormat getDepthStencilFormat() const
    {
        return color_buffer_->m_Impl->getDepthStencilFormat();
    }

    void setDepthStencilFormat(DepthStencilFormat depth_stencil_format)
    {
        if (depth_stencil_format != getDepthStencilFormat()) {
            color_buffer_->m_Impl->setDepthStencilFormat(depth_stencil_format);
            depth_buffer_->m_Impl->setDepthStencilFormat(depth_stencil_format);
        }
    }

    RenderTextureReadWrite getReadWrite() const
    {
        return color_buffer_->m_Impl->getReadWrite();
    }

    void setReadWrite(RenderTextureReadWrite read_write)
    {
        if (read_write != getReadWrite()) {
            color_buffer_->m_Impl->setReadWrite(read_write);
            depth_buffer_->m_Impl->setReadWrite(read_write);
        }
    }

    void reformat(RenderTextureDescriptor const& format_description)
    {
        if (format_description != color_buffer_->m_Impl->getDescriptor()) {
            color_buffer_->m_Impl->reformat(format_description);
            depth_buffer_->m_Impl->reformat(format_description);
        }
    }

    RenderBufferOpenGLData& getColorRenderBufferData()
    {
        return color_buffer_->m_Impl->upd_opengl_data();
    }

    RenderBufferOpenGLData& getDepthStencilRenderBufferData()
    {
        return depth_buffer_->m_Impl->upd_opengl_data();
    }

    bool has_been_rendered_to() const
    {
        return color_buffer_->m_Impl->has_been_rendered_to();
    }

    std::shared_ptr<RenderBuffer> updColorBuffer()
    {
        return color_buffer_;
    }

    std::shared_ptr<RenderBuffer> updDepthBuffer()
    {
        return depth_buffer_;
    }

private:
    friend class GraphicsBackend;

    std::shared_ptr<RenderBuffer> color_buffer_;
    std::shared_ptr<RenderBuffer> depth_buffer_;
};

osc::RenderTexture::RenderTexture() :
    m_Impl{make_cow<Impl>()}
{}

osc::RenderTexture::RenderTexture(Vec2i dimensions) :
    m_Impl{make_cow<Impl>(dimensions)}
{}

osc::RenderTexture::RenderTexture(const RenderTextureDescriptor& descriptor) :
    m_Impl{make_cow<Impl>(descriptor)}
{}

Vec2i osc::RenderTexture::getDimensions() const
{
    return m_Impl->getDimensions();
}

void osc::RenderTexture::setDimensions(Vec2i d)
{
    m_Impl.upd()->setDimensions(d);
}

TextureDimensionality osc::RenderTexture::getDimensionality() const
{
    return m_Impl->getDimensionality();
}

void osc::RenderTexture::setDimensionality(TextureDimensionality dimensionality)
{
    m_Impl.upd()->setDimensionality(dimensionality);
}

RenderTextureFormat osc::RenderTexture::getColorFormat() const
{
    return m_Impl->getColorFormat();
}

void osc::RenderTexture::setColorFormat(RenderTextureFormat format)
{
    m_Impl.upd()->setColorFormat(format);
}

AntiAliasingLevel osc::RenderTexture::getAntialiasingLevel() const
{
    return m_Impl->getAntialiasingLevel();
}

void osc::RenderTexture::setAntialiasingLevel(AntiAliasingLevel aa_level)
{
    m_Impl.upd()->setAntialiasingLevel(aa_level);
}

DepthStencilFormat osc::RenderTexture::getDepthStencilFormat() const
{
    return m_Impl->getDepthStencilFormat();
}

void osc::RenderTexture::setDepthStencilFormat(DepthStencilFormat depth_stencil_format)
{
    m_Impl.upd()->setDepthStencilFormat(depth_stencil_format);
}

RenderTextureReadWrite osc::RenderTexture::getReadWrite() const
{
    return m_Impl->getReadWrite();
}

void osc::RenderTexture::setReadWrite(RenderTextureReadWrite read_write)
{
    m_Impl.upd()->setReadWrite(read_write);
}

void osc::RenderTexture::reformat(const RenderTextureDescriptor& format_description)
{
    m_Impl.upd()->reformat(format_description);
}

std::shared_ptr<RenderBuffer> osc::RenderTexture::updColorBuffer()
{
    return m_Impl.upd()->updColorBuffer();
}

std::shared_ptr<RenderBuffer> osc::RenderTexture::updDepthBuffer()
{
    return m_Impl.upd()->updDepthBuffer();
}

std::ostream& osc::operator<<(std::ostream& o, const RenderTexture&)
{
    return o << "RenderTexture()";
}


class osc::Shader::Impl final {
public:
    Impl(
        CStringView vertex_shader_src,
        CStringView fragment_shader_src) :

        program_{gl::create_program_from(
            gl::compile_from_source<gl::VertexShader>(vertex_shader_src.c_str()),
            gl::compile_from_source<gl::FragmentShader>(fragment_shader_src.c_str())
        )}
    {
        parseUniformsAndAttributesFromProgram();
    }

    Impl(
        CStringView vertex_shader_src,
        CStringView geometry_shader_src,
        CStringView fragment_shader_src) :

        program_{gl::create_program_from(
            gl::compile_from_source<gl::VertexShader>(vertex_shader_src.c_str()),
            gl::compile_from_source<gl::FragmentShader>(fragment_shader_src.c_str()),
            gl::compile_from_source<gl::GeometryShader>(geometry_shader_src.c_str())
        )}
    {
        parseUniformsAndAttributesFromProgram();
    }

    size_t getPropertyCount() const
    {
        return uniforms_.size();
    }

    std::optional<ptrdiff_t> findPropertyIndex(std::string_view property_name) const
    {
        if (const auto it = uniforms_.find(property_name); it != uniforms_.end()) {
            return static_cast<ptrdiff_t>(std::distance(uniforms_.begin(), it));
        }
        else {
            return std::nullopt;
        }
    }

    std::string_view getPropertyName(ptrdiff_t i) const
    {
        auto it = uniforms_.begin();
        std::advance(it, i);
        return it->first;
    }

    ShaderPropertyType getPropertyType(ptrdiff_t i) const
    {
        auto it = uniforms_.begin();
        std::advance(it, i);
        return it->second.shader_type;
    }

    // non-PIMPL APIs

    const gl::Program& getProgram() const
    {
        return program_;
    }

    const FastStringHashtable<ShaderElement>& getUniforms() const
    {
        return uniforms_;
    }

    const FastStringHashtable<ShaderElement>& getAttributes() const
    {
        return attributes_;
    }

private:
    void parseUniformsAndAttributesFromProgram()
    {
        constexpr GLsizei c_shader_max_name_length = 128;

        GLint num_attrs = 0;
        glGetProgramiv(program_.get(), GL_ACTIVE_ATTRIBUTES, &num_attrs);

        GLint num_uniforms = 0;
        glGetProgramiv(program_.get(), GL_ACTIVE_UNIFORMS, &num_uniforms);

        attributes_.reserve(num_attrs);
        for (GLint attr_idx = 0; attr_idx < num_attrs; attr_idx++) {

            GLint size = 0;                                       // size of the variable
            GLenum type = 0;                                      // type of the variable (float, vec3 or mat4, etc)
            std::array<GLchar, c_shader_max_name_length> name{};  // variable name in GLSL
            GLsizei length = 0;                                   // name length
            glGetActiveAttrib(
                program_.get() ,
                static_cast<GLuint>(attr_idx),
                static_cast<GLsizei>(name.size()),
                &length,
                &size,
                &type,
                name.data()
            );

            static_assert(sizeof(GLint) <= sizeof(int32_t));
            attributes_.try_emplace<std::string>(
                normalize_shader_element_name(name.data()),
                static_cast<int32_t>(glGetAttribLocation(program_.get(), name.data())),
                opengl_shader_type_to_osc_shader_type(type),
                static_cast<int32_t>(size)
            );
        }

        uniforms_.reserve(num_uniforms);
        for (GLint uniform_idx = 0; uniform_idx < num_uniforms; uniform_idx++) {
            GLint size = 0;                                       // size of the variable
            GLenum type = 0;                                      // type of the variable (float, vec3 or mat4, etc)
            std::array<GLchar, c_shader_max_name_length> name{};  // variable name in GLSL
            GLsizei length = 0;                                   // name length
            glGetActiveUniform(
                program_.get(),
                static_cast<GLuint>(uniform_idx),
                static_cast<GLsizei>(name.size()),
                &length,
                &size,
                &type,
                name.data()
            );

            static_assert(sizeof(GLint) <= sizeof(int32_t));
            uniforms_.try_emplace<std::string>(
                normalize_shader_element_name(name.data()),
                static_cast<int32_t>(glGetUniformLocation(program_.get(), name.data())),
                opengl_shader_type_to_osc_shader_type(type),
                static_cast<int32_t>(size)
            );
        }

        // cache commonly-used "automatic" shader elements
        //
        // it's a perf optimization: the renderer uses this to skip lookups
        maybe_model_mat_uniform_ = find_or_optional(uniforms_, "uModelMat");
        maybe_normal_mat_uniform_ = find_or_optional(uniforms_, "uNormalMat");
        maybe_view_mat_uniform_ = find_or_optional(uniforms_, "uViewMat");
        maybe_proj_mat_uniform_ = find_or_optional(uniforms_, "uProjMat");
        maybe_view_proj_mat_uniform_ = find_or_optional(uniforms_, "uViewProjMat");
        maybe_instanced_model_mat_attr_ = find_or_optional(attributes_, "aModelMat");
        maybe_instanced_normal_mat_attr_ = find_or_optional(attributes_, "aNormalMat");
    }

    friend class GraphicsBackend;

    UID id_;
    gl::Program program_;
    FastStringHashtable<ShaderElement> uniforms_;
    FastStringHashtable<ShaderElement> attributes_;
    std::optional<ShaderElement> maybe_model_mat_uniform_;
    std::optional<ShaderElement> maybe_normal_mat_uniform_;
    std::optional<ShaderElement> maybe_view_mat_uniform_;
    std::optional<ShaderElement> maybe_proj_mat_uniform_;
    std::optional<ShaderElement> maybe_view_proj_mat_uniform_;
    std::optional<ShaderElement> maybe_instanced_model_mat_attr_;
    std::optional<ShaderElement> maybe_instanced_normal_mat_attr_;
};


std::ostream& osc::operator<<(std::ostream& o, ShaderPropertyType shader_type)
{
    constexpr auto lut = []<ShaderPropertyType... Types>(OptionList<ShaderPropertyType, Types...>)
    {
        return std::to_array({ ShaderPropertyTypeTraits<Types>::name... });
    }(ShaderPropertyTypeList{});

    return o << lut.at(to_index(shader_type));
}

osc::Shader::Shader(
    CStringView vertex_shader_src,
    CStringView fragment_shader_src) :

    m_Impl{make_cow<Impl>(vertex_shader_src, fragment_shader_src)}
{}

osc::Shader::Shader(
    CStringView vertex_shader_src,
    CStringView geometry_shader_src,
    CStringView fragment_shader_src) :

    m_Impl{make_cow<Impl>(vertex_shader_src, geometry_shader_src, fragment_shader_src)}
{}

size_t osc::Shader::getPropertyCount() const
{
    return m_Impl->getPropertyCount();
}

std::optional<ptrdiff_t> osc::Shader::findPropertyIndex(std::string_view property_name) const
{
    return m_Impl->findPropertyIndex(property_name);
}

std::string_view osc::Shader::getPropertyName(ptrdiff_t property_index) const
{
    return m_Impl->getPropertyName(property_index);
}

ShaderPropertyType osc::Shader::getPropertyType(ptrdiff_t property_index) const
{
    return m_Impl->getPropertyType(property_index);
}

std::ostream& osc::operator<<(std::ostream& o, const Shader& shader)
{
    o << "Shader(\n";
    {
        o << "    uniforms = [";

        const std::string_view delim = "\n        ";
        for (const auto& [name, data] : shader.m_Impl->getUniforms()) {
            o << delim;
            print_shader_element(o, name, data);
        }

        o << "\n    ],\n";
    }

    {
        o << "    attributes = [";

        const std::string_view delim = "\n        ";
        for (const auto& [name, data] : shader.m_Impl->getAttributes()) {
            o << delim;
            print_shader_element(o, name, data);
        }

        o << "\n    ]\n";
    }

    o << ')';

    return o;
}

namespace
{
    GLenum to_opengl_depth_function_enum(DepthFunction depth_function)
    {
        static_assert(num_options<DepthFunction>() == 2);

        switch (depth_function) {
        case DepthFunction::LessOrEqual: return GL_LEQUAL;
        case DepthFunction::Less:        return GL_LESS;
        default:                         return GL_LESS;
        }
    }

    GLenum to_opengl_cull_face_enum(CullMode cull_mode)
    {
        static_assert(num_options<CullMode>() == 3);

        switch (cull_mode) {
        case CullMode::Front: return GL_FRONT;
        case CullMode::Back:  return GL_BACK;
        default:              return GL_BACK;
        }
    }
}

class osc::Material::Impl final {
public:
    explicit Impl(Shader shader) :
        shader_{std::move(shader)}
    {}

    const Shader& getShader() const
    {
        return shader_;
    }

    std::optional<Color> getColor(std::string_view property_name) const
    {
        return get_value<Color>(property_name);
    }

    void setColor(std::string_view property_name, const Color& color)
    {
        set_value(property_name, color);
    }

    std::optional<std::span<const Color>> getColorArray(std::string_view property_name) const
    {
        return get_value<std::vector<Color>, std::span<const Color>>(property_name);
    }

    void setColorArray(std::string_view property_name, std::span<const Color> colors)
    {
        set_value<std::vector<Color>>(property_name, std::vector<Color>(colors.begin(), colors.end()));
    }

    std::optional<float> getFloat(std::string_view property_name) const
    {
        return get_value<float>(property_name);
    }

    void setFloat(std::string_view property_name, float value)
    {
        set_value(property_name, value);
    }

    std::optional<std::span<const float>> getFloatArray(std::string_view property_name) const
    {
        return get_value<std::vector<float>, std::span<const float>>(property_name);
    }

    void setFloatArray(std::string_view property_name, std::span<const float> values)
    {
        set_value<std::vector<float>>(property_name, std::vector<float>(values.begin(), values.end()));
    }

    std::optional<Vec2> getVec2(std::string_view property_name) const
    {
        return get_value<Vec2>(property_name);
    }

    void setVec2(std::string_view property_name, Vec2 vec)
    {
        set_value(property_name, vec);
    }

    std::optional<Vec3> getVec3(std::string_view property_name) const
    {
        return get_value<Vec3>(property_name);
    }

    void setVec3(std::string_view property_name, Vec3 vec)
    {
        set_value(property_name, vec);
    }

    std::optional<std::span<const Vec3>> getVec3Array(std::string_view property_name) const
    {
        return get_value<std::vector<Vec3>, std::span<const Vec3>>(property_name);
    }

    void setVec3Array(std::string_view property_name, std::span<const Vec3> vecs)
    {
        set_value(property_name, std::vector<Vec3>(vecs.begin(), vecs.end()));
    }

    std::optional<Vec4> getVec4(std::string_view property_name) const
    {
        return get_value<Vec4>(property_name);
    }

    void setVec4(std::string_view property_name, Vec4 vec)
    {
        set_value(property_name, vec);
    }

    std::optional<Mat3> getMat3(std::string_view property_name) const
    {
        return get_value<Mat3>(property_name);
    }

    void setMat3(std::string_view property_name, const Mat3& mat)
    {
        set_value(property_name, mat);
    }

    std::optional<Mat4> getMat4(std::string_view property_name) const
    {
        return get_value<Mat4>(property_name);
    }

    void setMat4(std::string_view property_name, const Mat4& mat)
    {
        set_value(property_name, mat);
    }

    std::optional<std::span<const Mat4>> getMat4Array(std::string_view property_name) const
    {
        return get_value<std::vector<Mat4>, std::span<const Mat4>>(property_name);
    }

    void setMat4Array(std::string_view property_name, std::span<const Mat4> mats)
    {
        set_value(property_name, std::vector<Mat4>(mats.begin(), mats.end()));
    }

    std::optional<int32_t> getInt(std::string_view property_name) const
    {
        return get_value<int32_t>(property_name);
    }

    void setInt(std::string_view property_name, int32_t value)
    {
        set_value(property_name, value);
    }

    std::optional<bool> getBool(std::string_view property_name) const
    {
        return get_value<bool>(property_name);
    }

    void setBool(std::string_view property_name, bool value)
    {
        set_value(property_name, value);
    }

    std::optional<Texture2D> getTexture(std::string_view property_name) const
    {
        return get_value<Texture2D>(property_name);
    }

    void setTexture(std::string_view property_name, Texture2D texture)
    {
        set_value(property_name, std::move(texture));
    }

    void clearTexture(std::string_view property_name)
    {
        values_.erase(property_name);
    }

    std::optional<RenderTexture> getRenderTexture(std::string_view property_name) const
    {
        return get_value<RenderTexture>(property_name);
    }

    void setRenderTexture(std::string_view property_name, RenderTexture render_texture)
    {
        set_value(property_name, std::move(render_texture));
    }

    void clearRenderTexture(std::string_view property_name)
    {
        values_.erase(property_name);
    }

    std::optional<Cubemap> getCubemap(std::string_view property_name) const
    {
        return get_value<Cubemap>(property_name);
    }

    void setCubemap(std::string_view property_name, Cubemap cubemap)
    {
        set_value(property_name, std::move(cubemap));
    }

    void clearCubemap(std::string_view property_name)
    {
        values_.erase(property_name);
    }

    bool getTransparent() const
    {
        return is_transparent_;
    }

    void setTransparent(bool value)
    {
        is_transparent_ = value;
    }

    bool getDepthTested() const
    {
        return is_depth_tested_;
    }

    void setDepthTested(bool value)
    {
        is_depth_tested_ = value;
    }

    DepthFunction getDepthFunction() const
    {
        return depth_function_;
    }

    void setDepthFunction(DepthFunction depth_function)
    {
        depth_function_ = depth_function;
    }

    bool getWireframeMode() const
    {
        return is_wireframe_mode_;
    }

    void setWireframeMode(bool value)
    {
        is_wireframe_mode_ = value;
    }

    CullMode getCullMode() const
    {
        return cull_mode_;
    }

    void setCullMode(CullMode cull_mode)
    {
        cull_mode_ = cull_mode;
    }

private:
    template<typename T, typename TConverted = T>
    requires std::convertible_to<T, TConverted>
    std::optional<TConverted> get_value(std::string_view property_name) const
    {
        const auto* value = try_find(values_, property_name);

        if (not value) {
            return std::nullopt;
        }
        if (not std::holds_alternative<T>(*value))
        {
            return std::nullopt;
        }
        return TConverted{std::get<T>(*value)};
    }

    template<typename T>
    void set_value(std::string_view property_name, T&& value)
    {
        values_.insert_or_assign(property_name, std::forward<T>(value));
    }

    friend class GraphicsBackend;

    Shader shader_;
    FastStringHashtable<MaterialValue> values_;
    bool is_transparent_ = false;
    bool is_depth_tested_ = true;
    bool is_wireframe_mode_ = false;
    DepthFunction depth_function_ = DepthFunction::Default;
    CullMode cull_mode_ = CullMode::Default;
};

osc::Material::Material(Shader shader) :
    m_Impl{make_cow<Impl>(std::move(shader))}
{}

const Shader& osc::Material::getShader() const
{
    return m_Impl->getShader();
}

std::optional<Color> osc::Material::getColor(std::string_view property_name) const
{
    return m_Impl->getColor(property_name);
}

void osc::Material::setColor(std::string_view property_name, const Color& color)
{
    m_Impl.upd()->setColor(property_name, color);
}

std::optional<std::span<const Color>> osc::Material::getColorArray(std::string_view property_name) const
{
    return m_Impl->getColorArray(property_name);
}

void osc::Material::setColorArray(std::string_view property_name, std::span<const Color> colors)
{
    m_Impl.upd()->setColorArray(property_name, colors);
}

std::optional<float> osc::Material::getFloat(std::string_view property_name) const
{
    return m_Impl->getFloat(property_name);
}

void osc::Material::setFloat(std::string_view property_name, float value)
{
    m_Impl.upd()->setFloat(property_name, value);
}

std::optional<std::span<const float>> osc::Material::getFloatArray(std::string_view property_name) const
{
    return m_Impl->getFloatArray(property_name);
}

void osc::Material::setFloatArray(std::string_view property_name, std::span<const float> values)
{
    m_Impl.upd()->setFloatArray(property_name, values);
}

std::optional<Vec2> osc::Material::getVec2(std::string_view property_name) const
{
    return m_Impl->getVec2(property_name);
}

void osc::Material::setVec2(std::string_view property_name, Vec2 vec)
{
    m_Impl.upd()->setVec2(property_name, vec);
}

std::optional<std::span<const Vec3>> osc::Material::getVec3Array(std::string_view property_name) const
{
    return m_Impl->getVec3Array(property_name);
}

void osc::Material::setVec3Array(std::string_view property_name, std::span<const Vec3> vecs)
{
    m_Impl.upd()->setVec3Array(property_name, vecs);
}

std::optional<Vec3> osc::Material::getVec3(std::string_view property_name) const
{
    return m_Impl->getVec3(property_name);
}

void osc::Material::setVec3(std::string_view property_name, Vec3 vec)
{
    m_Impl.upd()->setVec3(property_name, vec);
}

std::optional<Vec4> osc::Material::getVec4(std::string_view property_name) const
{
    return m_Impl->getVec4(property_name);
}

void osc::Material::setVec4(std::string_view property_name, Vec4 vec)
{
    m_Impl.upd()->setVec4(property_name, vec);
}

std::optional<Mat3> osc::Material::getMat3(std::string_view property_name) const
{
    return m_Impl->getMat3(property_name);
}

void osc::Material::setMat3(std::string_view property_name, const Mat3& mat)
{
    m_Impl.upd()->setMat3(property_name, mat);
}

std::optional<Mat4> osc::Material::getMat4(std::string_view property_name) const
{
    return m_Impl->getMat4(property_name);
}

void osc::Material::setMat4(std::string_view property_name, const Mat4& mat)
{
    m_Impl.upd()->setMat4(property_name, mat);
}

std::optional<std::span<const Mat4>> osc::Material::getMat4Array(std::string_view property_name) const
{
    return m_Impl->getMat4Array(property_name);
}

void osc::Material::setMat4Array(std::string_view property_name, std::span<const Mat4> mats)
{
    m_Impl.upd()->setMat4Array(property_name, mats);
}

std::optional<int32_t> osc::Material::getInt(std::string_view property_name) const
{
    return m_Impl->getInt(property_name);
}

void osc::Material::setInt(std::string_view property_name, int32_t value)
{
    m_Impl.upd()->setInt(property_name, value);
}

std::optional<bool> osc::Material::getBool(std::string_view property_name) const
{
    return m_Impl->getBool(property_name);
}

void osc::Material::setBool(std::string_view property_name, bool value)
{
    m_Impl.upd()->setBool(property_name, value);
}

std::optional<Texture2D> osc::Material::getTexture(std::string_view property_name) const
{
    return m_Impl->getTexture(property_name);
}

void osc::Material::setTexture(std::string_view property_name, Texture2D texture)
{
    m_Impl.upd()->setTexture(property_name, std::move(texture));
}

void osc::Material::clearTexture(std::string_view property_name)
{
    m_Impl.upd()->clearTexture(property_name);
}

std::optional<RenderTexture> osc::Material::getRenderTexture(std::string_view property_name) const
{
    return m_Impl->getRenderTexture(property_name);
}

void osc::Material::setRenderTexture(std::string_view property_name, RenderTexture render_texture)
{
    m_Impl.upd()->setRenderTexture(property_name, std::move(render_texture));
}

void osc::Material::clearRenderTexture(std::string_view property_name)
{
    m_Impl.upd()->clearRenderTexture(property_name);
}

std::optional<Cubemap> osc::Material::getCubemap(std::string_view property_name) const
{
    return m_Impl->getCubemap(property_name);
}

void osc::Material::setCubemap(std::string_view property_name, Cubemap cubemap)
{
    m_Impl.upd()->setCubemap(property_name, std::move(cubemap));
}

void osc::Material::clearCubemap(std::string_view property_name)
{
    m_Impl.upd()->clearCubemap(property_name);
}

bool osc::Material::getTransparent() const
{
    return m_Impl->getTransparent();
}

void osc::Material::setTransparent(bool value)
{
    m_Impl.upd()->setTransparent(value);
}

bool osc::Material::getDepthTested() const
{
    return m_Impl->getDepthTested();
}

void osc::Material::setDepthTested(bool value)
{
    m_Impl.upd()->setDepthTested(value);
}

DepthFunction osc::Material::getDepthFunction() const
{
    return m_Impl->getDepthFunction();
}

void osc::Material::setDepthFunction(DepthFunction depth_function)
{
    m_Impl.upd()->setDepthFunction(depth_function);
}

bool osc::Material::getWireframeMode() const
{
    return m_Impl->getWireframeMode();
}

void osc::Material::setWireframeMode(bool value)
{
    m_Impl.upd()->setWireframeMode(value);
}

CullMode osc::Material::getCullMode() const
{
    return m_Impl->getCullMode();
}

void osc::Material::setCullMode(CullMode cull_mode)
{
    m_Impl.upd()->setCullMode(cull_mode);
}

std::ostream& osc::operator<<(std::ostream& o, const Material&)
{
    return o << "Material()";
}


class osc::MaterialPropertyBlock::Impl final {
public:
    void clear()
    {
        values_.clear();
    }

    bool isEmpty() const
    {
        return values_.empty();
    }

    std::optional<Color> getColor(std::string_view property_name) const
    {
        return getValue<Color>(property_name);
    }

    void setColor(std::string_view property_name, Color const& color)
    {
        setValue(property_name, color);
    }

    std::optional<float> getFloat(std::string_view property_name) const
    {
        return getValue<float>(property_name);
    }

    void setFloat(std::string_view property_name, float value)
    {
        setValue(property_name, value);
    }

    std::optional<Vec3> getVec3(std::string_view property_name) const
    {
        return getValue<Vec3>(property_name);
    }

    void setVec3(std::string_view property_name, Vec3 vec)
    {
        setValue(property_name, vec);
    }

    std::optional<Vec4> getVec4(std::string_view property_name) const
    {
        return getValue<Vec4>(property_name);
    }

    void setVec4(std::string_view property_name, Vec4 value)
    {
        setValue(property_name, value);
    }

    std::optional<Mat3> getMat3(std::string_view property_name) const
    {
        return getValue<Mat3>(property_name);
    }

    void setMat3(std::string_view property_name, const Mat3& mat)
    {
        setValue(property_name, mat);
    }

    std::optional<Mat4> getMat4(std::string_view property_name) const
    {
        return getValue<Mat4>(property_name);
    }

    void setMat4(std::string_view property_name, const Mat4& mat)
    {
        setValue(property_name, mat);
    }

    std::optional<int32_t> getInt(std::string_view property_name) const
    {
        return getValue<int32_t>(property_name);
    }

    void setInt(std::string_view property_name, int32_t value)
    {
        setValue(property_name, value);
    }

    std::optional<bool> getBool(std::string_view property_name) const
    {
        return getValue<bool>(property_name);
    }

    void setBool(std::string_view property_name, bool value)
    {
        setValue(property_name, value);
    }

    std::optional<Texture2D> getTexture(std::string_view property_name) const
    {
        return getValue<Texture2D>(property_name);
    }

    void setTexture(std::string_view property_name, Texture2D texture)
    {
        setValue(property_name, std::move(texture));
    }

    friend bool operator==(const Impl&, const Impl&) = default;

private:
    template<typename T>
    std::optional<T> getValue(std::string_view property_name) const
    {
        const auto it = values_.find(property_name);

        if (it == values_.end()) {
            return std::nullopt;
        }
        if (not std::holds_alternative<T>(it->second)) {
            return std::nullopt;
        }

        return std::get<T>(it->second);
    }

    template<typename T>
    void setValue(std::string_view property_name, T&& value)
    {
        values_.insert_or_assign(property_name, std::forward<T>(value));
    }

    friend class GraphicsBackend;

    FastStringHashtable<MaterialValue> values_;
};

osc::MaterialPropertyBlock::MaterialPropertyBlock() :
    m_Impl{[]()
    {
        static const CopyOnUpdPtr<Impl> s_empty_property_block_impl = make_cow<Impl>();
        return s_empty_property_block_impl;
    }()}
{
}

void osc::MaterialPropertyBlock::clear()
{
    m_Impl.upd()->clear();
}

bool osc::MaterialPropertyBlock::isEmpty() const
{
    return m_Impl->isEmpty();
}

std::optional<Color> osc::MaterialPropertyBlock::getColor(std::string_view property_name) const
{
    return m_Impl->getColor(property_name);
}

void osc::MaterialPropertyBlock::setColor(std::string_view property_name, const Color& color)
{
    m_Impl.upd()->setColor(property_name, color);
}

std::optional<float> osc::MaterialPropertyBlock::getFloat(std::string_view property_name) const
{
    return m_Impl->getFloat(property_name);
}

void osc::MaterialPropertyBlock::setFloat(std::string_view property_name, float value)
{
    m_Impl.upd()->setFloat(property_name, value);
}

std::optional<Vec3> osc::MaterialPropertyBlock::getVec3(std::string_view property_name) const
{
    return m_Impl->getVec3(property_name);
}

void osc::MaterialPropertyBlock::setVec3(std::string_view property_name, Vec3 value)
{
    m_Impl.upd()->setVec3(property_name, value);
}

std::optional<Vec4> osc::MaterialPropertyBlock::getVec4(std::string_view property_name) const
{
    return m_Impl->getVec4(property_name);
}

void osc::MaterialPropertyBlock::setVec4(std::string_view property_name, Vec4 value)
{
    m_Impl.upd()->setVec4(property_name, value);
}

std::optional<Mat3> osc::MaterialPropertyBlock::getMat3(std::string_view property_name) const
{
    return m_Impl->getMat3(property_name);
}

void osc::MaterialPropertyBlock::setMat3(std::string_view property_name, const Mat3& value)
{
    m_Impl.upd()->setMat3(property_name, value);
}

std::optional<Mat4> osc::MaterialPropertyBlock::getMat4(std::string_view property_name) const
{
    return m_Impl->getMat4(property_name);
}

void osc::MaterialPropertyBlock::setMat4(std::string_view property_name, const Mat4& value)
{
    m_Impl.upd()->setMat4(property_name, value);
}

std::optional<int32_t> osc::MaterialPropertyBlock::getInt(std::string_view property_name) const
{
    return m_Impl->getInt(property_name);
}

void osc::MaterialPropertyBlock::setInt(std::string_view property_name, int32_t value)
{
    m_Impl.upd()->setInt(property_name, value);
}

std::optional<bool> osc::MaterialPropertyBlock::getBool(std::string_view property_name) const
{
    return m_Impl->getBool(property_name);
}

void osc::MaterialPropertyBlock::setBool(std::string_view property_name, bool value)
{
    m_Impl.upd()->setBool(property_name, value);
}

std::optional<Texture2D> osc::MaterialPropertyBlock::getTexture(std::string_view property_name) const
{
    return m_Impl->getTexture(property_name);
}

void osc::MaterialPropertyBlock::setTexture(std::string_view property_name, Texture2D texture)
{
    m_Impl.upd()->setTexture(property_name, std::move(texture));
}

bool osc::operator==(const MaterialPropertyBlock& lhs, const MaterialPropertyBlock& rhs)
{
    return lhs.m_Impl == rhs.m_Impl || *lhs.m_Impl == *rhs.m_Impl;
}

std::ostream& osc::operator<<(std::ostream& o, const MaterialPropertyBlock&)
{
    return o << "MaterialPropertyBlock()";
}

namespace
{
    constexpr auto c_mesh_topology_strings = std::to_array<CStringView>(
    {
        "Triangles",
        "Lines",
    });
    static_assert(c_mesh_topology_strings.size() == num_options<MeshTopology>());

    union PackedIndex {
        uint32_t u32;
        struct U16Pack { uint16_t a; uint16_t b; } u16;
    };

    static_assert(sizeof(PackedIndex) == sizeof(uint32_t));
    static_assert(alignof(PackedIndex) == alignof(uint32_t));

    GLenum to_opengl_topology_enum(MeshTopology mesh_topology)
    {
        static_assert(num_options<MeshTopology>() == 2);

        switch (mesh_topology) {
        case MeshTopology::Triangles: return GL_TRIANGLES;
        case MeshTopology::Lines:     return GL_LINES;
        default:                      return GL_TRIANGLES;
        }
    }

    // types that can be read/written to/from a vertex buffer by higher
    // levels of the API
    template<typename T>
    concept UserFacingVertexData = IsAnyOf<T, Vec2, Vec3, Vec4, Vec<4, Unorm8>, Color, Color32>;

    // types that are encode-/decode-able into a vertex buffer
    template<typename T>
    concept VertexBufferComponent = IsAnyOf<T, float, Unorm8>;

    // low-level single-component decode/encode functions
    template<VertexBufferComponent EncodedValue, typename DecodedValue>
    DecodedValue decode(const std::byte*);

    template<typename DecodedValue, VertexBufferComponent EncodedValue>
    void encode(std::byte*, DecodedValue);

    template<>
    float decode<float, float>(const std::byte* p)
    {
        return *std::launder(reinterpret_cast<const float*>(p));
    }

    template<>
    void encode<float, float>(std::byte* p, float v)
    {
        *std::launder(reinterpret_cast<float*>(p)) = v;
    }

    template<>
    float decode<Unorm8, float>(const std::byte* p)
    {
        return Unorm8{*p}.normalized_value();
    }

    template<>
    void encode<float, Unorm8>(std::byte* p, float v)
    {
        *p = Unorm8{v}.byte();
    }

    template<>
    Unorm8 decode<Unorm8, Unorm8>(const std::byte* p)
    {
        return Unorm8{*p};
    }

    template<>
    void encode<Unorm8, Unorm8>(std::byte* p, Unorm8 v)
    {
        *p = v.byte();
    }

    // mid-level multi-component decode/encode functions
    template<UserFacingVertexData T, VertexAttributeFormat EncodingFormat>
    void encode_many(std::byte* ptr, const T& values)
    {
        using ComponentType = typename VertexAttributeFormatTraits<EncodingFormat>::component_type;
        constexpr auto num_components = num_components_in(EncodingFormat);
        constexpr auto sizeof_component = component_size(EncodingFormat);
        constexpr auto n = min(std::tuple_size_v<T>, static_cast<typename T::size_type>(num_components));

        for (typename T::size_type i = 0; i < n; ++i) {
            encode<typename T::value_type, ComponentType>(ptr + i*sizeof_component, values[i]);
        }
    }

    template<VertexAttributeFormat EncodingFormat, UserFacingVertexData T>
    T decode_many(const std::byte* ptr)
    {
        using ComponentType = typename VertexAttributeFormatTraits<EncodingFormat>::component_type;
        constexpr auto num_components = num_components_in(EncodingFormat);
        constexpr auto sizeof_component = component_size(EncodingFormat);
        constexpr auto n = min(std::tuple_size_v<T>, static_cast<typename T::size_type>(num_components));

        T rv{};
        for (typename T::size_type i = 0; i < n; ++i) {
            rv[i] = decode<ComponentType, typename T::value_type>(ptr + i*sizeof_component);
        }
        return rv;
    }

    // high-level, compile-time multi-component decode + encode definition
    template<UserFacingVertexData T>
    class MultiComponentEncoding final {
    public:
        explicit MultiComponentEncoding(VertexAttributeFormat attribute_format)
        {
            static_assert(num_options<VertexAttributeFormat>() == 4);

            switch (attribute_format) {
            case VertexAttributeFormat::Float32x2:
                encoder_ = encode_many<T, VertexAttributeFormat::Float32x2>;
                decoder_ = decode_many<VertexAttributeFormat::Float32x2, T>;
                break;
            case VertexAttributeFormat::Float32x3:
                encoder_ = encode_many<T, VertexAttributeFormat::Float32x3>;
                decoder_ = decode_many<VertexAttributeFormat::Float32x3, T>;
                break;
            default:
            case VertexAttributeFormat::Float32x4:
                encoder_ = encode_many<T, VertexAttributeFormat::Float32x4>;
                decoder_ = decode_many<VertexAttributeFormat::Float32x4, T>;
                break;
            case VertexAttributeFormat::Unorm8x4:
                encoder_ = encode_many<T, VertexAttributeFormat::Unorm8x4>;
                decoder_ = decode_many<VertexAttributeFormat::Unorm8x4, T>;
                break;
            }
        }

        void encode(std::byte* ptr, const T& values) const
        {
            encoder_(ptr, values);
        }

        T decode(const std::byte* ptr) const
        {
            return decoder_(ptr);
        }

        friend bool operator==(const MultiComponentEncoding&, const MultiComponentEncoding&) = default;
    private:
        using Encoder = void(*)(std::byte*, const T&);
        Encoder encoder_;

        using Decoder = T(*)(const std::byte*);
        Decoder decoder_;
    };

    // a single compile-time reencoding function
    //
    // decodes in-memory data in a source format, converts it to a desination format, and then
    // writes it to the destination memory
    template<VertexAttributeFormat SourceFormat, VertexAttributeFormat DestinationFormat>
    void reencode_many(std::span<const std::byte> src, std::span<std::byte> dest)
    {
        using SourceCPUFormat = typename VertexAttributeFormatTraits<SourceFormat>::type;
        using DestCPUFormat = typename VertexAttributeFormatTraits<DestinationFormat>::type;
        constexpr auto n = min(std::tuple_size_v<SourceCPUFormat>, std::tuple_size_v<DestCPUFormat>);

        const auto decoded = decode_many<SourceFormat, SourceCPUFormat>(src.data());
        DestCPUFormat converted{};
        for (size_t i = 0; i < n; ++i) {
            converted[i] = typename DestCPUFormat::value_type{decoded[i]};
        }
        encode_many<DestCPUFormat, DestinationFormat>(dest.data(), converted);
    }

    // type-erased (i.e. runtime) reencoder function
    using ReencoderFunction = void(*)(std::span<const std::byte>, std::span<std::byte>);

    // compile-time lookup table (LUT) for runtime reencoder functions
    class ReencoderLut final {
    private:
        static constexpr size_t index_of(VertexAttributeFormat source_format, VertexAttributeFormat destination_format)
        {
            return static_cast<size_t>(source_format)*num_options<VertexAttributeFormat>() + static_cast<size_t>(destination_format);
        }

        template<VertexAttributeFormat... Formats>
        static constexpr void write_entries_top_level(ReencoderLut& lut, OptionList<VertexAttributeFormat, Formats...>)
        {
            (write_entries<Formats, Formats...>(lut), ...);
        }

        template<VertexAttributeFormat SourceFormat, VertexAttributeFormat... DestinationFormats>
        static constexpr void write_entries(ReencoderLut& lut)
        {
            (write_entry<SourceFormat, DestinationFormats>(lut), ...);
        }

        template<VertexAttributeFormat SourceFormat, VertexAttributeFormat DestinationFormat>
        static constexpr void write_entry(ReencoderLut& lut)
        {
            lut.assign(SourceFormat, DestinationFormat, reencode_many<SourceFormat, DestinationFormat>);
        }
    public:
        constexpr ReencoderLut()
        {
            write_entries_top_level(*this, VertexAttributeFormatList{});
        }

        constexpr void assign(
            VertexAttributeFormat source_format,
            VertexAttributeFormat destination_format,
            ReencoderFunction reencoder_function)
        {
            storage_.at(index_of(source_format, destination_format)) = reencoder_function;
        }

        constexpr const ReencoderFunction& lookup(
            VertexAttributeFormat source_format,
            VertexAttributeFormat destination_format) const
        {
            return storage_.at(index_of(source_format, destination_format));
        }

    private:
        std::array<ReencoderFunction, num_options<VertexAttributeFormat>()*num_options<VertexAttributeFormat>()> storage_{};
    };

    constexpr ReencoderLut c_reencoder_lut;

    struct VertexBufferAttributeReencoder final {
        ReencoderFunction reencoder_function;
        size_t source_offset;
        size_t source_stride;
        size_t destination_offset;
        size_t destination_stride;
    };

    std::vector<VertexBufferAttributeReencoder> get_attribute_reencoders(
        const VertexFormat& source_format,
        const VertexFormat& destination_format)
    {
        std::vector<VertexBufferAttributeReencoder> rv;
        rv.reserve(destination_format.numAttributes());  // guess

        for (const auto destination_layout : destination_format.attributeLayouts()) {
            if (const auto source_layout = source_format.attributeLayout(destination_layout.attribute())) {
                rv.push_back({
                    c_reencoder_lut.lookup(source_layout->format(), destination_layout.format()),
                    source_layout->offset(),
                    source_layout->stride(),
                    destination_layout.offset(),
                    destination_layout.stride(),
                });
            }
        }
        return rv;
    }

    void reencode_vertex_buffer(
        std::span<const std::byte> source_bytes,
        const VertexFormat& source_format,
        std::span<std::byte> destination_bytes,
        const VertexFormat& destination_format)
    {
        const size_t source_stride = source_format.stride();
        const size_t destination_stride = destination_format.stride();

        if (source_stride == 0 or destination_stride == 0) {
            return;  // no reencoding necessary
        }

        OSC_ASSERT(source_bytes.size() % source_stride == 0);
        OSC_ASSERT(destination_bytes.size() % destination_stride == 0);

        const size_t n = min(source_bytes.size() / source_stride, destination_bytes.size() / destination_stride);

        const auto reencoders = get_attribute_reencoders(source_format, destination_format);
        for (size_t i = 0; i < n; ++i) {
            const auto source_vertex_data = source_bytes.subspan(i*source_stride);
            const auto destination_vertex_data = destination_bytes.subspan(i*destination_stride);

            for (const auto& reencoder : reencoders) {
                const auto source_attr_data = source_vertex_data.subspan(reencoder.source_offset, reencoder.source_stride);
                const auto destination_attr_data = destination_vertex_data.subspan(reencoder.destination_offset, reencoder.destination_stride);
                reencoder.reencoder_function(source_attr_data, destination_attr_data);
            }
        }
    }

    // reperesents vertex data on the CPU
    class VertexBuffer final {
    public:

        // proxies (via encoders/decoders) access to a value in the vertex buffer's bytes
        template<UserFacingVertexData T, bool IsConst>
        class AttributeValueProxy final {
        public:
            using Byte = std::conditional_t<IsConst, const std::byte, std::byte>;

            AttributeValueProxy(Byte* data, MultiComponentEncoding<T> encoding) :
                data_{data},
                encoding_{encoding}
            {}

            AttributeValueProxy& operator=(const T& value)
                requires (not IsConst)
            {
                encoding_.encode(data_, value);
                return *this;
            }

            operator T () const  // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
            {
                return encoding_.decode(data_);
            }

            template<typename U>
            requires (not IsConst)
            AttributeValueProxy& operator/=(const U& value)
            {
                return *this = (T{*this} /= value);
            }

            template<typename U>
            requires (not IsConst)
            AttributeValueProxy& operator+=(const U& value)
            {
                return *this = (T{*this} += value);
            }
        private:
            Byte* data_;
            MultiComponentEncoding<T> encoding_;
        };

        // iterator for vertex buffer's contents (via encoders/decoders)
        template<UserFacingVertexData T, bool IsConst>
        class AttributeValueIterator final {
        public:
            using difference_type = ptrdiff_t;
            using value_type = AttributeValueProxy<T, IsConst>;
            using reference = value_type;
            using iterator_category = std::random_access_iterator_tag;

            using Byte = std::conditional_t<IsConst, const std::byte, std::byte>;

            AttributeValueIterator(
                Byte* data,
                size_t stride,
                MultiComponentEncoding<T> encoding) :

                data_{data},
                stride_{stride},
                encoding_{encoding}
            {}

            AttributeValueProxy<T, IsConst> operator*() const
            {
                return AttributeValueProxy<T, IsConst>{data_, encoding_};
            }

            AttributeValueIterator& operator++()
            {
                *this += 1;
                return *this;
            }

            AttributeValueIterator operator++(int)
            {
                auto tmp = *this;
                ++(*this);
                return tmp;
            }

            AttributeValueIterator& operator--()
            {
                *this -= 1;
                return *this;
            }

            AttributeValueIterator operator--(int)
            {
                auto tmp = *this;
                --(*this);
                return tmp;
            }

            AttributeValueIterator& operator+=(difference_type i)
            {
                data_ += i*stride_;
                return *this;
            }

            AttributeValueIterator operator+(difference_type i) const
            {
                auto copy = *this;
                copy += i;
                return copy;
            }

            AttributeValueIterator& operator-=(difference_type i)
            {
                data_ -= i*stride_;
            }

            AttributeValueIterator operator-(difference_type i)
            {
                auto copy = *this;
                copy -= i;
                return copy;
            }

            difference_type operator-(const AttributeValueIterator& rhs) const
            {
                return (data_ - rhs.data_) / stride_;
            }

            AttributeValueProxy<T, IsConst> operator[](difference_type n) const
            {
                return *(*this + n);
            }

            bool operator<(const AttributeValueIterator& rhs) const
            {
                return data_ < rhs.data_;
            }

            bool operator>(const AttributeValueIterator& rhs) const
            {
                return data_ > rhs.data_;
            }

            bool operator<=(const AttributeValueIterator& rhs) const
            {
                return data_ <= rhs.data_;
            }

            bool operator>=(const AttributeValueIterator& rhs) const
            {
                return data_ >= rhs.data_;
            }

            friend bool operator==(const AttributeValueIterator&, const AttributeValueIterator&) = default;
        private:
            Byte* data_;
            size_t stride_;
            MultiComponentEncoding<T> encoding_;
        };

        // range (C++20) for vertex buffer's contents
        template<UserFacingVertexData T, bool IsConst>
        class AttributeValueRange final {
        public:
            using Byte = std::conditional_t<IsConst, const std::byte, std::byte>;
            using iterator = AttributeValueIterator<T, IsConst>;
            using value_type = typename iterator::value_type;
            using difference_type = typename iterator::difference_type;

            AttributeValueRange() = default;

            AttributeValueRange(
                std::span<Byte> data,
                size_t stride,
                VertexAttributeFormat format) :

                data_{data},
                stride_{stride},
                encoding_{format}
            {}

            difference_type size() const
            {
                return std::distance(begin(), end());
            }

            iterator begin() const
            {
                return {data_.data(), stride_, encoding_};
            }

            iterator end() const
            {
                return {data_.data() + data_.size(), stride_, encoding_};
            }

            value_type at(difference_type i) const
            {
                return at(*this, i);
            }

            value_type at(difference_type i)
            {
                return at(*this, i);
            }

            value_type operator[](difference_type i) const
            {
                return begin()[i];
            }
        private:
            template<typename AttrValueRange>
            static value_type at(AttrValueRange&& range, difference_type i)
            {
                const auto beg = range.begin();
                if (i >= std::distance(beg, range.end())) {
                    throw std::out_of_range{"an attribute value was out-of-range: this is usually because of out-of-range mesh indices"};
                }
                return beg[i];
            }

            std::span<Byte> data_{};
            size_t stride_ = 1;  // care: divide by zero in an iterator is UB
            MultiComponentEncoding<T> encoding_{VertexAttributeFormat::Float32x3};  // dummy, for default ctor
        };

        // default ctor: make an empty buffer
        VertexBuffer() = default;

        // formatted ctor: make a buffer of the specified size+format
        VertexBuffer(size_t num_verts, const VertexFormat& format) :
            data_(num_verts * format.stride()),
            vertex_format_{format}
        {}

        void clear()
        {
            data_.clear();
            vertex_format_.clear();
        }

        size_t num_verts() const
        {
            return !vertex_format_.empty() ? (data_.size() / vertex_format_.stride()) : 0;
        }

        size_t num_attributes() const
        {
            return vertex_format_.numAttributes();
        }

        size_t stride() const
        {
            return vertex_format_.stride();
        }

        [[nodiscard]] bool has_verts() const
        {
            return num_verts() > 0;
        }

        std::span<const std::byte> bytes() const
        {
            return data_;
        }

        const VertexFormat& format() const
        {
            return vertex_format_;
        }

        auto attribute_layouts() const
        {
            return vertex_format_.attributeLayouts();
        }

        bool has_attribute(VertexAttribute attribute) const
        {
            return vertex_format_.contains(attribute);
        }

        template<UserFacingVertexData T>
        auto iter(VertexAttribute attribute) const
        {
            if (const auto layout = vertex_format_.attributeLayout(attribute)) {
                std::span<const std::byte> offset_span{data_.data() + layout->offset(), data_.size()};

                return AttributeValueRange<T, true>{
                    offset_span,
                    vertex_format_.stride(),
                    layout->format(),
                };
            }
            else {
                return AttributeValueRange<T, true>{};
            }
        }

        template<UserFacingVertexData T>
        auto iter(VertexAttribute attribute)
        {
            if (const auto layout = vertex_format_.attributeLayout(attribute)) {
                std::span<std::byte> offset_span{data_.data() + layout->offset(), data_.size()};
                return AttributeValueRange<T, false>{
                    offset_span,
                    vertex_format_.stride(),
                    layout->format(),
                };
            }
            else {
                return AttributeValueRange<T, false>{};
            }
        }

        template<UserFacingVertexData T>
        std::vector<T> read(VertexAttribute attribute) const
        {
            auto range = iter<T>(attribute);
            return std::vector<T>(range.begin(), range.end());
        }

        template<UserFacingVertexData T>
        void write(VertexAttribute attribute, std::span<const T> values)
        {
            // edge-case: size == 0 should be treated as "wipe/ignore it"
            if (values.empty()) {
                if (vertex_format_.contains(attribute)) {
                    VertexFormat new_format{vertex_format_};
                    new_format.erase(attribute);
                    set_params(num_verts(), new_format);
                }
                return;  // ignore/wipe
            }

            if (attribute != VertexAttribute::Position) {
                if (values.size() != num_verts()) {
                    // non-`Position` attributes must be size-matched
                    return;
                }

                if (not vertex_format_.contains(VertexAttribute::Position)) {
                    // callers must've already assigned `Position` before this
                    // function is able to assign additional attributes
                    return;
                }
            }

            if (not vertex_format_.contains(attribute)) {
                // reformat
                VertexFormat new_format{vertex_format_};
                new_format.insert({attribute, default_format(attribute)});
                set_params(values.size(), new_format);
            }
            else if (values.size() != num_verts()) {
                // resize
                set_params(values.size(), vertex_format_);
            }

            // write els to vertex buffer
            copy(values.begin(), values.end(), iter<T>(attribute).begin());
        }

        template<UserFacingVertexData T, typename UnaryOperation>
        requires std::invocable<UnaryOperation, T>
        void transform_attribute(VertexAttribute attribute, UnaryOperation f)
        {
            for (auto&& proxy : iter<T>(attribute)) {
                proxy = f(proxy);
            }
        }

        bool emplace_attribute_descriptor(VertexAttributeDescriptor descriptor)
        {
            if (has_attribute(descriptor.attribute())) {
                return false;
            }

            auto copy = format();
            copy.insert(descriptor);
            set_format(copy);
            return true;
        }

        void set_params(size_t new_num_verts, const VertexFormat& new_format)
        {
            if (data_.empty()) {
                // zero-initialize the buffer in the "new" format
                data_.resize(new_num_verts * new_format.stride());
                vertex_format_ = new_format;
            }

            if (new_format != vertex_format_) {
                // initialize a new buffer and re-encode the old one in the new format
                std::vector<std::byte> new_buffer(new_num_verts * new_format.stride());
                reencode_vertex_buffer(data_, vertex_format_, new_buffer, new_format);
                data_ = std::move(new_buffer);
                vertex_format_ = new_format;
            }
            else if (new_num_verts != num_verts()) {
                // resize (zero-initialized, if growing) the buffer
                data_.resize(new_num_verts * vertex_format_.stride());
            }
            else {
                // no change in format or size, do nothing
            }
        }

        void set_format(const VertexFormat& new_format)
        {
            set_params(num_verts(), new_format);
        }

        void set_data(std::span<const std::byte> data)
        {
            OSC_ASSERT(data.size() == data_.size() && "provided data size does not match the size of the vertex buffer");
            data_.assign(data.begin(), data.end());
        }
    private:
        std::vector<std::byte> data_;
        VertexFormat vertex_format_;
    };
}

class osc::Mesh::Impl final {
public:

    MeshTopology getTopology() const
    {
        return m_Topology;
    }

    void setTopology(MeshTopology newTopology)
    {
        m_Topology = newTopology;
        m_Version->reset();
    }

    size_t getNumVerts() const
    {
        return m_VertexBuffer.num_verts();
    }

    bool hasVerts() const
    {
        return m_VertexBuffer.has_verts();
    }

    std::vector<Vec3> getVerts() const
    {
        return m_VertexBuffer.read<Vec3>(VertexAttribute::Position);
    }

    void setVerts(std::span<const Vec3> verts)
    {
        m_VertexBuffer.write<Vec3>(VertexAttribute::Position, verts);

        rangeCheckIndicesAndRecalculateBounds();
        m_Version->reset();
    }

    void transformVerts(const std::function<Vec3(Vec3)>& f)
    {
        m_VertexBuffer.transform_attribute<Vec3>(VertexAttribute::Position, f);

        rangeCheckIndicesAndRecalculateBounds();
        m_Version->reset();
    }

    void transformVerts(const Transform& t)
    {
        m_VertexBuffer.transform_attribute<Vec3>(VertexAttribute::Position, [&t](Vec3 v)
        {
            return t * v;
        });

        rangeCheckIndicesAndRecalculateBounds();
        m_Version->reset();
    }

    void transformVerts(const Mat4& m)
    {
        m_VertexBuffer.transform_attribute<Vec3>(VertexAttribute::Position, [&m](Vec3 v)
        {
            return Vec3{m * Vec4{v, 1.0f}};
        });

        rangeCheckIndicesAndRecalculateBounds();
        m_Version->reset();
    }

    bool hasNormals() const
    {
        return m_VertexBuffer.has_attribute(VertexAttribute::Normal);
    }

    std::vector<Vec3> getNormals() const
    {
        return m_VertexBuffer.read<Vec3>(VertexAttribute::Normal);
    }

    void setNormals(std::span<const Vec3> normals)
    {
        m_VertexBuffer.write<Vec3>(VertexAttribute::Normal, normals);

        m_Version->reset();
    }

    void transformNormals(const std::function<Vec3(Vec3)>& f)
    {
        m_VertexBuffer.transform_attribute<Vec3>(VertexAttribute::Normal, f);

        m_Version->reset();
    }

    bool hasTexCoords() const
    {
        return m_VertexBuffer.has_attribute(VertexAttribute::TexCoord0);
    }

    std::vector<Vec2> getTexCoords() const
    {
        return m_VertexBuffer.read<Vec2>(VertexAttribute::TexCoord0);
    }

    void setTexCoords(std::span<const Vec2> coords)
    {
        m_VertexBuffer.write<Vec2>(VertexAttribute::TexCoord0, coords);

        m_Version->reset();
    }

    void transformTexCoords(const std::function<Vec2(Vec2)>& f)
    {
        m_VertexBuffer.transform_attribute<Vec2>(VertexAttribute::TexCoord0, f);

        m_Version->reset();
    }

    std::vector<Color> getColors() const
    {
        return m_VertexBuffer.read<Color>(VertexAttribute::Color);
    }

    void setColors(std::span<const Color> colors)
    {
        m_VertexBuffer.write<Color>(VertexAttribute::Color, colors);

        m_Version.reset();
    }

    std::vector<Vec4> getTangents() const
    {
        return m_VertexBuffer.read<Vec4>(VertexAttribute::Tangent);
    }

    void setTangents(std::span<const Vec4> newTangents)
    {
        m_VertexBuffer.write<Vec4>(VertexAttribute::Tangent, newTangents);

        m_Version->reset();
    }

    size_t getNumIndices() const
    {
        return m_NumIndices;
    }

    MeshIndicesView getIndices() const
    {
        if (m_NumIndices <= 0)
        {
            return {};
        }
        else if (m_IndicesAre32Bit)
        {
            return {&m_IndicesData.front().u32, m_NumIndices};
        }
        else
        {
            return {&m_IndicesData.front().u16.a, m_NumIndices};
        }
    }

    void setIndices(MeshIndicesView indices, MeshUpdateFlags flags)
    {
        indices.isU16() ? setIndices(indices.toU16Span(), flags) : setIndices(indices.toU32Span(), flags);
    }

    void forEachIndexedVert(const std::function<void(Vec3)>& f) const
    {
        const auto positions = m_VertexBuffer.iter<Vec3>(VertexAttribute::Position).begin();
        for (auto idx : getIndices()) {
            f(positions[idx]);
        }
    }

    void forEachIndexedTriangle(const std::function<void(Triangle)>& f) const
    {
        if (m_Topology != MeshTopology::Triangles) {
            return;
        }

        const MeshIndicesView indices = getIndices();
        const size_t steps = (indices.size() / 3) * 3;

        const auto positions = m_VertexBuffer.iter<Vec3>(VertexAttribute::Position).begin();
        for (size_t i = 0; i < steps; i += 3) {
            f(Triangle{
                positions[indices[i]],
                positions[indices[i+1]],
                positions[indices[i+2]],
            });
        }
    }

    Triangle getTriangleAt(size_t firstIndexOffset) const
    {
        if (m_Topology != MeshTopology::Triangles) {
            throw std::runtime_error{"cannot call getTriangleAt on a non-triangular-topology mesh"};
        }

        const auto indices = getIndices();

        if (firstIndexOffset+2 >= indices.size()) {
            throw std::runtime_error{"provided first index offset is out-of-bounds"};
        }

        const auto verts = m_VertexBuffer.iter<Vec3>(VertexAttribute::Position);

        // can use unchecked access here: `indices` are range-checked on writing
        return Triangle{
            verts[indices[firstIndexOffset+0]],
            verts[indices[firstIndexOffset+1]],
            verts[indices[firstIndexOffset+2]],
        };
    }

    std::vector<Vec3> getIndexedVerts() const
    {
        std::vector<Vec3> rv;
        rv.reserve(getNumIndices());
        forEachIndexedVert([&rv](Vec3 v) { rv.push_back(v); });
        return rv;
    }

    const AABB& getBounds() const
    {
        return m_AABB;
    }

    void clear()
    {
        m_Version->reset();
        m_Topology = MeshTopology::Triangles;
        m_VertexBuffer.clear();
        m_IndicesAre32Bit = false;
        m_NumIndices = 0;
        m_IndicesData.clear();
        m_AABB = {};
        m_SubMeshDescriptors.clear();
    }

    size_t getSubMeshCount() const
    {
        return m_SubMeshDescriptors.size();
    }

    void pushSubMeshDescriptor(const SubMeshDescriptor& desc)
    {
        m_SubMeshDescriptors.push_back(desc);
    }

    const SubMeshDescriptor& getSubMeshDescriptor(size_t i) const
    {
        return m_SubMeshDescriptors.at(i);
    }

    void clearSubMeshDescriptors()
    {
        m_SubMeshDescriptors.clear();
    }

    size_t getVertexAttributeCount() const
    {
        return m_VertexBuffer.num_attributes();
    }

    const VertexFormat& getVertexAttributes() const
    {
        return m_VertexBuffer.format();
    }

    void setVertexBufferParams(size_t newNumVerts, const VertexFormat& newFormat)
    {
        m_VertexBuffer.set_params(newNumVerts, newFormat);

        rangeCheckIndicesAndRecalculateBounds();
        m_Version->reset();
    }

    size_t getVertexBufferStride() const
    {
        return m_VertexBuffer.stride();
    }

    void setVertexBufferData(std::span<const uint8_t> newData, MeshUpdateFlags flags)
    {
        m_VertexBuffer.set_data(std::as_bytes(newData));

        rangeCheckIndicesAndRecalculateBounds(flags);
        m_Version->reset();
    }

    void recalculateNormals()
    {
        if (getTopology() != MeshTopology::Triangles) {
            // if the mesh isn't triangle-based, do nothing
            return;
        }

        // ensure the vertex buffer has a normal attribute
        m_VertexBuffer.emplace_attribute_descriptor({VertexAttribute::Normal, VertexAttributeFormat::Float32x3});

        // calculate normals from triangle faces:
        //
        // - keep a count of the number of times a normal has been assigned
        // - compute the normal from the triangle
        // - if counts[i] == 0 assign it (we can't assume the buffer is zeroed - could be reused)
        // - else, add (accumulate)
        // - ++counts[i]
        // - at the end, if counts[i] > 1, then renormalize that normal (it contains a sum)

        const auto indices = getIndices();
        const auto positions = m_VertexBuffer.iter<Vec3>(VertexAttribute::Position);
        auto normals = m_VertexBuffer.iter<Vec3>(VertexAttribute::Normal);
        std::vector<uint16_t> counts(normals.size());

        for (size_t i = 0, len = 3*(indices.size()/3); i < len; i+=3) {
            // get triangle indices
            const Vec3uz idxs = {indices[i], indices[i+1], indices[i+2]};

            // get triangle
            const Triangle triangle = {positions[idxs[0]], positions[idxs[1]], positions[idxs[2]]};

            // calculate + validate triangle normal
            const auto normal = triangle_normal(triangle).unwrap();
            if (any_of(isnan(normal))) {
                continue;  // probably co-located, or invalid: don't accumulate it
            }

            // accumulate
            for (auto idx : idxs) {
                if (counts[idx] == 0) {
                    normals[idx] = normal;
                }
                else {
                    normals[idx] += normal;
                }
                ++counts[idx];
            }
        }

        // renormalize shared normals
        for (size_t i = 0; i < counts.size(); ++i) {
            if (counts[i] > 1) {
                normals[i] = normalize(Vec3{normals[i]});
            }
        }
    }

    void recalculateTangents()
    {
        if (getTopology() != MeshTopology::Triangles) {
            // if the mesh isn't triangle-based, do nothing
            return;
        }
        if (!m_VertexBuffer.has_attribute(VertexAttribute::Normal)) {
            // if the mesh doesn't have normals, do nothing
            return;
        }
        if (!m_VertexBuffer.has_attribute(VertexAttribute::TexCoord0)) {
            // if the mesh doesn't have texture coordinates, do nothing
            return;
        }
        if (m_IndicesData.empty()) {
            // if the mesh has no indices, do nothing
            return;
        }

        // ensure the vertex buffer has space for tangents
        m_VertexBuffer.emplace_attribute_descriptor({ VertexAttribute::Tangent, VertexAttributeFormat::Float32x3 });

        // calculate tangents

        const auto vbverts = m_VertexBuffer.iter<Vec3>(VertexAttribute::Position);
        const auto vbnormals = m_VertexBuffer.iter<Vec3>(VertexAttribute::Normal);
        const auto vbtexcoords = m_VertexBuffer.iter<Vec2>(VertexAttribute::TexCoord0);

        const auto tangents = CalcTangentVectors(
            MeshTopology::Triangles,
            std::vector<Vec3>(vbverts.begin(), vbverts.end()),
            std::vector<Vec3>(vbnormals.begin(), vbnormals.end()),
            std::vector<Vec2>(vbtexcoords.begin(), vbtexcoords.end()),
            getIndices()
        );

        m_VertexBuffer.write<Vec4>(VertexAttribute::Tangent, tangents);
    }

    // non-PIMPL methods

    gl::VertexArray& updVertexArray()
    {
        if (!*m_MaybeGPUBuffers || (*m_MaybeGPUBuffers)->data_version != *m_Version)
        {
            uploadToGPU();
        }
        return (*m_MaybeGPUBuffers)->vao;
    }

    void drawInstanced(
        size_t n,
        std::optional<size_t> maybe_submesh_index)
    {
        const SubMeshDescriptor descriptor = maybe_submesh_index ?
            m_SubMeshDescriptors.at(*maybe_submesh_index) :         // draw the requested sub-mesh
            SubMeshDescriptor{0, m_NumIndices, m_Topology};       // else: draw the entire mesh as a "sub mesh"

        // convert mesh/descriptor data types into OpenGL-compatible formats
        const GLenum mode = to_opengl_topology_enum(descriptor.getTopology());
        const auto count = static_cast<GLsizei>(descriptor.getIndexCount());
        const GLenum type = m_IndicesAre32Bit ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;

        const size_t bytesPerIndex = m_IndicesAre32Bit ? sizeof(GLint) : sizeof(GLshort);
        const size_t firstIndexByteOffset = descriptor.getIndexStart() * bytesPerIndex;
        const void* indices = cpp20::bit_cast<void*>(firstIndexByteOffset);

        const auto instanceCount = static_cast<GLsizei>(n);

        glDrawElementsInstanced(
            mode,
            count,
            type,
            indices,
            instanceCount
        );
    }

private:

    void setIndices(std::span<const uint16_t> indices, MeshUpdateFlags flags)
    {
        m_IndicesAre32Bit = false;
        m_NumIndices = indices.size();
        m_IndicesData.resize((indices.size()+1)/2);
        copy(indices, &m_IndicesData.front().u16.a);

        rangeCheckIndicesAndRecalculateBounds(flags);
        m_Version->reset();
    }

    void setIndices(std::span<const uint32_t> vs, MeshUpdateFlags flags)
    {
        const auto isGreaterThanU16Max = [](uint32_t v)
        {
            return v > std::numeric_limits<uint16_t>::max();
        };

        if (any_of(vs, isGreaterThanU16Max))
        {
            m_IndicesAre32Bit = true;
            m_NumIndices = vs.size();
            m_IndicesData.resize(vs.size());
            copy(vs, &m_IndicesData.front().u32);
        }
        else
        {
            m_IndicesAre32Bit = false;
            m_NumIndices = vs.size();
            m_IndicesData.resize((vs.size()+1)/2);
            for (size_t i = 0; i < vs.size(); ++i)
            {
                (&m_IndicesData.front().u16.a)[i] = static_cast<uint16_t>(vs[i]);
            }
        }

        rangeCheckIndicesAndRecalculateBounds(flags);
        m_Version->reset();
    }

    void rangeCheckIndicesAndRecalculateBounds(
        MeshUpdateFlags flags = MeshUpdateFlags::Default)
    {
        // note: recalculating bounds will always validate indices anyway, because it's assumed
        //       that the caller's intention is that all indices are valid when computing the
        //       bounds
        const bool checkIndices = !((flags & MeshUpdateFlags::DontValidateIndices) && (flags & MeshUpdateFlags::DontRecalculateBounds));

        //       ... but it's perfectly reasonable for the caller to only want the indices to be
        //       validated, leaving the bounds untouched
        const bool recalculateBounds = !(flags & MeshUpdateFlags::DontRecalculateBounds);

        if (checkIndices && recalculateBounds)
        {
            if (m_NumIndices == 0)
            {
                m_AABB = {};
                return;
            }

            // recalculate bounds while also checking indices
            m_AABB.min =
            {
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
            };

            m_AABB.max =
            {
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest(),
            };

            auto range = m_VertexBuffer.iter<Vec3>(VertexAttribute::Position);
            for (auto idx : getIndices())
            {
                Vec3 pos = range.at(idx);  // bounds-check index
                m_AABB.min = elementwise_min(m_AABB.min, pos);
                m_AABB.max = elementwise_max(m_AABB.max, pos);
            }
        }
        else if (checkIndices && !recalculateBounds)
        {
            for (auto meshIndex : getIndices())
            {
                OSC_ASSERT(meshIndex < m_VertexBuffer.num_verts() && "a mesh index is out of bounds");
            }
        }
        else
        {
            return;  // do nothing
        }
    }

    static GLuint GetVertexAttributeIndex(VertexAttribute attr)
    {
        auto constexpr lut = []<VertexAttribute... Attrs>(OptionList<VertexAttribute, Attrs...>)
        {
            return std::to_array({ VertexAttributeTraits<Attrs>::shader_location... });
        }(VertexAttributeList{});

        return lut.at(to_index(attr));
    }

    static GLint GetVertexAttributeSize(const VertexAttributeFormat& format)
    {
        return static_cast<GLint>(num_components_in(format));
    }

    static GLenum GetVertexAttributeType(const VertexAttributeFormat& format)
    {
        static_assert(num_options<VertexAttributeFormat>() == 4);

        switch (format) {
        case VertexAttributeFormat::Float32x2:
        case VertexAttributeFormat::Float32x3:
        case VertexAttributeFormat::Float32x4:
            return GL_FLOAT;
        case VertexAttributeFormat::Unorm8x4:
            return GL_UNSIGNED_BYTE;
        default:
            throw std::runtime_error{"nyi"};
        }
    }

    static GLboolean GetVertexAttributeNormalized(const VertexAttributeFormat& format)
    {
        static_assert(num_options<VertexAttributeFormat>() == 4);

        switch (format) {
        case VertexAttributeFormat::Float32x2:
        case VertexAttributeFormat::Float32x3:
        case VertexAttributeFormat::Float32x4:
            return GL_FALSE;
        case VertexAttributeFormat::Unorm8x4:
            return GL_TRUE;
        default:
            throw std::runtime_error{"nyi"};
        }
    }

    static void OpenGLBindVertexAttribute(const VertexFormat& format, const VertexFormat::VertexAttributeLayout& layout)
    {
        glVertexAttribPointer(
            GetVertexAttributeIndex(layout.attribute()),
            GetVertexAttributeSize(layout.format()),
            GetVertexAttributeType(layout.format()),
            GetVertexAttributeNormalized(layout.format()),
            static_cast<GLsizei>(format.stride()),
            cpp20::bit_cast<void*>(layout.offset())
        );
        glEnableVertexAttribArray(GetVertexAttributeIndex(layout.attribute()));
    }

    void uploadToGPU()
    {
        // allocate GPU-side buffers (or re-use the last ones)
        if (!(*m_MaybeGPUBuffers))
        {
            *m_MaybeGPUBuffers = MeshOpenGLData{};
        }
        MeshOpenGLData& buffers = **m_MaybeGPUBuffers;

        // upload CPU-side vector data into the GPU-side buffer
        OSC_ASSERT(cpp20::bit_cast<uintptr_t>(m_VertexBuffer.bytes().data()) % alignof(float) == 0);
        gl::bind_buffer(
            GL_ARRAY_BUFFER,
            buffers.array_buffer
        );
        gl::buffer_data(
            GL_ARRAY_BUFFER,
            static_cast<GLsizei>(m_VertexBuffer.bytes().size()),
            m_VertexBuffer.bytes().data(),
            GL_STATIC_DRAW
        );

        // upload CPU-side element data into the GPU-side buffer
        const size_t eboNumBytes = m_NumIndices * (m_IndicesAre32Bit ? sizeof(uint32_t) : sizeof(uint16_t));
        gl::bind_buffer(
            GL_ELEMENT_ARRAY_BUFFER,
            buffers.indices_buffer
        );
        gl::buffer_data(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizei>(eboNumBytes),
            m_IndicesData.data(),
            GL_STATIC_DRAW
        );

        // configure mesh-level VAO
        gl::bind_vertex_array(buffers.vao);
        gl::bind_buffer(GL_ARRAY_BUFFER, buffers.array_buffer);
        gl::bind_buffer(GL_ELEMENT_ARRAY_BUFFER, buffers.indices_buffer);
        for (auto&& layout : m_VertexBuffer.attribute_layouts())
        {
            OpenGLBindVertexAttribute(m_VertexBuffer.format(), layout);
        }
        gl::bind_vertex_array();

        buffers.data_version = *m_Version;
    }

    DefaultConstructOnCopy<UID> m_Version;
    MeshTopology m_Topology = MeshTopology::Triangles;
    VertexBuffer m_VertexBuffer;

    bool m_IndicesAre32Bit = false;
    size_t m_NumIndices = 0;
    std::vector<PackedIndex> m_IndicesData;

    AABB m_AABB = {};

    std::vector<SubMeshDescriptor> m_SubMeshDescriptors;

    DefaultConstructOnCopy<std::optional<MeshOpenGLData>> m_MaybeGPUBuffers;
};

std::ostream& osc::operator<<(std::ostream& o, MeshTopology mt)
{
    return o << c_mesh_topology_strings.at(static_cast<size_t>(mt));
}

osc::Mesh::Mesh() :
    m_Impl{make_cow<Impl>()}
{
}

MeshTopology osc::Mesh::getTopology() const
{
    return m_Impl->getTopology();
}

void osc::Mesh::setTopology(MeshTopology topology)
{
    m_Impl.upd()->setTopology(topology);
}

size_t osc::Mesh::getNumVerts() const
{
    return m_Impl->getNumVerts();
}

bool osc::Mesh::hasVerts() const
{
    return m_Impl->hasVerts();
}

std::vector<Vec3> osc::Mesh::getVerts() const
{
    return m_Impl->getVerts();
}

void osc::Mesh::setVerts(std::span<const Vec3> verts)
{
    m_Impl.upd()->setVerts(verts);
}

void osc::Mesh::transformVerts(const std::function<Vec3(Vec3)>& f)
{
    m_Impl.upd()->transformVerts(f);
}

void osc::Mesh::transformVerts(const Transform& t)
{
    m_Impl.upd()->transformVerts(t);
}

void osc::Mesh::transformVerts(const Mat4& m)
{
    m_Impl.upd()->transformVerts(m);
}

bool osc::Mesh::hasNormals() const
{
    return m_Impl->hasNormals();
}

std::vector<Vec3> osc::Mesh::getNormals() const
{
    return m_Impl->getNormals();
}

void osc::Mesh::setNormals(std::span<const Vec3> verts)
{
    m_Impl.upd()->setNormals(verts);
}

void osc::Mesh::transformNormals(const std::function<Vec3(Vec3)>& f)
{
    m_Impl.upd()->transformNormals(f);
}

bool osc::Mesh::hasTexCoords() const
{
    return m_Impl->hasTexCoords();
}

std::vector<Vec2> osc::Mesh::getTexCoords() const
{
    return m_Impl->getTexCoords();
}

void osc::Mesh::setTexCoords(std::span<const Vec2> coords)
{
    m_Impl.upd()->setTexCoords(coords);
}

void osc::Mesh::transformTexCoords(const std::function<Vec2(Vec2)>& f)
{
    m_Impl.upd()->transformTexCoords(f);
}

std::vector<Color> osc::Mesh::getColors() const
{
    return m_Impl->getColors();
}

void osc::Mesh::setColors(std::span<const Color> colors)
{
    m_Impl.upd()->setColors(colors);
}

std::vector<Vec4> osc::Mesh::getTangents() const
{
    return m_Impl->getTangents();
}

void osc::Mesh::setTangents(std::span<const Vec4> newTangents)
{
    m_Impl.upd()->setTangents(newTangents);
}

size_t osc::Mesh::getNumIndices() const
{
    return m_Impl->getNumIndices();
}

MeshIndicesView osc::Mesh::getIndices() const
{
    return m_Impl->getIndices();
}

void osc::Mesh::setIndices(MeshIndicesView indices, MeshUpdateFlags flags)
{
    m_Impl.upd()->setIndices(indices, flags);
}

void osc::Mesh::forEachIndexedVert(const std::function<void(Vec3)>& f) const
{
    m_Impl->forEachIndexedVert(f);
}

void osc::Mesh::forEachIndexedTriangle(const std::function<void(Triangle)>& f) const
{
    m_Impl->forEachIndexedTriangle(f);
}

Triangle osc::Mesh::getTriangleAt(size_t firstIndexOffset) const
{
    return m_Impl->getTriangleAt(firstIndexOffset);
}

std::vector<Vec3> osc::Mesh::getIndexedVerts() const
{
    return m_Impl->getIndexedVerts();
}

const AABB& osc::Mesh::getBounds() const
{
    return m_Impl->getBounds();
}

void osc::Mesh::clear()
{
    m_Impl.upd()->clear();
}

size_t osc::Mesh::getSubMeshCount() const
{
    return m_Impl->getSubMeshCount();
}

void osc::Mesh::pushSubMeshDescriptor(const SubMeshDescriptor& desc)
{
    m_Impl.upd()->pushSubMeshDescriptor(desc);
}

const SubMeshDescriptor& osc::Mesh::getSubMeshDescriptor(size_t i) const
{
    return m_Impl->getSubMeshDescriptor(i);
}

void osc::Mesh::clearSubMeshDescriptors()
{
    m_Impl.upd()->clearSubMeshDescriptors();
}

size_t osc::Mesh::getVertexAttributeCount() const
{
    return m_Impl->getVertexAttributeCount();
}

const VertexFormat& osc::Mesh::getVertexAttributes() const
{
    return m_Impl->getVertexAttributes();
}

void osc::Mesh::setVertexBufferParams(size_t n, const VertexFormat& format)
{
    m_Impl.upd()->setVertexBufferParams(n, format);
}

size_t osc::Mesh::getVertexBufferStride() const
{
    return m_Impl->getVertexBufferStride();
}

void osc::Mesh::setVertexBufferData(std::span<const uint8_t> data, MeshUpdateFlags flags)
{
    m_Impl.upd()->setVertexBufferData(data, flags);
}

void osc::Mesh::recalculateNormals()
{
    m_Impl.upd()->recalculateNormals();
}

void osc::Mesh::recalculateTangents()
{
    m_Impl.upd()->recalculateTangents();
}

std::ostream& osc::operator<<(std::ostream& o, const Mesh&)
{
    return o << "Mesh()";
}


//////////////////////////////////
//
// camera stuff
//
//////////////////////////////////

namespace
{
    // LUT for human-readable form of the above
    constexpr auto c_CameraProjectionStrings = std::to_array<CStringView>(
    {
        "Perspective",
        "Orthographic",
    });
    static_assert(c_CameraProjectionStrings.size() == num_options<CameraProjection>());
}

class osc::Camera::Impl final {
public:

    void reset()
    {
        Impl newImpl;
        std::swap(*this, newImpl);
        m_RenderQueue = std::move(newImpl.m_RenderQueue);
    }

    Color background_color() const
    {
        return m_BackgroundColor;
    }

    void set_background_color(const Color& color)
    {
        m_BackgroundColor = color;
    }

    CameraProjection camera_projection() const
    {
        return m_CameraProjection;
    }

    void set_camera_projection(CameraProjection projection)
    {
        m_CameraProjection = projection;
    }

    float orthographic_size() const
    {
        return m_OrthographicSize;
    }

    void set_orthographic_size(float size)
    {
        m_OrthographicSize = size;
    }

    Radians vertical_fov() const
    {
        return m_PerspectiveFov;
    }

    void set_vertical_fov(Radians size)
    {
        m_PerspectiveFov = size;
    }

    float near_clipping_plane() const
    {
        return m_NearClippingPlane;
    }

    void set_near_clipping_plane(float distance)
    {
        m_NearClippingPlane = distance;
    }

    float get_far_clipping_plane() const
    {
        return m_FarClippingPlane;
    }

    void set_far_clipping_plane(float distance)
    {
        m_FarClippingPlane = distance;
    }

    CameraClearFlags clear_flags() const
    {
        return m_ClearFlags;
    }

    void set_clear_flags(CameraClearFlags flags)
    {
        m_ClearFlags = flags;
    }

    std::optional<Rect> pixel_rect() const
    {
        return m_MaybeScreenPixelRect;
    }

    void set_pixel_rect(std::optional<Rect> maybePixelRect)
    {
        m_MaybeScreenPixelRect = maybePixelRect;
    }

    std::optional<Rect> scissor_rect() const
    {
        return m_MaybeScissorRect;
    }

    void set_scissor_rect(std::optional<Rect> maybeScissorRect)
    {
        m_MaybeScissorRect = maybeScissorRect;
    }

    Vec3 position() const
    {
        return m_Position;
    }

    void set_position(const Vec3& position)
    {
        m_Position = position;
    }

    Quat rotation() const
    {
        return m_Rotation;
    }

    void set_rotation(const Quat& rotation)
    {
        m_Rotation = rotation;
    }

    Vec3 direction() const
    {
        return m_Rotation * Vec3{0.0f, 0.0f, -1.0f};
    }

    void set_direction(const Vec3& d)
    {
        m_Rotation = osc::rotation(Vec3{0.0f, 0.0f, -1.0f}, d);
    }

    Vec3 upwards_direction() const
    {
        return m_Rotation * Vec3{0.0f, 1.0f, 0.0f};
    }

    Mat4 view_matrix() const
    {
        if (m_MaybeViewMatrixOverride)
        {
            return *m_MaybeViewMatrixOverride;
        }
        else
        {
            return look_at(m_Position, m_Position + direction(), upwards_direction());
        }
    }

    std::optional<Mat4> view_matrix_override() const
    {
        return m_MaybeViewMatrixOverride;
    }

    void set_view_matrix_override(std::optional<Mat4> maybeViewMatrixOverride)
    {
        m_MaybeViewMatrixOverride = maybeViewMatrixOverride;
    }

    Mat4 projection_matrix(float aspect_ratio) const
    {
        if (m_MaybeProjectionMatrixOverride)
        {
            return *m_MaybeProjectionMatrixOverride;
        }
        else if (m_CameraProjection == CameraProjection::Perspective)
        {
            return perspective(
                m_PerspectiveFov,
                aspect_ratio,
                m_NearClippingPlane,
                m_FarClippingPlane
            );
        }
        else
        {
            const float height = m_OrthographicSize;
            const float width = height * aspect_ratio;

            const float right = 0.5f * width;
            const float left = -right;
            const float top = 0.5f * height;
            const float bottom = -top;

            return ortho(left, right, bottom, top, m_NearClippingPlane, m_FarClippingPlane);
        }
    }

    std::optional<Mat4> projection_matrix_override() const
    {
        return m_MaybeProjectionMatrixOverride;
    }

    void set_projection_matrix_override(std::optional<Mat4> maybeProjectionMatrixOverride)
    {
        m_MaybeProjectionMatrixOverride = maybeProjectionMatrixOverride;
    }

    Mat4 view_projection_matrix(float aspect_ratio) const
    {
        return projection_matrix(aspect_ratio) * view_matrix();
    }

    Mat4 inverse_view_projection_matrix(float aspect_ratio) const
    {
        return inverse(view_projection_matrix(aspect_ratio));
    }

    void render_to_screen()
    {
        GraphicsBackend::render_camera_queue(*this);
    }

    void render_to(RenderTexture& renderTexture)
    {
        static_assert(CameraClearFlags::All == (CameraClearFlags::SolidColor | CameraClearFlags::Depth));
        static_assert(num_options<RenderTextureReadWrite>() == 2);

        RenderTarget renderTargetThatWritesToRenderTexture
        {
            {
                RenderTargetColorAttachment
                {
                    // attach to render texture's color buffer
                    renderTexture.updColorBuffer(),

                    // load the color buffer based on this camera's clear flags
                    clear_flags() & CameraClearFlags::SolidColor ?
                        RenderBufferLoadAction::Clear :
                        RenderBufferLoadAction::Load,

                    RenderBufferStoreAction::Resolve,

                    // ensure clear color matches colorspace of render texture
                    renderTexture.getReadWrite() == RenderTextureReadWrite::sRGB ?
                        to_linear_colorspace(background_color()) :
                        background_color(),
                },
            },
            RenderTargetDepthAttachment
            {
                // attach to the render texture's depth buffer
                renderTexture.updDepthBuffer(),

                // load the depth buffer based on this camera's clear flags
                clear_flags() & CameraClearFlags::Depth ?
                    RenderBufferLoadAction::Clear :
                    RenderBufferLoadAction::Load,

                RenderBufferStoreAction::DontCare,
            },
        };

        render_to(renderTargetThatWritesToRenderTexture);
    }

    void render_to(RenderTarget& renderTarget)
    {
        GraphicsBackend::render_camera_queue(*this, &renderTarget);
    }

    friend bool operator==(const Impl&, const Impl&) = default;

private:
    friend class GraphicsBackend;

    Color m_BackgroundColor = Color::clear();
    CameraProjection m_CameraProjection = CameraProjection::Perspective;
    float m_OrthographicSize = 2.0f;
    Radians m_PerspectiveFov = 90_deg;
    float m_NearClippingPlane = 1.0f;
    float m_FarClippingPlane = -1.0f;
    CameraClearFlags m_ClearFlags = CameraClearFlags::Default;
    std::optional<Rect> m_MaybeScreenPixelRect = std::nullopt;
    std::optional<Rect> m_MaybeScissorRect = std::nullopt;
    Vec3 m_Position = {};
    Quat m_Rotation = identity<Quat>();
    std::optional<Mat4> m_MaybeViewMatrixOverride;
    std::optional<Mat4> m_MaybeProjectionMatrixOverride;
    std::vector<RenderObject> m_RenderQueue;
};



std::ostream& osc::operator<<(std::ostream& o, CameraProjection cp)
{
    return o << c_CameraProjectionStrings.at(static_cast<size_t>(cp));
}

osc::Camera::Camera() :
    impl_{make_cow<Impl>()}
{
}

void osc::Camera::reset()
{
    impl_.upd()->reset();
}

Color osc::Camera::background_color() const
{
    return impl_->background_color();
}

void osc::Camera::set_background_color(const Color& color)
{
    impl_.upd()->set_background_color(color);
}

CameraProjection osc::Camera::camera_projection() const
{
    return impl_->camera_projection();
}

void osc::Camera::set_camera_projection(CameraProjection projection)
{
    impl_.upd()->set_camera_projection(projection);
}

float osc::Camera::orthographic_size() const
{
    return impl_->orthographic_size();
}

void osc::Camera::set_orthographic_size(float sz)
{
    impl_.upd()->set_orthographic_size(sz);
}

Radians osc::Camera::vertical_fov() const
{
    return impl_->vertical_fov();
}

void osc::Camera::set_vertical_fov(Radians vertical_fov)
{
    impl_.upd()->set_vertical_fov(vertical_fov);
}

float osc::Camera::near_clipping_plane() const
{
    return impl_->near_clipping_plane();
}

void osc::Camera::set_near_clipping_plane(float d)
{
    impl_.upd()->set_near_clipping_plane(d);
}

float osc::Camera::get_far_clipping_plane() const
{
    return impl_->get_far_clipping_plane();
}

void osc::Camera::set_far_clipping_plane(float d)
{
    impl_.upd()->set_far_clipping_plane(d);
}

CameraClearFlags osc::Camera::clear_flags() const
{
    return impl_->clear_flags();
}

void osc::Camera::set_clear_flags(CameraClearFlags flags)
{
    impl_.upd()->set_clear_flags(flags);
}

std::optional<Rect> osc::Camera::pixel_rect() const
{
    return impl_->pixel_rect();
}

void osc::Camera::set_pixel_rect(std::optional<Rect> maybePixelRect)
{
    impl_.upd()->set_pixel_rect(maybePixelRect);
}

std::optional<Rect> osc::Camera::scissor_rect() const
{
    return impl_->scissor_rect();
}

void osc::Camera::set_scissor_rect(std::optional<Rect> maybeScissorRect)
{
    impl_.upd()->set_scissor_rect(maybeScissorRect);
}

Vec3 osc::Camera::position() const
{
    return impl_->position();
}

void osc::Camera::set_position(const Vec3& p)
{
    impl_.upd()->set_position(p);
}

Quat osc::Camera::rotation() const
{
    return impl_->rotation();
}

void osc::Camera::set_rotation(const Quat& rotation)
{
    impl_.upd()->set_rotation(rotation);
}

Vec3 osc::Camera::direction() const
{
    return impl_->direction();
}

void osc::Camera::set_direction(const Vec3& d)
{
    impl_.upd()->set_direction(d);
}

Vec3 osc::Camera::upwards_direction() const
{
    return impl_->upwards_direction();
}

Mat4 osc::Camera::view_matrix() const
{
    return impl_->view_matrix();
}

std::optional<Mat4> osc::Camera::view_matrix_override() const
{
    return impl_->view_matrix_override();
}

void osc::Camera::set_view_matrix_override(std::optional<Mat4> maybeViewMatrixOverride)
{
    impl_.upd()->set_view_matrix_override(maybeViewMatrixOverride);
}

Mat4 osc::Camera::projection_matrix(float aspect_ratio) const
{
    return impl_->projection_matrix(aspect_ratio);
}

std::optional<Mat4> osc::Camera::projection_matrix_override() const
{
    return impl_->projection_matrix_override();
}

void osc::Camera::set_projection_matrix_override(std::optional<Mat4> maybeProjectionMatrixOverride)
{
    impl_.upd()->set_projection_matrix_override(maybeProjectionMatrixOverride);
}

Mat4 osc::Camera::view_projection_matrix(float aspect_ratio) const
{
    return impl_->view_projection_matrix(aspect_ratio);
}

Mat4 osc::Camera::inverse_view_projection_matrix(float aspect_ratio) const
{
    return impl_->inverse_view_projection_matrix(aspect_ratio);
}

void osc::Camera::render_to_screen()
{
    impl_.upd()->render_to_screen();
}

void osc::Camera::render_to(RenderTexture& renderTexture)
{
    impl_.upd()->render_to(renderTexture);
}

void osc::Camera::render_to(RenderTarget& renderTarget)
{
    impl_.upd()->render_to(renderTarget);
}

std::ostream& osc::operator<<(std::ostream& o, const Camera& camera)
{
    return o << "Camera(position = " << camera.position() << ", direction = " << camera.direction() << ", projection = " << camera.camera_projection() << ')';
}

bool osc::operator==(const Camera& lhs, const Camera& rhs)
{
    return lhs.impl_ == rhs.impl_ || *lhs.impl_ == *rhs.impl_;
}


/////////////////////////////
//
// graphics context
//
/////////////////////////////

namespace
{
    struct RequiredOpenGLCapability final {
        GLenum id;
        CStringView label;
    };
    constexpr auto c_RequiredOpenGLCapabilities = std::to_array<RequiredOpenGLCapability>(
    {
        // ensures geometry is occlusion-culled correctly
        {GL_DEPTH_TEST, "GL_DEPTH_TEST"},

        // used to reduce pixel aliasing (jaggies)
        {GL_MULTISAMPLE, "GL_MULTISAMPLE"},

        // enables linear color rendering workflow
        //
        // in oscar, shader calculations are done in linear space, but reads/writes
        // from framebuffers respect whether they are internally using an sRGB format
        {GL_FRAMEBUFFER_SRGB, "GL_FRAMEBUFFER_SRGB"},

        // enable seamless cubemap sampling when sampling
        //
        // handy in Physically Based Rendering (PBR) workflows, which do advanced rendering
        // tricks, like writing to specific mip levels in cubemaps for irradiance sampling etc.
        {GL_TEXTURE_CUBE_MAP_SEAMLESS, "GL_TEXTURE_CUBE_MAP_SEAMLESS"},
    });

    // create an OpenGL context for an application window
    sdl::GLContext CreateOpenGLContext(SDL_Window& window)
    {
        log_debug("initializing OpenGL context");

        // create an OpenGL context for the application
        sdl::GLContext ctx = sdl::GL_CreateContext(&window);

        // enable the OpenGL context
        if (SDL_GL_MakeCurrent(&window, ctx.get()) != 0)
        {
            throw std::runtime_error{std::string{"SDL_GL_MakeCurrent failed: "} + SDL_GetError()};
        }

        // enable vsync by default
        //
        // vsync can feel a little laggy on some systems, but vsync reduces CPU usage
        // on *constrained* systems (e.g. laptops, which the majority of users are using)
        if (SDL_GL_SetSwapInterval(-1) != 0)
        {
            SDL_GL_SetSwapInterval(1);
        }

        // initialize GLEW
        //
        // effectively, enables the OpenGL API used by this application
        if (const auto err = glewInit(); err != GLEW_OK) {
            std::stringstream ss;
            ss << "glewInit() failed: ";
            ss << glewGetErrorString(err);
            throw std::runtime_error{ss.str()};
        }

        // validate that the runtime OpenGL backend supports the extensions that OSC
        // relies on
        //
        // reports anything missing to the log at the provided log level
        validate_opengl_backend_extension_support(LogLevel::debug);

        for (const auto& capability : c_RequiredOpenGLCapabilities) {
            glEnable(capability.id);
            if (!glIsEnabled(capability.id))
            {
                log_warn("failed to enable %s: this may cause rendering issues", capability.label.c_str());
            }
        }

        // print OpenGL information to console (handy for debugging user's rendering
        // issues)
        log_info(
            "OpenGL initialized: info: %s, %s, (%s), GLSL %s",
            glGetString(GL_VENDOR),
            glGetString(GL_RENDERER),
            glGetString(GL_VERSION),
            glGetString(GL_SHADING_LANGUAGE_VERSION)
        );

        return ctx;
    }

    // returns the maximum numbers of MSXAA antiAliasingLevel the active OpenGL context supports
    AntiAliasingLevel GetOpenGLMaxMSXAASamples(const sdl::GLContext&)
    {
        GLint v = 1;
        glGetIntegerv(GL_MAX_SAMPLES, &v);
        return AntiAliasingLevel{v};
    }

    // maps an OpenGL debug message severity level to a log level
    constexpr LogLevel OpenGLDebugSevToLogLvl(GLenum sev)
    {
        switch (sev) {
        case GL_DEBUG_SEVERITY_HIGH:
            return LogLevel::err;
        case GL_DEBUG_SEVERITY_MEDIUM:
            return LogLevel::warn;
        case GL_DEBUG_SEVERITY_LOW:
            return LogLevel::debug;
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            return LogLevel::trace;
        default:
            return LogLevel::info;
        }
    }

    // returns a string representation of an OpenGL debug message severity level
    constexpr CStringView OpenGLDebugSevToStrView(GLenum sev)
    {
        switch (sev) {
        case GL_DEBUG_SEVERITY_HIGH:
            return "GL_DEBUG_SEVERITY_HIGH";
        case GL_DEBUG_SEVERITY_MEDIUM:
            return "GL_DEBUG_SEVERITY_MEDIUM";
        case GL_DEBUG_SEVERITY_LOW:
            return "GL_DEBUG_SEVERITY_LOW";
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            return "GL_DEBUG_SEVERITY_NOTIFICATION";
        default:
            return "GL_DEBUG_SEVERITY_UNKNOWN";
        }
    }

    // returns a string representation of an OpenGL debug message source
    constexpr CStringView OpenGLDebugSrcToStrView(GLenum src)
    {
        switch (src) {
        case GL_DEBUG_SOURCE_API:
            return "GL_DEBUG_SOURCE_API";
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
            return "GL_DEBUG_SOURCE_WINDOW_SYSTEM";
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
            return "GL_DEBUG_SOURCE_SHADER_COMPILER";
        case GL_DEBUG_SOURCE_THIRD_PARTY:
            return "GL_DEBUG_SOURCE_THIRD_PARTY";
        case GL_DEBUG_SOURCE_APPLICATION:
            return "GL_DEBUG_SOURCE_APPLICATION";
        case GL_DEBUG_SOURCE_OTHER:
            return "GL_DEBUG_SOURCE_OTHER";
        default:
            return "GL_DEBUG_SOURCE_UNKNOWN";
        }
    }

    // returns a string representation of an OpenGL debug message type
    constexpr CStringView OpenGLDebugTypeToStrView(GLenum type)
    {
        switch (type) {
        case GL_DEBUG_TYPE_ERROR:
            return "GL_DEBUG_TYPE_ERROR";
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            return "GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR";
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            return "GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR";
        case GL_DEBUG_TYPE_PORTABILITY:
            return "GL_DEBUG_TYPE_PORTABILITY";
        case GL_DEBUG_TYPE_PERFORMANCE:
            return "GL_DEBUG_TYPE_PERFORMANCE";
        case GL_DEBUG_TYPE_MARKER:
            return "GL_DEBUG_TYPE_MARKER";
        case GL_DEBUG_TYPE_PUSH_GROUP:
            return "GL_DEBUG_TYPE_PUSH_GROUP";
        case GL_DEBUG_TYPE_POP_GROUP:
            return "GL_DEBUG_TYPE_POP_GROUP";
        case GL_DEBUG_TYPE_OTHER:
            return "GL_DEBUG_TYPE_OTHER";
        default:
            return "GL_DEBUG_TYPE_UNKNOWN";
        }
    }

    // returns `true` if current OpenGL context is in debug mode
    bool IsOpenGLInDebugMode()
    {
        // if context is not debug-mode, then some of the glGet*s below can fail
        // (e.g. GL_DEBUG_OUTPUT_SYNCHRONOUS on apple).
        {
            GLint flags = 0;
            glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
            if (!(flags & GL_CONTEXT_FLAG_DEBUG_BIT))
            {
                return false;
            }
        }

        {
            GLboolean b = GL_FALSE;
            glGetBooleanv(GL_DEBUG_OUTPUT, &b);
            if (!b)
            {
                return false;
            }
        }

        {
            GLboolean b = GL_FALSE;
            glGetBooleanv(GL_DEBUG_OUTPUT_SYNCHRONOUS, &b);
            if (!b)
            {
                return false;
            }
        }

        return true;
    }

    // raw handler function that can be used with `glDebugMessageCallback`
    void OpenGLDebugMessageHandler(
        GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei,
        const GLchar* message,
        const void*)
    {
        const LogLevel lvl = OpenGLDebugSevToLogLvl(severity);
        const CStringView sourceCStr = OpenGLDebugSrcToStrView(source);
        const CStringView typeCStr = OpenGLDebugTypeToStrView(type);
        const CStringView severityCStr = OpenGLDebugSevToStrView(severity);

        log_message(lvl,
            R"(OpenGL Debug message:
id = %u
message = %s
source = %s
type = %s
severity = %s
)", id, message, sourceCStr.c_str(), typeCStr.c_str(), severityCStr.c_str());
    }

    // enable OpenGL API debugging
    void EnableOpenGLDebugMessages()
    {
        if (IsOpenGLInDebugMode())
        {
            log_info("OpenGL debug mode appears to already be enabled: skipping enabling it");
            return;
        }

        GLint flags = 0;
        glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
        if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
        {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(OpenGLDebugMessageHandler, nullptr);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
            log_info("enabled OpenGL debug mode");
        }
        else
        {
            log_error("cannot enable OpenGL debug mode: the context does not have GL_CONTEXT_FLAG_DEBUG_BIT set");
        }
    }

    // disable OpenGL API debugging
    void DisableOpenGLDebugMessages()
    {
        if (!IsOpenGLInDebugMode())
        {
            log_info("OpenGL debug mode appears to already be disabled: skipping disabling it");
            return;
        }

        GLint flags{};
        glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
        if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
        {
            glDisable(GL_DEBUG_OUTPUT);
            log_info("disabled OpenGL debug mode");
        }
        else
        {
            log_error("cannot disable OpenGL debug mode: the context does not have a GL_CONTEXT_FLAG_DEBUG_BIT set");
        }
    }
}

class osc::GraphicsContext::Impl final {
public:
    explicit Impl(SDL_Window& window) : m_GLContext{CreateOpenGLContext(window)}
    {
        m_QuadMaterial.setDepthTested(false);  // it's for fullscreen rendering
    }

    AntiAliasingLevel max_antialiasing_level() const
    {
        return m_MaxMSXAASamples;
    }

    bool is_vsync_enabled() const
    {
        return m_VSyncEnabled;
    }

    void enable_vsync()
    {
        if (SDL_GL_SetSwapInterval(-1) == 0)
        {
            // adaptive vsync enabled
        }
        else if (SDL_GL_SetSwapInterval(1) == 0)
        {
            // normal vsync enabled
        }

        // always read the vsync state back from SDL
        m_VSyncEnabled = SDL_GL_GetSwapInterval() != 0;
    }

    void disable_vsync()
    {
        SDL_GL_SetSwapInterval(0);
        m_VSyncEnabled = SDL_GL_GetSwapInterval() != 0;
    }

    bool is_in_debug_mode() const
    {
        return m_DebugModeEnabled;
    }

    void enable_debug_mode()
    {
        if (IsOpenGLInDebugMode())
        {
            return;  // already in debug mode
        }

        log_info("enabling debug mode");
        EnableOpenGLDebugMessages();
        m_DebugModeEnabled = IsOpenGLInDebugMode();
    }
    void disable_debug_mode()
    {
        if (!IsOpenGLInDebugMode())
        {
            return;  // already not in debug mode
        }

        log_info("disabling debug mode");
        DisableOpenGLDebugMessages();
        m_DebugModeEnabled = IsOpenGLInDebugMode();
    }

    void clear_screen(const Color& color)
    {
        // clear color is in sRGB, but the framebuffer is sRGB-corrected (GL_FRAMEBUFFER_SRGB)
        // and assumes that the given colors are in linear space
        const Color linearColor = to_linear_colorspace(color);

        gl::bind_framebuffer(GL_DRAW_FRAMEBUFFER, gl::window_framebuffer);
        gl::clear_color(linearColor.r, linearColor.g, linearColor.b, linearColor.a);
        gl::clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void* upd_raw_opengl_context_handle_HACK()
    {
        return m_GLContext.get();
    }

    std::future<Texture2D> request_screenshot()
    {
        return m_ActiveScreenshotRequests.emplace_back().get_future();
    }

    void swap_buffers(SDL_Window& window)
    {
        // ensure window FBO is bound (see: SDL_GL_SwapWindow's note about MacOS requiring 0 is bound)
        gl::bind_framebuffer(GL_FRAMEBUFFER, gl::window_framebuffer);

        // flush outstanding screenshot requests
        if (!m_ActiveScreenshotRequests.empty())
        {
            // copy GPU-side window framebuffer into CPU-side `osc::Image` object
            const Vec2i dims = App::get().dims();

            std::vector<uint8_t> pixels(static_cast<size_t>(4*dims.x*dims.y));
            OSC_ASSERT(is_aligned_at_least(pixels.data(), 4) && "glReadPixels must be called with a buffer that is aligned to GL_PACK_ALIGNMENT (see: https://www.khronos.org/opengl/wiki/Common_Mistakes)");
            gl::pixel_store_i(GL_PACK_ALIGNMENT, 4);
            glReadPixels(
                0,
                0,
                dims.x,
                dims.y,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                pixels.data()
            );

            Texture2D screenshot
            {
                dims,
                TextureFormat::RGBA32,
                ColorSpace::sRGB
            };
            screenshot.set_pixel_data(pixels);

            // copy image to requests [0..n-2]
            for (ptrdiff_t i = 0, len = std::ssize(m_ActiveScreenshotRequests)-1; i < len; ++i)
            {
                m_ActiveScreenshotRequests[i].set_value(screenshot);
            }
            // move image to request `n-1`
            m_ActiveScreenshotRequests.back().set_value(std::move(screenshot));
            m_ActiveScreenshotRequests.clear();
        }

        SDL_GL_SwapWindow(&window);
    }

    std::string backend_vendor_string() const
    {
        return std::string{opengl_get_cstringview(GL_VENDOR)};
    }

    std::string backend_renderer_string() const
    {
        return std::string{opengl_get_cstringview(GL_RENDERER)};
    }

    std::string backend_version_string() const
    {
        return std::string{opengl_get_cstringview(GL_VERSION)};
    }

    std::string backend_shading_language_version_string() const
    {
        return std::string{opengl_get_cstringview(GL_SHADING_LANGUAGE_VERSION)};
    }

    const Material& getQuadMaterial() const
    {
        return m_QuadMaterial;
    }

    const Mesh& getQuadMesh() const
    {
        return m_QuadMesh;
    }

    std::vector<float>& updInstanceCPUBuffer()
    {
        return m_InstanceCPUBuffer;
    }

    gl::ArrayBuffer<float, GL_STREAM_DRAW>& updInstanceGPUBuffer()
    {
        return m_InstanceGPUBuffer;
    }

private:

    // active OpenGL context for the application
    sdl::GLContext m_GLContext;

    // maximum number of antiAliasingLevel supported by this hardware's OpenGL MSXAA API
    AntiAliasingLevel m_MaxMSXAASamples = GetOpenGLMaxMSXAASamples(m_GLContext);

    bool m_VSyncEnabled = SDL_GL_GetSwapInterval() != 0;

    // true if OpenGL's debug mode is enabled
    bool m_DebugModeEnabled = false;

    // a "queue" of active screenshot requests
    std::vector<std::promise<Texture2D>> m_ActiveScreenshotRequests;

    // a generic quad rendering material: used for some blitting operations
    Material m_QuadMaterial
    {
        Shader
        {
            c_quad_vertex_shader_src,
            c_quad_fragment_shader_src,
        }
    };

    // a generic quad mesh: two triangles covering NDC @ Z=0
    Mesh m_QuadMesh = PlaneGeometry{2.0f, 2.0f, 1, 1};

    // storage for instance data
    std::vector<float> m_InstanceCPUBuffer;
    gl::ArrayBuffer<float, GL_STREAM_DRAW> m_InstanceGPUBuffer;
};

static std::unique_ptr<osc::GraphicsContext::Impl> g_GraphicsContextImpl = nullptr;

osc::GraphicsContext::GraphicsContext(SDL_Window& window)
{
    if (g_GraphicsContextImpl)
    {
        throw std::runtime_error{"a graphics context has already been initialized: you cannot initialize a second"};
    }

    g_GraphicsContextImpl = std::make_unique<GraphicsContext::Impl>(window);
}

osc::GraphicsContext::~GraphicsContext() noexcept
{
    g_GraphicsContextImpl.reset();
}

AntiAliasingLevel osc::GraphicsContext::max_antialiasing_level() const
{
    return g_GraphicsContextImpl->max_antialiasing_level();
}

bool osc::GraphicsContext::is_vsync_enabled() const
{
    return g_GraphicsContextImpl->is_vsync_enabled();
}

void osc::GraphicsContext::enable_vsync()
{
    g_GraphicsContextImpl->enable_vsync();
}

void osc::GraphicsContext::disable_vsync()
{
    g_GraphicsContextImpl->disable_vsync();
}

bool osc::GraphicsContext::is_in_debug_mode() const
{
    return g_GraphicsContextImpl->is_in_debug_mode();
}

void osc::GraphicsContext::enable_debug_mode()
{
    g_GraphicsContextImpl->enable_debug_mode();
}

void osc::GraphicsContext::disable_debug_mode()
{
    g_GraphicsContextImpl->disable_debug_mode();
}

void osc::GraphicsContext::clear_screen(const Color& color)
{
    g_GraphicsContextImpl->clear_screen(color);
}

void* osc::GraphicsContext::upd_raw_opengl_context_handle_HACK()
{
    return g_GraphicsContextImpl->upd_raw_opengl_context_handle_HACK();
}

void osc::GraphicsContext::swap_buffers(SDL_Window& window)
{
    g_GraphicsContextImpl->swap_buffers(window);
}

std::future<Texture2D> osc::GraphicsContext::request_screenshot()
{
    return g_GraphicsContextImpl->request_screenshot();
}

std::string osc::GraphicsContext::backend_vendor_string() const
{
    return g_GraphicsContextImpl->backend_vendor_string();
}

std::string osc::GraphicsContext::backend_renderer_string() const
{
    return g_GraphicsContextImpl->backend_renderer_string();
}

std::string osc::GraphicsContext::backend_version_string() const
{
    return g_GraphicsContextImpl->backend_version_string();
}

std::string osc::GraphicsContext::backend_shading_language_version_string() const
{
    return g_GraphicsContextImpl->backend_shading_language_version_string();
}


/////////////////////////////
//
// drawing commands
//
/////////////////////////////

void osc::graphics::draw(
    const Mesh& mesh,
    const Transform& transform,
    const Material& material,
    Camera& camera,
    const std::optional<MaterialPropertyBlock>& maybeMaterialPropertyBlock,
    std::optional<size_t> maybe_submesh_index)
{
    GraphicsBackend::draw(mesh, transform, material, camera, maybeMaterialPropertyBlock, maybe_submesh_index);
}

void osc::graphics::draw(
    const Mesh& mesh,
    const Mat4& transform,
    const Material& material,
    Camera& camera,
    const std::optional<MaterialPropertyBlock>& maybeMaterialPropertyBlock,
    std::optional<size_t> maybe_submesh_index)
{
    GraphicsBackend::draw(mesh, transform, material, camera, maybeMaterialPropertyBlock, maybe_submesh_index);
}

void osc::graphics::blit(const Texture2D& source, RenderTexture& dest)
{
    GraphicsBackend::blit(source, dest);
}

void osc::graphics::blit_to_screen(
    const RenderTexture& t,
    const Rect& rect,
    BlitFlags flags)
{
    GraphicsBackend::blit_to_screen(t, rect, flags);
}

void osc::graphics::blit_to_screen(
    const RenderTexture& t,
    const Rect& rect,
    const Material& material,
    BlitFlags flags)
{
    GraphicsBackend::blit_to_screen(t, rect, material, flags);
}

void osc::graphics::blit_to_screen(
    const Texture2D& t,
    const Rect& rect)
{
    GraphicsBackend::blit_to_screen(t, rect);
}

void osc::graphics::copy_texture(
    const RenderTexture& src,
    Texture2D& dest)
{
    GraphicsBackend::copy_texture(src, dest);
}

void osc::graphics::copy_texture(
    const RenderTexture& src,
    Texture2D& dest,
    CubemapFace face)
{
    GraphicsBackend::copy_texture(src, dest, face);
}

void osc::graphics::copy_texture(
    const RenderTexture& sourceRenderTexture,
    Cubemap& destinationCubemap,
    size_t mip)
{
    GraphicsBackend::copy_texture(sourceRenderTexture, destinationCubemap, mip);
}

/////////////////////////
//
// backend implementation
//
/////////////////////////


// helper: binds to instanced attributes (per-drawcall)
void osc::GraphicsBackend::bind_to_instanced_attributes(
    const Shader::Impl& shader_impl,
    InstancingState& instancing_state)
{
    gl::bind_buffer(instancing_state.buffer);

    size_t byteOffset = 0;
    if (shader_impl.maybe_instanced_model_mat_attr_) {
        if (shader_impl.maybe_instanced_model_mat_attr_->shader_type == ShaderPropertyType::Mat4) {
            const gl::AttributeMat4 mmtxAttr{shader_impl.maybe_instanced_model_mat_attr_->location};
            gl::vertex_attrib_pointer(mmtxAttr, false, instancing_state.stride, instancing_state.base_offset + byteOffset);
            gl::vertex_attrib_divisor(mmtxAttr, 1);
            gl::enable_vertex_attrib_array(mmtxAttr);
            byteOffset += sizeof(float) * 16;
        }
    }
    if (shader_impl.maybe_instanced_normal_mat_attr_) {
        if (shader_impl.maybe_instanced_normal_mat_attr_->shader_type == ShaderPropertyType::Mat4) {
            const gl::AttributeMat4 mmtxAttr{shader_impl.maybe_instanced_normal_mat_attr_->location};
            gl::vertex_attrib_pointer(mmtxAttr, false, instancing_state.stride, instancing_state.base_offset + byteOffset);
            gl::vertex_attrib_divisor(mmtxAttr, 1);
            gl::enable_vertex_attrib_array(mmtxAttr);
            // unused: byteOffset += sizeof(float) * 16;
        }
        else if (shader_impl.maybe_instanced_normal_mat_attr_->shader_type == ShaderPropertyType::Mat3) {
            const gl::AttributeMat3 mmtxAttr{shader_impl.maybe_instanced_normal_mat_attr_->location};
            gl::vertex_attrib_pointer(mmtxAttr, false, instancing_state.stride, instancing_state.base_offset + byteOffset);
            gl::vertex_attrib_divisor(mmtxAttr, 1);
            gl::enable_vertex_attrib_array(mmtxAttr);
            // unused: byteOffset += sizeof(float) * 9;
        }
    }
}

// helper: unbinds from instanced attributes (per-drawcall)
void osc::GraphicsBackend::unbind_from_instanced_attributes(
    const Shader::Impl& shader_impl,
    InstancingState&)
{
    if (shader_impl.maybe_instanced_model_mat_attr_) {
        if (shader_impl.maybe_instanced_model_mat_attr_->shader_type == ShaderPropertyType::Mat4) {
            const gl::AttributeMat4 mmtxAttr{shader_impl.maybe_instanced_model_mat_attr_->location};
            gl::disable_vertex_attrib_array(mmtxAttr);
        }
    }
    if (shader_impl.maybe_instanced_normal_mat_attr_) {
        if (shader_impl.maybe_instanced_normal_mat_attr_->shader_type == ShaderPropertyType::Mat4) {
            const gl::AttributeMat4 mmtxAttr{shader_impl.maybe_instanced_normal_mat_attr_->location};
            gl::disable_vertex_attrib_array(mmtxAttr);
        }
        else if (shader_impl.maybe_instanced_normal_mat_attr_->shader_type == ShaderPropertyType::Mat3) {
            const gl::AttributeMat3 mmtxAttr{shader_impl.maybe_instanced_normal_mat_attr_->location};
            gl::disable_vertex_attrib_array(mmtxAttr);
        }
    }
}

// helper: upload instancing data for a batch
std::optional<InstancingState> osc::GraphicsBackend::upload_instance_data(
    std::span<const RenderObject> renderObjects,
    const Shader::Impl& shader_impl)
{
    // preemptively upload instancing data
    std::optional<InstancingState> maybeInstancingState;

    if (shader_impl.maybe_instanced_model_mat_attr_ || shader_impl.maybe_instanced_normal_mat_attr_) {

        // compute the stride between each instance
        size_t byteStride = 0;
        if (shader_impl.maybe_instanced_model_mat_attr_) {
            if (shader_impl.maybe_instanced_model_mat_attr_->shader_type == ShaderPropertyType::Mat4) {
                byteStride += sizeof(float) * 16;
            }
        }
        if (shader_impl.maybe_instanced_normal_mat_attr_) {
            if (shader_impl.maybe_instanced_normal_mat_attr_->shader_type == ShaderPropertyType::Mat4) {
                byteStride += sizeof(float) * 16;
            }
            else if (shader_impl.maybe_instanced_normal_mat_attr_->shader_type == ShaderPropertyType::Mat3) {
                byteStride += sizeof(float) * 9;
            }
        }

        // write the instance data into a CPU-side buffer

        OSC_PERF("GraphicsBackend::uploadInstanceData");
        std::vector<float>& buf = g_GraphicsContextImpl->updInstanceCPUBuffer();
        buf.clear();
        buf.reserve(renderObjects.size() * (byteStride/sizeof(float)));

        size_t floatOffset = 0;
        for (const RenderObject& el : renderObjects) {
            if (shader_impl.maybe_instanced_model_mat_attr_) {
                if (shader_impl.maybe_instanced_model_mat_attr_->shader_type == ShaderPropertyType::Mat4) {
                    const Mat4 m = model_mat4(el);
                    const std::span<const float> els = to_float_span(m);
                    buf.insert(buf.end(), els.begin(), els.end());
                    floatOffset += els.size();
                }
            }
            if (shader_impl.maybe_instanced_normal_mat_attr_) {
                if (shader_impl.maybe_instanced_normal_mat_attr_->shader_type == ShaderPropertyType::Mat4) {
                    const Mat4 m = normal_matrix4(el);
                    const std::span<const float> els = to_float_span(m);
                    buf.insert(buf.end(), els.begin(), els.end());
                    floatOffset += els.size();
                }
                else if (shader_impl.maybe_instanced_normal_mat_attr_->shader_type == ShaderPropertyType::Mat3) {
                    const Mat3 m = normal_matrix(el);
                    const std::span<const float> els = to_float_span(m);
                    buf.insert(buf.end(), els.begin(), els.end());
                    floatOffset += els.size();
                }
            }
        }
        OSC_ASSERT_ALWAYS(sizeof(float)*floatOffset == renderObjects.size() * byteStride);

        auto& vbo = maybeInstancingState.emplace(g_GraphicsContextImpl->updInstanceGPUBuffer(), byteStride).buffer;
        vbo.assign(std::span<const float>{buf.data(), floatOffset});
    }
    return maybeInstancingState;
}

void osc::GraphicsBackend::try_bind_material_value_to_shader_element(
    const ShaderElement& se,
    const MaterialValue& v,
    int32_t& texture_slot)
{
    if (get_shader_type(v) != se.shader_type) {
        return;  // mismatched types
    }

    switch (v.index()) {
    case variant_index<MaterialValue, Color>():
    {
        // colors are converted from sRGB to linear when passed to
        // the shader

        const Vec4 linearColor = to_linear_colorspace(std::get<Color>(v));
        gl::UniformVec4 u{se.location};
        gl::set_uniform(u, linearColor);
        break;
    }
    case variant_index<MaterialValue, std::vector<Color>>():
    {
        const auto& colors = std::get<std::vector<Color>>(v);
        const int32_t numToAssign = min(se.size, static_cast<int32_t>(colors.size()));

        if (numToAssign > 0)
        {
            // CARE: assigning to uniform arrays should be done in one `glUniform` call
            //
            // although many guides on the internet say it's valid to assign each array
            // element one-at-a-time by just calling the one-element version with `location + i`
            // I (AK) have encountered situations where some backends (e.g. MacOS) will behave
            // unusually if assigning this way
            //
            // so, for safety's sake, always upload arrays in one `glUniform*` call

            // CARE #2: colors should always be converted from sRGB-to-linear when passed to
            // a shader. OSC's rendering pipeline assumes that all color values in a shader
            // are linearized

            std::vector<Vec4> linearColors;
            linearColors.reserve(numToAssign);
            for (const auto& color : colors)
            {
                linearColors.emplace_back(to_linear_colorspace(color));
            }
            static_assert(sizeof(Vec4) == 4*sizeof(float));
            static_assert(alignof(Vec4) <= alignof(float));
            glUniform4fv(se.location, numToAssign, value_ptr(linearColors.front()));
        }
        break;
    }
    case variant_index<MaterialValue, float>():
    {
        gl::UniformFloat u{se.location};
        gl::set_uniform(u, std::get<float>(v));
        break;
    }
    case variant_index<MaterialValue, std::vector<float>>():
    {
        const auto& vals = std::get<std::vector<float>>(v);
        const int32_t numToAssign = min(se.size, static_cast<int32_t>(vals.size()));

        if (numToAssign > 0) {
            // CARE: assigning to uniform arrays should be done in one `glUniform` call
            //
            // although many guides on the internet say it's valid to assign each array
            // element one-at-a-time by just calling the one-element version with `location + i`
            // I (AK) have encountered situations where some backends (e.g. MacOS) will behave
            // unusually if assigning this way
            //
            // so, for safety's sake, always upload arrays in one `glUniform*` call

            glUniform1fv(se.location, numToAssign, vals.data());
        }
        break;
    }
    case variant_index<MaterialValue, Vec2>():
    {
        gl::UniformVec2 u{se.location};
        gl::set_uniform(u, std::get<Vec2>(v));
        break;
    }
    case variant_index<MaterialValue, Vec3>():
    {
        gl::UniformVec3 u{se.location};
        gl::set_uniform(u, std::get<Vec3>(v));
        break;
    }
    case variant_index<MaterialValue, std::vector<Vec3>>():
    {
        const auto& vals = std::get<std::vector<Vec3>>(v);
        const int32_t numToAssign = min(se.size, static_cast<int32_t>(vals.size()));

        if (numToAssign > 0) {
            // CARE: assigning to uniform arrays should be done in one `glUniform` call
            //
            // although many guides on the internet say it's valid to assign each array
            // element one-at-a-time by just calling the one-element version with `location + i`
            // I (AK) have encountered situations where some backends (e.g. MacOS) will behave
            // unusually if assigning this way
            //
            // so, for safety's sake, always upload arrays in one `glUniform*` call

            static_assert(sizeof(Vec3) == 3*sizeof(float));
            static_assert(alignof(Vec3) <= alignof(float));

            glUniform3fv(se.location, numToAssign, value_ptr(vals.front()));
        }
        break;
    }
    case variant_index<MaterialValue, Vec4>():
    {
        gl::UniformVec4 u{se.location};
        gl::set_uniform(u, std::get<Vec4>(v));
        break;
    }
    case variant_index<MaterialValue, Mat3>():
    {
        gl::UniformMat3 u{se.location};
        gl::set_uniform(u, std::get<Mat3>(v));
        break;
    }
    case variant_index<MaterialValue, Mat4>():
    {
        gl::UniformMat4 u{se.location};
        gl::set_uniform(u, std::get<Mat4>(v));
        break;
    }
    case variant_index<MaterialValue, std::vector<Mat4>>():
    {
        const auto& vals = std::get<std::vector<Mat4>>(v);
        const int32_t numToAssign = min(se.size, static_cast<int32_t>(vals.size()));
        if (numToAssign > 0) {
            // CARE: assigning to uniform arrays should be done in one `glUniform` call
            //
            // although many guides on the internet say it's valid to assign each array
            // element one-at-a-time by just calling the one-element version with `location + i`
            // I (AK) have encountered situations where some backends (e.g. MacOS) will behave
            // unusually if assigning this way
            //
            // so, for safety's sake, always upload arrays in one `glUniform*` call

            static_assert(sizeof(Mat4) == 16*sizeof(float));
            static_assert(alignof(Mat4) <= alignof(float));
            glUniformMatrix4fv(se.location, numToAssign, GL_FALSE, value_ptr(vals.front()));
        }
        break;
    }
    case variant_index<MaterialValue, int32_t>():
    {
        gl::UniformInt u{se.location};
        gl::set_uniform(u, std::get<int32_t>(v));
        break;
    }
    case variant_index<MaterialValue, bool>():
    {
        gl::UniformBool u{se.location};
        gl::set_uniform(u, std::get<bool>(v));
        break;
    }
    case variant_index<MaterialValue, Texture2D>():
    {
        auto& impl = const_cast<Texture2D::Impl&>(*std::get<Texture2D>(v).m_Impl);
        gl::Texture2D& texture = impl.updTexture();

        gl::active_texture(GL_TEXTURE0 + texture_slot);
        gl::bind_texture(texture);
        gl::UniformSampler2D u{se.location};
        gl::set_uniform(u, texture_slot);

        ++texture_slot;
        break;
    }
    case variant_index<MaterialValue, RenderTexture>():
    {
        static_assert(num_options<TextureDimensionality>() == 2);
        std::visit(Overload
            {
                [&texture_slot, &se](SingleSampledTexture& sst)
            {
                gl::active_texture(GL_TEXTURE0 + texture_slot);
                gl::bind_texture(sst.texture2D);
                gl::UniformSampler2D u{se.location};
                gl::set_uniform(u, texture_slot);
                ++texture_slot;
            },
            [&texture_slot, &se](MultisampledRBOAndResolvedTexture& mst)
            {
                gl::active_texture(GL_TEXTURE0 + texture_slot);
                gl::bind_texture(mst.single_sampled_texture2D);
                gl::UniformSampler2D u{se.location};
                gl::set_uniform(u, texture_slot);
                ++texture_slot;
            },
            [&texture_slot, &se](SingleSampledCubemap& cubemap)
            {
                gl::active_texture(GL_TEXTURE0 + texture_slot);
                gl::bind_texture(cubemap.cubemap);
                gl::UniformSamplerCube u{se.location};
                gl::set_uniform(u, texture_slot);
                ++texture_slot;
            },
            }, const_cast<RenderTexture::Impl&>(*std::get<RenderTexture>(v).m_Impl).getColorRenderBufferData());

        break;
    }
    case variant_index<MaterialValue, Cubemap>():
    {
        auto& impl = const_cast<Cubemap::Impl&>(*std::get<Cubemap>(v).impl_);
        const gl::TextureCubemap& texture = impl.upd_cubemap();

        gl::active_texture(GL_TEXTURE0 + texture_slot);
        gl::bind_texture(texture);
        gl::UniformSamplerCube u{se.location};
        gl::set_uniform(u, texture_slot);

        ++texture_slot;
        break;
    }
    default:
    {
        break;
    }
    }
}

// helper: draw a batch of `RenderObject`s that have the same:
//
//   - Material
//   - MaterialPropertyBlock
//   - Mesh
//   - sub-Mesh index (can be std::nullopt, to mean 'the entire mesh')
void osc::GraphicsBackend::handle_batch_with_same_submesh(
    std::span<const RenderObject> els,
    std::optional<InstancingState>& instancing_state)
{
    auto& meshImpl = const_cast<Mesh::Impl&>(*els.front().mesh.m_Impl);
    const Shader::Impl& shader_impl = *els.front().material.m_Impl->shader_.m_Impl;
    const std::optional<size_t> maybe_submesh_index = els.front().maybe_submesh_index;

    gl::bind_vertex_array(meshImpl.updVertexArray());

    if (shader_impl.maybe_model_mat_uniform_ || shader_impl.maybe_normal_mat_uniform_) {
        // if the shader requires per-instance uniforms, then we *have* to render one
        // instance at a time

        for (const RenderObject& el : els) {

            // try binding to uModel (standard)
            if (shader_impl.maybe_model_mat_uniform_) {
                if (shader_impl.maybe_model_mat_uniform_->shader_type == ShaderPropertyType::Mat4) {
                    gl::UniformMat4 u{shader_impl.maybe_model_mat_uniform_->location};
                    gl::set_uniform(u, model_mat4(el));
                }
            }

            // try binding to uNormalMat (standard)
            if (shader_impl.maybe_normal_mat_uniform_) {
                if (shader_impl.maybe_normal_mat_uniform_->shader_type == ShaderPropertyType::Mat3) {
                    gl::UniformMat3 u{shader_impl.maybe_normal_mat_uniform_->location};
                    gl::set_uniform(u, normal_matrix(el));
                }
                else if (shader_impl.maybe_normal_mat_uniform_->shader_type == ShaderPropertyType::Mat4) {
                    gl::UniformMat4 u{shader_impl.maybe_normal_mat_uniform_->location};
                    gl::set_uniform(u, normal_matrix4(el));
                }
            }

            if (instancing_state) {
                bind_to_instanced_attributes(shader_impl, *instancing_state);
            }
            meshImpl.drawInstanced(1, maybe_submesh_index);
            if (instancing_state) {
                unbind_from_instanced_attributes(shader_impl, *instancing_state);
                instancing_state->base_offset += 1 * instancing_state->stride;
            }
        }
    }
    else {
        // else: the shader supports instanced data, so we can draw multiple meshes in one call

        if (instancing_state) {
            bind_to_instanced_attributes(shader_impl, *instancing_state);
        }
        meshImpl.drawInstanced(els.size(), maybe_submesh_index);
        if (instancing_state) {
            unbind_from_instanced_attributes(shader_impl, *instancing_state);
            instancing_state->base_offset += els.size() * instancing_state->stride;
        }
    }

    gl::bind_vertex_array();
}

// helper: draw a batch of `RenderObject`s that have the same:
//
//   - Material
//   - MaterialPropertyBlock
//   - Mesh
void osc::GraphicsBackend::handle_batch_with_same_mesh(
    std::span<const RenderObject> els,
    std::optional<InstancingState>& instancing_state)
{
    // batch by sub-Mesh index
    auto batchIt = els.begin();
    while (batchIt != els.end()) {
        const auto batchEnd = find_if_not(batchIt, els.end(), RenderObjectHasSubMeshIndex{batchIt->maybe_submesh_index});
        handle_batch_with_same_submesh({batchIt, batchEnd}, instancing_state);
        batchIt = batchEnd;
    }
}

// helper: draw a batch of `RenderObject`s that have the same:
//
//   - Material
//   - MaterialPropertyBlock
void osc::GraphicsBackend::handle_batch_with_same_material_property_block(
    std::span<const RenderObject> els,
    int32_t& texture_slot,
    std::optional<InstancingState>& instancing_state)
{
    OSC_PERF("GraphicsBackend::handle_batch_with_same_material_property_block");

    const Material::Impl& matImpl = *els.front().material.m_Impl;
    const Shader::Impl& shader_impl = *matImpl.shader_.m_Impl;
    const FastStringHashtable<ShaderElement>& uniforms = shader_impl.getUniforms();

    // bind property block variables (if applicable)
    if (els.front().maybe_prop_block) {
        for (const auto& [name, value] : els.front().maybe_prop_block->m_Impl->values_) {
            if (const auto* uniform = try_find(uniforms, name)) {
                try_bind_material_value_to_shader_element(*uniform, value, texture_slot);
            }
        }
    }

    // batch by mesh
    auto batchIt = els.begin();
    while (batchIt != els.end())
    {
        const auto batchEnd = find_if_not(batchIt, els.end(), RenderObjectHasMesh{&batchIt->mesh});
        handle_batch_with_same_mesh({batchIt, batchEnd}, instancing_state);
        batchIt = batchEnd;
    }
}

// helper: draw a batch of `RenderObject`s that have the same:
//
//   - Material
void osc::GraphicsBackend::handle_batch_with_same_material(
    const RenderPassState& renderPassState,
    std::span<const RenderObject> els)
{
    OSC_PERF("GraphicsBackend::handle_batch_with_same_material");

    const auto& matImpl = *els.front().material.m_Impl;
    const auto& shader_impl = *matImpl.shader_.m_Impl;
    const FastStringHashtable<ShaderElement>& uniforms = shader_impl.getUniforms();

    // preemptively upload instance data
    std::optional<InstancingState> maybeInstances = upload_instance_data(els, shader_impl);

    // updated by various batches (which may bind to textures etc.)
    int32_t texture_slot = 0;

    gl::use_program(shader_impl.getProgram());

    if (matImpl.getWireframeMode())
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    if (matImpl.getDepthFunction() != DepthFunction::Default)
    {
        glDepthFunc(to_opengl_depth_function_enum(matImpl.getDepthFunction()));
    }

    if (matImpl.getCullMode() != CullMode::Off)
    {
        glEnable(GL_CULL_FACE);
        glCullFace(to_opengl_cull_face_enum(matImpl.getCullMode()));

        // winding order is assumed to be counter-clockwise
        //
        // (it's the initial value as defined by Khronos: https://registry.khronos.org/OpenGL-Refpages/gl4/html/glFrontFace.xhtml)
        // glFrontFace(GL_CCW);
    }

    // bind material variables
    {
        // try binding to uView (standard)
        if (shader_impl.maybe_view_mat_uniform_)
        {
            if (shader_impl.maybe_view_mat_uniform_->shader_type == ShaderPropertyType::Mat4)
            {
                gl::UniformMat4 u{shader_impl.maybe_view_mat_uniform_->location};
                gl::set_uniform(u, renderPassState.view_matrix);
            }
        }

        // try binding to uProjection (standard)
        if (shader_impl.maybe_proj_mat_uniform_)
        {
            if (shader_impl.maybe_proj_mat_uniform_->shader_type == ShaderPropertyType::Mat4)
            {
                gl::UniformMat4 u{shader_impl.maybe_proj_mat_uniform_->location};
                gl::set_uniform(u, renderPassState.projection_matrix);
            }
        }

        if (shader_impl.maybe_view_proj_mat_uniform_)
        {
            if (shader_impl.maybe_view_proj_mat_uniform_->shader_type == ShaderPropertyType::Mat4)
            {
                gl::UniformMat4 u{shader_impl.maybe_view_proj_mat_uniform_->location};
                gl::set_uniform(u, renderPassState.view_projection_matrix);
            }
        }

        // bind material values
        for (const auto& [name, value] : matImpl.values_) {
            if (const ShaderElement* e = try_find(uniforms, name)) {
                try_bind_material_value_to_shader_element(*e, value, texture_slot);
            }
        }
    }

    // batch by material property block
    auto batchIt = els.begin();
    while (batchIt != els.end())
    {
        const auto batchEnd = find_if_not(batchIt, els.end(), RenderObjectHasMaterialPropertyBlock{&batchIt->maybe_prop_block});
        handle_batch_with_same_material_property_block({batchIt, batchEnd}, texture_slot, maybeInstances);
        batchIt = batchEnd;
    }

    if (matImpl.getCullMode() != CullMode::Off)
    {
        glCullFace(GL_BACK);  // default from Khronos docs
        glDisable(GL_CULL_FACE);
    }

    if (matImpl.getDepthFunction() != DepthFunction::Default)
    {
        glDepthFunc(to_opengl_depth_function_enum(DepthFunction::Default));
    }

    if (matImpl.getWireframeMode())
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

// helper: draw a sequence of `RenderObject`s
void osc::GraphicsBackend::draw_render_objects(
    const RenderPassState& renderPassState,
    std::span<const RenderObject> els)
{
    OSC_PERF("GraphicsBackend::draw_render_objects");

    // batch by material
    auto batchIt = els.begin();
    while (batchIt != els.end()) {
        const auto batchEnd = find_if_not(batchIt, els.end(), RenderObjectHasMaterial{&batchIt->material});
        handle_batch_with_same_material(renderPassState, {batchIt, batchEnd});
        batchIt = batchEnd;
    }
}

void osc::GraphicsBackend::draw_batched_by_opaqueness(
    const RenderPassState& renderPassState,
    std::span<const RenderObject> els)
{
    OSC_PERF("GraphicsBackend::draw_batched_by_opaqueness");

    auto batchIt = els.begin();
    while (batchIt != els.end()) {
        const auto opaqueEnd = find_if_not(batchIt, els.end(), is_opaque);

        if (opaqueEnd != batchIt) {
            // [batchIt..opaqueEnd] contains opaque elements
            gl::disable(GL_BLEND);
            draw_render_objects(renderPassState, {batchIt, opaqueEnd});

            batchIt = opaqueEnd;
        }

        if (opaqueEnd != els.end()) {
            // [opaqueEnd..els.end()] contains transparent elements
            const auto transparentEnd = find_if(opaqueEnd, els.end(), is_opaque);
            gl::enable(GL_BLEND);
            draw_render_objects(renderPassState, {opaqueEnd, transparentEnd});

            batchIt = transparentEnd;
        }
    }
}

void osc::GraphicsBackend::flush_render_queue(Camera::Impl& camera, float aspect_ratio)
{
    OSC_PERF("GraphicsBackend::flush_render_queue");

    // flush the render queue in batches based on what's being rendered:
    //
    // - not-depth-tested elements (can't be reordered)
    // - depth-tested elements (can be reordered):
    //   - opaqueness (opaque first, then transparent back-to-front)
    //   - material
    //   - material property block
    //   - mesh

    std::vector<RenderObject>& queue = camera.m_RenderQueue;

    if (queue.empty())
    {
        return;
    }

    // precompute any render pass state used by the rendering algs
    const RenderPassState renderPassState{
        camera.position(),
        camera.view_matrix(),
        camera.projection_matrix(aspect_ratio),
    };

    gl::enable(GL_DEPTH_TEST);

    // draw by reordering depth-tested elements around the not-depth-tested elements
    auto batchIt = queue.begin();
    while (batchIt != queue.end())
    {
        const auto depthTestedEnd = find_if_not(batchIt, queue.end(), is_depth_tested);

        if (depthTestedEnd != batchIt)
        {
            // there are >0 depth-tested elements that are elegible for reordering

            sort_render_queue(batchIt, depthTestedEnd, renderPassState.camera_pos);
            draw_batched_by_opaqueness(renderPassState, {batchIt, depthTestedEnd});

            batchIt = depthTestedEnd;
        }

        if (depthTestedEnd != queue.end())
        {
            // there are >0 not-depth-tested elements that cannot be reordered

            const auto ignoreDepthTestEnd = find_if(depthTestedEnd, queue.end(), is_depth_tested);

            // these elements aren't depth-tested and should just be drawn as-is
            gl::disable(GL_DEPTH_TEST);
            draw_batched_by_opaqueness(renderPassState, {depthTestedEnd, ignoreDepthTestEnd});
            gl::enable(GL_DEPTH_TEST);

            batchIt = ignoreDepthTestEnd;
        }
    }

    // queue flushed: clear it
    queue.clear();
}

void osc::GraphicsBackend::validate_render_target(RenderTarget& renderTarget)
{
    // ensure there is at least one color attachment
    OSC_ASSERT(!renderTarget.colors.empty() && "a render target must have one or more color attachments");

    OSC_ASSERT(renderTarget.colors.front().buffer != nullptr && "a color attachment must have a non-null render buffer");
    const Vec2i firstColorBufferDimensions = renderTarget.colors.front().buffer->m_Impl->getDimensions();
    const AntiAliasingLevel firstColorBufferSamples = renderTarget.colors.front().buffer->m_Impl->getAntialiasingLevel();

    // validate other buffers against the first
    for (auto it = renderTarget.colors.begin()+1; it != renderTarget.colors.end(); ++it)
    {
        const RenderTargetColorAttachment& colorAttachment = *it;
        OSC_ASSERT(colorAttachment.buffer != nullptr);
        OSC_ASSERT(colorAttachment.buffer->m_Impl->getDimensions() == firstColorBufferDimensions);
        OSC_ASSERT(colorAttachment.buffer->m_Impl->getAntialiasingLevel() == firstColorBufferSamples);
    }
    OSC_ASSERT(renderTarget.depth.buffer != nullptr);
    OSC_ASSERT(renderTarget.depth.buffer->m_Impl->getDimensions() == firstColorBufferDimensions);
    OSC_ASSERT(renderTarget.depth.buffer->m_Impl->getAntialiasingLevel() == firstColorBufferSamples);
}

Rect osc::GraphicsBackend::calc_viewport_bounds(
    Camera::Impl& camera,
    RenderTarget* maybe_custom_render_target)
{
    const Vec2 targetDims = maybe_custom_render_target ?
        Vec2{maybe_custom_render_target->colors.front().buffer->m_Impl->getDimensions()} :
        App::get().dims();

    const Rect cameraRect = camera.pixel_rect() ?
        *camera.pixel_rect() :
        Rect{{}, targetDims};

    const Vec2 cameraRectBottomLeft = bottom_left_lh(cameraRect);
    const Vec2 outputDimensions = dimensions_of(cameraRect);
    const Vec2 topLeft = {cameraRectBottomLeft.x, targetDims.y - cameraRectBottomLeft.y};

    return Rect{topLeft, topLeft + outputDimensions};
}

Rect osc::GraphicsBackend::setup_top_level_pipeline_state(
    Camera::Impl& camera,
    RenderTarget* maybe_custom_render_target)
{
    const Rect viewportRect = calc_viewport_bounds(camera, maybe_custom_render_target);
    const Vec2 viewportDims = dimensions_of(viewportRect);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl::viewport(
        static_cast<GLsizei>(viewportRect.p1.x),
        static_cast<GLsizei>(viewportRect.p1.y),
        static_cast<GLsizei>(viewportDims.x),
        static_cast<GLsizei>(viewportDims.y)
    );

    if (camera.m_MaybeScissorRect)
    {
        const Rect scissorRect = *camera.m_MaybeScissorRect;
        const Vec2i scissorDims = dimensions_of(scissorRect);

        gl::enable(GL_SCISSOR_TEST);
        glScissor(
            static_cast<GLint>(scissorRect.p1.x),
            static_cast<GLint>(scissorRect.p1.y),
            scissorDims.x,
            scissorDims.y
        );
    }
    else
    {
        gl::disable(GL_SCISSOR_TEST);
    }

    return viewportRect;
}

void osc::GraphicsBackend::teardown_top_level_pipeline_state(
    Camera::Impl& camera,
    RenderTarget*)
{
    if (camera.m_MaybeScissorRect)
    {
        gl::disable(GL_SCISSOR_TEST);
    }
    gl::bind_framebuffer(GL_FRAMEBUFFER, gl::window_framebuffer);
    gl::use_program();
}

std::optional<gl::FrameBuffer> osc::GraphicsBackend::bind_and_clear_render_buffers(
    Camera::Impl& camera,
    RenderTarget* maybe_custom_render_target)
{
    // if necessary, create pass-specific FBO
    std::optional<gl::FrameBuffer> maybeRenderFBO;

    if (maybe_custom_render_target)
    {
        // caller wants to render to a custom render target of `n` color
        // buffers and a single depth buffer. Bind them all to one MRT FBO

        gl::FrameBuffer& rendererFBO = maybeRenderFBO.emplace();
        gl::bind_framebuffer(GL_DRAW_FRAMEBUFFER, rendererFBO);

        // attach color buffers to the FBO
        for (size_t i = 0; i < maybe_custom_render_target->colors.size(); ++i)
        {
            std::visit(Overload
            {
                [i](SingleSampledTexture& t)
                {
                    gl::framebuffer_texture2D(
                        GL_DRAW_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0 + static_cast<GLint>(i),
                        t.texture2D,
                        0
                    );
                },
                [i](MultisampledRBOAndResolvedTexture& t)
                {
                    gl::framebuffer_renderbuffer(
                        GL_DRAW_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0 + static_cast<GLint>(i),
                        t.multisampled_rbo
                    );
                },
#ifdef EMSCRIPTEN
                [](SingleSampledCubemap&) {}
#else
                [i](SingleSampledCubemap& t)
                {
                    glFramebufferTexture(
                        GL_DRAW_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0 + static_cast<GLint>(i),
                        t.cubemap.get(),
                        0
                    );
                }
#endif
            }, maybe_custom_render_target->colors[i].buffer->m_Impl->upd_opengl_data());
        }

        // attach depth buffer to the FBO
        std::visit(Overload
        {
            [](SingleSampledTexture& t)
            {
                gl::framebuffer_texture2D(
                    GL_DRAW_FRAMEBUFFER,
                    GL_DEPTH_STENCIL_ATTACHMENT,
                    t.texture2D,
                    0
                );
            },
            [](MultisampledRBOAndResolvedTexture& t)
            {
                gl::framebuffer_renderbuffer(
                    GL_DRAW_FRAMEBUFFER,
                    GL_DEPTH_STENCIL_ATTACHMENT,
                    t.multisampled_rbo
                );
            },
#ifdef EMSCRIPTEN
            [](SingleSampledCubemap&) {}
#else
            [](SingleSampledCubemap& t)
            {
                glFramebufferTexture(
                    GL_DRAW_FRAMEBUFFER,
                    GL_DEPTH_STENCIL_ATTACHMENT,
                    t.cubemap.get(),
                    0
                );
            }
#endif
        }, maybe_custom_render_target->depth.buffer->m_Impl->upd_opengl_data());

        // Multi-Render Target (MRT) support: tell OpenGL to use all specified
        // render targets when drawing and/or clearing
        {
            const size_t numColorAttachments = maybe_custom_render_target->colors.size();

            std::vector<GLenum> attachments;
            attachments.reserve(numColorAttachments);
            for (size_t i = 0; i < numColorAttachments; ++i)
            {
                attachments.push_back(GL_COLOR_ATTACHMENT0 + static_cast<GLint>(i));
            }
            glDrawBuffers(static_cast<GLsizei>(attachments.size()), attachments.data());
        }

        // if requested, clear the buffers
        {
            static_assert(num_options<RenderBufferLoadAction>() == 2);

            // if requested, clear color buffers
            for (size_t i = 0; i < maybe_custom_render_target->colors.size(); ++i)
            {
                RenderTargetColorAttachment& colorAttachment = maybe_custom_render_target->colors[i];
                if (colorAttachment.loadAction == RenderBufferLoadAction::Clear)
                {
                    glClearBufferfv(
                        GL_COLOR,
                        static_cast<GLint>(i),
                        value_ptr(static_cast<Vec4>(colorAttachment.clear_color))
                    );
                }
            }

            // if requested, clear depth buffer
            if (maybe_custom_render_target->depth.loadAction == RenderBufferLoadAction::Clear)
            {
                gl::clear(GL_DEPTH_BUFFER_BIT);
            }
        }
    }
    else
    {
        gl::bind_framebuffer(GL_FRAMEBUFFER, gl::window_framebuffer);

        // we're rendering to the window
        if (camera.m_ClearFlags != CameraClearFlags::Nothing)
        {
            // clear window
            const GLenum clearFlags = camera.m_ClearFlags & CameraClearFlags::SolidColor ?
                GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT :
                GL_DEPTH_BUFFER_BIT;

            // clear color is in sRGB, but the window's framebuffer is sRGB-corrected
            // and assume that clear colors are in linear space
            const Color linearColor = to_linear_colorspace(camera.m_BackgroundColor);
            gl::clear_color(
                linearColor.r,
                linearColor.g,
                linearColor.b,
                linearColor.a
            );
            gl::clear(clearFlags);
        }
    }

    return maybeRenderFBO;
}

void osc::GraphicsBackend::resolve_render_buffers(RenderTarget& renderTarget)
{
    static_assert(num_options<RenderBufferStoreAction>() == 2, "check 'if's etc. in this code");

    OSC_PERF("RenderTexture::resolveBuffers");

    // setup FBOs (reused per color buffer)
    gl::FrameBuffer multisampledReadFBO;
    gl::bind_framebuffer(GL_READ_FRAMEBUFFER, multisampledReadFBO);

    gl::FrameBuffer resolvedDrawFBO;
    gl::bind_framebuffer(GL_DRAW_FRAMEBUFFER, resolvedDrawFBO);

    // resolve each color buffer with a blit
    for (size_t i = 0; i < renderTarget.colors.size(); ++i)
    {
        const RenderTargetColorAttachment& attachment = renderTarget.colors[i];
        RenderBuffer& buffer = *attachment.buffer;
        RenderBufferOpenGLData& bufferOpenGLData = buffer.m_Impl->upd_opengl_data();

        if (attachment.storeAction != RenderBufferStoreAction::Resolve)
        {
            continue;  // we don't need to resolve this color buffer
        }

        bool bufferIsResolveable = false;  // changes if the underlying buffer data is resolve-able
        std::visit(Overload
        {
            [](SingleSampledTexture&)
            {
                // don't resolve: it's single-sampled
            },
            [&bufferIsResolveable, i](MultisampledRBOAndResolvedTexture& t)
            {
                const GLint attachmentLoc = GL_COLOR_ATTACHMENT0 + static_cast<GLint>(i);

                gl::framebuffer_renderbuffer(
                    GL_READ_FRAMEBUFFER,
                    attachmentLoc,
                    t.multisampled_rbo
                );
                glReadBuffer(attachmentLoc);

                gl::framebuffer_texture2D(
                    GL_DRAW_FRAMEBUFFER,
                    attachmentLoc,
                    t.single_sampled_texture2D,
                    0
                );
                glDrawBuffer(attachmentLoc);

                bufferIsResolveable = true;
            },
            [](SingleSampledCubemap&)
            {
                // don't resolve: it's single-sampled
            }
        }, bufferOpenGLData);

        if (bufferIsResolveable)
        {
            const Vec2i dimensions = attachment.buffer->m_Impl->getDimensions();
            gl::blit_framebuffer(
                0,
                0,
                dimensions.x,
                dimensions.y,
                0,
                0,
                dimensions.x,
                dimensions.y,
                GL_COLOR_BUFFER_BIT,
                GL_NEAREST
            );
        }
    }

    // resolve depth buffer with a blit
    if (renderTarget.depth.storeAction == RenderBufferStoreAction::Resolve)
    {
        bool bufferIsResolveable = false;  // changes if the underlying buffer data is resolve-able
        std::visit(Overload
        {
            [](SingleSampledTexture&)
            {
                // don't resolve: it's single-sampled
            },
            [&bufferIsResolveable](MultisampledRBOAndResolvedTexture& t)
            {
                gl::framebuffer_renderbuffer(
                    GL_READ_FRAMEBUFFER,
                    GL_DEPTH_ATTACHMENT,
                    t.multisampled_rbo
                );
                glReadBuffer(GL_DEPTH_ATTACHMENT);

                gl::framebuffer_texture2D(
                    GL_DRAW_FRAMEBUFFER,
                    GL_DEPTH_ATTACHMENT,
                    t.single_sampled_texture2D,
                    0
                );
                glDrawBuffer(GL_DEPTH_ATTACHMENT);

                bufferIsResolveable = true;
            },
            [](SingleSampledCubemap&)
            {
                // don't resolve: it's single-sampled
            }
        }, renderTarget.depth.buffer->m_Impl->upd_opengl_data());

        if (bufferIsResolveable)
        {
            const Vec2i dimensions = renderTarget.depth.buffer->m_Impl->getDimensions();
            gl::blit_framebuffer(
                0,
                0,
                dimensions.x,
                dimensions.y,
                0,
                0,
                dimensions.x,
                dimensions.y,
                GL_DEPTH_BUFFER_BIT,
                GL_NEAREST
            );
        }
    }
}

void osc::GraphicsBackend::render_camera_queue(
    Camera::Impl& camera,
    RenderTarget* maybe_custom_render_target)
{
    OSC_PERF("GraphicsBackend::render_camera_queue");

    if (maybe_custom_render_target)
    {
        validate_render_target(*maybe_custom_render_target);
    }

    const Rect viewportRect = setup_top_level_pipeline_state(
        camera,
        maybe_custom_render_target
    );

    {
        const std::optional<gl::FrameBuffer> maybeTmpFBO =
            bind_and_clear_render_buffers(camera, maybe_custom_render_target);
        flush_render_queue(camera, aspect_ratio(viewportRect));
    }

    if (maybe_custom_render_target)
    {
        resolve_render_buffers(*maybe_custom_render_target);
    }

    teardown_top_level_pipeline_state(
        camera,
        maybe_custom_render_target
    );
}

void osc::GraphicsBackend::draw(
    const Mesh& mesh,
    const Transform& transform,
    const Material& material,
    Camera& camera,
    const std::optional<MaterialPropertyBlock>& maybeMaterialPropertyBlock,
    std::optional<size_t> maybe_submesh_index)
{
    if (maybe_submesh_index && *maybe_submesh_index >= mesh.getSubMeshCount()) {
        throw std::out_of_range{"the given sub-mesh index was out of range (i.e. the given mesh does not have that many sub-meshes)"};
    }

    camera.impl_.upd()->m_RenderQueue.emplace_back(
        mesh,
        transform,
        material,
        maybeMaterialPropertyBlock,
        maybe_submesh_index
    );
}

void osc::GraphicsBackend::draw(
    const Mesh& mesh,
    const Mat4& transform,
    const Material& material,
    Camera& camera,
    const std::optional<MaterialPropertyBlock>& maybeMaterialPropertyBlock,
    std::optional<size_t> maybe_submesh_index)
{
    if (maybe_submesh_index && *maybe_submesh_index >= mesh.getSubMeshCount())
    {
        throw std::out_of_range{"the given sub-mesh index was out of range (i.e. the given mesh does not have that many sub-meshes)"};
    }

    camera.impl_.upd()->m_RenderQueue.emplace_back(
        mesh,
        transform,
        material,
        maybeMaterialPropertyBlock,
        maybe_submesh_index
    );
}

void osc::GraphicsBackend::blit(
    const Texture2D& source,
    RenderTexture& dest)
{
    Camera c;
    c.set_background_color(Color::clear());
    c.set_projection_matrix_override(identity<Mat4>());
    c.set_view_matrix_override(identity<Mat4>());

    Material m = g_GraphicsContextImpl->getQuadMaterial();
    m.setTexture("uTexture", source);

    graphics::draw(g_GraphicsContextImpl->getQuadMesh(), Transform{}, m, c);
    c.render_to(dest);
}

void osc::GraphicsBackend::blit_to_screen(
    const RenderTexture& t,
    const Rect& rect,
    BlitFlags flags)
{
    blit_to_screen(t, rect, g_GraphicsContextImpl->getQuadMaterial(), flags);
}

void osc::GraphicsBackend::blit_to_screen(
    const RenderTexture& t,
    const Rect& rect,
    const Material& material,
    BlitFlags)
{
    OSC_ASSERT(g_GraphicsContextImpl);
    OSC_ASSERT(t.m_Impl->has_been_rendered_to() && "the input texture has not been rendered to");

    Camera c;
    c.set_background_color(Color::clear());
    c.set_pixel_rect(rect);
    c.set_projection_matrix_override(identity<Mat4>());
    c.set_view_matrix_override(identity<Mat4>());
    c.set_clear_flags(CameraClearFlags::Nothing);

    Material copy{material};
    copy.setRenderTexture("uTexture", t);
    graphics::draw(g_GraphicsContextImpl->getQuadMesh(), Transform{}, copy, c);
    c.render_to_screen();
    copy.clearRenderTexture("uTexture");
}

void osc::GraphicsBackend::blit_to_screen(
    const Texture2D& t,
    const Rect& rect)
{
    OSC_ASSERT(g_GraphicsContextImpl);

    Camera c;
    c.set_background_color(Color::clear());
    c.set_pixel_rect(rect);
    c.set_projection_matrix_override(identity<Mat4>());
    c.set_view_matrix_override(identity<Mat4>());
    c.set_clear_flags(CameraClearFlags::Nothing);

    Material copy{g_GraphicsContextImpl->getQuadMaterial()};
    copy.setTexture("uTexture", t);
    graphics::draw(g_GraphicsContextImpl->getQuadMesh(), Transform{}, copy, c);
    c.render_to_screen();
    copy.clearTexture("uTexture");
}

void osc::GraphicsBackend::copy_texture(
    const RenderTexture& src,
    Texture2D& dest)
{
    copy_texture(src, dest, CubemapFace::PositiveX);
}

void osc::GraphicsBackend::copy_texture(
    const RenderTexture& src,
    Texture2D& dest,
    CubemapFace face)
{
    OSC_ASSERT(g_GraphicsContextImpl);
    OSC_ASSERT(src.m_Impl->has_been_rendered_to() && "the input texture has not been rendered to");

    // create a source (read) framebuffer for blitting from the source render texture
    gl::FrameBuffer readFBO;
    gl::bind_framebuffer(GL_READ_FRAMEBUFFER, readFBO);
    std::visit(Overload  // attach source texture depending on rendertexture's type
    {
        [](SingleSampledTexture& t)
        {
            gl::framebuffer_texture2D(
                GL_READ_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                t.texture2D,
                0
            );
        },
        [](MultisampledRBOAndResolvedTexture& t)
        {
            gl::framebuffer_texture2D(
                GL_READ_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                t.single_sampled_texture2D,
                0
            );
        },
        [face](SingleSampledCubemap& t)
        {
            glFramebufferTexture2D(
                GL_READ_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                to_opengl_texture_cubemap_enum(face),
                t.cubemap.get(),
                0
            );
        }
    }, const_cast<RenderTexture::Impl&>(*src.m_Impl).getColorRenderBufferData());
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    // create a destination (draw) framebuffer for blitting to the destination render texture
    gl::FrameBuffer drawFBO;
    gl::bind_framebuffer(GL_DRAW_FRAMEBUFFER, drawFBO);
    gl::framebuffer_texture2D(
        GL_DRAW_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        dest.m_Impl.upd()->updTexture(),
        0
    );
    glDrawBuffer(GL_COLOR_ATTACHMENT0);

    // blit the read framebuffer to the draw framebuffer
    gl::blit_framebuffer(
        0,
        0,
        src.getDimensions().x,
        src.getDimensions().y,
        0,
        0,
        dest.getDimensions().x,
        dest.getDimensions().y,
        GL_COLOR_BUFFER_BIT,
        GL_LINEAR  // the two texture may have different dimensions (avoid GL_NEAREST)
    );

    // then download the blitted data into the texture's CPU buffer
    {
        std::vector<uint8_t>& cpuBuffer = dest.m_Impl.upd()->pixel_data_;
        const GLint packFormat = to_opengl_image_pixel_pack_alignment(dest.texture_format());

        OSC_ASSERT(is_aligned_at_least(cpuBuffer.data(), packFormat) && "glReadPixels must be called with a buffer that is aligned to GL_PACK_ALIGNMENT (see: https://www.khronos.org/opengl/wiki/Common_Mistakes)");
        OSC_ASSERT(cpuBuffer.size() == static_cast<ptrdiff_t>(dest.getDimensions().x*dest.getDimensions().y)*num_bytes_per_pixel_in(dest.texture_format()));

        gl::viewport(0, 0, dest.getDimensions().x, dest.getDimensions().y);
        gl::bind_framebuffer(GL_READ_FRAMEBUFFER, drawFBO);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        gl::pixel_store_i(GL_PACK_ALIGNMENT, packFormat);
        glReadPixels(
            0,
            0,
            dest.getDimensions().x,
            dest.getDimensions().y,
            to_opengl_image_color_format_enum(dest.texture_format()),
            to_opengl_image_data_type_enum(dest.texture_format()),
            cpuBuffer.data()
        );
    }
    gl::bind_framebuffer(GL_FRAMEBUFFER, gl::window_framebuffer);
}

void osc::GraphicsBackend::copy_texture(
    const RenderTexture& sourceRenderTexture,
    Cubemap& destinationCubemap,
    size_t mip)
{
    // from: https://registry.khronos.org/OpenGL-Refpages/es2.0/xhtml/glTexParameter.xml
    //
    // > To define the mipmap levels, call glTexImage2D, glCompressedTexImage2D, or glCopyTexImage2D
    // > with the level argument indicating the order of the mipmaps. Level 0 is the original texture;
    // > level floor(log2(max(w, h))) is the final 1 x 1 mipmap.
    //
    // related:
    //
    // - https://registry.khronos.org/OpenGL-Refpages/es2.0/xhtml/glTexImage2D.xml
    const size_t maxMipmapLevel = static_cast<size_t>(max(
        0,
        cpp20::bit_width(static_cast<size_t>(destinationCubemap.width())) - 1
    ));

    OSC_ASSERT(sourceRenderTexture.getDimensionality() == TextureDimensionality::Cube && "provided render texture must be a cubemap to call this method");
    OSC_ASSERT(mip <= maxMipmapLevel);

    // blit each face of the source cubemap into the output cubemap
    for (size_t face = 0; face < 6; ++face)
    {
        gl::FrameBuffer readFBO;
        gl::bind_framebuffer(GL_READ_FRAMEBUFFER, readFBO);
        std::visit(Overload  // attach source texture depending on rendertexture's type
        {
            [](SingleSampledTexture&)
            {
                OSC_ASSERT(false && "cannot call copy_texture (Cubemap --> Cubemap) with a 2D render");
            },
            [](MultisampledRBOAndResolvedTexture&)
            {
                OSC_ASSERT(false && "cannot call copy_texture (Cubemap --> Cubemap) with a 2D render");
            },
            [face](SingleSampledCubemap& t)
            {
                glFramebufferTexture2D(
                    GL_READ_FRAMEBUFFER,
                    GL_COLOR_ATTACHMENT0,
                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(face),
                    t.cubemap.get(),
                    0
                );
            }
        }, const_cast<RenderTexture::Impl&>(*sourceRenderTexture.m_Impl).getColorRenderBufferData());
        glReadBuffer(GL_COLOR_ATTACHMENT0);

        gl::FrameBuffer drawFBO;
        gl::bind_framebuffer(GL_DRAW_FRAMEBUFFER, drawFBO);
        glFramebufferTexture2D(
            GL_DRAW_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(face),
            destinationCubemap.impl_.upd()->upd_cubemap().get(),
            static_cast<GLint>(mip)
        );
        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        // blit the read framebuffer to the draw framebuffer
        gl::blit_framebuffer(
            0,
            0,
            sourceRenderTexture.getDimensions().x,
            sourceRenderTexture.getDimensions().y,
            0,
            0,
            destinationCubemap.width() / (1<<mip),
            destinationCubemap.width() / (1<<mip),
            GL_COLOR_BUFFER_BIT,
            GL_LINEAR  // the two texture may have different dimensions (avoid GL_NEAREST)
        );
    }

    // TODO: should be copied into CPU memory if mip==0? (won't store mipmaps in the CPU but
    // maybe it makes sense to store the mip==0 in CPU?)
}
