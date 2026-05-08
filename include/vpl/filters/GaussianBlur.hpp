#pragma once
#include "../Frame.hpp"
#include "../ThreadPool.hpp"
#include <cmath>
#include <vector>
#include <algorithm>

namespace vpl {
namespace filters {

class GaussianBlur {
    std::vector<float> kernel_;
    int                radius_;

    static std::vector<float> make_kernel(int radius, float sigma) {
        std::vector<float> k(2 * radius + 1);
        float sum = 0.0f;
        for (int i = -radius; i <= radius; ++i) {
            k[i + radius] = std::exp(-0.5f * (i * i) / (sigma * sigma));
            sum += k[i + radius];
        }
        for (auto& v : k) v /= sum;
        return k;
    }

public:
    explicit GaussianBlur(int radius = 3, float sigma = 1.0f)
        : kernel_(make_kernel(radius, sigma)), radius_(radius) {}

    Frame operator()(Frame src) const {
        const uint32_t W = src.width(), H = src.height(), CH = src.channels();
        Frame tmp(W, H, CH);
        Frame dst(W, H, CH);

        // horizontal pass
        global_pool().parallel_for(H, [&](std::size_t y0, std::size_t y1) {
            for (std::size_t y = y0; y < y1; ++y) {
                for (uint32_t x = 0; x < W; ++x) {
                    for (uint32_t c = 0; c < CH; ++c) {
                        float acc = 0.0f;
                        for (int k = -radius_; k <= radius_; ++k) {
                            int sx = std::clamp(static_cast<int>(x) + k, 0, static_cast<int>(W) - 1);
                            acc += kernel_[k + radius_] * src.at(static_cast<uint32_t>(y), static_cast<uint32_t>(sx), c);
                        }
                        tmp.at(static_cast<uint32_t>(y), x, c) = static_cast<uint8_t>(std::clamp(acc, 0.0f, 255.0f));
                    }
                }
            }
        });

        // vertical pass
        global_pool().parallel_for(H, [&](std::size_t y0, std::size_t y1) {
            for (std::size_t y = y0; y < y1; ++y) {
                for (uint32_t x = 0; x < W; ++x) {
                    for (uint32_t c = 0; c < CH; ++c) {
                        float acc = 0.0f;
                        for (int k = -radius_; k <= radius_; ++k) {
                            int sy = std::clamp(static_cast<int>(y) + k, 0, static_cast<int>(H) - 1);
                            acc += kernel_[k + radius_] * tmp.at(static_cast<uint32_t>(sy), x, c);
                        }
                        dst.at(static_cast<uint32_t>(y), x, c) = static_cast<uint8_t>(std::clamp(acc, 0.0f, 255.0f));
                    }
                }
            }
        });

        dst.set_id(src.id());
        dst.set_ts(src.timestamp());
        return dst;
    }
};

} // namespace filters
} // namespace vpl
