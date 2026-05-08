#pragma once
#include "../Frame.hpp"
#include <cmath>
#include <cstring>
#include <vector>
#include <random>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace vpl {
namespace sources {

inline RGB hsv_to_rgb(float h, float s, float v) {
    h = h - std::floor(h);
    int   i = static_cast<int>(h * 6);
    float f = h * 6.0f - i;
    float p = v * (1 - s), q = v * (1 - f*s), t = v * (1 - (1-f)*s);
    float r, g, b;
    switch (i % 6) {
        case 0: r=v; g=t; b=p; break;
        case 1: r=q; g=v; b=p; break;
        case 2: r=p; g=v; b=t; break;
        case 3: r=p; g=q; b=v; break;
        case 4: r=t; g=p; b=v; break;
        default: r=v; g=p; b=q; break;
    }
    return { static_cast<uint8_t>(r*255), static_cast<uint8_t>(g*255), static_cast<uint8_t>(b*255) };
}

class PlasmaGenerator {
    uint32_t W_, H_;
    double   t_ = 0.0;

public:
    PlasmaGenerator(uint32_t W, uint32_t H) : W_(W), H_(H) {}

    Frame next(uint64_t id) {
        Frame f(W_, H_);
        const double nw = 1.0 / W_, nh = 1.0 / H_;
        for (uint32_t y = 0; y < H_; ++y) {
            const double ny = y * nh;
            for (uint32_t x = 0; x < W_; ++x) {
                const double nx = x * nw;
                double v = std::sin(nx*10.0 + t_)
                         + std::sin(ny*8.0  + t_*1.3)
                         + std::sin((nx+ny)*6.0 + t_*0.7)
                         + std::sin(std::sqrt(nx*nx + ny*ny)*15.0 + t_*1.1);
                v = (v / 4.0 + 1.0) * 0.5;
                f.set_pixel(y, x, hsv_to_rgb(static_cast<float>(v), 0.85f,
                    0.95f + 0.05f * static_cast<float>(std::sin(v * M_PI * 4))));
            }
        }
        f.set_id(id);
        t_ += 0.04;
        return f;
    }
};

class MandelbrotGenerator {
    uint32_t W_, H_;
    double   zoom_t_ = 0.0;
    static constexpr double kCX = -0.7435669, kCY = 0.1314023;
    static constexpr int    kMaxIter = 256;

public:
    MandelbrotGenerator(uint32_t W, uint32_t H) : W_(W), H_(H) {}

    Frame next(uint64_t id) {
        Frame f(W_, H_);
        const double zoom  = std::exp(zoom_t_ * 0.5);
        const double scale = 3.0 / (zoom * std::min(W_, H_));
        const double hw = W_ * 0.5, hh = H_ * 0.5;

        for (uint32_t y = 0; y < H_; ++y) {
            const double y0 = kCY + (y - hh) * scale;
            for (uint32_t x = 0; x < W_; ++x) {
                const double x0 = kCX + (x - hw) * scale;
                double zx = 0, zy = 0;
                int iter = 0;
                while (zx*zx + zy*zy <= 4.0 && iter < kMaxIter) {
                    const double tmp = zx*zx - zy*zy + x0;
                    zy = 2.0*zx*zy + y0;
                    zx = tmp;
                    ++iter;
                }
                if (iter == kMaxIter) {
                    f.set_pixel(y, x, {0, 0, 0});
                } else {
                    const double log_zn = std::log(zx*zx + zy*zy) * 0.5;
                    const float  t      = static_cast<float>(iter + 1 - std::log(log_zn / std::log(2.0)) / std::log(2.0)) / kMaxIter;
                    f.set_pixel(y, x, hsv_to_rgb(t, 0.9f, t > 0.01f ? 0.9f : 0.0f));
                }
            }
        }
        f.set_id(id);
        zoom_t_ += 0.02;
        return f;
    }
};

class FireGenerator {
    uint32_t W_, H_;
    std::vector<float> heat_;
    std::mt19937 rng_{42};

