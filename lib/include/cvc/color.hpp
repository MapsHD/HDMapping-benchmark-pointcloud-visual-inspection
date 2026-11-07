#pragma once

namespace cvc
{

    enum class PixelFormat
    {
        grayscale,
        grayscale_alpha,
        color,
        color_alpha,
    };

    enum class Colormap
    {
        viridis,
        turbo,
        inferno,
        plasma,
        cividis,
        magma,
        cubehelix,
        hot,
        blues,
        blue_white_red,
    };

    inline int channel_count(PixelFormat format)
    {
        switch (format)
        {
        case PixelFormat::grayscale:
            return 1;
        case PixelFormat::grayscale_alpha:
            return 2;
        case PixelFormat::color:
            return 3;
        case PixelFormat::color_alpha:
            return 4;
        }
        return 4;
    }

    inline int opaque_channel_count(PixelFormat format) { return channel_count(format) <= 2 ? 1 : 3; }

}
