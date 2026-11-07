#include <cvc/text.hpp>

#include <stb_easy_font.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace cvc
{

    namespace
    {

        constexpr float font_line_height = 12.0f;

        struct FontVertex
        {
            float        x;
            float        y;
            float        z;
            std::uint8_t color[4];
        };

        static_assert(sizeof(FontVertex) == 16);

        std::string printable(const std::string& text)
        {
            std::string result = text;
            std::replace_if(result.begin(), result.end(), [](char c)
                            { return c < 32 || c > 126; }, '?');
            return result;
        }

        int scaled(float value, float scale) { return static_cast<int>(std::lround(value * scale)); }

    }

    int text_width(const std::string& text, int height)
    {
        std::string buffer = printable(text);
        return scaled(static_cast<float>(stb_easy_font_width(buffer.data())), height / font_line_height);
    }

    void draw_text(Image& image, int x, int y, int height, const std::string& text, const Color& color)
    {
        std::string             buffer = printable(text);
        std::vector<FontVertex> vertices((buffer.size() + 1) * 64);
        const int               quads = stb_easy_font_print(0.0f, 0.0f, buffer.data(), nullptr, vertices.data(),
                                                            static_cast<int>(vertices.size() * sizeof(FontVertex)));

        const float scale    = height / font_line_height;
        const int   channels = std::min(image.channels, static_cast<int>(color.size()));
        for (int quad = 0; quad < quads; ++quad)
        {
            const FontVertex& first = vertices[static_cast<std::size_t>(quad) * 4];
            const FontVertex& last  = vertices[static_cast<std::size_t>(quad) * 4 + 2];
            const int         x0    = std::max(0, x + scaled(first.x, scale));
            const int         y0    = std::max(0, y + scaled(first.y, scale));
            const int         x1    = std::min(image.width, x + scaled(last.x, scale));
            const int         y1    = std::min(image.height, y + scaled(last.y, scale));
            for (int py = y0; py < y1; ++py)
            {
                for (int px = x0; px < x1; ++px)
                {
                    std::uint8_t* pixel = &image.pixels[(static_cast<std::size_t>(py) * image.width + px) * image.channels];
                    std::copy_n(color.begin(), channels, pixel);
                }
            }
        }
    }

}
