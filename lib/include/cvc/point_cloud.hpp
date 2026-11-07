#pragma once

#include <glm/vec3.hpp>

#include <filesystem>
#include <vector>

namespace cvc
{

    struct Point
    {
        glm::vec3 position;
        float     intensity;
    };

    static_assert(sizeof(Point) == 16);

    struct PointCloud
    {
        glm::dvec3         origin{0.0};
        std::vector<Point> points;
    };

    struct ValueRange
    {
        float min = 0.0f;
        float max = 0.0f;
    };

    PointCloud load_ply(const std::filesystem::path& path);

    ValueRange intensity_range(const PointCloud& cloud, double lower_quantile, double upper_quantile);

}
