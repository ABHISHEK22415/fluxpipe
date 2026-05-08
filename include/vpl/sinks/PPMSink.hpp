#pragma once
#include "../Frame.hpp"
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace vpl {
namespace sinks {

class PPMSink {
    std::filesystem::path dir_;
    bool                  enabled_;

public:
    explicit PPMSink(std::string dir, bool enabled = true)
        : dir_(std::move(dir)), enabled_(enabled)
    {
        if (enabled_) std::filesystem::create_directories(dir_);
    }

    void write(const Frame& f) {
        if (!enabled_) return;
        char name[32];
        std::snprintf(name, sizeof(name), "frame_%06llu.ppm",
                      static_cast<unsigned long long>(f.id()));
        const auto path = dir_ / name;
        FILE* fp = std::fopen(path.string().c_str(), "wb");
        if (!fp) throw std::runtime_error("PPMSink: cannot open " + path.string());
        std::fprintf(fp, "P6\n%u %u\n255\n", f.width(), f.height());
        if (f.channels() == 3) {
            std::fwrite(f.data(), 1, f.bytes(), fp);
        } else {
            for (uint32_t y = 0; y < f.height(); ++y)
                for (uint32_t x = 0; x < f.width(); ++x) {
                    const uint8_t px[3] = {f.at(y,x,0), f.at(y,x,0), f.at(y,x,0)};
                    std::fwrite(px, 1, 3, fp);
                }
        }
        std::fclose(fp);
    }

    void set_enabled(bool e) { enabled_ = e; }
};

struct NullSink {
    void write(const Frame&) noexcept {}
};

} // namespace sinks
} // namespace vpl
