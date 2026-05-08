#pragma once
#include "../Frame.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

#if defined(FLUXPIPE_NEON)
  #include <arm_neon.h>
#endif

namespace vpl {
namespace filters {

#if defined(FLUXPIPE_NEON)
inline Frame color_invert(Frame src) {
    Frame dst = src.clone();
    uint8_t* d = dst.data();
    std::size_t i = 0;
    for (; i + 16 <= dst.bytes(); i += 16)
        vst1q_u8(d + i, vmvnq_u8(vld1q_u8(d + i)));
    for (; i < dst.bytes(); ++i) d[i] ^= 0xFF;
    return dst;
}
#else
inline Frame color_invert(Frame src) {
    Frame dst = src.clone();
    for (std::size_t i = 0; i < dst.bytes(); ++i) dst.data()[i] ^= 0xFF;
    return dst;
}
#endif

inline Frame vignette(Frame src, float strength = 1.2f) {
    Frame dst = src.clone();
    const float cx = src.width() * 0.5f, cy = src.height() * 0.5f;
    const float maxd = std::sqrt(cx*cx + cy*cy);
    for (uint32_t y = 0; y < src.height(); ++y) {
        for (uint32_t x = 0; x < src.width(); ++x) {
            const float dx = (x - cx) / maxd, dy = (y - cy) / maxd;
            const float f  = std::max(0.0f, 1.0f - strength * (dx*dx + dy*dy));
            for (uint32_t c = 0; c < src.channels(); ++c)
                dst.at(y, x, c) = static_cast<uint8_t>(src.at(y, x, c) * f);
        }
    }
    return dst;
}

inline Frame chromatic_aberration(Frame src, int shift = 4) {
    Frame dst = src.clone();
    const uint32_t W = src.width(), H = src.height();
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            const uint32_t rx = static_cast<uint32_t>(std::clamp(static_cast<int>(x) - shift, 0, static_cast<int>(W) - 1));
            const uint32_t bx = static_cast<uint32_t>(std::clamp(static_cast<int>(x) + shift, 0, static_cast<int>(W) - 1));
            dst.at(y, x, 0) = src.at(y, rx, 0);
            dst.at(y, x, 1) = src.at(y, x,  1);
            dst.at(y, x, 2) = src.at(y, bx, 2);
        }
    }
    return dst;
}

inline Frame pixelate(Frame src, uint32_t block = 8) {
    Frame dst = src.clone();
    const uint32_t W = src.width(), H = src.height(), CH = src.channels();
    for (uint32_t by = 0; by < H; by += block) {
        for (uint32_t bx = 0; bx < W; bx += block) {
            std::vector<uint32_t> acc(CH, 0);
            uint32_t count = 0;
            for (uint32_t dy = 0; dy < block && by + dy < H; ++dy)
                for (uint32_t dx = 0; dx < block && bx + dx < W; ++dx, ++count)
                    for (uint32_t c = 0; c < CH; ++c)
                        acc[c] += src.at(by+dy, bx+dx, c);
            for (uint32_t c = 0; c < CH; ++c) acc[c] /= count;
            for (uint32_t dy = 0; dy < block && by + dy < H; ++dy)
                for (uint32_t dx = 0; dx < block && bx + dx < W; ++dx)
                    for (uint32_t c = 0; c < CH; ++c)
                        dst.at(by+dy, bx+dx, c) = static_cast<uint8_t>(acc[c]);
        }
    }
    return dst;
}

inline Frame pixel_sort(Frame src, float threshold = 0.3f) {
    Frame dst = src.clone();
    const uint32_t W = src.width(), H = src.height();
    const uint8_t lo = static_cast<uint8_t>(threshold * 255);
    const auto luma  = [](const uint8_t* p) -> uint32_t {
        return (77u*p[0] + 150u*p[1] + 29u*p[2]) >> 8u;
    };

    for (uint32_t y = 0; y < H; ++y) {
        uint32_t x = 0;
        while (x < W) {
            while (x < W && luma(dst.row(y) + x*3) < lo) ++x;
            uint32_t start = x;
            while (x < W && luma(dst.row(y) + x*3) >= lo) ++x;
            if (x == start) continue;

            std::vector<std::array<uint8_t,3>> pixels(x - start);
            for (uint32_t i = 0; i < x - start; ++i) {
                const uint8_t* p = dst.row(y) + (start + i) * 3;
                pixels[i] = {p[0], p[1], p[2]};
            }
            std::sort(pixels.begin(), pixels.end(),
                [&](const auto& a, const auto& b) { return luma(a.data()) > luma(b.data()); });
            for (uint32_t i = 0; i < x - start; ++i) {
                uint8_t* p = dst.row(y) + (start + i) * 3;
                p[0] = pixels[i][0]; p[1] = pixels[i][1]; p[2] = pixels[i][2];
            }
        }
    }
    return dst;
}

inline Frame threshold(Frame src, uint8_t level = 128) {
    Frame dst = src.clone();
    const uint8_t* s = src.data();
    uint8_t*       d = dst.data();
    const uint32_t n = src.width() * src.height();
    for (uint32_t i = 0; i < n; ++i, s += 3, d += 3) {
        const uint8_t v = ((77u*s[0] + 150u*s[1] + 29u*s[2]) >> 8u) >= level ? 255 : 0;
        d[0] = d[1] = d[2] = v;
    }
    return dst;
}

} // namespace filters
} // namespace vpl
