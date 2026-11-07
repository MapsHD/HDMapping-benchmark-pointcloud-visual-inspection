#pragma once

#include <cvc/camera.hpp>
#include <cvc/color.hpp>
#include <cvc/image.hpp>

#include <cstddef>
#include <string>

namespace cvc
{

    struct MosaicLayout
    {
        int columns      = 0;
        int rows         = 0;
        int tile_width   = 0;
        int tile_height  = 0;
        int label_height = 0;
        int gap          = 0;

        int width() const;
        int height() const;
        int cell_x(std::size_t index) const;
        int cell_y(std::size_t index) const;
    };

    struct MosaicPlan
    {
        OrthographicCamera tile_camera;
        MosaicLayout       layout;
        int                tile_channels = 4;
        int                channels      = 3;
    };

    MosaicPlan plan_mosaic(std::size_t count, const OrthographicCamera& camera, int resolution, PixelFormat format);

    MosaicPlan plan_largest_mosaic(std::size_t count, const OrthographicCamera& camera, PixelFormat format);

    bool png_writable(const MosaicPlan& plan);

    class Mosaic
    {
    public:
        Mosaic(const MosaicPlan& plan, std::size_t count);

        void place(std::size_t index, const Image& tile, const std::string& label);

        Image release();

    private:
        MosaicLayout layout_;
        std::size_t  count_;
        int          tile_channels_;
        Image        image_;
    };

}
