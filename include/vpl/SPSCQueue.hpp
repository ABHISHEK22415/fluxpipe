#pragma once
#include <atomic>
#include <array>
#include <optional>
#include <type_traits>
#include <thread>

namespace vpl {

// Wait-free single-producer / single-consumer circular buffer.
// Cap must be power of two. Head and tail sit on separate cache lines
// to eliminate false sharing between producer and consumer cores.
template<typename T, std::size_t Cap>
class SPSCQueue {
    static_assert((Cap & (Cap - 1)) == 0, "Cap must be power of two");
    static_assert(Cap >= 2);
    static constexpr std::size_t kMask = Cap - 1;

    struct alignas(64) Idx { std::atomic<std::size_t> v{0}; };

    Idx               head_;
    Idx               tail_;
    std::array<T,Cap> slots_;

public:
    SPSCQueue()  = default;
    ~SPSCQueue() = default;
    SPSCQueue(const SPSCQueue&)            = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    bool push(T item) noexcept(std::is_nothrow_move_assignable_v<T>) {
        const std::size_t t    = tail_.v.load(std::memory_order_relaxed);
        const std::size_t next = (t + 1) & kMask;
        if (next == head_.v.load(std::memory_order_acquire)) return false;
        slots_[t] = std::move(item);
        tail_.v.store(next, std::memory_order_release);
        return true;
    }

    void push_wait(T item) noexcept(std::is_nothrow_move_assignable_v<T>) {
        while (!push(std::move(item))) std::this_thread::yield();
    }

    std::optional<T> pop() noexcept(std::is_nothrow_move_constructible_v<T>) {
        const std::size_t h = head_.v.load(std::memory_order_relaxed);
        if (h == tail_.v.load(std::memory_order_acquire)) return std::nullopt;
        T item = std::move(slots_[h]);
        head_.v.store((h + 1) & kMask, std::memory_order_release);
        return item;
    }

    bool        empty()       const noexcept { return head_.v.load(std::memory_order_acquire) == tail_.v.load(std::memory_order_acquire); }
    bool        full()        const noexcept { return ((tail_.v.load(std::memory_order_acquire) + 1) & kMask) == head_.v.load(std::memory_order_acquire); }
    std::size_t approx_size() const noexcept { return (tail_.v.load(std::memory_order_acquire) - head_.v.load(std::memory_order_acquire) + Cap) & kMask; }

    static constexpr std::size_t capacity() noexcept { return Cap - 1; }
};

} // namespace vpl