    static RGB palette(float v) {
        v = std::clamp(v, 0.0f, 1.0f);
        if (v < 0.333f) return { static_cast<uint8_t>(v/0.333f*255), 0, 0 };
        if (v < 0.666f) { float t = (v-0.333f)/0.333f; return { 255, static_cast<uint8_t>(t*165), 0 }; }
        float t = (v-0.666f)/0.334f;
        return { 255, static_cast<uint8_t>(165+t*90), static_cast<uint8_t>(t*255) };
    }

public:
    FireGenerator(uint32_t W, uint32_t H) : W_(W), H_(H), heat_(W*H, 0.0f) {}

    Frame next(uint64_t id) {
        std::uniform_real_distribution<float> dist(0.6f, 1.0f);
        for (uint32_t x = 0; x < W_; ++x) {
            heat_[(H_-1)*W_ + x] = dist(rng_);
            heat_[(H_-2)*W_ + x] = dist(rng_);
        }

        std::vector<float> next(W_ * H_, 0.0f);
        for (uint32_t y = 1; y < H_; ++y) {
            for (uint32_t x = 0; x < W_; ++x) {
                const uint32_t xm = x > 0    ? x-1 : W_-1;
                const uint32_t xp = x < W_-1 ? x+1 : 0;
                next[(y-1)*W_ + x] = (heat_[y*W_+xm] + heat_[y*W_+x] + heat_[y*W_+xp] + heat_[(y-1)*W_+x]) * 0.245f;
            }
        }
        heat_ = std::move(next);

        Frame f(W_, H_);
        for (uint32_t y = 0; y < H_; ++y)
            for (uint32_t x = 0; x < W_; ++x)
                f.set_pixel(y, x, palette(heat_[y*W_+x]));
        f.set_id(id);
        return f;
    }
};

class ReactionDiffusionGenerator {
    uint32_t W_, H_;
    std::vector<float> U_, V_, nU_, nV_;
    float Du_ = 0.2100f, Dv_ = 0.1050f;
    float F_  = 0.0545f, k_  = 0.0620f;

public:
    ReactionDiffusionGenerator(uint32_t W, uint32_t H)
        : W_(W), H_(H), U_(W*H, 1.0f), V_(W*H, 0.0f), nU_(W*H), nV_(W*H)
    {
        std::mt19937 rng(123);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        for (uint32_t y = H/2-20; y < H/2+20; ++y)
            for (uint32_t x = W/2-20; x < W/2+20; ++x)
                if (y < H && x < W) { V_[y*W+x] = 0.5f + 0.1f*dist(rng); U_[y*W+x] = 0.25f; }
    }

    Frame next(uint64_t id) {
        for (int step = 0; step < 8; ++step) {
            for (uint32_t y = 0; y < H_; ++y) {
                for (uint32_t x = 0; x < W_; ++x) {
                    const uint32_t xm = x > 0    ? x-1 : W_-1, xp = x < W_-1 ? x+1 : 0;
                    const uint32_t ym = y > 0    ? y-1 : H_-1, yp = y < H_-1 ? y+1 : 0;
                    const float u = U_[y*W_+x], v = V_[y*W_+x];
                    const float lap_u = U_[ym*W_+x]+U_[yp*W_+x]+U_[y*W_+xm]+U_[y*W_+xp] - 4.0f*u;
                    const float lap_v = V_[ym*W_+x]+V_[yp*W_+x]+V_[y*W_+xm]+V_[y*W_+xp] - 4.0f*v;
                    const float uvv = u*v*v;
                    nU_[y*W_+x] = std::clamp(u + Du_*lap_u - uvv + F_*(1.0f-u), 0.0f, 1.0f);
                    nV_[y*W_+x] = std::clamp(v + Dv_*lap_v + uvv - (F_+k_)*v,   0.0f, 1.0f);
                }
            }
            std::swap(U_, nU_); std::swap(V_, nV_);
        }

        Frame f(W_, H_);
        for (uint32_t y = 0; y < H_; ++y) {
            for (uint32_t x = 0; x < W_; ++x) {
                const uint8_t c = static_cast<uint8_t>(V_[y*W_+x] * 255);
                f.set_pixel(y, x, { static_cast<uint8_t>(255-c),
                                    static_cast<uint8_t>(std::min(c*2, 255)),
                                    c });
            }
        }
        f.set_id(id);
        return f;
    }
};

} // namespace sources
} // namespace vpl
