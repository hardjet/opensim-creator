#pragma once

#include <oscar/Graphics/Detail/CPUImageFormat.h>
#include <oscar/Graphics/OpenGL/Gl.h>

namespace osc::detail
{
    template<CPUImageFormat>
    struct CPUImageFormatOpenGLTraits;

    template<>
    struct CPUImageFormatOpenGLTraits<CPUImageFormat::R8> {
        static inline constexpr GLenum opengl_format = GL_RED;
    };

    template<>
    struct CPUImageFormatOpenGLTraits<CPUImageFormat::RG> {
        static inline constexpr GLenum opengl_format = GL_RG;
    };

    template<>
    struct CPUImageFormatOpenGLTraits<CPUImageFormat::RGB> {
        static inline constexpr GLenum opengl_format = GL_RGB;
    };

    template<>
    struct CPUImageFormatOpenGLTraits<CPUImageFormat::RGBA> {
        static inline constexpr GLenum opengl_format = GL_RGBA;
    };

    template<>
    struct CPUImageFormatOpenGLTraits<CPUImageFormat::Depth> {
        static inline constexpr GLenum opengl_format = GL_DEPTH_COMPONENT;
    };

    template<>
    struct CPUImageFormatOpenGLTraits<CPUImageFormat::DepthStencil> {
        static inline constexpr GLenum opengl_format = GL_DEPTH_STENCIL;
    };
}
