#pragma once

#include <cvc/camera.hpp>
#include <cvc/color.hpp>
#include <cvc/image.hpp>
#include <cvc/point_cloud.hpp>

#include <memory>
#include <string>

namespace cvc
{

    struct RenderSettings
    {
        ValueRange  intensity{0.0f, 1.0f};
        int         point_radius = 0;
        PixelFormat format       = PixelFormat::grayscale_alpha;
        Colormap    colormap     = Colormap::viridis;
    };

    class OpenCLRenderer
    {
    public:
        OpenCLRenderer();
        ~OpenCLRenderer();

        OpenCLRenderer(const OpenCLRenderer&)            = delete;
        OpenCLRenderer& operator=(const OpenCLRenderer&) = delete;

        const std::string& device_name() const;

        void upload(const PointCloud& cloud);

        Image render(const View& view, const OrthographicCamera& camera, const RenderSettings& settings);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

}
