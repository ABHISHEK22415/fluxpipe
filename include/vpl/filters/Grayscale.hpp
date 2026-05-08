#pragma once
#include "../Frame.hpp"
#include <cstdint>

#if defined(FLUXPIPE_NEON)
  #include <arm_neon.h>
#elif defined(FLUXPIPE_AVX2) || defined(FLUXPIPE_SSE4)
  #include <immintrin.h>
#endif

namespace vpl {
namespace filters {

namespace detail {

inline void gray_scalar(const uint8_t* src, uint8_t* dst, uint32_t n) noexcept {
    for (uint32_t i = 0; i < n; ++i) {
        const uint8_t g = static_cast<uint8_t>((77u*src[0] + 150u*src[1] + 29u*src[2]) >> 8u);
        dst[0] = dst[1] = dst[2] = g;
        src += 3; dst += 3;
    }
}

#if defined(FLUXPIPE_NEON)
// Y = (77R + 150G + 29B) >> 8  (~BT.601, error < 0.4%)
// vld3_u8 deinterleaves 24 bytes → three 8-lane vectors in one instruction
inline void gray_neon(const uint8_t* src, uint8_t* dst, uint32_t n) noexcept {
    uint32_t i = 0;
    for (; i + 8 <= n; i += 8, src += 24, dst += 24) {
        const uint8x8x3_t rgb = vld3_u8(src);
        uint16x8_t y = vaddq_u16(
            vaddq_u16(vmulq_n_u16(vmovl_u8(rgb.val[0]), 77u),
                      vmulq_n_u16(vmovl_u8(rgb.val[1]), 150u)),
            vmulq_n_u16(vmovl_u8(rgb.val[2]), 29u));
        const uint8x8_t gray = vshrn_n_u16(y, 8);
        uint8x8x3_t out; out.val[0] = out.val[1] = out.val[2] = gray;
        vst3_u8(dst, out);
    }
    gray_scalar(src, dst, n - i);
}

#elif defined(FLUXPIPE_AVX2) || defined(FLUXPIPE_SSE4)
inline void gray_sse(const uint8_t* src, uint8_t* dst, uint32_t n) noexcept {
    const __m128i r_shuf = _mm_set_epi8(-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 9,6,3,0);
    const __m128i g_shuf = _mm_set_epi8(-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,10,7,4,1);
    const __m128i b_shuf = _mm_set_epi8(-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,11,8,5,2);
    uint32_t i = 0;
    for (; i + 4 <= n; i += 4, src += 12, dst += 12) {
        __m128i data = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
        __m128i y = _mm_add_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_shuffle_epi8(data, r_shuf)), _mm_set1_epi16(77)),
                          _mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_shuffle_epi8(data, g_shuf)), _mm_set1_epi16(150))),
            _mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_shuffle_epi8(data, b_shuf)), _mm_set1_epi16(29)));
        y = _mm_srli_epi16(y, 8);
        uint32_t gv = static_cast<uint32_t>(_mm_cvtsi128_si32(_mm_packus_epi16(y, y)));
        for (int j = 0; j < 4; ++j) {
            const uint8_t g = static_cast<uint8_t>((gv >> (j * 8)) & 0xFF);
            dst[j*3] = dst[j*3+1] = dst[j*3+2] = g;
        }
    }
    gray_scalar(src, dst, n - i);
}
#endif

} // namespace detail

inline Frame grayscale(Frame src) {
    Frame dst = src.clone();
    const uint32_t n = src.width() * src.height();
#if defined(FLUXPIPE_NEON)
    detail::gray_neon(src.data(), dst.data(), n);
#elif defined(FLUXPIPE_AVX2) || defined(FLUXPIPE_SSE4)
    detail::gray_sse(src.data(), dst.data(), n);
#else
    detail::gray_scalar(src.data(), dst.data(), n);
#endif
    return dst;
}

} // namespace filters
} // namespace vpl
