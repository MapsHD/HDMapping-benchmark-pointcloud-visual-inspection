#include <cvc/image.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <climits>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>

namespace cvc
{

    bool png_writable(int width, int height, int channels)
    {
        if (width <= 0 || height <= 0 || channels <= 0)
        {
            return false;
        }
        const std::uint64_t limit     = static_cast<std::uint64_t>(INT_MAX);
        const std::uint64_t row_bytes = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(channels) + 1;
        const std::uint64_t data      = row_bytes * static_cast<std::uint64_t>(height);
        const std::uint64_t stored    = data + 2 + ((data + 32766) / 32767) * 5;
        return data + 32766 <= limit && stored + 64 <= limit;
    }

    void save_png(const Image& image, const std::filesystem::path& path)
    {
        const std::size_t expected = static_cast<std::size_t>(image.width) * image.height * image.channels;
        if (image.width <= 0 || image.height <= 0 || image.channels <= 0 || image.pixels.size() != expected)
        {
            throw std::invalid_argument("invalid image for " + path.string());
        }
        if (!png_writable(image.width, image.height, image.channels))
        {
            throw std::invalid_argument(std::to_string(image.width) + " x " + std::to_string(image.height) +
                                        " image exceeds the 2 GiB buffer limit of the PNG writer: " + path.string());
        }

        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path());
        }

        const int stride = image.width * image.channels;
        if (!stbi_write_png(path.string().c_str(), image.width, image.height, image.channels, image.pixels.data(), stride))
        {
            throw std::runtime_error("failed to write " + path.string());
        }
    }

    Image load_png(const std::filesystem::path& path, int channels)
    {
        int width           = 0;
        int height          = 0;
        int source_channels = 0;

        const std::unique_ptr<stbi_uc, void (*)(void*)> pixels(
            stbi_load(path.string().c_str(), &width, &height, &source_channels, channels), stbi_image_free);
        if (!pixels)
        {
            throw std::runtime_error("failed to read " + path.string() + ": " + stbi_failure_reason());
        }

        Image image;
        image.width    = width;
        image.height   = height;
        image.channels = channels;
        image.pixels.assign(pixels.get(), pixels.get() + static_cast<std::size_t>(width) * height * channels);
        return image;
    }

}
