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

        constexpr float font_cap_height = 7.0f;

        struct FontVertex
        {
            float        x;
            float        y;
            float        z;
            std::uint8_t color[4];
        };

        static_assert(sizeof(FontVertex) == 16);

        struct Rect
        {
            int x0;
            int y0;
            int x1;
            int y1;
        };

        std::string printable(const std::string& text)
        {
            std::string result = text;
            std::replace_if(result.begin(), result.end(), [](char c)
                            { return c < 32 || c > 126; }, '?');
            return result;
        }

        int scaled(float value, float scale) { return static_cast<int>(std::lround(value * scale)); }

        std::vector<Rect> glyph_rects(const std::string& text, int cap_height)
        {
            std::string             buffer = printable(text);
            std::vector<FontVertex> vertices((buffer.size() + 1) * 64);
            const int               quads = stb_easy_font_print(0.0f, 0.0f, buffer.data(), nullptr, vertices.data(),
                                                                static_cast<int>(vertices.size() * sizeof(FontVertex)));

            const float       scale = cap_height / font_cap_height;
            std::vector<Rect> rects;
            rects.reserve(static_cast<std::size_t>(quads));
            for (int quad = 0; quad < quads; ++quad)
            {
                const FontVertex& first = vertices[static_cast<std::size_t>(quad) * 4];
                const FontVertex& last  = vertices[static_cast<std::size_t>(quad) * 4 + 2];
                rects.push_back({scaled(first.x, scale), scaled(first.y, scale), scaled(last.x, scale), scaled(last.y, scale)});
            }
            return rects;
        }

        void fill_rects(Image& image, int x, int y, const std::vector<Rect>& rects, int grow, const Color& color)
        {
            const int channels = std::min(image.channels, static_cast<int>(color.size()));
            for (const Rect& rect : rects)
            {
                const int x0 = std::max(0, x + rect.x0 - grow);
                const int y0 = std::max(0, y + rect.y0 - grow);
                const int x1 = std::min(image.width, x + rect.x1 + grow);
                const int y1 = std::min(image.height, y + rect.y1 + grow);
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

    TextBounds text_bounds(const std::string& text, int cap_height)
    {
        TextBounds bounds;
        for (const Rect& rect : glyph_rects(text, cap_height))
        {
            bounds.width  = std::max(bounds.width, rect.x1);
            bounds.height = std::max(bounds.height, rect.y1);
        }
        return bounds;
    }

    void draw_text(Image& image, int x, int y, int cap_height, const std::string& text, const Color& color)
    {
        fill_rects(image, x, y, glyph_rects(text, cap_height), 0, color);
    }

    void draw_outlined_text(Image& image, int x, int y, int cap_height, const std::string& text, const Color& color,
                            const Color& outline_color, int outline_width)
    {
        const std::vector<Rect> rects = glyph_rects(text, cap_height);
        fill_rects(image, x, y, rects, std::max(0, outline_width), outline_color);
        fill_rects(image, x, y, rects, 0, color);
    }

}
