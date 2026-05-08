cmake_minimum_required(VERSION 3.20)

project(FluxPipe
    VERSION 1.0.0
    DESCRIPTION "High-Performance Real-Time Video Processing Pipeline in Modern C++20"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-O3 -Wall -Wextra -Wpedantic -Wno-unused-parameter)

# ── SIMD auto-detection ───────────────────────────────────────────────────────
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    message(STATUS "[FluxPipe] ARM64 → enabling NEON SIMD")
    add_compile_options(-march=armv8-a+simd)
    add_compile_definitions(FLUXPIPE_NEON=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-mavx2" HAVE_AVX2)
    if(HAVE_AVX2)
        message(STATUS "[FluxPipe] x86_64 → enabling AVX2 SIMD")
        add_compile_options(-mavx2 -mfma)
        add_compile_definitions(FLUXPIPE_AVX2=1)
    else()
        message(STATUS "[FluxPipe] x86_64 → falling back to SSE4.1")
        add_compile_options(-msse4.1)
        add_compile_definitions(FLUXPIPE_SSE4=1)
    endif()
endif()

include_directories(include)

add_executable(fluxpipe src/main.cpp)
target_compile_features(fluxpipe PRIVATE cxx_std_20)

find_package(Threads REQUIRED)
target_link_libraries(fluxpipe Threads::Threads)

# ── Optional OpenCV (camera / video file) ────────────────────────────────────
find_package(OpenCV QUIET COMPONENTS core videoio highgui imgproc)
if(OpenCV_FOUND)
    message(STATUS "[FluxPipe] OpenCV ${OpenCV_VERSION} found — real video sources enabled")
    target_compile_definitions(fluxpipe PRIVATE FLUXPIPE_HAS_OPENCV=1)
    target_include_directories(fluxpipe PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(fluxpipe ${OpenCV_LIBS})
else()
    message(STATUS "[FluxPipe] OpenCV not found — running with synthetic sources only")
endif()

install(TARGETS fluxpipe DESTINATION bin)
