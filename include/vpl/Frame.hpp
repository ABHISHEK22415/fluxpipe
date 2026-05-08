#pragma once
#include <cstdint>
#include <memory>
#include <chrono>
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace vpl {

struct RGB  { uint8_t r, g, b; };
struct RGBA { uint8_t r, g, b, a; };

class Frame {
public:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    Frame() noexcept = default;

    Frame(uint32_t w, uint32_t h, uint32_t ch = 3)
        : w_(w), h_(h), ch_(ch), stride_(w * ch)
        , buf_(std::make_shared<std::vector<uint8_t>>(
               static_cast<size_t>(h) * w * ch, uint8_t{0}))
        , ts_(Clock::now())
    {
        if (!w || !h || !ch)
            throw std::invalid_argument("Frame: dimensions must be > 0");
    }

    Frame(const Frame&)            = default;
    Frame& operator=(const Frame&) = default;
    Frame(Frame&&) noexcept            = default;
    Frame& operator=(Frame&&) noexcept = default;

    [[nodiscard]] Frame clone() const {
        Frame f(w_, h_, ch_);
        std::memcpy(f.buf_->data(), buf_->data(), bytes());
        f.id_ = id_;
        f.ts_ = ts_;
        return f;
    }

    uint32_t  width()    const noexcept { return w_; }
    uint32_t  height()   const noexcept { return h_; }
    uint32_t  channels() const noexcept { return ch_; }
    uint32_t  stride()   const noexcept { return stride_; }
    uint64_t  id()       const noexcept { return id_; }
    TimePoint timestamp() const noexcept { return ts_; }
    size_t    bytes()    const noexcept { return buf_ ? buf_->size() : 0; }

    void set_id(uint64_t id) noexcept { id_ = id; }
    void set_ts(TimePoint t) noexcept { ts_ = t;  }

    uint8_t*       data()       noexcept { return buf_ ? buf_->data() : nullptr; }
    const uint8_t* data() const noexcept { return buf_ ? buf_->data() : nullptr; }

    uint8_t*       row(uint32_t y)       noexcept { return buf_->data() + y * stride_; }
    const uint8_t* row(uint32_t y) const noexcept { return buf_->data() + y * stride_; }

    uint8_t& at(uint32_t y, uint32_t x, uint32_t c = 0) noexcept {
        assert(y < h_ && x < w_ && c < ch_);
        return (*buf_)[y * stride_ + x * ch_ + c];
    }
    const uint8_t& at(uint32_t y, uint32_t x, uint32_t c = 0) const noexcept {
        assert(y < h_ && x < w_ && c < ch_);
        return (*buf_)[y * stride_ + x * ch_ + c];
    }

    RGB pixel(uint32_t y, uint32_t x) const noexcept {
        const uint8_t* p = row(y) + x * ch_;
        return {p[0], p[1], p[2]};
    }
    void set_pixel(uint32_t y, uint32_t x, RGB px) noexcept {
        uint8_t* p = row(y) + x * ch_;
        p[0] = px.r; p[1] = px.g; p[2] = px.b;
    }

    bool valid() const noexcept { return buf_ && !buf_->empty(); }
    explicit operator bool() const noexcept { return valid(); }

private:
    uint32_t  w_ = 0, h_ = 0, ch_ = 0, stride_ = 0;
    std::shared_ptr<std::vector<uint8_t>> buf_;
    uint64_t  id_ = 0;
    TimePoint ts_ = {};
};

} // namespace vpl
