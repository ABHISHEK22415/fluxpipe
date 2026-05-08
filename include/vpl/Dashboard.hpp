#pragma once
#include "Pipeline.hpp"
#include <cstdio>
#include <string>
#include <chrono>

namespace vpl {

namespace ansi {
    constexpr const char* RESET   = "\033[0m";
    constexpr const char* BOLD    = "\033[1m";
    constexpr const char* RED     = "\033[31m";
    constexpr const char* GREEN   = "\033[32m";
    constexpr const char* YELLOW  = "\033[33m";
    constexpr const char* CYAN    = "\033[36m";
    constexpr const char* BLUE    = "\033[34m";
    constexpr const char* CLEAR   = "\033[2J\033[H";
    constexpr const char* UP      = "\033[A";
}

class Dashboard {
    const Pipeline&  pipe_;
    uint64_t         prev_frames_ = 0;
    Frame::TimePoint prev_t_      = Frame::Clock::now();
    int              lines_drawn_ = 0;

    static std::string bar(std::size_t val, std::size_t max, int w = 20) {
        if (max == 0) return std::string(w, ' ');
        const int filled = static_cast<int>(val * w / max);
        std::string b;
        b += ansi::GREEN;
        for (int i = 0; i < filled; ++i)   b += '#';
        b += ansi::RESET;
        for (int i = filled; i < w; ++i)   b += '-';
        return b;
    }

public:
    explicit Dashboard(const Pipeline& p) : pipe_(p) {}

    void print_header() {
        std::printf("%s", ansi::CLEAR);
        std::printf("%s  FluxPipe — Video Processing Pipeline%s\n\n", ansi::BOLD, ansi::RESET);
    }

    void render(uint64_t frames_done, uint64_t total_frames) {
        if (lines_drawn_ > 0)
            for (int i = 0; i < lines_drawn_; ++i) std::printf("%s", ansi::UP);

        auto now       = Frame::Clock::now();
        double dt      = std::chrono::duration<double>(now - prev_t_).count();
        double inst_fps = dt > 0.01 ? (frames_done - prev_frames_) / dt : 0.0;
        double elapsed  = pipe_.elapsed_sec();
        double avg_fps  = elapsed > 0.0 ? frames_done / elapsed : 0.0;

        prev_frames_ = frames_done;
        prev_t_      = now;

        int lines = 0;

        const int pw = 40;
        const int dw = total_frames > 0 ? static_cast<int>(frames_done * pw / total_frames)
                                        : static_cast<int>(frames_done % pw);
        std::printf("  %sProgress%s  [", ansi::BOLD, ansi::RESET);
        for (int i = 0; i < dw; ++i)  std::printf("%s#%s", ansi::GREEN, ansi::RESET);
        for (int i = dw; i < pw; ++i) std::printf(".");
        if (total_frames > 0)
            std::printf("] %llu / %llu\n", (unsigned long long)frames_done, (unsigned long long)total_frames);
        else
            std::printf("] %llu\n", (unsigned long long)frames_done);
        ++lines;

        std::printf("  FPS  instant: %s%.1f%s  avg: %s%.1f%s  elapsed: %.1fs\n",
                     ansi::YELLOW, inst_fps, ansi::RESET,
                     ansi::CYAN,   avg_fps,  ansi::RESET, elapsed);
        ++lines;

        std::printf("\n"); ++lines;

        std::printf("  %s%-18s  %8s  %9s  %-22s%s\n",
                     ansi::BOLD, "Stage", "Frames", "Avg(ms)", "Queue", ansi::RESET);
        std::printf("  %s%s%s\n", ansi::BLUE, std::string(62, '-').c_str(), ansi::RESET);
        lines += 2;

        for (const auto& s : pipe_.snapshot()) {
            const char* lc = s.avg_ms < 1.0 ? ansi::GREEN : s.avg_ms < 5.0 ? ansi::YELLOW : ansi::RED;
            std::printf("  %-18s  %8llu  %s%8.2f%s  [%s] %2zu/%zu\n",
                         s.name.substr(0,18).c_str(), (unsigned long long)s.frames_in,
                         lc, s.avg_ms, ansi::RESET,
                         bar(s.queue_depth, s.queue_cap).c_str(),
                         s.queue_depth, s.queue_cap);
            ++lines;
        }

        std::printf("\n"); ++lines;
        std::fflush(stdout);
        lines_drawn_ = lines;
    }

    void print_summary(uint64_t total_frames) {
        const double elapsed = pipe_.elapsed_sec();
        const double fps     = elapsed > 0 ? total_frames / elapsed : 0;
        std::printf("\n  %sSummary%s\n", ansi::BOLD, ansi::RESET);
        std::printf("  Frames    : %llu\n", (unsigned long long)total_frames);
        std::printf("  Time      : %.3f s\n", elapsed);
        std::printf("  Throughput: %s%.1f fps%s\n", ansi::GREEN, fps, ansi::RESET);
        std::printf("  Mpx/s     : %s%.2f%s\n\n", ansi::YELLOW, fps * 1280 * 720 / 1e6, ansi::RESET);
        std::printf("  %s%-18s  %8s  %10s%s\n", ansi::BOLD, "Stage", "Frames", "Avg(ms)", ansi::RESET);
        for (const auto& s : pipe_.snapshot())
            std::printf("  %-18s  %8llu  %10.3f\n", s.name.c_str(), (unsigned long long)s.frames_in, s.avg_ms);
        std::printf("\n");
    }
};

} // namespace vpl
