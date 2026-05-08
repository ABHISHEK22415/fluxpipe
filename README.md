# ⚡ FluxPipe — High-Performance C++20 Video Processing Pipeline

A showcase of **modern C++ systems programming** — zero-dependency, header-only filters, lock-free concurrency, and SIMD-accelerated pixel kernels. All running through a multi-stage concurrent pipeline with a live terminal dashboard.

## What's Inside

| Component | Technique |
|---|---|
| `SPSCQueue<T,Cap>` | Wait-free, cache-line–aligned circular buffer |
| `Pipeline` | Multi-threaded DAG — each stage in its own thread, connected via blocking channels; EOF cascades automatically |
| `ThreadPool` | Work-stealing pool for parallel row processing inside filters |
| `GrayscaleFilter` | **ARM NEON / x86 AVX2 / SSE4 SIMD** dispatch — 8 pixels per instruction |
| `GaussianBlur` | Separable two-pass convolution (O(N·k) vs O(N·k²)) |
| `SobelEdge` | 3×3 gradient magnitude with parallelised rows |
| `PixelSort` | Glitch-art: sorts bright pixel runs by luminance |
| `ChromaticAberration` | Per-channel horizontal shift |
| `PlasmaSource` | Multi-sine wave interference → animated rainbow field |
| `MandelbrotSource` | Deep zoom into seahorse valley, smooth orbit-trap colouring |
| `FireSource` | Heat-propagation convolution with lava colour palette |
| `ReactionDiffusionSource` | **Gray-Scott model** (Turing spots / stripes / coral) |
| `Dashboard` | ANSI-escape live terminal UI: FPS, queue depths, latency per stage |

## Quick Start

**Requirements:** C++20 compiler, CMake ≥ 3.20. No other dependencies.

```bash
git clone https://github.com/<your-username>/cpp-video-pipeline
cd cpp-video-pipeline
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Usage

```
./fluxpipe [options]

  --demo    plasma | mandelbrot | fire | rdiff     Source generator  [plasma]
  --filter  none | gray | blur | sobel | chain | glitch   Filter chain  [chain]
  --width   <px>     Frame width            [640]
  --height  <px>     Frame height           [360]
  --frames  <n>      Number of frames       [60]
  --output  <dir>    Write PPM sequence here (omit = no file I/O)
  --benchmark        No output, pure throughput test
```

### Examples

```bash
# Full-chain: grayscale → blur → sobel → invert → vignette
./fluxpipe --demo plasma --filter chain --frames 120 --output out/

# Glitch art: chromatic aberration + pixel sort + vignette
./fluxpipe --demo fire --filter glitch --frames 60 --output out/

# Fractal zoom — no post-processing
./fluxpipe --demo mandelbrot --filter none --frames 200 --output frames/

# Turing patterns
./fluxpipe --demo rdiff --filter none --width 512 --height 512 --frames 300 --output rdiff/

# Throughput benchmark (1080p, no I/O)
./fluxpipe --benchmark --demo plasma --filter chain --width 1920 --height 1080 --frames 120
```

### Convert to video (requires ffmpeg)

```bash
ffmpeg -framerate 30 -i out/frame_%06d.ppm -c:v libx264 -pix_fmt yuv420p out.mp4
```

## Benchmark Results (Apple M-series, Release build)

| Resolution | Filter Chain | Throughput |
|---|---|---|
| 640×360 | glitch (3 stages) | ~1,100 fps |
| 640×360 | chain (5 stages) | ~350 fps |
| 1280×720 | chain (5 stages) | ~37 fps |

The bottleneck is Gaussian blur (separable convolution) — a GPU-side or FFT-based blur would push this well past 120fps at 720p.

## Architecture

```
Source thread                     Per-stage threads
──────────────                    ─────────────────
PlasmaSource → [Chan 0] → Stage 0 → [Chan 1] → Stage 1 → … → [Chan N] → Sink
                           (Gray)               (Blur)                (Write)
```

- **Zero-copy frame sharing:** `Frame` is a value type backed by a `shared_ptr<vector<uint8_t>>`. Sharing is O(1); mutation calls `clone()`.
- **Back-pressure:** bounded `Channel<Frame>` blocks the producer when any stage lags. Queue depths are visible in the dashboard.
- **SIMD dispatch:** compile-time `#if FLUXPIPE_NEON / AVX2 / SSE4` selects the fastest available kernel. Falls back to scalar automatically.

## Project Layout

```
include/vpl/
  Frame.hpp            — Core video frame (zero-copy semantics)
  SPSCQueue.hpp        — Wait-free lock-free SPSC queue
  Pipeline.hpp         — Multi-stage concurrent pipeline + metrics
  ThreadPool.hpp       — Work-stealing thread pool
  filters/
    Grayscale.hpp      — SIMD luminance conversion
    GaussianBlur.hpp   — Separable two-pass convolution
    SobelEdge.hpp      — Gradient magnitude edge detector
    Effects.hpp        — Invert, vignette, chromatic aberration, pixel-sort, pixelate
  sources/
    PlasmaSource.hpp   — Plasma, Mandelbrot, Fire, Reaction-Diffusion generators
  sinks/
    PPMSink.hpp        — PPM sequence writer + NullSink
  Dashboard.hpp        — ANSI terminal live stats UI
src/
  main.cpp             — CLI entry point, demo modes
```

## License

MIT
