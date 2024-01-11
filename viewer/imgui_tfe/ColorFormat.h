#pragma once

#include <GL/gl.h>

namespace hs {

    enum class ColorFormat
    {
        Unspecified,

        R8,
        RG8,
        RGB8,
        RGBA8,
        R16UI,
        RG16UI,
        RGB16UI,
        RGBA16UI,
        R32UI,
        RG32UI,
        RGB32UI,
        RGBA32UI,
        R32F,
        RG32F,
        RGB32F,
        RGBA32F,

        // Keep last!
        Count,
    };

    struct ColorFormatInfoGL
    {
        ColorFormat colorFormat;
        GLuint format;
        GLuint internalFormat;
        GLuint type;
    };

    static ColorFormatInfoGL ColorFormatInfoTableGL[(int)ColorFormat::Count] = {
            { ColorFormat::Unspecified, 0,               0,           0                },
            { ColorFormat::R8,          GL_RED,          GL_R8,       GL_UNSIGNED_BYTE },
            { ColorFormat::RG8,         GL_RG,           GL_RG8,      GL_UNSIGNED_BYTE },
            { ColorFormat::RGB8,        GL_RGB,          GL_RGB8,     GL_UNSIGNED_BYTE },
            { ColorFormat::RGBA8,       GL_RGBA,         GL_RGBA8,    GL_UNSIGNED_BYTE },
            { ColorFormat::R16UI,       GL_RED_INTEGER,  GL_R16UI,    GL_UNSIGNED_INT  },
            { ColorFormat::RG16UI,      GL_RG_INTEGER,   GL_RG16UI,   GL_UNSIGNED_INT  },
            { ColorFormat::RGB16UI,     GL_RGB_INTEGER,  GL_RGB16UI,  GL_UNSIGNED_INT  },
            { ColorFormat::RGBA16UI,    GL_RGBA_INTEGER, GL_RGBA16UI, GL_UNSIGNED_INT  },
            { ColorFormat::R32UI,       GL_RED_INTEGER,  GL_R32UI,    GL_UNSIGNED_INT  },
            { ColorFormat::RG32UI,      GL_RG_INTEGER,   GL_RG32UI,   GL_UNSIGNED_INT  },
            { ColorFormat::RGB32UI,     GL_RGB_INTEGER,  GL_RGB32UI,  GL_UNSIGNED_INT  },
            { ColorFormat::RGBA32UI,    GL_RGBA_INTEGER, GL_RGBA32UI, GL_UNSIGNED_INT  },
            { ColorFormat::R32F,        GL_RED,          GL_R32F,     GL_FLOAT         },
            { ColorFormat::RG32F,       GL_RG,           GL_RG32F,    GL_FLOAT         },
            { ColorFormat::RGB32F,      GL_RGB,          GL_RGB32F,   GL_FLOAT         },
            { ColorFormat::RGBA32F,     GL_RGBA,         GL_RGBA32F,  GL_FLOAT         },

    };
} // namespace hs
