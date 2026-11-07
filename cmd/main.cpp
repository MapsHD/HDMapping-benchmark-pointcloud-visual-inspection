#include <cvc/bounds.hpp>
#include <cvc/camera.hpp>
#include <cvc/color.hpp>
#include <cvc/image.hpp>
#include <cvc/mosaic.hpp>
#include <cvc/opencl_renderer.hpp>
#include <cvc/point_cloud.hpp>

#include <CLI/CLI.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <deque>
#include <exception>
#include <filesystem>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{

    class Stopwatch
    {
    public:
        double seconds() const { return std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count(); }

    private:
        std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();
    };

    class PngWriter
    {
    public:
        PngWriter(std::size_t workers, std::size_t budget) : workers_(workers), budget_(budget) {}

        void write(cvc::Image image, std::filesystem::path path)
        {
            const std::size_t bytes = image.pixels.size();
            while (!pending_.empty() && (pending_.size() >= workers_ || pending_bytes_ + bytes > budget_))
            {
                finish_oldest();
            }
            pending_bytes_ += bytes;
            pending_.push_back({std::async(std::launch::async, [image = std::move(image), path = std::move(path)]
                                           { cvc::save_png(image, path); }),
                                bytes});
        }

        void wait()
        {
            while (!pending_.empty())
            {
                finish_oldest();
            }
        }

    private:
        struct Job
        {
            std::future<void> done;
            std::size_t       bytes = 0;
        };

        void finish_oldest()
        {
            Job job = std::move(pending_.front());
            pending_.pop_front();
            pending_bytes_ -= job.bytes;
            job.done.get();
        }

        std::size_t     workers_;
        std::size_t     budget_;
        std::size_t     pending_bytes_ = 0;
        std::deque<Job> pending_;
    };

    struct Options
    {
        std::filesystem::path              reference;
        std::vector<std::filesystem::path> compare;
        std::filesystem::path              output_dir;
        int                                resolution        = 16384;
        int                                mosaic_resolution = 0;
        int                                point_radius      = 0;
        double                             fit_fraction      = 0.9999;
        cvc::PixelFormat                   format            = cvc::PixelFormat::grayscale_alpha;
        cvc::Colormap                      colormap          = cvc::Colormap::viridis;
    };

    const std::map<std::string, cvc::PixelFormat>& format_names()
    {
        static const std::map<std::string, cvc::PixelFormat> names{
            {"grayscale", cvc::PixelFormat::grayscale},
            {"grayscale_alpha", cvc::PixelFormat::grayscale_alpha},
            {"color", cvc::PixelFormat::color},
            {"color_alpha", cvc::PixelFormat::color_alpha},
        };
        return names;
    }

    const std::map<std::string, cvc::Colormap>& colormap_names()
    {
        static const std::map<std::string, cvc::Colormap> names{
            {"viridis", cvc::Colormap::viridis},
            {"turbo", cvc::Colormap::turbo},
            {"inferno", cvc::Colormap::inferno},
            {"plasma", cvc::Colormap::plasma},
            {"cividis", cvc::Colormap::cividis},
            {"magma", cvc::Colormap::magma},
            {"cubehelix", cvc::Colormap::cubehelix},
            {"hot", cvc::Colormap::hot},
            {"blues", cvc::Colormap::blues},
            {"blue_white_red", cvc::Colormap::blue_white_red},
        };
        return names;
    }

    template <typename T>
    std::string name_of(const std::map<std::string, T>& names, T value)
    {
        for (const auto& [name, candidate] : names)
        {
            if (candidate == value)
            {
                return name;
            }
        }
        return "unknown";
    }

    bool uses_colormap(cvc::PixelFormat format) { return cvc::channel_count(format) >= 3; }

    struct ViewPlan
    {
        cvc::Shot       shot;
        cvc::MosaicPlan mosaic;
    };

    std::string format(const glm::dvec3& v)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(3) << "(" << v.x << ", " << v.y << ", " << v.z << ")";
        return out.str();
    }

    std::string size_text(int width, int height) { return std::to_string(width) + " x " + std::to_string(height) + " px"; }

    std::string describe(const cvc::OrthographicCamera& camera)
    {
        std::ostringstream out;
        out << size_text(camera.width, camera.height) << ", " << std::fixed << std::setprecision(3)
            << 2.0 * camera.half_width << " x " << 2.0 * camera.half_height << " m, " << std::setprecision(5)
            << 2.0 * camera.half_width / camera.width << " m/px";
        return out.str();
    }

    std::string directory_label(const std::filesystem::path& path)
    {
        const std::filesystem::path directory = std::filesystem::absolute(path).parent_path().filename();
        return directory.empty() ? path.stem().string() : directory.string();
    }

    std::vector<std::string> make_labels(const std::vector<std::filesystem::path>& paths)
    {
        std::vector<std::string> directories;
        for (const std::filesystem::path& path : paths)
        {
            directories.push_back(directory_label(path));
        }

        std::vector<std::string> labels = directories;
        for (std::size_t i = 0; i < labels.size(); ++i)
        {
            if (std::count(directories.begin(), directories.end(), directories[i]) > 1)
            {
                labels[i] += "_" + paths[i].stem().string();
            }
        }

        for (const std::string& label : labels)
        {
            if (std::count(labels.begin(), labels.end(), label) > 1)
            {
                throw std::runtime_error("output name '" + label + "' is shared by more than one point cloud");
            }
        }
        return labels;
    }

    std::filesystem::path png_path(const std::filesystem::path& directory, const std::string& name)
    {
        return (directory / (name + ".png")).make_preferred();
    }

    std::vector<ViewPlan> plan_views(const cvc::PointCloud& reference, const cvc::Bounds& bounds, std::size_t count,
                                     const Options& options)
    {
        std::vector<ViewPlan> plans;
        for (const cvc::Shot& shot :
             cvc::fit_views(reference, cvc::make_views(bounds), options.resolution, 1.02, options.fit_fraction))
        {
            const ViewPlan plan{shot, options.mosaic_resolution > 0
                                          ? cvc::plan_mosaic(count, shot.camera, options.mosaic_resolution, options.format)
                                          : cvc::plan_largest_mosaic(count, shot.camera, options.format)};
            if (!cvc::png_writable(shot.camera.width, shot.camera.height, cvc::channel_count(options.format)))
            {
                throw std::runtime_error("render of view " + shot.view.name + " (" +
                                         size_text(shot.camera.width, shot.camera.height) +
                                         ") exceeds the 2 GiB PNG writer limit, lower --resolution");
            }
            const cvc::MosaicLayout& layout = plan.mosaic.layout;
            if (!cvc::png_writable(plan.mosaic))
            {
                throw std::runtime_error("mosaic of view " + shot.view.name + " (" + size_text(layout.width(), layout.height()) +
                                         ", tiles " + size_text(layout.tile_width, layout.tile_height) +
                                         ") exceeds the 2 GiB PNG writer limit, lower --mosaic-resolution");
            }
            plans.push_back(plan);
        }
        return plans;
    }

    void run(const Options& options)
    {
        const Stopwatch total;

        std::vector<std::filesystem::path> paths{options.reference};
        paths.insert(paths.end(), options.compare.begin(), options.compare.end());
        const std::vector<std::string> labels     = make_labels(paths);
        const std::filesystem::path    full_dir   = options.output_dir / "full";
        const std::filesystem::path    tile_dir   = options.output_dir / "tile";
        const std::filesystem::path    mosaic_dir = options.output_dir / "mosaic";

        cvc::OpenCLRenderer renderer;
        std::cout << "OpenCL device: " << renderer.device_name() << "\n";
        std::cout << "Format: " << name_of(format_names(), options.format);
        if (uses_colormap(options.format))
        {
            std::cout << ", colormap " << name_of(colormap_names(), options.colormap);
        }
        std::cout << "\n";

        PngWriter             writer(std::clamp<std::size_t>(std::thread::hardware_concurrency(), 1, 4), std::size_t{3} << 30);
        std::vector<ViewPlan> plans;

        for (std::size_t i = 0; i < paths.size(); ++i)
        {
            std::cout << "[" << i + 1 << "/" << paths.size() << "] " << labels[i] << ": "
                      << std::filesystem::path(paths[i]).make_preferred().string() << "\n";

            cvc::RenderSettings settings;
            settings.point_radius = options.point_radius;
            settings.format       = options.format;
            settings.colormap     = options.colormap;
            {
                const Stopwatch       load_watch;
                const cvc::PointCloud cloud  = cvc::load_ply(paths[i]);
                const cvc::Bounds     bounds = cvc::compute_bounds(cloud);
                std::cout << "  points: " << cloud.points.size() << " (" << load_watch.seconds() << " s)\n";
                std::cout << "  bounds min: " << format(bounds.min) << "\n";
                std::cout << "  bounds max: " << format(bounds.max) << "\n";

                if (i == 0)
                {
                    const Stopwatch fit_watch;
                    plans = plan_views(cloud, bounds, paths.size(), options);
                    std::cout << "Views fitted to " << 100.0 * options.fit_fraction << "% of " << labels[i] << " ("
                              << fit_watch.seconds() << " s):\n";
                    for (const ViewPlan& plan : plans)
                    {
                        const cvc::MosaicLayout& layout = plan.mosaic.layout;
                        std::cout << "  " << std::left << std::setw(9) << plan.shot.view.name << std::right << " "
                                  << describe(plan.shot.camera) << ", mosaic " << size_text(layout.width(), layout.height())
                                  << " (" << layout.columns << " x " << layout.rows << " tiles of "
                                  << size_text(layout.tile_width, layout.tile_height) << ")\n";
                    }
                }

                settings.intensity = cvc::intensity_range(cloud, 0.01, 0.99);
                std::cout << "  intensity range: [" << settings.intensity.min << ", " << settings.intensity.max << "]\n";

                renderer.upload(cloud);
            }

            const Stopwatch render_watch;
            for (const ViewPlan& plan : plans)
            {
                const std::string name = labels[i] + "_" + plan.shot.view.name;
                writer.write(renderer.render(plan.shot.view, plan.shot.camera, settings), png_path(full_dir, name));
                writer.write(renderer.render(plan.shot.view, plan.mosaic.tile_camera, settings), png_path(tile_dir, name));
            }
            std::cout << "  rendered " << plans.size() << " views and mosaic tiles (" << render_watch.seconds() << " s)\n";
        }

        const Stopwatch render_save_watch;
        writer.wait();
        std::cout << "Saved " << paths.size() * plans.size() << " renders to " << full_dir.string() << " and tiles to "
                  << tile_dir.string() << " (waited " << render_save_watch.seconds() << " s for encoding)\n";

        std::cout << "Building mosaics from " << tile_dir.string() << "\n";
        for (const ViewPlan& plan : plans)
        {
            const Stopwatch mosaic_watch;
            cvc::Mosaic     mosaic(plan.mosaic, paths.size());
            for (std::size_t i = 0; i < paths.size(); ++i)
            {
                mosaic.place(i,
                             cvc::load_png(png_path(tile_dir, labels[i] + "_" + plan.shot.view.name),
                                           plan.mosaic.tile_channels),
                             labels[i]);
            }

            std::filesystem::path path = png_path(mosaic_dir, plan.shot.view.name);
            std::cout << "  " << std::left << std::setw(9) << plan.shot.view.name << std::right << " -> " << path.string()
                      << " (" << mosaic_watch.seconds() << " s)\n";
            writer.write(mosaic.release(), std::move(path));
        }

        const Stopwatch mosaic_save_watch;
        writer.wait();
        std::cout << "Saved " << plans.size() << " mosaics to " << mosaic_dir.string() << " (waited "
                  << mosaic_save_watch.seconds() << " s for encoding)\n";
        std::cout << "Done in " << total.seconds() << " s\n";
    }

}

