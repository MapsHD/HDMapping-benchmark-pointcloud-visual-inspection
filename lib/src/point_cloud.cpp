#include <cvc/point_cloud.hpp>

#include <happly.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace cvc
{

    namespace
    {

        template <typename Target, typename Source>
        std::vector<Target> cast_values(const std::vector<Source>& values)
        {
            std::vector<Target> result(values.size());
            std::transform(values.begin(), values.end(), result.begin(), [](Source value)
                           { return static_cast<Target>(value); });
            return result;
        }

        std::vector<double> read_property(happly::Element& element, const std::string& name)
        {
            if (!element.hasProperty(name))
            {
                throw std::runtime_error("PLY element '" + element.name + "' has no property '" + name + "'");
            }
            const std::string type = element.getPropertyPtr(name)->propertyTypeName();
            if (type == "float" || type == "double")
            {
                return element.getProperty<double>(name);
            }
            if (type == "char" || type == "short" || type == "int")
            {
                return cast_values<double>(element.getProperty<std::int32_t>(name));
            }
            if (type == "uchar" || type == "ushort" || type == "uint")
            {
                return cast_values<double>(element.getProperty<std::uint32_t>(name));
            }
            throw std::runtime_error("PLY property '" + name + "' has unsupported type '" + type + "'");
        }

    }

    PointCloud load_ply(const std::filesystem::path& path)
    {
        happly::PLYData  ply(path.string());
        happly::Element& vertex = ply.getElement("vertex");

        PointCloud cloud;
        cloud.points.resize(vertex.count);

        const std::array<const char*, 3> axis_names{"x", "y", "z"};
        for (glm::length_t axis = 0; axis < 3; ++axis)
        {
            const std::vector<double> values = read_property(vertex, axis_names[static_cast<std::size_t>(axis)]);
            double                    origin = 0.0;
            if (!values.empty())
            {
                const auto [lo, hi] = std::minmax_element(values.begin(), values.end());
                origin              = 0.5 * (*lo + *hi);
            }
            cloud.origin[axis] = origin;
            for (std::size_t i = 0; i < values.size(); ++i)
            {
                cloud.points[i].position[axis] = static_cast<float>(values[i] - origin);
            }
        }

        const std::vector<double> intensity = read_property(vertex, "intensity");
        for (std::size_t i = 0; i < intensity.size(); ++i)
        {
            cloud.points[i].intensity = static_cast<float>(intensity[i]);
        }

        return cloud;
    }

    ValueRange intensity_range(const PointCloud& cloud, double lower_quantile, double upper_quantile)
    {
        if (cloud.points.empty())
        {
            return {};
        }

        std::vector<float> values(cloud.points.size());
        std::transform(cloud.points.begin(), cloud.points.end(), values.begin(), [](const Point& p)
                       { return p.intensity; });

        const auto quantile = [&values](double q)
        {
            const auto k = static_cast<std::ptrdiff_t>(std::clamp(q, 0.0, 1.0) * static_cast<double>(values.size() - 1));
            std::nth_element(values.begin(), values.begin() + k, values.end());
            return values[static_cast<std::size_t>(k)];
        };

        return {quantile(lower_quantile), quantile(upper_quantile)};
    }

}
