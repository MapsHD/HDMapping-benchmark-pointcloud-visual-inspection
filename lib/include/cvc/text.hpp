#pragma once

#include <cvc/image.hpp>

#include <array>
#include <cstdint>
#include <string>

namespace cvc
{

    using Color = std::array<std::uint8_t, 4>;

    // Pixel extents of the drawn glyphs, relative to the text origin. Descenders reach below cap_height.
    struct TextBounds
    {
        int width  = 0;
        int height = 0;
    };

    // cap_height is the pixel height of capital letters and digits.
    TextBounds text_bounds(const std::string& text, int cap_height);

    void draw_text(Image& image, int x, int y, int cap_height, const std::string& text, const Color& color);

    // Draws the text with a border of outline_width pixels around every glyph stroke.
    void draw_outlined_text(Image& image, int x, int y, int cap_height, const std::string& text, const Color& color,
                            const Color& outline_color, int outline_width);

}
