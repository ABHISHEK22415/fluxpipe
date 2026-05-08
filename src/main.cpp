#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include "vpl/Frame.hpp"
#include "vpl/Pipeline.hpp"
#include "vpl/Dashboard.hpp"
#include "vpl/filters/Grayscale.hpp"
#include "vpl/filters/GaussianBlur.hpp"
#include "vpl/filters/SobelEdge.hpp"
#include "vpl/filters/Effects.hpp"
#include "vpl/sources/PlasmaSource.hpp"
#include "vpl/sinks/PPMSink.hpp"

using namespace vpl;

struct Config {
    std::string demo      = "plasma";
    std::string filter    = "chain";
    std::string output    = "";
    uint32_t    width     = 640;
    uint32_t    height    = 360;
    uint64_t    frames    = 60;
    bool        benchmark = false;
    bool        help      = false;
};

static Config parse_args(int argc, char* argv[]) {
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return (i+1 < argc) ? argv[++i] : ""; };
        if      (a == "--demo")      cfg.demo      = next();
        else if (a == "--filter")    cfg.filter    = next();
        else if (a == "--output")    cfg.output    = next();
        else if (a == "--frames")    cfg.frames    = std::stoull(next());
        else if (a == "--width")     cfg.width     = static_cast<uint32_t>(std::stoul(next()));
        else if (a == "--height")    cfg.height    = static_cast<uint32_t>(std::stoul(next()));
        else if (a == "--benchmark") cfg.benchmark = true;
        else if (a == "--help" || a == "-h") cfg.help = true;
    }
    return cfg;
}

static void print_help() {
    std::puts(
        "\n  FluxPipe — C++20 Video Processing Pipeline\n"
        "\n"
        "  --demo    plasma|mandelbrot|fire|rdiff    [plasma]\n"
        "  --filter  none|gray|blur|sobel|chain|glitch  [chain]\n"
        "  --width   <px>   [640]\n"
        "  --height  <px>   [360]\n"
        "  --frames  <n>    [60]\n"
        "  --output  <dir>  write PPM sequence\n"
        "  --benchmark      no file I/O, pure throughput\n"
        "\n"
        "  ffmpeg -framerate 30 -i out/frame_%06d.ppm -c:v libx264 out.mp4\n"
    );
}

static void build_pipeline(Pipeline& pipe, const std::string& name) {
    if (name == "none") {
        pipe.add_stage("PassThrough", [](Frame f){ return f; });
    } else if (name == "gray") {
        pipe.add_stage("Grayscale", filters::grayscale);
    } else if (name == "blur") {
        filters::GaussianBlur blur(5, 1.5f);
        pipe.add_stage("GaussianBlur", [blur](Frame f) mutable { return blur(std::move(f)); });
    } else if (name == "sobel") {
        filters::GaussianBlur blur(3, 1.0f);
        filters::SobelEdge sobel(1.5f);
        pipe.add_stage("Grayscale",    filters::grayscale);
        pipe.add_stage("GaussianBlur", [blur](Frame f) mutable { return blur(std::move(f)); });
        pipe.add_stage("SobelEdge",    [sobel](Frame f) mutable { return sobel(std::move(f)); });
    } else if (name == "glitch") {
        pipe.add_stage("ChromAbr",  [](Frame f){ return filters::chromatic_aberration(std::move(f), 6); });
        pipe.add_stage("PixelSort", [](Frame f){ return filters::pixel_sort(std::move(f), 0.25f); });
        pipe.add_stage("Vignette",  [](Frame f){ return filters::vignette(std::move(f), 1.5f); });
    } else {
        filters::GaussianBlur blur(3, 0.8f);
        filters::SobelEdge sobel(2.0f);
        pipe.add_stage("Grayscale",    filters::grayscale);
        pipe.add_stage("GaussianBlur", [blur](Frame f) mutable { return blur(std::move(f)); });
        pipe.add_stage("SobelEdge",    [sobel](Frame f) mutable { return sobel(std::move(f)); });
        pipe.add_stage("Invert",       filters::color_invert);
        pipe.add_stage("Vignette",     [](Frame f){ return filters::vignette(std::move(f), 0.8f); });
    }
}

int main(int argc, char* argv[]) {
    const Config cfg = parse_args(argc, argv);
    if (cfg.help) { print_help(); return 0; }

    const bool write_frames = !cfg.output.empty() && !cfg.benchmark;

    std::printf("\n  \033[1;36mFluxPipe\033[0m  %s | %s | %ux%u | %llu frames | SIMD: \033[32m%s\033[0m\n\n",
                cfg.demo.c_str(), cfg.filter.c_str(), cfg.width, cfg.height,
                (unsigned long long)cfg.frames,
#if defined(FLUXPIPE_NEON)
                "NEON"
#elif defined(FLUXPIPE_AVX2)
                "AVX2"
#elif defined(FLUXPIPE_SSE4)
                "SSE4"
#else
                "scalar"
#endif
    );

    Pipeline pipe;
    build_pipeline(pipe, cfg.filter);
    pipe.start();

    sinks::PPMSink ppm(cfg.output.empty() ? "output" : cfg.output, write_frames);

    Dashboard dash(pipe);
    dash.print_header();

    std::thread source_thread([&]() {
        std::unique_ptr<sources::PlasmaGenerator>            plasma_gen;
        std::unique_ptr<sources::MandelbrotGenerator>        mand_gen;
        std::unique_ptr<sources::FireGenerator>              fire_gen;
        std::unique_ptr<sources::ReactionDiffusionGenerator> rdiff_gen;

        if      (cfg.demo == "plasma")     plasma_gen = std::make_unique<sources::PlasmaGenerator>(cfg.width, cfg.height);
        else if (cfg.demo == "mandelbrot") mand_gen   = std::make_unique<sources::MandelbrotGenerator>(cfg.width, cfg.height);
        else if (cfg.demo == "fire")       fire_gen   = std::make_unique<sources::FireGenerator>(cfg.width, cfg.height);
        else                               rdiff_gen  = std::make_unique<sources::ReactionDiffusionGenerator>(cfg.width, cfg.height);

        for (uint64_t i = 0; i < cfg.frames; ++i) {
            Frame f;
            if      (plasma_gen) f = plasma_gen->next(i);
            else if (mand_gen)   f = mand_gen->next(i);
            else if (fire_gen)   f = fire_gen->next(i);
            else                 f = rdiff_gen->next(i);
            pipe.push(std::move(f));
        }
        pipe.finish();
    });

    uint64_t received = 0;
    while (true) {
        auto opt = pipe.pull();
        if (!opt) break;
        ppm.write(*opt);
        ++received;
        if (received % 5 == 0 || received == cfg.frames)
            dash.render(received, cfg.frames);
    }

    if (source_thread.joinable()) source_thread.join();
    dash.print_summary(received);

    if (write_frames) {
        std::printf("  Output: \033[36m%s/\033[0m\n", cfg.output.c_str());
        std::printf("  ffmpeg -framerate 30 -i %s/frame_%%06d.ppm -c:v libx264 out.mp4\n\n",
                     cfg.output.c_str());
    }

    return 0;
}
