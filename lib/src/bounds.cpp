#include <cvc/bounds.hpp>

#include <glm/common.hpp>

#include <cstddef>
#include <limits>
#include <stdexcept>

namespace cvc
{

    glm::dvec3 Bounds::center() const { return (min + max) * 0.5; }

    glm::dvec3 Bounds::half_extent() const { return (max - min) * 0.5; }

    std::array<glm::dvec3, 8> Bounds::corners() const
    {
        std::array<glm::dvec3, 8> result;
        for (std::size_t i = 0; i < result.size(); ++i)
        {
            result[i] = glm::dvec3((i & 1) ? max.x : min.x, (i & 2) ? max.y : min.y, (i & 4) ? max.z : min.z);
        }
        return result;
    }

    Bounds compute_bounds(const PointCloud& cloud)
    {
        if (cloud.points.empty())
        {
            throw std::runtime_error("cannot compute bounds of an empty point cloud");
        }

        glm::vec3 lo(std::numeric_limits<float>::infinity());
        glm::vec3 hi(-std::numeric_limits<float>::infinity());
        for (const Point& p : cloud.points)
        {
            lo = glm::min(lo, p.position);
            hi = glm::max(hi, p.position);
        }

        return {cloud.origin + glm::dvec3(lo), cloud.origin + glm::dvec3(hi)};
    }

}
