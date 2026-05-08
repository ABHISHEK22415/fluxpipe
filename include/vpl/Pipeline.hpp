#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "Frame.hpp"

namespace vpl {

template<typename T>
class Channel {
    mutable std::mutex      mu_;
    std::condition_variable cv_push_, cv_pop_;
    std::deque<T>           buf_;
    std::size_t             cap_;
    bool                    closed_ = false;

public:
    explicit Channel(std::size_t cap = 32) : cap_(cap) {}

    bool send(T item) {
        std::unique_lock lk(mu_);
        cv_push_.wait(lk, [&]{ return buf_.size() < cap_ || closed_; });
        if (closed_) return false;
        buf_.push_back(std::move(item));
        cv_pop_.notify_one();
        return true;
    }

    std::optional<T> recv() {
        std::unique_lock lk(mu_);
        cv_pop_.wait(lk, [&]{ return !buf_.empty() || closed_; });
        if (buf_.empty()) return std::nullopt;
        T v = std::move(buf_.front());
        buf_.pop_front();
        cv_push_.notify_one();
        return v;
    }

    void close() {
        std::lock_guard lk(mu_);
        closed_ = true;
        cv_pop_.notify_all();
        cv_push_.notify_all();
    }

    std::size_t size()       const { std::lock_guard lk(mu_); return buf_.size(); }
    std::size_t capacity()   const noexcept { return cap_; }
    bool        is_closed()  const { std::lock_guard lk(mu_); return closed_; }
};

struct StageMetrics {
    std::atomic<uint64_t> frames_in{0};
    std::atomic<uint64_t> total_ns{0};

    double avg_ms() const noexcept {
        auto fi = frames_in.load();
        return fi ? static_cast<double>(total_ns.load()) / (fi * 1e6) : 0.0;
    }
    double fps(double elapsed_sec) const noexcept {
        return elapsed_sec > 0.0 ? frames_in.load() / elapsed_sec : 0.0;
    }
};

class Pipeline {
    static constexpr std::size_t kQueueDepth = 24;
    using ChanPtr = std::shared_ptr<Channel<Frame>>;

    struct StageNode {
        std::string                 name;
        std::function<Frame(Frame)> fn;
        ChanPtr                     in;
        ChanPtr                     out;
        std::thread                 worker;
        StageMetrics                metrics;

        StageNode()                            = default;
        StageNode(const StageNode&)            = delete;
        StageNode& operator=(const StageNode&) = delete;
        StageNode(StageNode&&)                 = delete;
        StageNode& operator=(StageNode&&)      = delete;
    };

    std::vector<std::unique_ptr<StageNode>> nodes_;
    ChanPtr          source_chan_;
    ChanPtr          sink_chan_;
    bool             started_ = false;
    Frame::TimePoint t_start_;

public:
    Pipeline() = default;
    ~Pipeline() { if (started_) finish(); }

    Pipeline(const Pipeline&)            = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    Pipeline& add_stage(std::string name, std::function<Frame(Frame)> fn) {
        if (started_) throw std::logic_error("Pipeline already running");

        auto node  = std::make_unique<StageNode>();
        node->name = std::move(name);
        node->fn   = std::move(fn);
        node->in   = nodes_.empty()
            ? std::make_shared<Channel<Frame>>(kQueueDepth)
            : nodes_.back()->out;
        node->out  = std::make_shared<Channel<Frame>>(kQueueDepth);
        nodes_.push_back(std::move(node));
        return *this;
    }

    void start() {
        if (nodes_.empty()) throw std::runtime_error("Pipeline: no stages");
        if (started_)       throw std::logic_error("Already started");

        source_chan_ = nodes_.front()->in;
        sink_chan_   = nodes_.back()->out;
        t_start_     = Frame::Clock::now();
        started_     = true;

        for (auto& np : nodes_) {
            StageNode* node = np.get();
            node->worker = std::thread([node]() {
                while (true) {
                    auto opt = node->in->recv();
                    if (!opt) break;

                    auto t0   = Frame::Clock::now();
                    Frame out = node->fn(std::move(*opt));
                    auto t1   = Frame::Clock::now();

                    node->metrics.total_ns.fetch_add(
                        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()),
                        std::memory_order_relaxed);
                    node->metrics.frames_in.fetch_add(1, std::memory_order_relaxed);

                    node->out->send(std::move(out));
                }
                node->out->close();
            });
        }
    }

    bool                 push(Frame f) { return source_chan_->send(std::move(f)); }
    std::optional<Frame> pull()        { return sink_chan_->recv(); }

    void finish() {
        if (!started_) return;
        source_chan_->close();
        for (auto& np : nodes_)
            if (np->worker.joinable()) np->worker.join();
        started_ = false;
    }

    double elapsed_sec() const {
        return std::chrono::duration<double>(Frame::Clock::now() - t_start_).count();
    }

    struct SnapMetrics {
        std::string name;
        uint64_t    frames_in;
        double      avg_ms;
        std::size_t queue_depth;
        std::size_t queue_cap;
    };

    std::vector<SnapMetrics> snapshot() const {
        std::vector<SnapMetrics> v;
        v.reserve(nodes_.size());
        for (const auto& np : nodes_) {
            v.push_back({
                np->name,
                np->metrics.frames_in.load(),
                np->metrics.avg_ms(),
                np->in->size(),
                np->in->capacity()
            });
        }
        return v;
    }

    using Clock = Frame::Clock;
};

} // namespace vpl