int main(int argc, char** argv)
{
    Options options;

    CLI::App app{"Renders a reference point cloud and point clouds registered to it from the same 14 orthographic views into PNG images and per-view mosaics"};
    app.add_option("--reference", options.reference, "Reference point cloud (PLY with x, y, z, intensity)")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("--compare", options.compare, "Point clouds registered to the reference, rendered with the reference views")
        ->check(CLI::ExistingFile);
    app.add_option("--output-dir", options.output_dir, "Output directory; renders go to full, mosaic tiles to tile and mosaics to mosaic inside it")
        ->required();
    app.add_option("--resolution", options.resolution, "Render size in pixels along the longer axis, limited by the 2 GiB PNG writer buffer")
        ->check(CLI::Range(1024, 65535))
        ->capture_default_str();
    app.add_option("--mosaic-resolution", options.mosaic_resolution, "Mosaic size in pixels along the longer axis, 0 picks the largest size the 2 GiB PNG writer buffer allows")
        ->check(CLI::Range(0, 65535))
        ->capture_default_str();
    app.add_option("--point-radius", options.point_radius, "Point splat radius in pixels")
        ->check(CLI::Range(0, 8))
        ->capture_default_str();
    app.add_option("--fit-fraction", options.fit_fraction, "Minimum fraction of reference points inside every view (1 keeps all)")
        ->check(CLI::Range(0.5, 1.0))
        ->default_str("0.9999");
    app.add_option("--format", options.format, "Pixel format of renders and mosaics: grayscale, grayscale_alpha, color or color_alpha")
        ->transform(CLI::CheckedTransformer(format_names(), CLI::ignore_case))
        ->default_str("grayscale_alpha");
    CLI::Option* colormap = app.add_option("--colormap", options.colormap, "Intensity colormap for the color and color_alpha formats")
                                ->transform(CLI::CheckedTransformer(colormap_names(), CLI::ignore_case))
                                ->default_str("viridis");
    CLI11_PARSE(app, argc, argv);

    if (colormap->count() > 0 && !uses_colormap(options.format))
    {
        return app.exit(CLI::ValidationError("--colormap", "applies only to --format color or color_alpha"));
    }

    try
    {
        run(options);
    }
    catch (const std::exception& e)
    {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
