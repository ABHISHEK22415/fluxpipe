#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace vpl {

class ThreadPool {
    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex                        mu_;
    std::condition_variable           cv_;
    std::atomic<bool>                 stop_{false};

public:
    explicit ThreadPool(std::size_t n = std::thread::hardware_concurrency()) {
        for (std::size_t i = 0; i < n; ++i)
            workers_.emplace_back([this]() { worker_loop(); });
    }

    ~ThreadPool() {
        stop_.store(true, std::memory_order_relaxed);
        cv_.notify_all();
        for (auto& t : workers_) t.join();
    }

    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template<typename F>
    auto submit(F&& fn) -> std::future<std::invoke_result_t<F>> {
        using R   = std::invoke_result_t<F>;
        auto pkg  = std::make_shared<std::packaged_task<R()>>(std::forward<F>(fn));
        auto fut  = pkg->get_future();
        {
            std::lock_guard lk(mu_);
            tasks_.emplace([pkg]{ (*pkg)(); });
        }
        cv_.notify_one();
        return fut;
    }

    void parallel_for(std::size_t count, std::function<void(std::size_t, std::size_t)> fn) {
        if (count == 0) return;
        const std::size_t n_workers = workers_.size();
        const std::size_t chunk     = (count + n_workers - 1) / n_workers;

        std::vector<std::future<void>> futs;
        futs.reserve(n_workers);
        for (std::size_t base = 0; base < count; base += chunk) {
            const std::size_t end = std::min(base + chunk, count);
            futs.push_back(submit([fn, base, end]() { fn(base, end); }));
        }
        for (auto& f : futs) f.get();
    }

    std::size_t size() const noexcept { return workers_.size(); }

private:
    void worker_loop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock lk(mu_);
                cv_.wait(lk, [&]{ return stop_.load() || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) return;
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }
};

inline ThreadPool& global_pool() {
    static ThreadPool pool(std::max(1u, std::thread::hardware_concurrency() / 2));
    return pool;
}

} // namespace vpl
