#pragma once

#include <cvc/image.hpp>

#include <array>
#include <cstdint>
#include <string>

namespace cvc
{

    using Color = std::array<std::uint8_t, 4>;

    int text_width(const std::string& text, int height);

    void draw_text(Image& image, int x, int y, int height, const std::string& text, const Color& color);

}
