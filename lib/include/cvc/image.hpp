#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace cvc
{

    struct Image
    {
        int                       width    = 0;
        int                       height   = 0;
        int                       channels = 0;
        std::vector<std::uint8_t> pixels;
    };

    bool png_writable(int width, int height, int channels);

    void save_png(const Image& image, const std::filesystem::path& path);

    Image load_png(const std::filesystem::path& path, int channels);

}
