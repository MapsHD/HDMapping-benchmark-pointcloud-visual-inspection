#pragma once

#include <cvc/point_cloud.hpp>

#include <glm/vec3.hpp>

#include <array>

namespace cvc
{

    struct Bounds
    {
        glm::dvec3 min{0.0};
        glm::dvec3 max{0.0};

        glm::dvec3                center() const;
        glm::dvec3                half_extent() const;
        std::array<glm::dvec3, 8> corners() const;
    };

    Bounds compute_bounds(const PointCloud& cloud);

}
