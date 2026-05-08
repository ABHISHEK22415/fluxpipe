#pragma once
#include "../Frame.hpp"
#include "../ThreadPool.hpp"
#include <cmath>
#include <algorithm>

namespace vpl {
namespace filters {

class SobelEdge {
    float scale_;

public:
    explicit SobelEdge(float scale = 1.0f) : scale_(scale) {}

    Frame operator()(Frame src) const {
        const uint32_t W = src.width(), H = src.height(), CH = src.channels();
        Frame dst(W, H, CH);

        global_pool().parallel_for(H, [&](std::size_t r0, std::size_t r1) {
            for (std::size_t row = r0; row < r1; ++row) {
                const uint32_t y  = static_cast<uint32_t>(row);
                const uint32_t ym = y > 0     ? y - 1 : 0;
                const uint32_t yp = y < H - 1 ? y + 1 : H - 1;

                for (uint32_t x = 0; x < W; ++x) {
                    const uint32_t xm = x > 0     ? x - 1 : 0;
                    const uint32_t xp = x < W - 1 ? x + 1 : W - 1;

                    const float tl = src.at(ym, xm), tc = src.at(ym, x), tr = src.at(ym, xp);
                    const float ml = src.at(y,  xm),                      mr = src.at(y,  xp);
                    const float bl = src.at(yp, xm), bc = src.at(yp, x), br = src.at(yp, xp);

                    const float gx  = (-tl - 2*ml - bl) + (tr + 2*mr + br);
                    const float gy  = (-tl - 2*tc - tr) + (bl + 2*bc + br);
                    const uint8_t m = static_cast<uint8_t>(std::clamp(std::sqrt(gx*gx + gy*gy) * scale_, 0.0f, 255.0f));

                    for (uint32_t c = 0; c < CH; ++c)
                        dst.at(y, x, c) = m;
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
