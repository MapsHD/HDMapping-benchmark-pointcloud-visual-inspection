#include <cvc/mosaic.hpp>
#include <cvc/text.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace cvc
{

    namespace
    {

        // Label cap height relative to the smaller tile side, so it stays about 1/10 of the tile on both axes.
        constexpr double label_ratio = 1.0 / 10.0;
        constexpr double gap_ratio   = 1.0 / 256.0;

        constexpr Color background_color{64, 64, 64, 255};
        constexpr Color outline_color{24, 24, 24, 255};
        constexpr Color text_color{255, 255, 255, 255};

        std::size_t rows_for(std::size_t count, std::size_t columns) { return (count + columns - 1) / columns; }

        std::size_t choose_columns(std::size_t count, double cell_height)
        {
            std::size_t best_columns = 1;
            double      best_score   = std::numeric_limits<double>::infinity();
            for (std::size_t columns = 1; columns <= count; ++columns)
            {
                const double aspect = static_cast<double>(columns) / (static_cast<double>(rows_for(count, columns)) * cell_height);
                const double score  = std::abs(std::log(aspect));
                if (score < best_score)
                {
                    best_score   = score;
                    best_columns = columns;
                }
            }
            return best_columns;
        }

        void fill(Image& image, int x, int y, int width, int height, const Color& color)
        {
            for (int row = y; row < y + height; ++row)
            {
                std::uint8_t* pixel = &image.pixels[(static_cast<std::size_t>(row) * image.width + x) * image.channels];
                for (int column = 0; column < width; ++column, pixel += image.channels)
                {
                    std::copy_n(color.begin(), image.channels, pixel);
                }
            }
        }

        int outline_for(int cap_height) { return std::max(1, cap_height / 14); }

        int margin_for(int cap_height) { return std::max(outline_for(cap_height) + 1, cap_height / 3); }

    }

    int MosaicLayout::width() const { return columns * tile_width + (columns + 1) * gap; }

    int MosaicLayout::height() const { return rows * tile_height + (rows + 1) * gap; }

    int MosaicLayout::cell_x(std::size_t index) const
    {
        return gap + static_cast<int>(index % static_cast<std::size_t>(columns)) * (tile_width + gap);
    }

    int MosaicLayout::cell_y(std::size_t index) const
    {
        return gap + static_cast<int>(index / static_cast<std::size_t>(columns)) * (tile_height + gap);
    }

    MosaicPlan plan_mosaic(std::size_t count, const OrthographicCamera& camera, int resolution, PixelFormat format)
    {
        if (count == 0)
        {
            throw std::invalid_argument("mosaic needs at least one tile");
        }
        if (resolution <= 0 || camera.half_width <= 0.0 || camera.half_height <= 0.0)
        {
            throw std::invalid_argument("invalid mosaic camera or resolution");
        }

        const double      aspect      = camera.half_width / camera.half_height;
        const double      cell_height = 1.0 / aspect;
        const std::size_t columns     = choose_columns(count, cell_height);
        const std::size_t rows        = rows_for(count, columns);
        const double      tile_width  = resolution / std::max(static_cast<double>(columns), rows * cell_height);
        const double      long_side   = aspect >= 1.0 ? tile_width : tile_width / aspect;

        MosaicPlan plan;
        plan.tile_camera = make_camera(camera.half_width, camera.half_height, std::max(1, static_cast<int>(long_side)));

        plan.layout.columns     = static_cast<int>(columns);
        plan.layout.rows        = static_cast<int>(rows);
        plan.layout.tile_width  = plan.tile_camera.width;
        plan.layout.tile_height = plan.tile_camera.height;
        plan.layout.gap         = std::max(2, static_cast<int>(std::lround(plan.tile_camera.width * gap_ratio)));
        plan.tile_channels      = channel_count(format);
        plan.channels           = opaque_channel_count(format);
        return plan;
    }

    bool png_writable(const MosaicPlan& plan)
    {
        return png_writable(plan.layout.width(), plan.layout.height(), plan.channels) &&
               png_writable(plan.layout.tile_width, plan.layout.tile_height, plan.tile_channels);
    }

    MosaicPlan plan_largest_mosaic(std::size_t count, const OrthographicCamera& camera, PixelFormat format)
    {
        int low  = 1;
        int high = 1 << 16;
        while (low < high)
        {
            const int middle = low + (high - low + 1) / 2;
            if (png_writable(plan_mosaic(count, camera, middle, format)))
            {
                low = middle;
            }
            else
            {
                high = middle - 1;
            }
        }

        const MosaicPlan plan = plan_mosaic(count, camera, low, format);
        if (!png_writable(plan))
        {
            throw std::runtime_error("no mosaic size fits the 2 GiB PNG writer limit");
        }
        return plan;
    }

    Mosaic::Mosaic(const MosaicPlan& plan, std::size_t count)
        : layout_(plan.layout), count_(count), tile_channels_(plan.tile_channels)
    {
        if (count == 0 || count > static_cast<std::size_t>(layout_.columns) * static_cast<std::size_t>(layout_.rows))
        {
            throw std::invalid_argument("mosaic layout cannot hold " + std::to_string(count) + " tiles");
        }
        if (plan.channels != 1 && plan.channels != 3)
        {
            throw std::invalid_argument("mosaic must have 1 or 3 channels");
        }

        image_.width    = layout_.width();
        image_.height   = layout_.height();
        image_.channels = plan.channels;
        image_.pixels.resize(static_cast<std::size_t>(image_.width) * image_.height * image_.channels);
        fill(image_, 0, 0, image_.width, image_.height, background_color);
    }

    void Mosaic::place(std::size_t index, const Image& tile, const std::string& label)
    {
        if (index >= count_)
        {
            throw std::out_of_range("mosaic tile index out of range");
        }
        if (tile.width != layout_.tile_width || tile.height != layout_.tile_height || tile.channels != tile_channels_)
        {
            throw std::invalid_argument("mosaic tile for '" + label + "' is " + std::to_string(tile.width) + " x " +
                                        std::to_string(tile.height) + " px with " + std::to_string(tile.channels) +
                                        " channels, layout expects " + std::to_string(layout_.tile_width) + " x " +
                                        std::to_string(layout_.tile_height) + " px with " +
                                        std::to_string(tile_channels_) + " channels");
        }

        const int x = layout_.cell_x(index);
        const int y = layout_.cell_y(index);

        const int  colors    = image_.channels;
        const bool has_alpha = tile.channels > colors;
        for (int row = 0; row < tile.height; ++row)
        {
            const std::uint8_t* source = &tile.pixels[static_cast<std::size_t>(row) * tile.width * tile.channels];
            std::uint8_t*       target = &image_.pixels[(static_cast<std::size_t>(y + row) * image_.width + x) * colors];
            for (int column = 0; column < tile.width; ++column, source += tile.channels, target += colors)
            {
                const unsigned alpha = has_alpha ? source[colors] : 255u;
                for (int channel = 0; channel < colors; ++channel)
                {
                    target[channel] = static_cast<std::uint8_t>((source[channel] * alpha + 127) / 255);
                }
            }
        }

        int        cap_height = std::max(1, static_cast<int>(std::lround(std::min(tile.width, tile.height) * label_ratio)));
        TextBounds bounds     = text_bounds(label, cap_height);
        const int  space      = tile.width - 2 * margin_for(cap_height);
        if (bounds.width > space && bounds.width > 0)
        {
            cap_height = std::max(1, cap_height * std::max(1, space) / bounds.width);
            bounds     = text_bounds(label, cap_height);
        }
        const int margin = margin_for(cap_height);
        draw_outlined_text(image_, std::max(x + margin, x + tile.width - margin - bounds.width), y + margin, cap_height,
                           label, text_color, outline_color, outline_for(cap_height));
    }

    Image Mosaic::release() { return std::move(image_); }

}
